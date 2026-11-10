// MCPServerUI - editor-only status bar controls for the Model Context Protocol server.

#pragma once

#include "CoreMinimal.h"

class FModelContextProtocolServer;

enum class EMCPServerState : uint8
{
  /** The ModelContextProtocol module isn't loaded, e.g. while the editor is shutting down. */
  Unavailable,
  Stopped,
  Running,
};

/**
 * Starts and stops the engine's Model Context Protocol server through IModelContextProtocolModule and keeps the port in
 * UModelContextProtocolSettings, so the status bar, Auto Start Server and the editor's settings panel all agree.
 */
class FMCPServerController
{
public:
  void StartServer();
  void StopServer();
  void ToggleServer();

  /** Saves the port to the MCP settings and restarts the server on it if it's running. */
  void SetPort(int32 NewPort);

  EMCPServerState GetState() const;
  bool IsRunning() const { return GetState() == EMCPServerState::Running; }

  /** The port the server is listening on while running, otherwise the port it will start on. */
  int32 GetPort() const;
  FString GetServerUrl() const;

  /** Why the last start attempt failed. Empty if it succeeded or the server was stopped since. */
  const FText& GetLastError() const { return LastError; }

private:
  FModelContextProtocolServer* FindRunningServer() const;

  FText LastError;
};
