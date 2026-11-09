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
	static const FButtonStyle& GetStatusBarButtonStyle()
	{
		static const FName StatusBarButton(TEXT("StatusBar.StatusBarButton"));
		static const FName SimpleButton(TEXT("SimpleButton"));
		const ISlateStyle& AppStyle = FAppStyle::Get();
		return AppStyle.HasWidgetStyle<FButtonStyle>(StatusBarButton)
			? AppStyle.GetWidgetStyle<FButtonStyle>(StatusBarButton)
			: AppStyle.GetWidgetStyle<FButtonStyle>(SimpleButton);
	}

	static FText PortAsText(int32 Port)
	{
		return FText::AsNumber(Port, &FNumberFormattingOptions::DefaultNoGrouping());
	}
}

void SMCPServerStatusBar::Construct(const FArguments& InArgs, const TSharedRef<FMCPServerController>& InController)
{
	Controller = InController;

	const FButtonStyle& ButtonStyle = MCPServerUI::Private::GetStatusBarButtonStyle();
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
			.ButtonStyle(&ButtonStyle)
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
			.ButtonStyle(&ButtonStyle)
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
	if (const TSharedPtr<FMCPServerController> Pinned = Controller.Pin())
	{
		Pinned->ToggleServer();
	}
	return FReply::Handled();
}

FReply SMCPServerStatusBar::OnPortClicked()
{
	// Keep ourselves and the controller alive while the modal loop runs.
	const TSharedRef<SWidget> KeepAlive = AsShared();
	const TSharedPtr<FMCPServerController> Pinned = Controller.Pin();
	if (!Pinned.IsValid())
	{
		return FReply::Handled();
	}

	const bool bServerRunning = Pinned->IsRunning() || Pinned->GetState() == EMCPServerState::Starting;
	const TOptional<int32> NewPort = SMCPPortDialog::ShowModal(Pinned->GetPort(), bServerRunning, KeepAlive);
	if (NewPort.IsSet())
	{
		Pinned->SetPort(NewPort.GetValue());
	}
	return FReply::Handled();
}

bool SMCPServerStatusBar::IsToggleEnabled() const
{
	const TSharedPtr<FMCPServerController> Pinned = Controller.Pin();
	return Pinned.IsValid() && Pinned->GetState() != EMCPServerState::Unavailable && !Pinned->IsTransitioning();
}

const FSlateBrush* SMCPServerStatusBar::GetToggleIcon() const
{
	const TSharedPtr<FMCPServerController> Pinned = Controller.Pin();
	const bool bShowStop = Pinned.IsValid() && (Pinned->IsRunning() || Pinned->GetState() == EMCPServerState::Stopping);
	return FMCPServerUIStyle::Get().GetBrush(bShowStop ? "MCPServerUI.Stop" : "MCPServerUI.Start");
}

FText SMCPServerStatusBar::GetToggleLabel() const
{
	const TSharedPtr<FMCPServerController> Pinned = Controller.Pin();
	const bool bShowStop = Pinned.IsValid() && (Pinned->IsRunning() || Pinned->GetState() == EMCPServerState::Stopping);
	return bShowStop ? LOCTEXT("StopLabel", "Stop MCP") : LOCTEXT("StartLabel", "Start MCP");
}

FText SMCPServerStatusBar::GetToggleToolTip() const
{
	const TSharedPtr<FMCPServerController> Pinned = Controller.Pin();
	if (!Pinned.IsValid() || Pinned->GetState() == EMCPServerState::Unavailable)
	{
		return LOCTEXT("ToggleUnavailableTip", "The ModelContextProtocol console commands are not available. Enable the Model Context Protocol plugin and restart the editor.");
	}
	if (Pinned->IsRunning())
	{
		return LOCTEXT("StopTip", "Stop the Model Context Protocol server (ModelContextProtocol.StopServer).");
	}
	return FText::Format(LOCTEXT("StartTip", "Start the Model Context Protocol server on port {0} (ModelContextProtocol.StartServer {0})."),
		MCPServerUI::Private::PortAsText(Pinned->GetPort()));
}

FSlateColor SMCPServerStatusBar::GetStatusColor() const
{
	const TSharedPtr<FMCPServerController> Pinned = Controller.Pin();
	if (!Pinned.IsValid())
	{
		return FLinearColor(0.4f, 0.4f, 0.4f);
	}

	switch (Pinned->GetState())
	{
	case EMCPServerState::Running:
		return FLinearColor(0.15f, 0.75f, 0.3f);
	case EMCPServerState::Starting:
	case EMCPServerState::Stopping:
		return FLinearColor(0.95f, 0.7f, 0.1f);
	case EMCPServerState::Unavailable:
		return FLinearColor(0.85f, 0.2f, 0.2f);
	case EMCPServerState::Stopped:
	default:
		return Pinned->GetLastError().IsEmpty() ? FLinearColor(0.4f, 0.4f, 0.4f) : FLinearColor(0.85f, 0.2f, 0.2f);
	}
}

FText SMCPServerStatusBar::GetStatusLabel() const
{
	const TSharedPtr<FMCPServerController> Pinned = Controller.Pin();
	if (!Pinned.IsValid())
	{
		return FText::GetEmpty();
	}

	switch (Pinned->GetState())
	{
	case EMCPServerState::Running:     return LOCTEXT("StatusRunning", "Running");
	case EMCPServerState::Starting:    return LOCTEXT("StatusStarting", "Starting...");
	case EMCPServerState::Stopping:    return LOCTEXT("StatusStopping", "Stopping...");
	case EMCPServerState::Unavailable: return LOCTEXT("StatusUnavailable", "Unavailable");
	case EMCPServerState::Stopped:
	default:                           return LOCTEXT("StatusStopped", "Stopped");
	}
}

FText SMCPServerStatusBar::GetStatusToolTip() const
{
	const TSharedPtr<FMCPServerController> Pinned = Controller.Pin();
	if (!Pinned.IsValid())
	{
		return FText::GetEmpty();
	}

	const FText Address = FText::FromString(Pinned->GetServerAddress());
	FText Description;
	switch (Pinned->GetState())
	{
	case EMCPServerState::Running:
		Description = FText::Format(LOCTEXT("TipRunning", "MCP server is listening on {0}."), Address);
		break;
	case EMCPServerState::Starting:
		Description = FText::Format(LOCTEXT("TipStarting", "Waiting for the MCP server to listen on {0}..."), Address);
		break;
	case EMCPServerState::Stopping:
		Description = LOCTEXT("TipStopping", "Waiting for the MCP server to shut down...");
		break;
	case EMCPServerState::Unavailable:
		Description = LOCTEXT("TipUnavailable", "The Model Context Protocol plugin is not loaded, so the server cannot be controlled.");
		break;
	case EMCPServerState::Stopped:
	default:
		Description = FText::Format(LOCTEXT("TipStopped", "MCP server is not running (nothing is listening on {0})."), Address);
		break;
	}

	if (!Pinned->GetLastError().IsEmpty())
	{
		return FText::Format(LOCTEXT("TipWithError", "{0}\n\n{1}"), Description, Pinned->GetLastError());
	}
	return Description;
}

FText SMCPServerStatusBar::GetPortLabel() const
{
	const TSharedPtr<FMCPServerController> Pinned = Controller.Pin();
	return Pinned.IsValid() ? MCPServerUI::Private::PortAsText(Pinned->GetPort()) : FText::GetEmpty();
}

FText SMCPServerStatusBar::GetPortToolTip() const
{
	return LOCTEXT("PortTip", "Change the port the MCP server listens on.");
}

#undef LOCTEXT_NAMESPACE
