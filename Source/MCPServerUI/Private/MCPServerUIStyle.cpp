// MCPServerUI - editor-only status bar controls for the Model Context Protocol server.

#include "MCPServerUIStyle.h"

#include "Brushes/SlateImageBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyleRegistry.h"

TSharedPtr<FSlateStyleSet> FMCPServerUIStyle::StyleSet;

void FMCPServerUIStyle::Initialize()
{
	if (StyleSet.IsValid())
	{
		return;
	}

	StyleSet = MakeShared<FSlateStyleSet>(GetStyleSetName());

	if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("MCPServerUI")))
	{
		StyleSet->SetContentRoot(Plugin->GetBaseDir() / TEXT("Resources"));
	}

	const FVector2D Icon16x16(16.0, 16.0);
	StyleSet->Set("MCPServerUI.Start", new FSlateVectorImageBrush(StyleSet->RootToContentDir(TEXT("Icons/Start"), TEXT(".svg")), Icon16x16));
	StyleSet->Set("MCPServerUI.Stop", new FSlateVectorImageBrush(StyleSet->RootToContentDir(TEXT("Icons/Stop"), TEXT(".svg")), Icon16x16));
	StyleSet->Set("MCPServerUI.Port", new FSlateVectorImageBrush(StyleSet->RootToContentDir(TEXT("Icons/Port"), TEXT(".svg")), Icon16x16));

	// White dot, tinted at runtime to reflect the server state.
	StyleSet->Set("MCPServerUI.StatusDot", new FSlateRoundedBoxBrush(FLinearColor::White, 4.0f, FVector2f(8.0f, 8.0f)));

	FSlateStyleRegistry::RegisterSlateStyle(*StyleSet);
}

void FMCPServerUIStyle::Shutdown()
{
	if (StyleSet.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet);
		ensure(StyleSet.IsUnique());
		StyleSet.Reset();
	}
}

const ISlateStyle& FMCPServerUIStyle::Get()
{
	check(StyleSet.IsValid());
	return *StyleSet;
}

FName FMCPServerUIStyle::GetStyleSetName()
{
	static const FName StyleSetName(TEXT("MCPServerUIStyle"));
	return StyleSetName;
}
