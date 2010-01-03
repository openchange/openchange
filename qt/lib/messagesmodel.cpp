#include "messagesmodel.h"

#include <libmapi++/libmapi++.h>

using namespace libmapipp;

MessagesModel::MessagesModel( libmapipp::folder *folder ):
  m_mapi_folder( folder )
{
}
    
QStandardItemModel* MessagesModel::buildModel()
{
    // Fetch messages in Inbox Folder
    folder::message_container_type messages = m_mapi_folder->fetch_messages();

    QStandardItemModel *messagesModel = new QStandardItemModel();
    QStringList messagesModelHeaders;
    messagesModelHeaders << QString( "Topic" ) << QString( "To" ) << QString( "From" );
    messagesModel->setHorizontalHeaderLabels( messagesModelHeaders );

    for ( unsigned int i = 0; i < messages.size(); ++i ) {
	property_container message_property_container = messages[i]->get_property_container();

	// Add Property Tags to be fetched and then fetch the properties.
	message_property_container << PR_DISPLAY_TO << PR_CONVERSATION_TOPIC << PR_SENDER_NAME;
	message_property_container.fetch_all();

	std::string to;
	std::string subject;
	std::string from;

	for (property_container::iterator Iter = message_property_container.begin(); Iter != message_property_container.end(); Iter++)
	{
		if (Iter.get_tag() == PR_DISPLAY_TO)
			to = (const char*) *Iter;
		else if (Iter.get_tag() == PR_CONVERSATION_TOPIC)
			subject = (const char*) *Iter;
		else if (Iter.get_tag() == PR_SENDER_NAME)
			from = (const char*) *Iter;
	}
	QList< QStandardItem * > row;
	row << new QStandardItem( QString::fromStdString( subject ) );
	row[0]->setData( (quint32) i );
	row << new QStandardItem( QString::fromStdString( to ) );
	row << new QStandardItem( QString::fromStdString( from ) );
	messagesModel->appendRow( row );
    }
    
    return messagesModel;
}



#include "messagesmodel.moc"
