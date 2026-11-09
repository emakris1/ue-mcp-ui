// MCPServerUI - editor-only status bar controls for the Model Context Protocol server.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class SWindow;
class SWidget;

/** Modal dialog for editing the MCP server port. */
class SMCPPortDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMCPPortDialog)
		: _InitialPort(8000)
		, _bServerRunning(false)
	{}
		SLATE_ARGUMENT(int32, InitialPort)
		SLATE_ARGUMENT(bool, bServerRunning)
		SLATE_ARGUMENT(TWeakPtr<SWindow>, ParentWindow)
	SLATE_END_ARGS()

	/** Shows the dialog modally. Returns the new port if the user applied a change, unset otherwise. */
	static TOptional<int32> ShowModal(int32 CurrentPort, bool bServerRunning, const TSharedPtr<const SWidget>& ParentWidget);

	void Construct(const FArguments& InArgs);

	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

private:
	TOptional<int32> GetPortValue() const { return PendingPort; }
	void OnPortChanged(int32 NewValue);
	void OnPortCommitted(int32 NewValue, ETextCommit::Type CommitType);

	bool CanApply() const;
	FReply OnApplyClicked();
	FReply OnCancelClicked();
	void CloseDialog(bool bApply);

	TWeakPtr<SWindow> ParentWindow;
	TSharedPtr<SWidget> PortEntry;
	int32 InitialPort = 8000;
	int32 PendingPort = 8000;
	bool bApplied = false;
};
