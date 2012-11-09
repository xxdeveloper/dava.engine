#include "QtMainWindowHandler.h"

#include "../Commands/CommandCreateNode.h"
#include "../Commands/CommandsManager.h"
#include "../Commands/FileCommands.h"
#include "../Commands/ToolsCommands.h"
#include "../Commands/CommandViewport.h"
#include "../Commands/SceneGraphCommands.h"
#include "../Commands/ViewCommands.h"
#include "../Commands/CommandReloadTextures.h"
#include "../Commands/ParticleEditorCommands.h"
#include "../Commands/LandscapeOptionsCommands.h"
#include "../Commands/CustomColorCommands.h"
#include "../Constants.h"
#include "../SceneEditor/EditorSettings.h"
#include "../SceneEditor/SceneEditorScreenMain.h"
#include "GUIState.h"
#include "SceneDataManager.h"
#include "SceneData.h"
#include "QtUtils.h"
#include "mainwindow.h"

#include <QPoint>
#include <QMenu>
#include <QAction>
#include <QCursor>
#include <QWidget>
#include <QPushButton>
#include <QSlider>
#include <QComboBox>
#include <QStatusBar>

using namespace DAVA;

QtMainWindowHandler::QtMainWindowHandler(QObject *parent)
    :   QObject(parent)
	,	menuResentScenes(NULL)
    ,   resentAncorAction(NULL)
	,	defaultFocusWidget(NULL)
    ,   statusBar(NULL)
{
    new CommandsManager();
    
    ClearActions(ResourceEditor::NODE_COUNT, nodeActions);
    ClearActions(ResourceEditor::VIEWPORT_COUNT, viewportActions);
    ClearActions(ResourceEditor::HIDABLEWIDGET_COUNT, hidablewidgetActions);

    for(int32 i = 0; i < EditorSettings::RESENT_FILES_COUNT; ++i)
    {
        resentSceneActions[i] = new QAction(this);
        resentSceneActions[i]->setObjectName(QString::fromUtf8(Format("resentSceneActions[%d]", i)));
    }
}

QtMainWindowHandler::~QtMainWindowHandler()
{
	defaultFocusWidget = NULL;
    for(int32 i = 0; i < EditorSettings::RESENT_FILES_COUNT; ++i)
    {
        SafeDelete(resentSceneActions[i]);
    }

    ClearActions(ResourceEditor::NODE_COUNT, nodeActions);
    ClearActions(ResourceEditor::VIEWPORT_COUNT, viewportActions);
    ClearActions(ResourceEditor::HIDABLEWIDGET_COUNT, hidablewidgetActions);
    
    CommandsManager::Instance()->Release();
}

void QtMainWindowHandler::ClearActions(int32 count, QAction **actions)
{
    for(int32 i = 0; i < count; ++i)
    {
        actions[i] = NULL;
    }
}

void QtMainWindowHandler::Execute(Command *command)
{
    CommandsManager::Instance()->Execute(command);
    SafeRelease(command);
}


void QtMainWindowHandler::NewScene()
{
    Execute(new CommandNewScene());
}


void QtMainWindowHandler::OpenScene()
{
    Execute(new CommandOpenScene());
}


void QtMainWindowHandler::OpenProject()
{
    Execute(new CommandOpenProject());
}

void QtMainWindowHandler::OpenResentScene(int32 index)
{
    Execute(new CommandOpenScene(EditorSettings::Instance()->GetLastOpenedFile(index)));
}

void QtMainWindowHandler::SaveScene()
{
    Execute(new CommandSaveScene());
}

void QtMainWindowHandler::ExportAsPNG()
{
    Execute(new CommandExport(ResourceEditor::FORMAT_PNG));
}

void QtMainWindowHandler::ExportAsPVR()
{
    Execute(new CommandExport(ResourceEditor::FORMAT_PVR));
}

void QtMainWindowHandler::ExportAsDXT()
{
    Execute(new CommandExport(ResourceEditor::FORMAT_DXT));
}

void QtMainWindowHandler::SaveToFolderWithChilds()
{
    Execute(new CommandSaveToFolderWithChilds());
}

void QtMainWindowHandler::CreateNode(ResourceEditor::eNodeType type)
{
    Execute(new CommandCreateNode(type));
}

void QtMainWindowHandler::Materials()
{
    Execute(new CommandMaterials());
}

void QtMainWindowHandler::HeightmapEditor()
{
    Execute(new CommandHeightmapEditor());
}

void QtMainWindowHandler::TilemapEditor()
{
    Execute(new CommandTilemapEditor());
}

void QtMainWindowHandler::ConvertTextures()
{
    Execute(new CommandTextureConverter());
}

void QtMainWindowHandler::SetViewport(ResourceEditor::eViewportType type)
{
    Execute(new CommandViewport(type));
}


void QtMainWindowHandler::CreateNodeTriggered(QAction *nodeAction)
{
    for(int32 i = 0; i < ResourceEditor::NODE_COUNT; ++i)
    {
        if(nodeAction == nodeActions[i])
        {
            CreateNode((ResourceEditor::eNodeType)i);
            break;
        }
    }
}

void QtMainWindowHandler::ViewportTriggered(QAction *viewportAction)
{
    for(int32 i = 0; i < ResourceEditor::VIEWPORT_COUNT; ++i)
    {
        if(viewportAction == viewportActions[i])
        {
            SetViewport((ResourceEditor::eViewportType)i);
            break;
        }
    }
}

void QtMainWindowHandler::SetResentMenu(QMenu *menu)
{
    menuResentScenes = menu;
}

void QtMainWindowHandler::SetResentAncorAction(QAction *ancorAction)
{
    resentAncorAction = ancorAction;
}


void QtMainWindowHandler::MenuFileWillShow()
{
    if(!GUIState::Instance()->GetNeedUpdatedFileMenu()) return;
    
    //TODO: what a bug?
    DVASSERT(menuResentScenes && "Call SetResentMenu() to setup resent menu");

    for(DAVA::int32 i = 0; i < EditorSettings::RESENT_FILES_COUNT; ++i)
    {
        if(resentSceneActions[i]->parentWidget())
        {
            menuResentScenes->removeAction(resentSceneActions[i]);
        }
    }

    
    int32 sceneCount = EditorSettings::Instance()->GetLastOpenedCount();
    if(0 < sceneCount)
    {
        QList<QAction *> resentActions;
        for(int32 i = 0; i < sceneCount; ++i)
        {
            resentSceneActions[i]->setText(QString(EditorSettings::Instance()->GetLastOpenedFile(i).c_str()));
            resentActions.push_back(resentSceneActions[i]);
        }
        
        menuResentScenes->insertActions(resentAncorAction, resentActions);
        menuResentScenes->insertSeparator(resentAncorAction);
    }
 
    GUIState::Instance()->SetNeedUpdatedFileMenu(false);
}

void QtMainWindowHandler::MenuToolsWillShow()
{
    if(!GUIState::Instance()->GetNeedUpdatedToolsMenu()) return;

    //TODO: need code here

//    SceneEditorScreenMain *screen = dynamic_cast<SceneEditorScreenMain *>(UIScreenManager::Instance()->GetScreen());
//    if(screen)
//    {
////        screen->;
//    }
    
    GUIState::Instance()->SetNeedUpdatedToolsMenu(false);
}


void QtMainWindowHandler::FileMenuTriggered(QAction *resentScene)
{
    for(int32 i = 0; i < EditorSettings::RESENT_FILES_COUNT; ++i)
    {
        if(resentScene == resentSceneActions[i])
        {
            OpenResentScene(i);
        }
    }
}


void QtMainWindowHandler::RegisterNodeActions(int32 count, ...)
{
    DVASSERT((ResourceEditor::NODE_COUNT == count) && "Wrong count of actions");

    va_list vl;
    va_start(vl, count);
    
    RegisterActions(nodeActions, count, vl);    
    
    va_end(vl);
}


void QtMainWindowHandler::RegisterViewportActions(int32 count, ...)
{
    DVASSERT((ResourceEditor::VIEWPORT_COUNT == count) && "Wrong count of actions");
    
    va_list vl;
    va_start(vl, count);
    
    RegisterActions(viewportActions, count, vl);    
  
    va_end(vl);
}

void QtMainWindowHandler::RegisterDockActions(int32 count, ...)
{
    DVASSERT((ResourceEditor::HIDABLEWIDGET_COUNT == count) && "Wrong count of actions");
    
    va_list vl;
    va_start(vl, count);
    
    RegisterActions(hidablewidgetActions, count, vl);
    
    va_end(vl);
}


void QtMainWindowHandler::RegisterActions(QAction **actions, int32 count, va_list &vl)
{
    for(int32 i = 0; i < count; ++i)
    {
        actions[i] = va_arg(vl, QAction *);
    }
}


void QtMainWindowHandler::RestoreViews()
{
    for(int32 i = 0; i < ResourceEditor::HIDABLEWIDGET_COUNT; ++i)
    {
        if(hidablewidgetActions[i] && !hidablewidgetActions[i]->isChecked())
        {
            hidablewidgetActions[i]->trigger();
        }
    }
}

void QtMainWindowHandler::RefreshSceneGraph()
{
    Execute(new CommandRefreshSceneGraph());
}

void QtMainWindowHandler::ToggleSceneInfo()
{
    Execute(new CommandSceneInfo());
}

void QtMainWindowHandler::ShowSettings()
{
    Execute(new CommandSettings());
}

void QtMainWindowHandler::BakeScene()
{
    Execute(new CommandBakeScene());
}

void QtMainWindowHandler::Beast()
{
    Execute(new CommandBeast());
}

void QtMainWindowHandler::SetDefaultFocusWidget(QWidget *widget)
{
	defaultFocusWidget = widget;
}

void QtMainWindowHandler::RestoreDefaultFocus()
{
	if(defaultFocusWidget)
	{
		defaultFocusWidget->setEnabled(false);
		defaultFocusWidget->setEnabled(true);
        defaultFocusWidget->setFocus();
	}
}


void QtMainWindowHandler::ReloadTexturesFromFileSystem()
{
    Execute(new CommandReloadTextures());
}

void QtMainWindowHandler::OpenParticleEditorConfig()
{
	Execute(new CommandOpenParticleEditorConfig());
}

void QtMainWindowHandler::SaveParticleEditorConfig()
{
	Execute(new CommandSaveParticleEditorConfig());
}

void QtMainWindowHandler::OpenParticleEditorSprite()
{
	Execute(new CommandOpenParticleEditorSprite());
}

void QtMainWindowHandler::ToggleNotPassableTerrain()
{
	Execute(new CommandNotPassableTerrain());
}

void QtMainWindowHandler::RegisterStatusBar(QStatusBar *registeredSatusBar)
{
    statusBar = registeredSatusBar;
}

void QtMainWindowHandler::ShowStatusBarMessage(const DAVA::String &message, DAVA::int32 displayTime)
{
    if(statusBar)
    {
        statusBar->showMessage(QString(message.c_str()), displayTime);
    }
}


void QtMainWindowHandler::SetWaitingCursorEnabled(bool enabled)
{
    QMainWindow *window = dynamic_cast<QMainWindow *>(parent());
    
    if(window)
    {
        if(enabled)
        {
            window->setCursor(QCursor(Qt::WaitCursor));
        }
        else
        {
            window->setCursor(QCursor(Qt::ArrowCursor));
        }
    }
}

void QtMainWindowHandler::RulerTool()
{
    Execute(new CommandRulerTool());
}

void QtMainWindowHandler::ToggleCustomColors()
{
    Execute(new CommandToggleCustomColors());
}

void QtMainWindowHandler::SaveTextureCustomColors()
{
    Execute(new CommandSaveTextureCustomColors());
}

void QtMainWindowHandler::ChangeBrushSizeCustomColors(int newSize)
{
    Execute(new CommandChangeBrushSizeCustomColors(newSize));
}

void QtMainWindowHandler::ChangeColorCustomColors(int newColorIndex)
{
    Execute(new CommandChangeColorCustomColors(newColorIndex));
}

void QtMainWindowHandler::RegisterCustomColorsWidgets(QPushButton* toggleButton, QPushButton* saveTextureButton, QSlider* brushSizeSlider, QComboBox* colorComboBox)
{
	this->customColorsToggleButton = toggleButton;
	this->customColorsSaveTextureButton = saveTextureButton;
	this->customColorsBrushSizeSlider = brushSizeSlider;
	this->customColorsColorComboBox = colorComboBox;
}

void QtMainWindowHandler::SetCustomColorsWidgetsState(bool state)
{
	customColorsToggleButton->blockSignals(true);
	customColorsToggleButton->setChecked(state);
	customColorsToggleButton->blockSignals(false);

	customColorsSaveTextureButton->setEnabled(state);
	customColorsBrushSizeSlider->setEnabled(state);
	customColorsColorComboBox->setEnabled(state);
	customColorsSaveTextureButton->blockSignals(!state);
	customColorsBrushSizeSlider->blockSignals(!state);
	customColorsColorComboBox->blockSignals(!state);

	if(state == true)
	{
		ChangeBrushSizeCustomColors(customColorsBrushSizeSlider->value());
		ChangeColorCustomColors(customColorsColorComboBox->currentIndex());
	}
}
