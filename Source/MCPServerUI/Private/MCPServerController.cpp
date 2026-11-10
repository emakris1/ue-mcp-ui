// MCPServerUI - editor-only status bar controls for the Model Context Protocol server.

#include "MCPServerController.h"

#include "IModelContextProtocolModule.h"
#include "ModelContextProtocol.h"
#include "ModelContextProtocolServer.h"
#include "ModelContextProtocolSettings.h"

#include "CoreGlobals.h"
#include "Logging/LogVerbosity.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"

#define LOCTEXT_NAMESPACE "MCPServerUI"

namespace MCPServerUI::Private
{
  /**
   * Captures the first error logged on the game thread while the server is starting.
   *
   * FModelContextProtocolServer::IsServerRunning() only reports that its HTTP routes exist. If the port is already taken,
   * the HTTP listener logs "unable to bind" and the server still claims to be running, so the log is the only signal that
   * the start actually failed. The bind happens synchronously inside StartServer, so scoping the capture to that call is
   * enough to attribute any error to it.
   */
  class FServerStartErrorCapture final : public FOutputDevice
  {
  public:
    FServerStartErrorCapture()
    {
      GLog->AddOutputDevice(this);
    }

    virtual ~FServerStartErrorCapture() override
    {
      GLog->RemoveOutputDevice(this);
    }

    bool HasError() const
    {
      return !FirstErrorMessage.IsEmpty();
    }

    const FString& GetFirstErrorMessage() const
    {
      return FirstErrorMessage;
    }

    virtual void Serialize(const TCHAR* Message, ELogVerbosity::Type Verbosity, const FName& Category) override
    {
      // Other threads keep logging while we wait. Only the game thread is inside our StartServer call, and filtering on
      // it also means FirstErrorMessage is only ever touched by one thread.
      if (!IsInGameThread())
      {
        return;
      }

      if ((Verbosity & ELogVerbosity::VerbosityMask) > ELogVerbosity::Error)
      {
        return;
      }

      // LogHttpListener is declared in a private HTTPServer header, so it can only be matched by name.
      static const FName HttpListenerCategory(TEXT("LogHttpListener"));
      const bool bIsServerStartCategory =
        Category == HttpListenerCategory || Category == LogModelContextProtocol.GetCategoryName();

      if (bIsServerStartCategory && FirstErrorMessage.IsEmpty())
      {
        FirstErrorMessage = Message;
      }
    }

    // Lets the redirector call us synchronously from the logging thread instead of buffering the message until after
    // StartServer has returned.
    virtual bool CanBeUsedOnAnyThread() const override
    {
      return true;
    }

    virtual bool CanBeUsedOnMultipleThreads() const override
    {
      return true;
    }

  private:
    FString FirstErrorMessage;
  };
}

void FMCPServerController::StartServer()
{
  IModelContextProtocolModule* McpModule = IModelContextProtocolModule::Get();
  if (!McpModule)
  {
    LastError = LOCTEXT("ModuleUnavailable", "The Model Context Protocol module isn't loaded.");
    return;
  }

  // Use the configured URL path as well as the port. The ModelContextProtocol.StartServer console command only takes a
  // port and would silently fall back to the default path.
  const uint32 Port = GetDefault<UModelContextProtocolSettings>()->ServerPortNumber;
  const FString UrlPath = UE::ModelContextProtocol::GetServerUrlPath();

  FString StartErrorMessage;
  {
    MCPServerUI::Private::FServerStartErrorCapture ErrorCapture;
    McpModule->StartServer(Port, UrlPath);
    StartErrorMessage = ErrorCapture.GetFirstErrorMessage();
  }

  if (!StartErrorMessage.IsEmpty())
  {
    // Don't leave the routes registered on a listener that never bound. It would report "running" while serving nothing.
    McpModule->StopServer();
    LastError = FText::FromString(StartErrorMessage);
    return;
  }

  if (!FindRunningServer())
  {
    LastError = LOCTEXT("StartFailed", "The MCP server didn't start. See the Output Log for details.");
    return;
  }

  LastError = FText::GetEmpty();
}

void FMCPServerController::StopServer()
{
  if (IModelContextProtocolModule* McpModule = IModelContextProtocolModule::Get())
  {
    McpModule->StopServer();
  }
  LastError = FText::GetEmpty();
}

void FMCPServerController::ToggleServer()
{
  if (IsRunning())
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

  UModelContextProtocolSettings* Settings = GetMutableDefault<UModelContextProtocolSettings>();
  if (Settings->ServerPortNumber != static_cast<uint32>(NewPort))
  {
    Settings->ServerPortNumber = static_cast<uint32>(NewPort);
    Settings->SaveConfig();
  }

  // Compare against the live port, not the old setting. The server may have been started on another port from the
  // console or with -ModelContextProtocolPort.
  const FModelContextProtocolServer* RunningServer = FindRunningServer();
  if (RunningServer && RunningServer->GetServerPort() != static_cast<uint32>(NewPort))
  {
    StopServer();
    StartServer();
  }
}

EMCPServerState FMCPServerController::GetState() const
{
  if (!IModelContextProtocolModule::Get())
  {
    return EMCPServerState::Unavailable;
  }
  return FindRunningServer() ? EMCPServerState::Running : EMCPServerState::Stopped;
}

int32 FMCPServerController::GetPort() const
{
  if (const FModelContextProtocolServer* RunningServer = FindRunningServer())
  {
    return static_cast<int32>(RunningServer->GetServerPort());
  }
  return static_cast<int32>(GetDefault<UModelContextProtocolSettings>()->ServerPortNumber);
}

FString FMCPServerController::GetServerUrl() const
{
  // The HTTP server binds to localhost unless [HTTPServer.Listeners] in the engine config says otherwise.
  return FString::Printf(TEXT("http://localhost:%d%s"), GetPort(), *UE::ModelContextProtocol::GetServerUrlPath());
}

FModelContextProtocolServer* FMCPServerController::FindRunningServer() const
{
  IModelContextProtocolModule* McpModule = IModelContextProtocolModule::Get();
  FModelContextProtocolServer* Server = McpModule ? McpModule->GetServer() : nullptr;
  return Server && Server->IsServerRunning() ? Server : nullptr;
}

#undef LOCTEXT_NAMESPACE
