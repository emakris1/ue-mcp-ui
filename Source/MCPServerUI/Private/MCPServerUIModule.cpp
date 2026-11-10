// MCPServerUI - editor-only status bar controls for the Model Context Protocol server.

#include "MCPServerController.h"
#include "MCPServerUIStyle.h"
#include "SMCPServerStatusBar.h"

#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"
#include "ToolMenus.h"

namespace MCPServerUI::Private
{
  // Each status bar names its tool bar menu after its drawer (see SStatusBar::GetToolbarName), so this targets only the
  // main level editor's status bar. That's also where Diagnostics, Trace and Revision Control live.
  static const FName LevelEditorStatusBarToolBarName(TEXT("LevelEditor.StatusBar.ToolBar"));
  static const FName SectionName(TEXT("MCPServerUI"));
  static const FName EntryName(TEXT("MCPServerControls"));
}

class FMCPServerUIModule : public IModuleInterface
{
public:
  virtual void StartupModule() override
  {
    FMCPServerUIStyle::Initialize();

    Controller = MakeShared<FMCPServerController>();

    UToolMenus::RegisterStartupCallback(
      FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FMCPServerUIModule::RegisterStatusBarExtension));

    if (GIsRunning)
    {
      // Enabled from the Plugins window after startup, so PostEngineInit has already fired.
      MoveSectionToFront();
    }
    else
    {
      PostEngineInitHandle =
        FCoreDelegates::GetOnPostEngineInit().AddRaw(this, &FMCPServerUIModule::MoveSectionToFront);
    }
  }

  virtual void ShutdownModule() override
  {
    FCoreDelegates::GetOnPostEngineInit().Remove(PostEngineInitHandle);

    if (UObjectInitialized())
    {
      UToolMenus::UnRegisterStartupCallback(this);
      UToolMenus::UnregisterOwner(this);
      if (UToolMenus* ToolMenus = UToolMenus::TryGet())
      {
        ToolMenus->RemoveSection(MCPServerUI::Private::LevelEditorStatusBarToolBarName, MCPServerUI::Private::SectionName);
      }
    }

    Controller.Reset();
    FMCPServerUIStyle::Shutdown();
  }

private:
  void RegisterStatusBarExtension()
  {
    using namespace MCPServerUI::Private;

    FToolMenuOwnerScoped OwnerScoped(this);

    UToolMenu* StatusBarToolBar = UToolMenus::Get()->ExtendMenu(LevelEditorStatusBarToolBarName);
    FToolMenuSection& Section = StatusBarToolBar->AddSection(
      SectionName,
      TAttribute<FText>(),
      FToolMenuInsert(NAME_None, EToolMenuInsertType::First));

    // A dynamic entry builds a new widget each time the menu is regenerated. A single shared widget instance can't be
    // parented into more than one generated tool bar.
    const TWeakPtr<FMCPServerController> WeakController = Controller;
    Section.AddDynamicEntry(EntryName, FNewToolMenuSectionDelegate::CreateLambda([WeakController](FToolMenuSection& InSection)
    {
      if (const TSharedPtr<FMCPServerController> PinnedController = WeakController.Pin())
      {
        InSection.AddEntry(FToolMenuEntry::InitWidget(
          EntryName,
          SNew(SMCPServerStatusBar, PinnedController.ToSharedRef()),
          FText::GetEmpty(),
          /*bNoIndent*/ true));
      }
    }));

    MoveSectionToFront();
  }

  /**
   * Insights, LiveLinkHub and MultiUser also insert their sections as "First", and ToolMenus keeps "First" sections in
   * the order they were added (UToolMenu::FindInsertIndex). Diagnostics is inserted before Insights. Moving our section
   * to the front of the array keeps us left of all of them, and sections added later are appended after us.
   * This runs once more at PostEngineInit for sections added between our registration and then.
   */
  void MoveSectionToFront()
  {
    using namespace MCPServerUI::Private;

    UToolMenus* ToolMenus = UToolMenus::TryGet();
    UToolMenu* StatusBarToolBar = ToolMenus ? ToolMenus->FindMenu(LevelEditorStatusBarToolBarName) : nullptr;
    if (!StatusBarToolBar)
    {
      return;
    }

    const int32 SectionIndex = StatusBarToolBar->Sections.IndexOfByPredicate([](const FToolMenuSection& Section)
    {
      return Section.Name == SectionName;
    });

    if (SectionIndex > 0)
    {
      FToolMenuSection Section = StatusBarToolBar->Sections[SectionIndex];
      StatusBarToolBar->Sections.RemoveAt(SectionIndex);
      StatusBarToolBar->Sections.Insert(MoveTemp(Section), 0);
      ToolMenus->RefreshAllWidgets();
    }
  }

  TSharedPtr<FMCPServerController> Controller;
  FDelegateHandle PostEngineInitHandle;
};

IMPLEMENT_MODULE(FMCPServerUIModule, MCPServerUI)
