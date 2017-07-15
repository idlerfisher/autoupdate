#ifndef CAUTOUPDATE_H
#define CAUTOUPDATE_H

#include <QDialog>
#include <QtCore>
#include <QtGui>

namespace Ui {
class CAutoUpdate;
}

struct stSettings {
    QString strUpdateDir; //����Ŀ¼
    QString strUpdateUrl; //������ַ
};

class CAutoUpdate : public QDialog
{
    Q_OBJECT
    
public:
    explicit CAutoUpdate(QWidget *parent = 0);
    ~CAutoUpdate();

private:
    void initUi();
    void readSettings();
    bool createLocalManifest(const QString& strPath);
    bool createRemoteManifest(const QString& strPath);
    void searchFile(QFileInfoList &infoList, const QString& strPath);
    QString getCurrentDirName();
    
private:
    Ui::CAutoUpdate *ui;

    stSettings m_settings;
    QFileInfoList m_fileInfoList;
    std::map<QString, QString> m_mapLocalManifest;
};
#endif // CAUTOUPDATE_H
