# MCP Server UI

An editor-only Unreal Engine plugin that puts controls for the engine's built-in
**Model Context Protocol (MCP) server** (UE 5.8+, Experimental) in the editor status bar.

```
 ... | [▶ Start MCP]  ● Stopped  [🔌 8000] | Diagnostics | Revision Control |
```

The controls sit at the leftmost position of the status bar tool bar in the bottom-right
corner, to the left of **Diagnostics**:

| Control | What it does |
| --- | --- |
| **Start MCP / Stop MCP** | Runs `ModelContextProtocol.StartServer <port>` or `ModelContextProtocol.StopServer`. Disabled while the server is starting or stopping, or if the MCP plugin isn't loaded. |
| **Status dot + label** | Green **Running**, amber **Starting... / Stopping...**, grey **Stopped**, red **Unavailable** (or a failed start/stop). Hover for the address and any error. |
| **Port button** | Opens a modal dialog for the port (1-65535). If the server is running, it restarts on the new port. |

## Installation

1. Copy or clone this repository into your project's `Plugins/` folder, for example
   `MyProject/Plugins/MCPServerUI/`. The folder must contain `MCPServerUI.uplugin`.
2. Regenerate project files and build the editor target, or let the editor build the plugin
   when it starts.
3. The plugin depends on the engine's `ModelContextProtocol` plugin and enables it automatically.

## How it works

* **Start/stop** go through the documented console commands, so the plugin doesn't compile
  against the experimental MCP module's C++ API.
* **Status** comes from a background thread that checks whether `127.0.0.1:<port>`
  accepts TCP connections. It checks every 2 s, or every 250 ms while starting or stopping.
  This also picks up servers started some other way, such as *Auto Start Server*, the console,
  or `-ModelContextProtocolStartServer`.
* **Port** is stored in two places:
  * The MCP plugin's own settings (`ModelContextProtocolSettings`), found by reflection,
    so *Auto Start Server* uses the same port.
  * `[MCPServerUI] Port=` in `EditorPerProjectUserSettings.ini`, as a fallback.

  On startup the port is read from the MCP settings if they are found, otherwise from the
  fallback, otherwise it defaults to `8000`.

## Notes

* "Running" means something accepts connections on that port. If another process holds the
  port, the status shows Running, and Stop times out with an error in the tooltip.
* If the server is started on a port other than the one shown in the status bar, the
  status bar can't see it. Set the port with the port button so both match.
* The status bar section is added to the `StatusBar.ToolBar` tool menu with
  `EToolMenuInsertType::First`. After engine init it is moved to the front of the section
  list, so it stays left of the other status bar items.
