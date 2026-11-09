// MCPServerUI - editor-only status bar controls for the Model Context Protocol server.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"

class FMCPServerProbe;
class FRunnableThread;

enum class EMCPServerState : uint8
{
  /** The ModelContextProtocol console commands are not registered (plugin disabled / not loaded). */
  Unavailable,
  Stopped,
  Starting,
  Running,
  Stopping,
};

/**
 * Drives the engine's Model Context Protocol server through its console commands
 * (ModelContextProtocol.StartServer / ModelContextProtocol.StopServer) and tracks whether
 * the server is actually listening by probing 127.0.0.1:<Port> on a background thread.
 */
class FMCPServerController : public TSharedFromThis<FMCPServerController>
{
public:
  static constexpr int32 DefaultPort = 8000;

  FMCPServerController();
  ~FMCPServerController();

  void Initialize();
  void Shutdown();

  /** Re-reads the port from the ModelContextProtocol settings, if they can be found. */
  void SyncPortFromMCPSettings();

  void StartServer();
  void StopServer();
  void ToggleServer();

  /** Changes the port, persists it and restarts the server on the new port if it was running. */
  void SetPort(int32 NewPort);

  EMCPServerState GetState() const { return State; }
  int32 GetPort() const { return Port; }
  const FText& GetLastError() const { return LastError; }

  bool IsRunning() const { return State == EMCPServerState::Running; }
  bool IsTransitioning() const { return State == EMCPServerState::Starting || State == EMCPServerState::Stopping; }
  bool AreCommandsAvailable() const;
  FString GetServerAddress() const;

private:
  bool Tick(float DeltaTime);
  void HandleProbeResult(int32 ProbedPort, bool bListening);
  void BeginTransition(EMCPServerState NewState);
  void UpdateProbeTarget();
  bool ExecuteConsoleCommand(const FString& Command);
  void SavePort() const;

  EMCPServerState State = EMCPServerState::Stopped;
  int32 Port = DefaultPort;
  double TransitionStartTime = 0.0;
  FText LastError;

  FTSTicker::FDelegateHandle TickHandle;
  TUniquePtr<FMCPServerProbe> Probe;
  FRunnableThread* ProbeThread = nullptr;
};
