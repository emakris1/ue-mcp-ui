// MCPServerUI - editor-only status bar controls for the Model Context Protocol server.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"

/** Slate style set holding the plugin's icons and brushes. */
class FMCPServerUIStyle
{
public:
  static void Initialize();
  static void Shutdown();

  static const ISlateStyle& Get();
  static FName GetStyleSetName();

private:
  static TSharedPtr<FSlateStyleSet> StyleSet;
};
