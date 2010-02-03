#include <Qt/QtGui>

#include "demoapp.h"

#include "../lib/foldermodel.h"
#include "../lib/messagesmodel.h"

#include <libmapi++/libmapi++.h>

using namespace libmapipp;

DemoApp::DemoApp()
{
    m_textEdit = new QTextEdit;
    setCentralWidget( m_textEdit );

    createActions();
    createMenus();

    login();
    
    addFolderDockWidget();
    addMessagesDockWidget();
    openMessage( 0 );
      
    resize( 1100, 900 );
}

void DemoApp::about()
{
    QMessageBox::about( this, tr( "OpenChange GUI Test Harness" ),
			tr( "<qt>This test harness is for demonstrating and testing "
			    "some of the OpenChange functionality. It is not "
			    "intended for use in production.<br/><br/> <b>We mean it.</b></qt>" ) );
}

void DemoApp::createActions()
{
    m_aboutAction = new QAction( tr( "About" ), this );
    connect( m_aboutAction, SIGNAL( triggered() ), this, SLOT( about() ) );
    
    m_quitAction = new QAction( tr( "&Quit" ), this );
    // Use this if we ever require Qt 4.6
    // m_quitAction->setShortcuts( QKeySequence::Quit );
    m_quitAction->setShortcut( QKeySequence( Qt::Key_Q, Qt::CTRL ) );
    m_quitAction->setStatusTip( tr( "Quit the application" ) );
    connect( m_quitAction, SIGNAL( triggered() ), this, SLOT( close() ) );
}

void DemoApp::createMenus()
{
    m_fileMenu = menuBar()->addMenu( tr( "&File" ) );
    m_fileMenu->addAction( m_quitAction );
    
    m_helpMenu = menuBar()->addMenu( tr( "&Help" ) );
    m_helpMenu->addAction( m_aboutAction );
}

void DemoApp::login()
{
    // Initialize MAPI Session
    m_mapi_session = new libmapipp::session();

    m_mapi_session->login();
}

void DemoApp::addFolderDockWidget()
{
    m_folderDock = new QDockWidget( tr( "Folders" ), this );
    
    FolderModel *folder = new FolderModel( m_mapi_session );
    m_folderModel = folder->buildModel();
    
    QTreeView *folderDockView = new QTreeView( m_folderDock );
    folderDockView->setModel( m_folderModel );
    folderDockView->expandToDepth( 2 );
    connect( folderDockView, SIGNAL( clicked(QModelIndex) ), this, SLOT( folderChanged(QModelIndex) ) );
    m_folderDock->setWidget( folderDockView );
    
    addDockWidget(Qt::LeftDockWidgetArea, m_folderDock);
}

void DemoApp::addMessagesDockWidget()
{
    QDockWidget *messagesDock = new QDockWidget( tr( "Messages" ), this );
    
    // Get Default Inbox folder ID.
    mapi_id_t inbox_id = m_mapi_session->get_message_store().get_default_folder(olFolderInbox);
    // std::cout << "inbox_id: " << inbox_id << std::endl;

    // Open Inbox Folder
    m_folder = new folder(m_mapi_session->get_message_store(), inbox_id);
    MessagesModel *messages = new MessagesModel( m_folder );
    
    m_messagesDockView = new QTableView( messagesDock );
    m_messagesModel = messages->buildModel();
    m_messagesDockView->setModel( m_messagesModel );
    m_messagesDockView->setShowGrid( false );
    m_messagesDockView->resizeColumnsToContents();
    m_messagesDockView->resizeRowsToContents();
    connect( m_messagesDockView, SIGNAL( clicked(QModelIndex) ), this, SLOT( messageChanged(QModelIndex) ) );
    messagesDock->setWidget( m_messagesDockView );
    
    splitDockWidget( m_folderDock, messagesDock, Qt::Horizontal );
}

void DemoApp::folderChanged(const QModelIndex &index)
{
    QStandardItem *item = m_folderModel->itemFromIndex( index );
    if (item) {
	qlonglong folderId = item->data().toLongLong();
	m_folder = new folder(m_mapi_session->get_message_store(), folderId);
	MessagesModel *messages = new MessagesModel( m_folder );
	m_messagesModel = messages->buildModel();
	m_messagesDockView->setModel( m_messagesModel );
    }
}

void DemoApp::messageChanged( const QModelIndex &index )
{
    QStandardItem *item = m_messagesModel->itemFromIndex( index );
    if ( item ) {
	quint32 messageNumber = item->data().toUInt();
	openMessage( messageNumber );
    }
}

void DemoApp::openMessage( quint32 messageNumber )
{
    libmapipp::folder::message_container_type messages = m_folder->fetch_messages();
    // std::cout << "Folder contains " << messages.size() << " messages" << std::endl;

    // Get the properties we are interested in
    libmapipp::property_container msg_props = messages[messageNumber]->get_property_container();
    msg_props << PR_BODY_HTML;
    msg_props.fetch();

    if ( msg_props[PR_BODY_HTML] ) {
	QString body = QString( (const char*)msg_props[PR_BODY_HTML] );
	m_textEdit->setHtml( body );
    } else {
	m_textEdit->setHtml( QString() );
    }
}

#include "demoapp.moc"
