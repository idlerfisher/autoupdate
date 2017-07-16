#include "cautoupdate.h"
#include "ui_cautoupdate.h"
#include "jsoncpp/CJson.h"
#include <QCryptographicHash>

#define SET_UPDATE          "config.ini"
#define APP_TITLE           "�Զ�����"
#define APP_NAME            "autoupdate.exe"
#define PROJECT_MANIFEST    "project.manifest"

CAutoUpdate::CAutoUpdate(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::CAutoUpdate)
{
    ui->setupUi(this);

    initUi();
    readSettings();

//    createRemoteManifest(m_mapLocalManifest, m_settings.strUpdateDir);

    QTimer::singleShot(1000, this, SLOT(slotTimeout()));
}

CAutoUpdate::~CAutoUpdate()
{
    delete ui;
}

void CAutoUpdate::initUi()
{
    setWindowFlags(Qt::WindowCloseButtonHint | Qt::WindowMinimizeButtonHint);
    setFixedSize(width(), height());
    setWindowTitle(APP_TITLE);
    ui->labelFileName->clear();
    ui->progressBar->setValue(0);
    ui->progressBar->setMaximum(1);
}

void CAutoUpdate::readSettings()
{
    QString strKey, strTmp;
    QString strPath = qApp->applicationDirPath() + "/" + tr(SET_UPDATE);
    QSettings set(strPath, QSettings::IniFormat);

    strKey = "update/update_dir";
    if (set.contains(strKey)) {
        m_settings.strUpdateDir = set.value(strKey).toString();
    } else {
        m_settings.strUpdateDir = "../";
        set.setValue(strKey, m_settings.strUpdateDir);
    }

    strKey = "update/dir_whitelist";
    if (set.contains(strKey)) {
        strTmp = set.value(strKey).toString();
        m_settings.strDirWhiteList = strTmp.split(';');
    } else {
        set.setValue(strKey, "");
    }

    strKey = "update/update_url";
    if (set.contains(strKey)) {
        m_settings.strUpdateUrl = set.value(strKey).toString();
    } else {
        set.setValue(strKey, "http://www.abc.com");
        QMessageBox::warning(this, "��ʾ", "�����ø��µ�ַupdate_url��");
        return;
    }
}

bool CAutoUpdate::createLocalManifest(QQMAP &mapManifest, const QString &strPath)
{
    m_fileInfoList.clear();
    m_mapLocalManifest.clear();

    searchFile(m_fileInfoList, strPath);

    QString strFileName, strMd5;
    foreach (QFileInfo info, m_fileInfoList) {
        strFileName = info.filePath().replace(m_settings.strUpdateDir, "");
        QFile file(info.filePath());
        if (file.open(QFile::ReadOnly)) {
            strMd5 = QCryptographicHash::hash(file.readAll(), QCryptographicHash::Md5).toHex();
        } else {
            qDebug() << "��ȡ�ļ�ʧ��!";
            m_mapLocalManifest.clear();
            return false;
        }
        file.close();
        m_mapLocalManifest[strFileName] = strMd5;
        QCoreApplication::processEvents();
//        qDebug() << strFileName << " -> " << strMd5;
    }

    return true;
}

bool CAutoUpdate::createRemoteManifest(QQMAP &mapManifest, const QString &strPath)
{
    m_fileInfoList.clear();
    m_mapLocalManifest.clear();

    searchFile(m_fileInfoList, strPath);

    Json::Value jsonVal;
    QString strFileName, strMd5;
    foreach (QFileInfo info, m_fileInfoList) {
        strFileName = info.filePath().replace(m_settings.strUpdateDir, "");
        QFile file(info.filePath());
        if (file.open(QFile::ReadOnly)) {
            strMd5 = QCryptographicHash::hash(file.readAll(), QCryptographicHash::Md5).toHex();
        } else {
            qDebug() << "��ȡ�ļ�ʧ��!";
            m_mapLocalManifest.clear();
            return false;
        }
        file.close();
        m_mapLocalManifest[strFileName] = strMd5;
        jsonVal[strFileName.toStdString()] = strMd5.toStdString();
        QCoreApplication::processEvents();
    }

    QFile file("project.manifest");
    if (file.open(QFile::WriteOnly)) {
        file.write(jsonVal.toStyledString().c_str());
    }
    file.close();

    return true;
}

void CAutoUpdate::searchFile(QFileInfoList &infoList, const QString &strPath)
{
    QString strCurrentPath = getCurrentDirName();
    QDir dir(strPath);
    QFileInfoList fileList = dir.entryInfoList();
    foreach (QFileInfo info, fileList) {
        if (info.isFile()) {
            infoList.append(info);
        } else if (info.isDir() && info.fileName() != "." &&
                   info.fileName() != ".." && strCurrentPath != info.fileName() &&
                   m_settings.strDirWhiteList.indexOf(info.fileName()) == -1) {
            searchFile(infoList, info.filePath());
        }
    }
}

QString CAutoUpdate::getCurrentDirName()
{
    QString strPath = QDir::currentPath();
    QStringList strList = strPath.split('/');
    if (!strList.isEmpty()) {
        return strList.last();
    }
    return "";
}

bool CAutoUpdate::getRemoteManifest(QQMAP &mapManifest)
{
    //��ȡԶ����Ϣ
    QString strUrlManifest = m_settings.strUpdateUrl + "/" + tr(PROJECT_MANIFEST);
    QString strOutput;
    bool bRes = m_curl.Get(strUrlManifest, strOutput);
    if (!bRes) {
        return false;
    }

    JsonStringMap kvMap;
    CJson::JsonToMap(strOutput.toStdString(), kvMap);

    mapManifest.clear();
    for (auto& it : kvMap) {
        mapManifest[QString::fromStdString(it.first)] = QString::fromStdString(it.second);
    }
    return true;
}

void CAutoUpdate::compareLocalRemoteManifest(QQMAP &local, QQMAP &remote, FileList &fileList)
{
    for (auto& it : remote) {
        auto fit = local.find(it.first);
        if (local.end() == fit) {
            //�����ļ�
            fileList.push_back(it.first);
        } else if (fit->second != it.second) {
            //md5��ͬ
            fileList.push_back(it.first);
        }
    }
}

bool CAutoUpdate::downloadDiffFiles(const FileList &fileList)
{
    static const QString s_tmpDir = "tmp";
    QString strUrl;

    //ÿ�����´���Ŀ¼
    QDir dir = QDir::current();
    if (dir.exists(s_tmpDir)) {
        dir.remove(s_tmpDir);
    }
    dir.mkdir(s_tmpDir);

    int index = 0, count = 0;
    QString strFileName, strName, strPath;
    ui->progressBar->setMaximum(fileList.size());
    ui->progressBar->setValue(0);
    for (auto& it : fileList) {
        //����Ŀ¼
        strFileName = it;
        index = strFileName.lastIndexOf('/');
        if (-1 != index) {
            //��Ŀ¼���ļ�
            strName = strFileName.right(strFileName.length() - index - 1);
            strPath = strFileName.left(index);
        } else {
            strName = strFileName;
            strPath = "";
        }
        if (!strPath.isEmpty()) {
            dir.mkpath(strPath);
        }
//        qDebug() << "name=" << strName << " path=" << strPath;

        ui->labelFileName->setText(strFileName);

        //����
        strFileName = "DG2013_Final_1021E.exe";
        QFile file(s_tmpDir + "/" + strFileName);
        if (!file.open(QFile::WriteOnly)) {
            return false;
        }
        strUrl = m_settings.strUpdateUrl + "/" + strFileName;
        m_curl.Get(strUrl, &file);
        qDebug() << strUrl;
        file.close();

        ui->progressBar->setValue(++count);
        QCoreApplication::processEvents();
    }

    return true;
}

void CAutoUpdate::slotTimeout()
{
    QString strTmp;

    //��ȡ���������Ϣ
    createLocalManifest(m_mapLocalManifest, m_settings.strUpdateDir);
    createRemoteManifest(m_mapLocalManifest, m_settings.strUpdateDir);

    if (!getRemoteManifest(m_mapRemoteManifest)) {
        qDebug() << "��ȡԶ��manifestʧ��";
        QMessageBox::warning(this, "��ʾ", "��ȡ�汾��Ϣʧ�ܣ�");
        return;
    }

    //�ȶ԰汾��Ϣ
    FileList diffList;
    compareLocalRemoteManifest(m_mapLocalManifest, m_mapRemoteManifest, diffList);
    if (diffList.empty()) {
        QMessageBox::warning(this, "��ʾ", "����Ѿ������°汾��������£�");
        return;
    }

    //�����ļ�
    if (!downloadDiffFiles(diffList)) {
        QMessageBox::warning(this, "��ʾ", "����ʧ�ܣ�");
        return;
    }

    //ɱ������
    strTmp = "taskkill /im FaceHuaMaWT* /f";
    QProcess::execute(strTmp);

    //���������ļ�

    //�������
}

