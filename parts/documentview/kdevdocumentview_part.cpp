/* This file is part of KDevelop
Copyright (C) 2005 Adam Treat <treat@kde.org>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public License
along with this library; see the file COPYING.LIB.  If not, write to
the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
Boston, MA 02110-1301, USA.
*/

#include "kdevdocumentview_part.h"
#include "kdevdocumentviewdelegate.h"
#include "kdevdocumentview.h"
#include "kdevdocumentmodel.h"
#include "kdevdocumentselection.h"

#include <QtGui/QVBoxLayout>

#include <kaction.h>
#include <klocale.h>
#include <ktrader.h>
#include <kmimetype.h>
#include <kaboutdata.h>
#include <kiconloader.h>
#include <kparts/componentfactory.h>

#include <kdevapi.h>
#include <urlutil.h>
#include <kdevcore.h>
#include <kdevplugininfo.h>
#include <kdevmainwindow.h>
#include <kdevgenericfactory.h>
#include <kfiltermodel.h>

typedef KDevGenericFactory<KDevDocumentViewPart> KDevDocumentViewFactory;
static const KDevPluginInfo data( "kdevdocumentview" );
K_EXPORT_COMPONENT_FACTORY( libkdevdocumentview,
                            KDevDocumentViewFactory( data ) );

KDevDocumentViewPart::KDevDocumentViewPart( QObject *parent,
        const char *name,
        const QStringList& )
        : KDevPlugin( &data, parent )
{
    setObjectName( QString::fromUtf8( name ) );

    m_documentModel = new KDevDocumentModel( this );

    setInstance( KDevDocumentViewFactory::instance() );

    m_documentView = new KDevDocumentView( this, 0 );
    m_documentView->setIcon( SmallIcon( "kmultiple" ) );
    m_documentView->setCaption( i18n( "Documents" ) );

    KDevDocumentViewDelegate *delegate =
        new KDevDocumentViewDelegate( m_documentView, this );
    m_documentView->setModel( m_documentModel );
    m_documentView->setSelectionModel(
        new KDevDocumentSelection( m_documentModel ) );
    m_documentView->setItemDelegate( delegate );
    m_documentView->setWhatsThis( i18n( "Document View" ) );

    mainWindow() ->embedSelectView( m_documentView,
                                    i18n( "Documents" ), i18n( "Documents" ) );

    connect( m_documentView, SIGNAL( pressed( QModelIndex ) ),
             this, SLOT( pressed( QModelIndex ) ) );
    connect( documentController(), SIGNAL( documentActivated( const KURL & ) ),
             this, SLOT( activated( const KURL & ) ) );
    connect( documentController(), SIGNAL( documentSaved( const KURL & ) ),
             this, SLOT( saved( const KURL & ) ) );
    connect( documentController(), SIGNAL( documentLoaded( const KURL & ) ),
             this, SLOT( loaded( const KURL & ) ) );
    connect( documentController(), SIGNAL( documentClosed( const KURL & ) ),
             this, SLOT( closed( const KURL & ) ) );
    connect( documentController(),
             SIGNAL( documentExternallyModified( const KURL & ) ),
             this, SLOT( externallyModified( const KURL & ) ) );
    connect( documentController(),
             SIGNAL( documentURLChanged( const KURL &, const KURL & ) ),
             this, SLOT( URLChanged( const KURL &, const KURL & ) ) );
    connect( documentController(),
             SIGNAL( documentStateChanged( const KURL &, DocumentState ) ),
             this, SLOT( stateChanged( const KURL &, DocumentState ) ) );

    setXMLFile( "kdevdocumentview.rc" );
}

KDevDocumentViewPart::~KDevDocumentViewPart()
{
    if ( m_documentView )
    {
        mainWindow() ->removeView( m_documentView );
        delete m_documentView;
    }
}

void KDevDocumentViewPart::import( RefreshPolicy /*policy*/ )
{}

void KDevDocumentViewPart::activated( const KURL & url )
{
    m_documentView->setCurrentIndex( m_url2index[ url.path() ] );
}

void KDevDocumentViewPart::saved( const KURL & /*url*/ )
{
    kdDebug() << k_funcinfo << endl;
}

void KDevDocumentViewPart::loaded( const KURL &url )
{
    QString mimeType = KMimeType::findByURL( url ) ->comment();
    KDevMimeTypeItem *mimeItem = m_documentModel->mimeType( mimeType );
    if ( !mimeItem )
    {
        mimeItem = new KDevMimeTypeItem( mimeType.toLatin1() );
        m_documentModel->appendItem( mimeItem );
        m_documentView->expand( m_documentModel->indexOf( mimeItem ) );
    }

    if ( !mimeItem->file( url ) )
    {
        KDevFileItem * fileItem = new KDevFileItem( url );
        m_documentModel->appendItem( fileItem, mimeItem );
        m_documentView->setCurrentIndex( m_documentModel->indexOf( fileItem ) );
        m_url2index[ url.path() ] = m_documentModel->indexOf( fileItem );
        m_documentModel->sort( 0, Qt::Ascending );
    }
}

void KDevDocumentViewPart::closed( const KURL & url )
{
    kdDebug() << k_funcinfo << endl;
    QModelIndex fileIndex = m_url2index[ url.path() ];
    KDevDocumentItem *fileItem = m_documentModel->item( fileIndex );
    if ( !fileItem )
        return ;

    QModelIndex mimeIndex = m_documentModel->parent( fileIndex );

    m_documentModel->removeItem( fileItem );
    m_url2index.remove( url.path() );

    if ( m_documentModel->hasChildren( mimeIndex ) )
        return ;

    KDevDocumentItem *mimeItem = m_documentModel->item( mimeIndex );
    if ( !mimeItem )
        return ;

    m_documentModel->removeItem( mimeItem );
}

void KDevDocumentViewPart::externallyModified( const KURL & /*url*/ )
{
    kdDebug() << k_funcinfo << endl;
}

void KDevDocumentViewPart::URLChanged( const KURL & /*oldurl*/,
                                       const KURL & /*newurl*/ )
{
    kdDebug() << k_funcinfo << endl;
}

void KDevDocumentViewPart::stateChanged( const KURL & url,
        DocumentState state )
{
    KDevDocumentItem * documentItem =
        m_documentModel->item( m_url2index[ url.path() ] );

    if ( !documentItem )
        return ;

    if ( documentItem->documentState() == state )
        return;

    documentItem->setDocumentState( state );
    m_documentView->doItemsLayout();
}

void KDevDocumentViewPart::pressed( const QModelIndex & index )
{
    if ( index.parent().isValid() )
        documentController() ->editDocument(
            m_documentModel->item( index ) ->fileItem() ->URL() );
}

#include "kdevdocumentview_part.moc"

// kate: space-indent on; indent-width 4; tab-width 4; replace-tabs on
