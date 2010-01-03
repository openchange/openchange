#ifndef DEMOAPP_H
#define DEMOAPP_H

#include <QtGui/QMainWindow>
#include <QtGui/QStandardItemModel>

class QAction;
class QMenu;
class QTextEdit;
class QStandardItem;
class QTableView;

namespace libmapipp
{
  class session;
  class folder;
};

class DemoApp : public QMainWindow
{
    Q_OBJECT

  public:
    DemoApp();

  private slots:
    void about();
    void folderChanged( const QModelIndex &index );
    void messageChanged( const QModelIndex &index );
    
  private:
    void createActions();
    void createMenus();

    void login();
    
    void addFolderDockWidget();
    void addMessagesDockWidget();
    
    void openMessage( quint32 messageNumber );

    QMenu *m_fileMenu;
    QMenu *m_helpMenu;

    QAction *m_quitAction;
    QAction *m_aboutAction;
    
    QDockWidget *m_folderDock;
    QStandardItemModel *m_folderModel;
    
    QTableView *m_messagesDockView;
    QStandardItemModel *m_messagesModel;
    
    libmapipp::folder *m_folder;
    QTextEdit *m_textEdit;
    
    libmapipp::session *m_mapi_session;
};

#endif

