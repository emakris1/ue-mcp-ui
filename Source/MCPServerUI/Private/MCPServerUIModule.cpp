// MCPServerUI - editor-only status bar controls for the Model Context Protocol server.

#include "MCPServerController.h"
#include "MCPServerUIStyle.h"
#include "SMCPServerStatusBar.h"

#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"
#include "ToolMenus.h"

namespace MCPServerUI::Private
{
  /** The tool bar on the right side of the editor status bar (Revision Control, Diagnostics, ...). */
  static const FName StatusBarToolBarName(TEXT("StatusBar.ToolBar"));
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
    Controller->Initialize();

    UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FMCPServerUIModule::RegisterStatusBarExtension));

    if (GIsRunning)
    {
      // Enabled after editor startup: everything else is already loaded.
      OnPostEngineInit();
    }
    else
    {
      PostEngineInitHandle = FCoreDelegates::OnPostEngineInit.AddRaw(this, &FMCPServerUIModule::OnPostEngineInit);
    }
  }

  virtual void ShutdownModule() override
  {
    FCoreDelegates::OnPostEngineInit.Remove(PostEngineInitHandle);

    if (UObjectInitialized())
    {
      UToolMenus::UnRegisterStartupCallback(this);
      UToolMenus::UnregisterOwner(this);
      if (UToolMenus* ToolMenus = UToolMenus::TryGet())
      {
        ToolMenus->RemoveSection(MCPServerUI::Private::StatusBarToolBarName, MCPServerUI::Private::SectionName);
      }
    }

    if (Controller.IsValid())
    {
      Controller->Shutdown();
      Controller.Reset();
    }

    FMCPServerUIStyle::Shutdown();
  }

private:
  void RegisterStatusBarExtension()
  {
    using namespace MCPServerUI::Private;

    FToolMenuOwnerScoped OwnerScoped(this);

    UToolMenu* StatusBarToolBar = UToolMenus::Get()->ExtendMenu(StatusBarToolBarName);
    FToolMenuSection& Section = StatusBarToolBar->AddSection(SectionName, TAttribute<FText>(), FToolMenuInsert(NAME_None, EToolMenuInsertType::First));

    // The status bar menu is generated once per status bar (level editor, asset editors, ...), so
    // create a fresh widget each time rather than sharing a single instance.
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
   * Sections inserted with EToolMenuInsertType::First are laid out in the order they were added, so another
   * module that also asks for "First" could end up left of us. Once all modules have loaded, make sure our
   * section is the very first one so the controls sit at the leftmost position of the status bar tool bar.
   */
  void MoveSectionToFront()
  {
    using namespace MCPServerUI::Private;

    UToolMenus* ToolMenus = UToolMenus::TryGet();
    UToolMenu* StatusBarToolBar = ToolMenus ? ToolMenus->FindMenu(StatusBarToolBarName) : nullptr;
    if (!StatusBarToolBar)
    {
      return;
    }

    const int32 Index = StatusBarToolBar->Sections.IndexOfByPredicate([](const FToolMenuSection& Section)
    {
      return Section.Name == SectionName;
    });

    if (Index > 0)
    {
      FToolMenuSection Section = StatusBarToolBar->Sections[Index];
      StatusBarToolBar->Sections.RemoveAt(Index);
      StatusBarToolBar->Sections.Insert(MoveTemp(Section), 0);
      ToolMenus->RefreshAllWidgets();
    }
  }

  void OnPostEngineInit()
  {
    if (Controller.IsValid())
    {
      // The ModelContextProtocol settings class may not have existed when we started up.
      Controller->SyncPortFromMCPSettings();
    }
    MoveSectionToFront();
  }

  TSharedPtr<FMCPServerController> Controller;
  FDelegateHandle PostEngineInitHandle;
};

IMPLEMENT_MODULE(FMCPServerUIModule, MCPServerUI)
