# MCP Server UI

An editor-only Unreal Engine plugin that puts controls for the engine's built-in **Model Context Protocol (MCP)
server** (UE 5.8+, Experimental) in the level editor's status bar.

```
 ... | [▶ Start MCP]  ● Stopped  [🔌 8000] | Diagnostics | Trace | Revision Control |
```

The controls sit at the leftmost position of the status bar tool bar in the bottom-right corner, to the left of
**Diagnostics**:

| Control | What it does |
| --- | --- |
| **Start MCP / Stop MCP** | Starts the server on the configured port and URL path, or stops it. |
| **Status dot + label** | Green **Running**, grey **Stopped**, red **Failed**. Hover for the URL and any error. |
| **Port button** | Opens a dialog to change the port. A running server restarts on the new port. |

## Installation

1. Copy or clone this repository into your project's `Plugins/` folder, for example
   `MyProject/Plugins/MCPServerUI/`. The folder must contain `MCPServerUI.uplugin`.
2. Regenerate project files and build the editor target, or let the editor build the plugin when it starts.
3. The plugin depends on the engine's `ModelContextProtocol` plugin (*Unreal MCP*, Experimental) and enables it
   automatically.

## How it works

* **Start/stop** call `IModelContextProtocolModule::StartServer` / `StopServer` directly, passing the port and URL
  path from the MCP settings. The `ModelContextProtocol.StartServer <port>` console command would ignore a custom URL
  path.
* **Status** comes from `FModelContextProtocolServer::IsServerRunning()`, so it also reflects servers started by
  *Auto Start Server*, the console or `-ModelContextProtocolStartServer`.
* **Start failures:** if the port can't be bound, the engine only logs an error and the server still reports itself
  as running. The plugin watches the log during its own start call, stops the half-started server, and shows
  **Failed** with the logged error.
* **Port** is the MCP plugin's own `ServerPortNumber` setting (Model Context Protocol settings, saved to
  `EditorPerProjectUserSettings.ini`), so *Auto Start Server* uses the same port.

## Notes

* **Stopping doesn't free the port.** The engine's HTTP server keeps its listener open after the MCP server stops
  (requests get a 404) until the editor exits. Starting again on the same port is fine. Another program can't take
  that port until then.
* **Failures outside the button:** start failures from *Auto Start Server* or the console aren't detected. The
  status shows Running, and the Output Log has the `LogHttpListener` bind error.
* **Command-line port:** `-ModelContextProtocolPort=N` only affects *Auto Start Server*. While the server is running,
  the port button shows the live port; while it's stopped, it shows the saved setting.
* **Placement:** the controls are added to the `LevelEditor.StatusBar.ToolBar` tool menu as a `First` section and
  then moved to the front of its section list, so they stay left of Diagnostics, Trace, Live Link Hub and Multi-User.
  Asset editor status bars don't show them.
