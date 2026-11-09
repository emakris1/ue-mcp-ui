// MCPServerUI - editor-only status bar controls for the Model Context Protocol server.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class FMCPServerController;
struct FSlateBrush;

/** Status bar controls: [Start/Stop button] [status dot + label] [port button]. */
class SMCPServerStatusBar : public SCompoundWidget
{
public:
  SLATE_BEGIN_ARGS(SMCPServerStatusBar) {}
  SLATE_END_ARGS()

  void Construct(const FArguments& InArgs, const TSharedRef<FMCPServerController>& InController);

private:
  FReply OnToggleClicked();
  FReply OnPortClicked();

  bool IsToggleEnabled() const;
  const FSlateBrush* GetToggleIcon() const;
  FText GetToggleLabel() const;
  FText GetToggleToolTip() const;

  FSlateColor GetStatusColor() const;
  FText GetStatusLabel() const;
  FText GetStatusToolTip() const;

  FText GetPortLabel() const;
  FText GetPortToolTip() const;

  TWeakPtr<FMCPServerController> Controller;
};
