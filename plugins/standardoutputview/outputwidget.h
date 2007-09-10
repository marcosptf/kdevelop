/* This file is part of KDevelop
 *
 * Copyright 2007 Andreas Pakulat <apaku@gmx.de>
 * Copyright 2007 Dukju Ahn <dukjuahn@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifndef OUTPUTWIDGET_H
#define OUTPUTWIDGET_H

#include <ktabwidget.h>
#include <QtCore/QMap>
class QAbstractItemModel;
class QString;
class StandardOutputView;
class OutputViewCommand;
class QModelIndex;
class QListView;
class QToolButton;

class OutputWidget : public KTabWidget
{
    Q_OBJECT
    public:
        OutputWidget(QWidget* parent, StandardOutputView* view);
    public Q_SLOTS:
        void changeModel( int id );
        void changeDelegate( int id );
        void removeView( int id );
        void closeActiveView();
        void selectNextItem();
        void selectPrevItem();
        void activate(const QModelIndex&);

    Q_SIGNALS:
        void viewRemoved( int );
//         void activated( const QModelIndex& );

    private:
        QMap<int, QListView*> m_listviews;
        QMap<QWidget*, int> m_widgetMap;
        StandardOutputView* m_outputView;
        QToolButton* m_closeButton;
};

#endif

//kate: space-indent on; indent-width 4; replace-tabs on; auto-insert-doxygen on; indent-mode cstyle;
