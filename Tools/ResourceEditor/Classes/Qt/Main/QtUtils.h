#ifndef __QT_UTILS_H__
#define __QT_UTILS_H__

#include "DAVAEngine.h"
#include <QString>
#include <QColor>
#include <QToolBar>
#include <QAction>
#include <QMessageBox>

DAVA::FilePath PathnameToDAVAStyle(const QString& convertedPathname);

DAVA::FilePath GetOpenFileName(const DAVA::String& title, const DAVA::FilePath& pathname, const DAVA::String& filter);

void ShowActionWithText(QToolBar* toolbar, QAction* action, bool showText);

DAVA::WideString SizeInBytesToWideString(DAVA::float32 size);
DAVA::String SizeInBytesToString(DAVA::float32 size);

DAVA::Image* CreateTopLevelImage(const DAVA::FilePath& imagePathname);

void ShowErrorDialog(const DAVA::Set<DAVA::String>& errors, const DAVA::String& title = "");
void ShowErrorDialog(const DAVA::String& errorMessage, const DAVA::String& title = "Error");

bool IsKeyModificatorPressed(DAVA::Key key);
bool IsKeyModificatorsPressed();

enum eMessageBoxFlags
{
    MB_FLAG_YES = QMessageBox::Yes,
    MB_FLAG_NO = QMessageBox::No,
    MB_FLAG_CANCEL = QMessageBox::Cancel
};

int ShowQuestion(const DAVA::String& header, const DAVA::String& question, int buttons, int defaultButton);

#ifdef __DAVAENGINE_WIN32__
const Qt::WindowFlags WINDOWFLAG_ON_TOP_OF_APPLICATION = Qt::Window;
#else
const Qt::WindowFlags WINDOWFLAG_ON_TOP_OF_APPLICATION = Qt::Tool;
#endif

DAVA::String ReplaceInString(const DAVA::String& sourceString, const DAVA::String& what, const DAVA::String& on);

void ShowFileInExplorer(const QString& path);

// Method for debugging. Save image to file
void SaveSpriteToFile(DAVA::Sprite* sprite, const DAVA::FilePath& path);
void SaveTextureToFile(DAVA::Texture* texture, const DAVA::FilePath& path);
void SaveImageToFile(DAVA::Image* image, const DAVA::FilePath& path);


#endif // __QT_UTILS_H__
