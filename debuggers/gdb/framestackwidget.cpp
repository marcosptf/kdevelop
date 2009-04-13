/*
 * GDB Debugger Support
 *
 * Copyright 1999 John Birch <jbb@kdevelop.org>
 * Copyright 2007 Hamish Rodda <rodda@kde.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "framestackwidget.h"

#include "gdbparser.h"
#include "gdbcommand.h"
#include "debuggerplugin.h"

#include "util/treemodel.h"

#include <klocale.h>
#include <kdebug.h>
#include <kglobalsettings.h>
#include <KIcon>

#include "gdbcontroller.h"
#include "stackmanager.h"

#include <QHeaderView>

using namespace GDBMI;
using namespace GDBDebugger;

FramestackWidget::FramestackWidget(CppDebuggerPlugin* plugin, GDBController* controller,
                                   QWidget *parent)
: AsyncTreeView(controller->stackManager()->model(), parent),
  controller_(controller), firstShow_(true)
{
    setToolTip(i18n("<b>Frame stack</b><p>"
                    "Often referred to as the \"call stack\", "
                    "this is a list showing which function is "
                    "currently active, and what called each "
                    "function to get to this point in your "
                    "program. By clicking on an item you "
                    "can see the values in any of the "
                    "previous calling functions."));
    setWindowIcon(KIcon("view-list-text"));
    setRootIsDecorated(true);
    setSelectionMode(QAbstractItemView::SingleSelection);

    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
    header()->setHighlightSections(false);

//    header()->hide();

//    setModel(controller->stackManager());
    controller->stackManager()->setAutoUpdate(isVisible());

    connect(selectionModel(), 
            SIGNAL(selectionChanged(QItemSelection, QItemSelection)),
            this,
            SLOT(slotSelectionChanged(QItemSelection, QItemSelection)));

    connect(controller->stackManager(), 
            SIGNAL(selectThread(const QModelIndex&)),
            this,
            SLOT(selectThread(const QModelIndex&)));

    connect(plugin, SIGNAL(raiseFramestackViews()), this, SIGNAL(requestRaise()));
}

FramestackWidget::~FramestackWidget()
{
}

void FramestackWidget::selectThread(const QModelIndex& index)
{
    selectionModel()->select(
        index, 
        QItemSelectionModel::Rows
        | QItemSelectionModel::ClearAndSelect);
}

void FramestackWidget::slotSelectionChanged(const QItemSelection & selected, 
                                            const QItemSelection & deselected)
{
    kDebug(9012) << "SELECTION CHANGE";

    Q_UNUSED(deselected);
    if (selected.isEmpty())
        return;

    if (selected.count() > 1) {
        kWarning() << "Selection not single as requested";
        return;
    }
   
    // Set current frame
    TreeItem *selectedObject = controller_->stackManager()->model()
        ->itemForIndex(selected.first().topLeft());

    Thread* thread = dynamic_cast<Thread*>(selectedObject);
    if (thread)
    {
        controller_->selectFrame(0, thread->id());
    }
    else
    {
        Frame* frame = dynamic_cast<Frame*>(selectedObject);
        if (frame)
        {
            controller_->selectFrame(frame->id(), frame->thread()->id());
        }
    }
}

void FramestackWidget::showEvent(QShowEvent * event)
{
    kDebug(9012) << "framestack shown\n";
    controller_->stackManager()->setAutoUpdate(true);

    if (firstShow_)
    {
        int id_width = QFontMetrics(font()).width("MMThread 99");
        header()->resizeSection(0, QFontMetrics(font()).width("MMThread 99"));
        header()->resizeSection(1, (header()->width()-id_width)/2);
        firstShow_ = false;
    }
}

void FramestackWidget::hideEvent(QHideEvent * event)
{
    kDebug(9012) << "framestack hidden\n";
    controller_->stackManager()->setAutoUpdate(false);
}

#include "framestackwidget.moc"
