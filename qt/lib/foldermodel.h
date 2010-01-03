#ifndef FOLDERMODEL_H
#define FOLDERMODEL_H

class QStandardItem;
class QStandardItemModel;

namespace libmapipp
{
class folder;
class session;
}

class FolderModel : public QStandardItemModel
{
    Q_OBJECT

  public:
    FolderModel( libmapipp::session *mapi_session );

    QStandardItemModel* buildModel();

  private:
    libmapipp::session *m_mapi_session;
    QStandardItemModel *m_folderModel;
    
    void add_folder_to_tree(libmapipp::folder& up_folder, QStandardItem *parentItem);
};


#endif
