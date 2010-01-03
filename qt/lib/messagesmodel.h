#ifndef MESSAGESMODEL_H
#define MESSAGESMODEL_H

class QStandardItem;
#include <QStandardItemModel>

namespace libmapipp
{
class folder;
class session;
}


class MessagesModel : public QStandardItemModel
{
    Q_OBJECT
    
  public:
    MessagesModel( libmapipp::folder *folder );
    
    QStandardItemModel *buildModel();
    
  private:
    libmapipp::folder *m_mapi_folder;
};

#endif
