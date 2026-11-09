# Handoff: MCP Server UI plugin (continue locally)

This file is for a Claude Code agent picking up this work on a local machine. Delete it
once the work is finished.

## Goal

An **editor-only** Unreal Engine 5.8 plugin that exposes the engine's built-in Model Context
Protocol server commands (`ModelContextProtocol.StartServer [port]` and
`ModelContextProtocol.StopServer`) as editor UI. The user's requirements:

1. A **Start/Stop button** in the bottom-right status bar, at the **leftmost position of that
   tool bar (left of "Diagnostics")**.
2. A **status icon or label** showing whether the server is running/connected.
3. A **small port button** next to it that opens a **modal dialog** to change the port.

## Current state

Branch `claude/unreal-mcp-server-ui-wazfya` in `emakris1/ue-mcp-ui` has a complete first
version. It was written in a cloud container with **no engine source and no compiler**, and
Epic's docs site was blocked there. It has **never been compiled**. Some parts are guesses
(listed below) that should now be checked against the real 5.8 source.

```
MCPServerUI.uplugin                       Editor module, LoadingPhase Default, depends on "ModelContextProtocol"
Source/MCPServerUI/MCPServerUI.Build.cs
Source/MCPServerUI/Private/
  MCPServerUIModule.cpp     Adds a section to tool menu "StatusBar.ToolBar" (First), moves it to the front after engine init
  MCPServerController.*     State machine (Unavailable/Stopped/Starting/Running/Stopping); runs console commands;
                            background FRunnable checks whether 127.0.0.1:<port> accepts TCP connections;
                            reads/writes the port on ModelContextProtocolSettings by REFLECTION (property name guessed)
                            plus a fallback copy in EditorPerProjectUserSettings.ini under [MCPServerUI] Port
  SMCPServerStatusBar.*     [Start/Stop button] [status dot + label] [port button]
  SMCPPortDialog.*          Modal SWindow with SNumericEntryBox<int32> (1-65535), Apply/Cancel, Enter/Esc
  MCPServerUIStyle.*        Slate style set: Resources/Icons/{Start,Stop,Port}.svg plus a rounded-box status dot
README.md                   User-facing docs
```

## Assumptions to check against UE 5.8 source

| # | Assumption in code | Where it is in our code | What to check in engine source |
|---|---|---|---|
| 1 | The MCP plugin is named `ModelContextProtocol` | `MCPServerUI.uplugin` `"Plugins"` | The MCP plugin's `.uplugin` file name |
| 2 | Settings class is `/Script/ModelContextProtocolEngine.ModelContextProtocolSettings`; the port property is one of `ServerPort`, `ServerPortNumber`, `PortNumber`, `Port`, ... | `MCPServerController.cpp` `FindMCPSettingsObject` / `FindPortProperty` | The `UModelContextProtocolSettings` header: real property names, config file, any URL-path setting |
| 3 | The only control is through console commands; status comes from a TCP port check | `MCPServerController.cpp` | MCP module public headers (`IModelContextProtocolModule`, `FModelContextProtocolServer`): is there a running check, start/stop functions, events for server or session changes? If yes, **replace the TCP check** and possibly link the module directly instead of using reflection |
| 4 | Status bar tool bar menu is `StatusBar.ToolBar`, and Diagnostics is an entry in it | `MCPServerUIModule.cpp` | `Engine/Source/Editor/StatusBar/` (`SStatusBar.cpp`) and wherever the "Diagnostics" status bar entry is registered |
| 5 | `UToolMenu::FindInsertIndex` puts "First" sections in the order they were added, and `UToolMenu::Sections` is public and can be reordered | `MCPServerUIModule.cpp` `MoveSectionToFront` | `ToolMenu.cpp` / `ToolMenu.h` in ToolMenus |
| 6 | Style names `StatusBar.StatusBarButton`, `SimpleButton`, `PrimaryButton`, `Button`, `Brushes.Panel` exist (code falls back if a button style is missing) | `SMCPServerStatusBar.cpp`, `SMCPPortDialog.cpp` | `StarshipStyle.cpp` in EditorStyle |
| 7 | API signatures: `FSlateRoundedBoxBrush(FLinearColor, float, FVector2f)`, `FSlateVectorImageBrush`, `ISlateStyle::HasWidgetStyle<T>`, `FToolMenuEntry::InitWidget`, `FToolMenuSection::AddDynamicEntry`, `UToolMenus::RemoveSection`, `IConsoleManager::ProcessUserConsoleInput`, `FTSTicker` | various | SlateCore / Slate / ToolMenus / Core headers |

Known weakness of the TCP check: anything listening on the port counts as "Running". If
assumption 3 turns up a real running check in the MCP API, use it.

## Git commands

### 1. Plugin repo

```bash
git clone https://github.com/emakris1/ue-mcp-ui.git MCPServerUI
cd MCPServerUI
git checkout claude/unreal-mcp-server-ui-wazfya
git pull origin claude/unreal-mcp-server-ui-wazfya
```

Keep committing to `claude/unreal-mcp-server-ui-wazfya` and push with
`git push -u origin claude/unreal-mcp-server-ui-wazfya`.

### 2. Engine source, partial checkout (outside the plugin repo)

Replace `5.8` with the branch or tag the user builds from. To list options:
`git ls-remote --heads --tags https://github.com/EpicGames/UnrealEngine.git "*5.8*"`.

```bash
git clone --depth 1 --filter=blob:none --no-checkout --branch 5.8 \
  https://github.com/EpicGames/UnrealEngine.git UE5.8-src
cd UE5.8-src

git sparse-checkout set --no-cone \
  '/Engine/Plugins/**/ModelContextProtocol*/' \
  '/Engine/Source/Editor/StatusBar/' \
  '/Engine/Source/**/ToolMenus/' \
  '/Engine/Source/Editor/EditorStyle/' \
  '/Engine/Source/Runtime/SlateCore/' \
  '/Engine/Source/Runtime/Slate/' \
  '/Engine/Source/Runtime/Sockets/' \
  '/Engine/Source/Runtime/Core/Public/HAL/IConsoleManager.h' \
  '/Engine/Source/Runtime/Core/Public/Containers/Ticker.h' \
  '/Engine/Plugins/Developer/TraceUtilities/'

git checkout 5.8
```

To find paths that aren't checked out yet (works without downloading anything else), then add them:

```bash
git ls-tree -r --name-only HEAD | grep -i -E "diagnostic|statusbar|ModelContext"
git sparse-checkout add '/path/found/'
```

If the user has a full local engine install or source build, read from that instead and skip this step.

**Never copy engine source into the plugin repo.** Epic's license does not allow that kind of
redistribution.

## Building

Fastest compile check without a project, using an installed or source-built 5.8 engine:

```bat
"<UE_5.8>\Engine\Build\BatchFiles\RunUAT.bat" BuildPlugin ^
  -Plugin="<path>\MCPServerUI\MCPServerUI.uplugin" ^
  -Package="<temp>\MCPServerUI_Build" -TargetPlatforms=Win64 -Rocket
```

(`RunUAT.sh` on macOS/Linux; drop `-Rocket` for a source-built engine.)

To test in the editor, put the plugin under `<Project>/Plugins/MCPServerUI/`, open the
project, and check:
- The controls appear at the leftmost position of the status bar tool bar, left of Diagnostics.
- Start leads to Running (green); Stop leads to Stopped (grey).
- Changing the port while running restarts the server on the new port. Confirm with
  `ModelContextProtocol.StartServer` messages in the Output Log, and with an MCP client.
- With the MCP plugin disabled, the controls show Unavailable (red) and Start is disabled.

## Suggested next steps

1. Check assumptions 1–7. Fix code where it's wrong.
2. If the MCP module has a running-status API or events, switch the controller to it and
   remove `FMCPServerProbe`. Consider depending on `ModelContextProtocolEngine` directly
   for settings access instead of reflection.
3. Run the BuildPlugin compile check and fix errors.
4. Test in the editor (list above), then update `README.md` to match.
