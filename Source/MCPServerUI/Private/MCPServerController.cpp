// MCPServerUI - editor-only status bar controls for the Model Context Protocol server.

#include "MCPServerController.h"

#include "CoreGlobals.h"
#include "Editor.h"
#include "HAL/Event.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "IPAddress.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/ScopeLock.h"
#include "SocketSubsystem.h"
#include "Sockets.h"
#include "UObject/Class.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

#include <atomic>

#define LOCTEXT_NAMESPACE "MCPServerUI"

DEFINE_LOG_CATEGORY_STATIC(LogMCPServerUI, Log, All);

namespace MCPServerUI::Private
{
  static const TCHAR* StartServerCommand = TEXT("ModelContextProtocol.StartServer");
  static const TCHAR* StopServerCommand = TEXT("ModelContextProtocol.StopServer");

  static const TCHAR* ConfigSection = TEXT("MCPServerUI");
  static const TCHAR* ConfigPortKey = TEXT("Port");

  /** How long to wait for the server to start/stop listening before giving up. */
  static constexpr double TransitionTimeoutSeconds = 6.0;
  static constexpr uint32 IdleProbeIntervalMs = 2000;
  static constexpr uint32 TransitionProbeIntervalMs = 250;

  /** Returns true if something accepts TCP connections on 127.0.0.1:InPort. Blocking; call off the game thread. */
  static bool IsLocalPortListening(int32 InPort)
  {
    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!SocketSubsystem)
    {
      return false;
    }

    TSharedRef<FInternetAddr> Address = SocketSubsystem->CreateInternetAddr();
    bool bIsValidIp = false;
    Address->SetIp(TEXT("127.0.0.1"), bIsValidIp);
    Address->SetPort(InPort);
    if (!bIsValidIp)
    {
      return false;
    }

    FSocket* Socket = SocketSubsystem->CreateSocket(NAME_Stream, TEXT("MCPServerUI port probe"), Address->GetProtocolType());
    if (!Socket)
    {
      return false;
    }

    const bool bConnected = Socket->Connect(*Address);
    Socket->Close();
    SocketSubsystem->DestroySocket(Socket);
    return bConnected;
  }

  /**
   * The ModelContextProtocol settings live in the ModelContextProtocolEngine module. We look them up by
   * reflection so this plugin does not need to compile against that (experimental) module's headers.
   */
  static UObject* FindMCPSettingsObject()
  {
    static const TCHAR* CandidateClassPaths[] =
    {
      TEXT("/Script/ModelContextProtocolEngine.ModelContextProtocolSettings"),
      TEXT("/Script/ModelContextProtocol.ModelContextProtocolSettings"),
    };

    for (const TCHAR* ClassPath : CandidateClassPaths)
    {
      if (UClass* SettingsClass = FindObject<UClass>(nullptr, ClassPath))
      {
        return SettingsClass->GetDefaultObject();
      }
    }
    return nullptr;
  }

  static FNumericProperty* FindPortProperty(const UClass* SettingsClass)
  {
    static const FName CandidateNames[] =
    {
      TEXT("ServerPort"),
      TEXT("ServerPortNumber"),
      TEXT("PortNumber"),
      TEXT("Port"),
      TEXT("HttpServerPort"),
      TEXT("HttpPort"),
    };

    for (const FName& Name : CandidateNames)
    {
      FNumericProperty* Property = FindFProperty<FNumericProperty>(SettingsClass, Name);
      if (Property && Property->IsInteger())
      {
        return Property;
      }
    }

    for (TFieldIterator<FNumericProperty> It(SettingsClass); It; ++It)
    {
      if (It->IsInteger() && It->GetName().Contains(TEXT("Port")))
      {
        return *It;
      }
    }
    return nullptr;
  }

  static bool TryReadMCPSettingsPort(int32& OutPort)
  {
    UObject* Settings = FindMCPSettingsObject();
    FNumericProperty* PortProperty = Settings ? FindPortProperty(Settings->GetClass()) : nullptr;
    if (!PortProperty)
    {
      return false;
    }

    const int64 Value = PortProperty->GetSignedIntPropertyValue(PortProperty->ContainerPtrToValuePtr<void>(Settings));
    if (Value < 1 || Value > 65535)
    {
      return false;
    }
    OutPort = static_cast<int32>(Value);
    return true;
  }

  static bool TryWriteMCPSettingsPort(int32 InPort)
  {
    UObject* Settings = FindMCPSettingsObject();
    FNumericProperty* PortProperty = Settings ? FindPortProperty(Settings->GetClass()) : nullptr;
    if (!PortProperty)
    {
      return false;
    }

    PortProperty->SetIntPropertyValue(PortProperty->ContainerPtrToValuePtr<void>(Settings), static_cast<int64>(InPort));

    if (PortProperty->HasAnyPropertyFlags(CPF_Config))
    {
      if (Settings->GetClass()->HasAnyClassFlags(CLASS_DefaultConfig))
      {
        Settings->TryUpdateDefaultConfigFile();
      }
      else
      {
        Settings->SaveConfig();
      }
    }

    UE_LOG(LogMCPServerUI, Log, TEXT("Set %s.%s to %d"), *Settings->GetClass()->GetName(), *PortProperty->GetName(), InPort);
    return true;
  }
}

/** Background thread that repeatedly checks whether the configured port is accepting connections. */
class FMCPServerProbe : public FRunnable
{
public:
  FMCPServerProbe(int32 InPort)
    : WakeEvent(FPlatformProcess::GetSynchEventFromPool(false))
    , TargetPort(InPort)
  {
  }

  virtual ~FMCPServerProbe() override
  {
    FPlatformProcess::ReturnSynchEventToPool(WakeEvent);
  }

  void SetTarget(int32 InPort, uint32 InIntervalMs)
  {
    TargetPort = InPort;
    IntervalMs = InIntervalMs;
  }

  /** Wakes the thread so it probes immediately instead of waiting for the next interval. */
  void ProbeNow()
  {
    WakeEvent->Trigger();
  }

  bool ConsumeResult(int32& OutPort, bool& bOutListening)
  {
    FScopeLock Lock(&ResultLock);
    if (!bHasNewResult)
    {
      return false;
    }
    bHasNewResult = false;
    OutPort = ResultPort;
    bOutListening = bResultListening;
    return true;
  }

  virtual uint32 Run() override
  {
    while (!bStopRequested)
    {
      const int32 Port = TargetPort.load();
      const bool bListening = MCPServerUI::Private::IsLocalPortListening(Port);
      {
        FScopeLock Lock(&ResultLock);
        ResultPort = Port;
        bResultListening = bListening;
        bHasNewResult = true;
      }

      if (!bStopRequested)
      {
        WakeEvent->Wait(IntervalMs.load());
      }
    }
    return 0;
  }

  virtual void Stop() override
  {
    bStopRequested = true;
    WakeEvent->Trigger();
  }

private:
  FEvent* WakeEvent;
  std::atomic<int32> TargetPort;
  std::atomic<uint32> IntervalMs { MCPServerUI::Private::IdleProbeIntervalMs };
  std::atomic<bool> bStopRequested { false };

  FCriticalSection ResultLock;
  int32 ResultPort = 0;
  bool bResultListening = false;
  bool bHasNewResult = false;
};

FMCPServerController::FMCPServerController() = default;

FMCPServerController::~FMCPServerController()
{
  Shutdown();
}

void FMCPServerController::Initialize()
{
  using namespace MCPServerUI::Private;

  int32 SavedPort = DefaultPort;
  if (TryReadMCPSettingsPort(SavedPort))
  {
    Port = SavedPort;
  }
  else if (GConfig && GConfig->GetInt(ConfigSection, ConfigPortKey, SavedPort, GEditorPerProjectIni) && SavedPort >= 1 && SavedPort <= 65535)
  {
    Port = SavedPort;
  }

  State = AreCommandsAvailable() ? EMCPServerState::Stopped : EMCPServerState::Unavailable;

  Probe = MakeUnique<FMCPServerProbe>(Port);
  ProbeThread = FRunnableThread::Create(Probe.Get(), TEXT("MCPServerUIProbe"), 0, TPri_BelowNormal);

  TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateSP(this, &FMCPServerController::Tick), 0.1f);
}

void FMCPServerController::Shutdown()
{
  if (TickHandle.IsValid())
  {
    FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
    TickHandle.Reset();
  }

  if (ProbeThread)
  {
    ProbeThread->Kill(true);
    delete ProbeThread;
    ProbeThread = nullptr;
  }
  Probe.Reset();
}

void FMCPServerController::SyncPortFromMCPSettings()
{
  int32 SettingsPort = Port;
  if (MCPServerUI::Private::TryReadMCPSettingsPort(SettingsPort) && SettingsPort != Port && !IsRunning() && !IsTransitioning())
  {
    Port = SettingsPort;
    SavePort();
    UpdateProbeTarget();
  }
}

bool FMCPServerController::AreCommandsAvailable() const
{
  return IConsoleManager::Get().FindConsoleObject(MCPServerUI::Private::StartServerCommand) != nullptr
    && IConsoleManager::Get().FindConsoleObject(MCPServerUI::Private::StopServerCommand) != nullptr;
}

FString FMCPServerController::GetServerAddress() const
{
  return FString::Printf(TEXT("127.0.0.1:%d"), Port);
}

void FMCPServerController::StartServer()
{
  if (!AreCommandsAvailable())
  {
    State = EMCPServerState::Unavailable;
    return;
  }
  if (State == EMCPServerState::Running || IsTransitioning())
  {
    return;
  }

  LastError = FText::GetEmpty();
  if (!ExecuteConsoleCommand(FString::Printf(TEXT("%s %d"), MCPServerUI::Private::StartServerCommand, Port)))
  {
    LastError = LOCTEXT("StartCommandFailed", "The StartServer console command was not handled. See the Output Log.");
    return;
  }
  BeginTransition(EMCPServerState::Starting);
}

void FMCPServerController::StopServer()
{
  if (!AreCommandsAvailable())
  {
    State = EMCPServerState::Unavailable;
    return;
  }
  if (State != EMCPServerState::Running)
  {
    return;
  }

  LastError = FText::GetEmpty();
  if (!ExecuteConsoleCommand(MCPServerUI::Private::StopServerCommand))
  {
    LastError = LOCTEXT("StopCommandFailed", "The StopServer console command was not handled. See the Output Log.");
    return;
  }
  BeginTransition(EMCPServerState::Stopping);
}

void FMCPServerController::ToggleServer()
{
  if (State == EMCPServerState::Running)
  {
    StopServer();
  }
  else
  {
    StartServer();
  }
}

void FMCPServerController::SetPort(int32 NewPort)
{
  NewPort = FMath::Clamp(NewPort, 1, 65535);
  if (NewPort == Port)
  {
    return;
  }

  const bool bRestart = State == EMCPServerState::Running || State == EMCPServerState::Starting;
  if (bRestart)
  {
    ExecuteConsoleCommand(MCPServerUI::Private::StopServerCommand);
  }

  Port = NewPort;
  SavePort();
  LastError = FText::GetEmpty();

  if (bRestart)
  {
    State = EMCPServerState::Stopped;
    StartServer();
  }
  else
  {
    State = AreCommandsAvailable() ? EMCPServerState::Stopped : EMCPServerState::Unavailable;
    UpdateProbeTarget();
  }
}

bool FMCPServerController::Tick(float DeltaTime)
{
  if (State == EMCPServerState::Unavailable && AreCommandsAvailable())
  {
    State = EMCPServerState::Stopped;
  }

  int32 ProbedPort = 0;
  bool bListening = false;
  if (Probe && Probe->ConsumeResult(ProbedPort, bListening))
  {
    HandleProbeResult(ProbedPort, bListening);
  }
  return true;
}

void FMCPServerController::HandleProbeResult(int32 ProbedPort, bool bListening)
{
  if (ProbedPort != Port)
  {
    return; // Stale result from before a port change.
  }

  const double Elapsed = FPlatformTime::Seconds() - TransitionStartTime;
  const bool bTimedOut = Elapsed > MCPServerUI::Private::TransitionTimeoutSeconds;

  switch (State)
  {
  case EMCPServerState::Starting:
    if (bListening)
    {
      State = EMCPServerState::Running;
      LastError = FText::GetEmpty();
    }
    else if (bTimedOut)
    {
      State = EMCPServerState::Stopped;
      LastError = FText::Format(LOCTEXT("StartTimedOut", "The server did not start listening on port {0}. See the Output Log for details."), FText::AsNumber(Port, &FNumberFormattingOptions::DefaultNoGrouping()));
    }
    break;

  case EMCPServerState::Stopping:
    if (!bListening)
    {
      State = EMCPServerState::Stopped;
    }
    else if (bTimedOut)
    {
      State = EMCPServerState::Running;
      LastError = FText::Format(LOCTEXT("StopTimedOut", "Port {0} is still accepting connections after StopServer. Another process may be using it."), FText::AsNumber(Port, &FNumberFormattingOptions::DefaultNoGrouping()));
    }
    break;

  default:
    if (bListening)
    {
      State = EMCPServerState::Running;
    }
    else
    {
      State = AreCommandsAvailable() ? EMCPServerState::Stopped : EMCPServerState::Unavailable;
    }
    break;
  }

  UpdateProbeTarget();
}

void FMCPServerController::BeginTransition(EMCPServerState NewState)
{
  State = NewState;
  TransitionStartTime = FPlatformTime::Seconds();
  UpdateProbeTarget();
  if (Probe)
  {
    Probe->ProbeNow();
  }
}

void FMCPServerController::UpdateProbeTarget()
{
  if (Probe)
  {
    using namespace MCPServerUI::Private;
    Probe->SetTarget(Port, IsTransitioning() ? TransitionProbeIntervalMs : IdleProbeIntervalMs);
  }
}

bool FMCPServerController::ExecuteConsoleCommand(const FString& Command)
{
  UE_LOG(LogMCPServerUI, Log, TEXT("Executing: %s"), *Command);
  UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
  return IConsoleManager::Get().ProcessUserConsoleInput(*Command, *GLog, World);
}

void FMCPServerController::SavePort() const
{
  MCPServerUI::Private::TryWriteMCPSettingsPort(Port);

  if (GConfig)
  {
    GConfig->SetInt(MCPServerUI::Private::ConfigSection, MCPServerUI::Private::ConfigPortKey, Port, GEditorPerProjectIni);
    GConfig->Flush(false, GEditorPerProjectIni);
  }
}

#undef LOCTEXT_NAMESPACE
