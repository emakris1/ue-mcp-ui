// MCPServerUI - editor-only status bar controls for the Model Context Protocol server.

#include "SMCPPortDialog.h"

#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "MCPServerUI"

namespace MCPServerUI::Private
{
	static const FButtonStyle& GetDialogButtonStyle(FName PreferredStyle)
	{
		static const FName FallbackStyle(TEXT("Button"));
		const ISlateStyle& AppStyle = FAppStyle::Get();
		return AppStyle.HasWidgetStyle<FButtonStyle>(PreferredStyle)
			? AppStyle.GetWidgetStyle<FButtonStyle>(PreferredStyle)
			: AppStyle.GetWidgetStyle<FButtonStyle>(FallbackStyle);
	}
}

TOptional<int32> SMCPPortDialog::ShowModal(int32 CurrentPort, bool bServerRunning, const TSharedPtr<const SWidget>& ParentWidget)
{
	const TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(LOCTEXT("PortDialogTitle", "MCP Server Port"))
		.SizingRule(ESizingRule::Autosized)
		.AutoCenter(EAutoCenter::PreferredWorkArea)
		.SupportsMinimize(false)
		.SupportsMaximize(false);

	const TSharedRef<SMCPPortDialog> Dialog = SNew(SMCPPortDialog)
		.InitialPort(CurrentPort)
		.bServerRunning(bServerRunning)
		.ParentWindow(Window);

	Window->SetContent(Dialog);
	if (Dialog->PortEntry.IsValid())
	{
		Window->SetWidgetToFocusOnActivate(Dialog->PortEntry);
	}

	FSlateApplication::Get().AddModalWindow(Window, ParentWidget);

	if (Dialog->bApplied && Dialog->PendingPort != CurrentPort)
	{
		return Dialog->PendingPort;
	}
	return {};
}

void SMCPPortDialog::Construct(const FArguments& InArgs)
{
	ParentWindow = InArgs._ParentWindow;
	InitialPort = FMath::Clamp(InArgs._InitialPort, 1, 65535);
	PendingPort = InitialPort;

	const FText RestartNote = InArgs._bServerRunning
		? LOCTEXT("RestartNote", "The server is running and will be restarted on the new port.")
		: FText::GetEmpty();

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("Brushes.Panel"))
		.Padding(16.0f)
		[
			SNew(SBox)
			.MinDesiredWidth(320.0f)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("PortPrompt", "Port the Model Context Protocol server listens on (127.0.0.1):"))
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 8.0f, 0.0f, 0.0f)
				[
					SAssignNew(PortEntry, SNumericEntryBox<int32>)
					.AllowSpin(false)
					.MinValue(1)
					.MaxValue(65535)
					.Value(this, &SMCPPortDialog::GetPortValue)
					.OnValueChanged(this, &SMCPPortDialog::OnPortChanged)
					.OnValueCommitted(this, &SMCPPortDialog::OnPortCommitted)
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 8.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(RestartNote)
					.Visibility(RestartNote.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible)
					.AutoWrapText(true)
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Right)
				.Padding(0.0f, 16.0f, 0.0f, 0.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0f, 0.0f, 8.0f, 0.0f)
					[
						SNew(SButton)
						.ButtonStyle(&MCPServerUI::Private::GetDialogButtonStyle(TEXT("PrimaryButton")))
						.Text(LOCTEXT("Apply", "Apply"))
						.HAlign(HAlign_Center)
						.IsEnabled(this, &SMCPPortDialog::CanApply)
						.OnClicked(this, &SMCPPortDialog::OnApplyClicked)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SButton)
						.ButtonStyle(&MCPServerUI::Private::GetDialogButtonStyle(TEXT("Button")))
						.Text(LOCTEXT("Cancel", "Cancel"))
						.HAlign(HAlign_Center)
						.OnClicked(this, &SMCPPortDialog::OnCancelClicked)
					]
				]
			]
		]
	];
}

FReply SMCPPortDialog::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		CloseDialog(false);
		return FReply::Handled();
	}
	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

void SMCPPortDialog::OnPortChanged(int32 NewValue)
{
	PendingPort = FMath::Clamp(NewValue, 1, 65535);
}

void SMCPPortDialog::OnPortCommitted(int32 NewValue, ETextCommit::Type CommitType)
{
	PendingPort = FMath::Clamp(NewValue, 1, 65535);
	if (CommitType == ETextCommit::OnEnter)
	{
		CloseDialog(true);
	}
}

bool SMCPPortDialog::CanApply() const
{
	return PendingPort >= 1 && PendingPort <= 65535;
}

FReply SMCPPortDialog::OnApplyClicked()
{
	CloseDialog(true);
	return FReply::Handled();
}

FReply SMCPPortDialog::OnCancelClicked()
{
	CloseDialog(false);
	return FReply::Handled();
}

void SMCPPortDialog::CloseDialog(bool bApply)
{
	bApplied = bApply && CanApply();
	if (const TSharedPtr<SWindow> Window = ParentWindow.Pin())
	{
		Window->RequestDestroyWindow();
	}
}

#undef LOCTEXT_NAMESPACE
