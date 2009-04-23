/*
 * This file is part of KDevelop
 *
 * Copyright 2008 Vladimir Prus <ghost@cs.msu.su>
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

#include "breakpointwidget.h"

#include <QHBoxLayout>
#include <QSplitter>
#include <QTableView>
#include <QHeaderView>
#include <QMenu>
#include <QContextMenuEvent>

#include <KIcon>
#include <KLocalizedString>
#include <KPassivePopup>

#include "breakpointdetails.h"
#include "../breakpoint/breakpoint.h"
#include "../breakpoint/breakpointcontroller.h"
#include "../../shell/debugcontroller.h"

using namespace KDevelop;

BreakpointWidget::BreakpointWidget(DebugController *controller, QWidget *parent)
: QWidget(parent), firstShow_(true), m_debugController(controller)
{
    setWindowTitle(i18n("Debugger Breakpoints"));
    setWhatsThis(i18n("<b>Breakpoint list</b><p>"
                        "Displays a list of breakpoints with "
                        "their current status. Clicking on a "
                        "breakpoint item allows you to change "
                        "the breakpoint and will take you "
                        "to the source in the editor window.</p>"));
    setWindowIcon( KIcon("process-stop") );

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setMargin(0);
    QSplitter *s = new QSplitter(this);
    layout->addWidget(s);

    table_ = new QTableView(s);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->horizontalHeader()->setHighlightSections(false);
    table_->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft);
    details_ = new BreakpointDetails(s);

    s->setStretchFactor(0, 2);

    table_->verticalHeader()->hide();

    table_->setModel(m_debugController->breakpointController());

    connect(table_->selectionModel(),
            SIGNAL(selectionChanged(const QItemSelection&,
                                    const QItemSelection&)),
            this, SLOT(slotSelectionChanged(const QItemSelection &,
                                            const QItemSelection&)));

    connect(m_debugController->breakpointController(),
            SIGNAL(dataChanged(const QModelIndex&, const QModelIndex&)),
            this,
            SLOT(slotDataChanged(const QModelIndex&, const QModelIndex&)));

// TODO NIKO
//     connect(controller, SIGNAL(breakpointHit(int)),
//             this, SLOT(slotBreakpointHit(int)));

    connect(m_debugController->breakpointController()->breakpointsItem(),
            SIGNAL(error(KDevelop::Breakpoint *, const QString&, int)),
            this,
            SLOT(breakpointError(KDevelop::Breakpoint *, const QString&, int)));

    setupPopupMenu();
}

void BreakpointWidget::setupPopupMenu()
{
    popup_ = new QMenu(this);

    QMenu* newBreakpoint = popup_->addMenu( i18nc("New breakpoint", "New") );

    QAction* action = newBreakpoint->addAction(
        i18nc("Code breakpoint", "Code"),
        this,
        SLOT(slotAddBlankBreakpoint()) );
    // Use this action also to provide a local shortcut
    action->setShortcut(QKeySequence(Qt::Key_B + Qt::CTRL,
                                        Qt::Key_C));
    addAction(action);

    newBreakpoint->addAction(
        i18nc("Data breakpoint", "Data write"),
        this, SLOT(slotAddBlankWatchpoint()));
    newBreakpoint->addAction(
        i18nc("Data read breakpoint", "Data read"),
        this, SLOT(slotAddBlankReadWatchpoint()));

    #if 0
    m_breakpointShow = m_ctxMenu->addAction( i18n( "Show text" ) );


    m_breakpointEdit = m_ctxMenu->addAction( i18n( "Edit" ) );
    m_breakpointEdit->setShortcut(Qt::Key_Enter);

    m_breakpointDisable = m_ctxMenu->addAction( i18n( "Disable" ) );
    #endif

    QAction* breakpointDelete = popup_->addAction(
        KIcon("breakpoint_delete"),
        i18n( "Delete" ),
        this,
        SLOT(slotRemoveBreakpoint()));
    breakpointDelete->setShortcut(Qt::Key_Delete);
    addAction(breakpointDelete);

    #if 0
    m_ctxMenu->addSeparator();

    m_breakpointDisableAll = m_ctxMenu->addAction( i18n( "Disable all") );
    m_breakpointEnableAll = m_ctxMenu->addAction( i18n( "Enable all") );
    m_breakpointDeleteAll = m_ctxMenu->addAction( i18n( "Delete all"), this, SLOT(slotRemoveAllBreakpoints()));
    #endif

#if 0
    connect( m_ctxMenu,     SIGNAL(triggered(QAction*)),
        this,          SLOT(slotContextMenuSelect(QAction*)) );
#endif
}


void BreakpointWidget::contextMenuEvent(QContextMenuEvent* event)
{
#if 0
    Breakpoint *bp = breakpoints()->breakpointForIndex(indexAt(event->pos()));

    if (!bp)
    {
        bp = breakpoints()->breakpointForIndex(currentIndex());
    }

    if (bp)
    {
        m_breakpointShow->setEnabled(bp->hasFileAndLine());

        if (bp->isEnabled( ))
        {
            m_breakpointDisable->setText( i18n("Disable") );
        }
        else
        {
            m_breakpointDisable->setText( i18n("Enable") );
        }
    }
    else
    {
        m_breakpointShow->setEnabled(false);
    }

    m_breakpointDisable->setEnabled(bp);
    m_breakpointDelete->setEnabled(bp);
    m_breakpointEdit->setEnabled(bp);

    bool has_bps = !breakpoints()->breakpoints().isEmpty();
    m_breakpointDisableAll->setEnabled(has_bps);
    m_breakpointEnableAll->setEnabled(has_bps);
    m_breakpointDelete->setEnabled(has_bps);

    m_ctxMenuBreakpoint = bp;
    m_ctxMenu->popup( event->globalPos() );
#endif
    popup_->popup(event->globalPos());
}

#if 0
void slotContextMenuSelect( QAction* action )
{
    #if 0
    int                  col;
    Breakpoint          *bp = m_ctxMenuBreakpoint;

    if ( action == m_breakpointShow ) {
        if (FilePosBreakpoint* fbp = dynamic_cast<FilePosBreakpoint*>(bp))
            emit gotoSourcePosition(fbp->fileName(), fbp->lineNum()-1);

    } else if ( action == m_breakpointEdit ) {
        col = currentIndex().column();
        if (col == BreakpointController::Location || col ==  BreakpointController::Condition || col == BreakpointController::IgnoreCount)
            openPersistentEditor(model()->index(currentIndex().row(), col, QModelIndex()));

    } else if ( action == m_breakpointDisable ) {
        bp->setEnabled( !bp->isEnabled( ) );
        bp->sendToGdb();

    } else if ( action == m_breakpointDisableAll || action == m_breakpointEnableAll ) {
        foreach (Breakpoint* breakpoint, breakpoints()->breakpoints())
        {
            breakpoint->setEnabled(action == m_breakpointEnableAll);
            breakpoint->sendToGdb();
        }
    }
    #endif
}
#endif


void BreakpointWidget::showEvent(QShowEvent * event)
{
    Q_UNUSED(event);
    if (firstShow_)
    {
        /* FIXME: iterate over all possible names. */
        int id_width = QFontMetrics(font()).width("MMWrite");
        QHeaderView* header = table_->horizontalHeader();
        int width = header->width();

        header->resizeSection(0, 32);
        width -= 32;
        header->resizeSection(1, 32);
        width -= 32;
        header->resizeSection(2, id_width);
        width -= id_width;
        header->resizeSection(3, width/2);
        header->resizeSection(4, width/2);
        firstShow_ = false;
    }
}

void BreakpointWidget::edit(KDevelop::Breakpoint *n)
{
    QModelIndex index = m_debugController->breakpointController()->indexForItem(n, Breakpoint::LocationColumn);
    table_->setCurrentIndex(index);
    table_->edit(index);
}


void BreakpointWidget::slotAddBlankBreakpoint()
{
    edit(m_debugController->breakpointController()->breakpointsItem()->addCodeBreakpoint());
}

void BreakpointWidget::slotAddBlankWatchpoint()
{
    edit(m_debugController->breakpointController()->breakpointsItem()->addWatchpoint());
}

void BreakpointWidget::slotAddBlankReadWatchpoint()
{
    edit(m_debugController->breakpointController()->breakpointsItem()->addReadWatchpoint());
}

void BreakpointWidget::slotRemoveBreakpoint()
{
    QItemSelectionModel* sel = table_->selectionModel();
    QModelIndexList selected = sel->selectedIndexes();
    if (!selected.empty())
    {
        m_debugController->breakpointController()->breakpointsItem()->remove(selected.first());
    }
}

void BreakpointWidget::slotSelectionChanged(const QItemSelection& selected,
                            const QItemSelection&)
{
    if (!selected.empty())
    {
    details_->setItem(
                static_cast<Breakpoint*>(
                    m_debugController->breakpointController()->itemForIndex(selected.indexes().first())));
    }
}

void BreakpointWidget::slotBreakpointHit(int id)
{
    /* This method will not do the right thing if we hit a breakpoint
        that is added in GDB outside kdevelop.  In this case we'll
        first try to find the breakpoint, and fail, and only then
        update the breakpoint table and notice the new one.  */
    KDevelop::Breakpoint *b = m_debugController->breakpointController()->breakpointsItem()
        ->breakpointById(id);
    if (b)
    {
        QModelIndex index = m_debugController->breakpointController()->indexForItem(b, 0);
        table_->selectionModel()->select(
            index,
            QItemSelectionModel::Rows
            | QItemSelectionModel::ClearAndSelect);
    }
}

void BreakpointWidget::slotDataChanged(const QModelIndex& index, const QModelIndex&)
{
    /* This method works around another problem with highliting
        the current breakpoint -- we select it before the new
        hit count is read, so it remains stale.  For that reason,
        we get to notice when breakpoint changes.  */
    details_->setItem(
        static_cast<Breakpoint*>(m_debugController->breakpointController()->itemForIndex(index)));
}

void BreakpointWidget::breakpointError(KDevelop::Breakpoint* b, const QString& msg, int column)
{
    // FIXME: we probably should prevent this error notification during
    // initial setting of breakpoint, to avoid a cloud of popups.
    if (!table_->isVisible())
        return;

    QModelIndex index = m_debugController->breakpointController()->indexForItem(b, column);
    QPoint p = table_->visualRect(index).topLeft();
    p = table_->mapToGlobal(p);

    KPassivePopup *pop = new KPassivePopup(table_);
    pop->setPopupStyle(KPassivePopup::Boxed);
    pop->setAutoDelete(true);
    // FIXME: the icon, too.
    pop->setView("", msg);
    pop->setTimeout(-1);
    pop->show(p);
}

#include "breakpointwidget.moc"
