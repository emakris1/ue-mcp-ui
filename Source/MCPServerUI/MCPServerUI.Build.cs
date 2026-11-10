// MCPServerUI - editor-only status bar controls for the Model Context Protocol server.

using UnrealBuildTool;

public class MCPServerUI : ModuleRules
{
  public MCPServerUI(ReadOnlyTargetRules Target) : base(Target)
  {
    PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

    PrivateDependencyModuleNames.AddRange(new string[]
    {
      "Core",
      "CoreUObject",
      "Engine",
      "InputCore",
      "ModelContextProtocol",
      "ModelContextProtocolEngine",
      "Projects",
      "Slate",
      "SlateCore",
      "ToolMenus",
      "UnrealEd",
    });
  }
}
