/* This file is part of KDevelop
    Copyright 2010 Aleix Pol Gonzalez <aleixpol@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "uihelper.h"
#include <interfaces/icore.h>
#include <interfaces/iuicontroller.h>
#include <KParts/MainWindow>
#include <KActionCollection>
#include <interfaces/iprojectcontroller.h>
#include <QDebug>
#include <KMenuBar>
#include <qdesktopservices.h>

using namespace KDevelop;

UiHelper::UiHelper(QObject* parent): QObject(parent)
{}

QAction* findActionRec(const QStringList& path, const QList<QAction*>& actions)
{
    QStringList newPath = path;
    QString current = newPath.takeFirst();
    
    foreach(QAction* a, actions) {
//         qDebug() << "leee" << a->objectName() << current;
        if(a->objectName() == current) {
            if(newPath.isEmpty())
                return a;
            else if(a->menu())
                return findActionRec(newPath, a->menu()->actions());
        }
    }
    
    qWarning() << "error: action path not found: " << path;
    return 0;
}

QAction* UiHelper::retrieveMenuAction(const QString& menuPath)
{
    KMenuBar* m = ICore::self()->uiController()->activeMainWindow()->menuBar();
    
    QAction* a=findActionRec(menuPath.split('/'), m->actions());
    return a;
}

QList< QObject* > UiHelper::recentProjects()
{
    QAction* a=retrieveMenuAction("project/project_open_recent");
    
    QList<QObject*> ret;
    if(a) {
        foreach(QAction* a, a->menu()->actions())
            if(a->text().contains('['))
                ret+=a;
    }
    
    return ret;
}

bool UiHelper::openUrl(const QUrl& url)
{
    return QDesktopServices::openUrl(url);
}

void UiHelper::setArea(const QString& name)
{
    ICore::self()->uiController()->switchToArea(name, IUiController::ThisWindow);
}
