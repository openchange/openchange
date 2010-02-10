#include <Qt/QtGui>
#include <exception>
#include <vector>
#include <string>

#include <QtCore/QStringList>
#include <QtCore/QDebug>

#include "foldermodel.h"

#include <libmapi++/libmapi++.h>

using namespace libmapipp;

FolderModel::FolderModel( libmapipp::session *mapi_session ) :
  m_mapi_session( mapi_session )
{
      m_folderModel = new QStandardItemModel();
      QStringList folderModelHeaders;
      folderModelHeaders << QString( "Folder Name" ) << QString( "FolderId" ) << QString( "Container Class" );
      m_folderModel->setHorizontalHeaderLabels( folderModelHeaders );
}

void FolderModel::add_folder_to_tree(libmapipp::folder& up_folder, QStandardItem *parentItem)
{
      property_container up_folder_property_container = up_folder.get_property_container();
      up_folder_property_container << PR_DISPLAY_NAME << PR_CONTAINER_CLASS;
      up_folder_property_container.fetch();

      std::string display_name = static_cast<const char*>(up_folder_property_container[PR_DISPLAY_NAME]);
      std::string container_class;
      if (up_folder_property_container[PR_CONTAINER_CLASS])
	      container_class = static_cast<const char*>(up_folder_property_container[PR_CONTAINER_CLASS]);

      QList< QStandardItem * > row;
      QStandardItem *name = new QStandardItem( QString::fromStdString( display_name ) );
      name->setData( (qlonglong)up_folder.get_id() );
      QStandardItem *folderId = new QStandardItem( QString::number( up_folder.get_id(), 16 ) );
      QStandardItem *containerClass = new QStandardItem( QString::fromStdString( container_class ) );
      row << name << folderId << containerClass;

      parentItem->appendRow( row );

      // Fetch Top Information Folder Hierarchy (child folders)
      folder::hierarchy_container_type hierarchy = up_folder.fetch_hierarchy();
      for (unsigned int i = 0; i < hierarchy.size(); ++i) {
	      add_folder_to_tree(*hierarchy[i], name);
      }
}

QStandardItemModel* FolderModel::buildModel()
{
      try {
	    // Get Default Top Information Store folder ID
	    mapi_id_t top_folder_id = m_mapi_session->get_message_store().get_default_folder(olFolderTopInformationStore);

	    // Open Top Information Folder
	    folder top_folder( m_mapi_session->get_message_store(), top_folder_id );

	    QStandardItem *parentItem = m_folderModel->invisibleRootItem();

	    add_folder_to_tree(top_folder, parentItem);
      }
      catch (mapi_exception e) // Catch any mapi exceptions
      {
              std::cout << "MAPI Exception @ main: " <<  e.what() << std::endl;
      }
      catch (std::runtime_error e) // Catch runtime exceptions
      {
              std::cout << "std::runtime_error exception @ main: " << e.what() << std::endl;
      }
      return m_folderModel;
}


#include "foldermodel.moc"
