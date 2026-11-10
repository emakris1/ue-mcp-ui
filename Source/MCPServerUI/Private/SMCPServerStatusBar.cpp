// MCPServerUI - editor-only status bar controls for the Model Context Protocol server.

#include "SMCPServerStatusBar.h"

#include "MCPServerController.h"
#include "MCPServerUIStyle.h"
#include "SMCPPortDialog.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "MCPServerUI"

namespace MCPServerUI::Private
{
  static FText PortAsText(int32 Port)
  {
    return FText::AsNumber(Port, &FNumberFormattingOptions::DefaultNoGrouping());
  }
}

void SMCPServerStatusBar::Construct(const FArguments& InArgs, const TSharedRef<FMCPServerController>& InController)
{
  Controller = InController;

  // Same button style as the other status bar buttons (Content Drawer, Output Log, ...) so the controls blend in.
  const FButtonStyle& StatusBarButtonStyle = FAppStyle::Get().GetWidgetStyle<FButtonStyle>("StatusBar.StatusBarButton");
  const FMargin ButtonPadding(6.0f, 2.0f);

  ChildSlot
  [
    SNew(SHorizontalBox)

    // Start / Stop
    + SHorizontalBox::Slot()
    .AutoWidth()
    .VAlign(VAlign_Center)
    [
      SNew(SButton)
      .ButtonStyle(&StatusBarButtonStyle)
      .ContentPadding(ButtonPadding)
      .IsEnabled(this, &SMCPServerStatusBar::IsToggleEnabled)
      .ToolTipText(this, &SMCPServerStatusBar::GetToggleToolTip)
      .OnClicked(this, &SMCPServerStatusBar::OnToggleClicked)
      [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(0.0f, 0.0f, 4.0f, 0.0f)
        [
          SNew(SImage)
          .Image(this, &SMCPServerStatusBar::GetToggleIcon)
          .ColorAndOpacity(FSlateColor::UseForeground())
        ]
        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        [
          SNew(STextBlock)
          .Text(this, &SMCPServerStatusBar::GetToggleLabel)
        ]
      ]
    ]

    // Status indicator
    + SHorizontalBox::Slot()
    .AutoWidth()
    .VAlign(VAlign_Center)
    .Padding(4.0f, 0.0f)
    [
      SNew(SHorizontalBox)
      .ToolTipText(this, &SMCPServerStatusBar::GetStatusToolTip)
      + SHorizontalBox::Slot()
      .AutoWidth()
      .VAlign(VAlign_Center)
      .Padding(0.0f, 0.0f, 4.0f, 0.0f)
      [
        SNew(SBox)
        .WidthOverride(8.0f)
        .HeightOverride(8.0f)
        [
          SNew(SImage)
          .Image(FMCPServerUIStyle::Get().GetBrush("MCPServerUI.StatusDot"))
          .ColorAndOpacity(this, &SMCPServerStatusBar::GetStatusColor)
        ]
      ]
      + SHorizontalBox::Slot()
      .AutoWidth()
      .VAlign(VAlign_Center)
      [
        SNew(STextBlock)
        .Text(this, &SMCPServerStatusBar::GetStatusLabel)
      ]
    ]

    // Port
    + SHorizontalBox::Slot()
    .AutoWidth()
    .VAlign(VAlign_Center)
    .Padding(0.0f, 0.0f, 4.0f, 0.0f)
    [
      SNew(SButton)
      .ButtonStyle(&StatusBarButtonStyle)
      .ContentPadding(ButtonPadding)
      .ToolTipText(this, &SMCPServerStatusBar::GetPortToolTip)
      .OnClicked(this, &SMCPServerStatusBar::OnPortClicked)
      [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(0.0f, 0.0f, 3.0f, 0.0f)
        [
          SNew(SImage)
          .Image(FMCPServerUIStyle::Get().GetBrush("MCPServerUI.Port"))
          .ColorAndOpacity(FSlateColor::UseForeground())
        ]
        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        [
          SNew(STextBlock)
          .Text(this, &SMCPServerStatusBar::GetPortLabel)
        ]
      ]
    ]
  ];
}

FReply SMCPServerStatusBar::OnToggleClicked()
{
  if (const TSharedPtr<FMCPServerController> PinnedController = Controller.Pin())
  {
    PinnedController->ToggleServer();
  }
  return FReply::Handled();
}

FReply SMCPServerStatusBar::OnPortClicked()
{
  // The modal loop keeps ticking Slate, and a tool menu refresh during it could destroy this widget. Hold references so
  // neither we nor the controller disappear before the dialog returns.
  const TSharedRef<SWidget> KeepThisWidgetAlive = AsShared();
  const TSharedPtr<FMCPServerController> PinnedController = Controller.Pin();
  if (!PinnedController.IsValid())
  {
    return FReply::Handled();
  }

  const TOptional<int32> NewPort =
    SMCPPortDialog::ShowModal(PinnedController->GetPort(), PinnedController->IsRunning(), KeepThisWidgetAlive);
  if (NewPort.IsSet())
  {
    PinnedController->SetPort(NewPort.GetValue());
  }
  return FReply::Handled();
}

bool SMCPServerStatusBar::IsToggleEnabled() const
{
  const TSharedPtr<FMCPServerController> PinnedController = Controller.Pin();
  return PinnedController.IsValid() && PinnedController->GetState() != EMCPServerState::Unavailable;
}

const FSlateBrush* SMCPServerStatusBar::GetToggleIcon() const
{
  const TSharedPtr<FMCPServerController> PinnedController = Controller.Pin();
  const bool bShowStop = PinnedController.IsValid() && PinnedController->IsRunning();
  return FMCPServerUIStyle::Get().GetBrush(bShowStop ? "MCPServerUI.Stop" : "MCPServerUI.Start");
}

FText SMCPServerStatusBar::GetToggleLabel() const
{
  const TSharedPtr<FMCPServerController> PinnedController = Controller.Pin();
  const bool bShowStop = PinnedController.IsValid() && PinnedController->IsRunning();
  return bShowStop ? LOCTEXT("StopLabel", "Stop MCP") : LOCTEXT("StartLabel", "Start MCP");
}

FText SMCPServerStatusBar::GetToggleToolTip() const
{
  const TSharedPtr<FMCPServerController> PinnedController = Controller.Pin();
  if (!PinnedController.IsValid() || PinnedController->GetState() == EMCPServerState::Unavailable)
  {
    return LOCTEXT("ToggleUnavailableTip", "The Model Context Protocol module isn't loaded.");
  }
  if (PinnedController->IsRunning())
  {
    return LOCTEXT("StopTip", "Stop the Model Context Protocol server.");
  }
  return FText::Format(
    LOCTEXT("StartTip", "Start the Model Context Protocol server at {0}."),
    FText::FromString(PinnedController->GetServerUrl()));
}

FSlateColor SMCPServerStatusBar::GetStatusColor() const
{
  static const FLinearColor RunningColor(0.15f, 0.75f, 0.3f);
  static const FLinearColor StoppedColor(0.4f, 0.4f, 0.4f);
  static const FLinearColor ErrorColor(0.85f, 0.2f, 0.2f);

  const TSharedPtr<FMCPServerController> PinnedController = Controller.Pin();
  if (!PinnedController.IsValid())
  {
    return StoppedColor;
  }

  switch (PinnedController->GetState())
  {
  case EMCPServerState::Running:
    return RunningColor;
  case EMCPServerState::Unavailable:
    return ErrorColor;
  case EMCPServerState::Stopped:
  default:
    return PinnedController->GetLastError().IsEmpty() ? StoppedColor : ErrorColor;
  }
}

FText SMCPServerStatusBar::GetStatusLabel() const
{
  const TSharedPtr<FMCPServerController> PinnedController = Controller.Pin();
  if (!PinnedController.IsValid())
  {
    return FText::GetEmpty();
  }

  switch (PinnedController->GetState())
  {
  case EMCPServerState::Running:
    return LOCTEXT("StatusRunning", "Running");
  case EMCPServerState::Unavailable:
    return LOCTEXT("StatusUnavailable", "Unavailable");
  case EMCPServerState::Stopped:
  default:
    return PinnedController->GetLastError().IsEmpty()
      ? LOCTEXT("StatusStopped", "Stopped")
      : LOCTEXT("StatusFailed", "Failed");
  }
}

FText SMCPServerStatusBar::GetStatusToolTip() const
{
  const TSharedPtr<FMCPServerController> PinnedController = Controller.Pin();
  if (!PinnedController.IsValid())
  {
    return FText::GetEmpty();
  }

  const FText ServerUrl = FText::FromString(PinnedController->GetServerUrl());
  switch (PinnedController->GetState())
  {
  case EMCPServerState::Running:
    return FText::Format(LOCTEXT("TipRunning", "MCP server is running at {0}."), ServerUrl);
  case EMCPServerState::Unavailable:
    return LOCTEXT("TipUnavailable", "The Model Context Protocol module isn't loaded, so the server can't be controlled.");
  case EMCPServerState::Stopped:
  default:
    if (!PinnedController->GetLastError().IsEmpty())
    {
      return FText::Format(
        LOCTEXT("TipFailed", "MCP server failed to start at {0}:\n{1}"),
        ServerUrl,
        PinnedController->GetLastError());
    }
    return LOCTEXT("TipStopped", "MCP server is not running.");
  }
}

FText SMCPServerStatusBar::GetPortLabel() const
{
  const TSharedPtr<FMCPServerController> PinnedController = Controller.Pin();
  return PinnedController.IsValid() ? MCPServerUI::Private::PortAsText(PinnedController->GetPort()) : FText::GetEmpty();
}

FText SMCPServerStatusBar::GetPortToolTip() const
{
  return LOCTEXT("PortTip", "Change the port the MCP server listens on.");
}

#undef LOCTEXT_NAMESPACE
