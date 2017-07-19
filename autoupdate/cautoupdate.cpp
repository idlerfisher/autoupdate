#include "cautoupdate.h"
#include "ui_cautoupdate.h"
#include "jsoncpp/CJson.h"
#include <QCryptographicHash>
#include <winbase.h>
#include <windows.h>

#define SET_UPDATE          "config.ini"
#define APP_TITLE           "����Ƽ��Զ����� v1.2"
#define APP_NAME            "autoupdate.exe"
#define PROJECT_MANIFEST    "project.manifest"
#define TMP_DIR             "tmp"

CAutoUpdate::CAutoUpdate(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::CAutoUpdate)
{
    ui->setupUi(this);

    initUi();
    readSettings();

//    HANDLE hMap=CreateFileMappingA((HANDLE)0xffffffff,NULL,
//                                  PAGE_READWRITE,0,128,"TH_FACER_DEMO");
//    if (hMap != NULL) {
//        if (GetLastError() == ERROR_ALREADY_EXISTS) {
//            CloseHandle(hMap);
//            hMap = NULL;
//            qDebug() << "####";
//        } else {
//            qDebug() << "aaAAA";
//        }
//    }

//    QDir dir = QDir::current();
//    dir.cdUp();
//    qDebug() << dir.entryList();

//    createRemoteManifest(m_mapLocalManifest, m_settings.strUpdateDir);

    QTimer::singleShot(1, this, SLOT(slotTimeout()));
}

CAutoUpdate::~CAutoUpdate()
{
    delete ui;
}

void CAutoUpdate::closeEvent(QCloseEvent *e)
{
    this->hide();
    e->ignore();
    if (m_pSysTrayIcon)
    {
        m_pSysTrayIcon->showMessage(tr("��ʾ"), tr("������ں�̨���£�"), QSystemTrayIcon::Information);
    }
}

void CAutoUpdate::initUi()
{
    m_nSize = 0;
    ui->labelSize->setText("0B");
    m_bStopUpdate = false;

    setWindowIcon(QIcon(":/images/logo.ico"));
    setWindowFlags(Qt::WindowCloseButtonHint | Qt::WindowMinimizeButtonHint);
    setFixedSize(width(), height());
    setWindowTitle(APP_TITLE);
    ui->labelFileName->clear();
    ui->pgsBarUpdate->setValue(0);
    ui->pgsBarUpdate->setMaximum(1);
    ui->pgsBarCopy->setValue(0);
    ui->pgsBarCopy->setMaximum(1);

    m_pActQuit = new QAction(QIcon(":/images/exit.png"), tr("�˳�"), this);
    connect(m_pActQuit, SIGNAL(triggered()), this, SLOT(slotActQuit()));

//    m_pActStop = new QAction(QIcon(":/images/stop.png"), tr("ֹͣ����"), this);
//    connect(m_pActStop, SIGNAL(triggered()), this, SLOT(slotActStop()));

    m_pSysTrayMenu = new QMenu(this);
//    m_pSysTrayMenu->addAction(m_pActStop);
    m_pSysTrayMenu->addAction(m_pActQuit);

    m_pSysTrayIcon = new QSystemTrayIcon(QIcon(":/images/logo.ico"), this);
    m_pSysTrayIcon->setToolTip(APP_TITLE);
    m_pSysTrayIcon->setContextMenu(m_pSysTrayMenu);
    m_pSysTrayIcon->show();
    connect(m_pSysTrayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this, SLOT(slotSysTrayIconActivated(QSystemTrayIcon::ActivationReason)));

    connect(&m_timer, SIGNAL(timeout()), this, SLOT(slotTimeout()));
    connect(&m_curl, SIGNAL(signalSize(int)), this ,SLOT(slotCurlSize(int)));
}

void CAutoUpdate::readSettings()
{
    QString strKey, strTmp;
    QString strPath = qApp->applicationDirPath() + "/" + tr(SET_UPDATE);
    QSettings set(strPath, QSettings::IniFormat);

    strKey = "update/update_dir";
    if (set.contains(strKey)) {
        m_settings.strUpdateDir = toGbk(set.value(strKey).toString());
    } else {
        m_settings.strUpdateDir = "..";
        set.setValue(strKey, m_settings.strUpdateDir);
    }

    strKey = "update/not_update_dirs";
    if (set.contains(strKey)) {
        strTmp = toGbk(set.value(strKey).toString());
        m_settings.strNotUpdateDirList = strTmp.split(';');
    } else {
        strTmp = "id_logs;";
        set.setValue(strKey, strTmp);
        m_settings.strNotUpdateDirList = strTmp.split(';');
    }

    strKey = "update/not_update_files";
    if (set.contains(strKey)) {
        strTmp = toGbk(set.value(strKey).toString());
        m_settings.strNotUpdateFileList = strTmp.split(';');
    } else {
        strTmp = "cardreadlog.txt;CollectConfig.ini;IDInfoLog.txt;Log.dat;upload.txt;";
        set.setValue(strKey, strTmp);
        m_settings.strNotUpdateFileList = strTmp.split(';');
    }

    strKey = "update/update_url";
    if (set.contains(strKey)) {
        m_settings.strUpdateUrl = set.value(strKey).toString();
    } else {
        m_settings.strUpdateUrl = "";
        set.setValue(strKey, "http://127.0.0.1:8080");
        QMessageBox::warning(this, "��ʾ", "�����ø��µ�ַupdate_url��");
        return;
    }

    strKey = "update/app";
    if (set.contains(strKey)) {
        m_settings.strApp = toGbk(set.value(strKey).toString());
    } else {
        m_settings.strApp = "FaceHuaMaWT.exe";
        set.setValue(strKey, m_settings.strApp);
    }

    strKey = "update/update_interval";
    if (set.contains(strKey)) {
        m_settings.nUpdateInterval = set.value(strKey).toInt();
    } else {
        m_settings.nUpdateInterval = 3600;
        set.setValue(strKey, m_settings.nUpdateInterval);
    }
}

bool CAutoUpdate::createLocalManifest(QQMAP &mapManifest, const QString &strPath)
{
    m_fileInfoList.clear();
    mapManifest.clear();

    searchFile(m_fileInfoList, strPath);

    QString strFileName, strMd5;
    foreach (QFileInfo info, m_fileInfoList) {
        strFileName = info.filePath().replace(m_settings.strUpdateDir + "/", "");
        QFile file(info.filePath());
        if (file.open(QFile::ReadOnly)) {
            strMd5 = QCryptographicHash::hash(file.readAll(), QCryptographicHash::Md5).toHex();
        } else {
            qDebug() << "��ȡ�ļ�ʧ��!";
            mapManifest.clear();
            return false;
        }
        file.close();
        mapManifest[strFileName] = strMd5;
        QCoreApplication::processEvents();
//        qDebug() << strFileName << " -> " << strMd5;
    }

    return true;
}

bool CAutoUpdate::createRemoteManifest(QQMAP &mapManifest, const QString &strPath)
{
    m_fileInfoList.clear();
    mapManifest.clear();

    searchFile(m_fileInfoList, strPath);

    Json::Value jsonVal;
    QString strFileName, strMd5;
    foreach (QFileInfo info, m_fileInfoList) {
        strFileName = info.filePath().replace(m_settings.strUpdateDir + "/", "");
        QFile file(info.filePath());
        if (file.open(QFile::ReadOnly)) {
            strMd5 = QCryptographicHash::hash(file.readAll(), QCryptographicHash::Md5).toHex();
        } else {
            qDebug() << "��ȡ�ļ�ʧ��!";
            mapManifest.clear();
            return false;
        }
        file.close();
        mapManifest[strFileName] = strMd5;
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
        if (strCurrentPath == info.fileName() || info.fileName() == "." || info.fileName() == "..") {
            continue;
        }

        if (info.isFile() && m_settings.strNotUpdateFileList.indexOf(info.fileName()) == -1) {
            infoList.append(info);
        } else if (info.isDir() && m_settings.strNotUpdateDirList.indexOf(info.fileName()) == -1) {
            searchFile(infoList, info.filePath());
        }
    }
}

void CAutoUpdate::searchFileEx(QFileInfoList &infoList, const QString &strPath)
{
    QString strCurrentPath = getCurrentDirName();
    QDir dir(strPath);
    QFileInfoList fileList = dir.entryInfoList();
    foreach (QFileInfo info, fileList) {
        if (info.fileName() == "." || info.fileName() == "..") {
            continue;
        }

        if (info.isFile()) {
            infoList.append(info);
        } else if (info.isDir() && strCurrentPath != info.fileName()) {
            searchFileEx(infoList, info.filePath());
        }
    }
}

QString CAutoUpdate::getCurrentDirName()
{
    QString strPath = qApp->applicationDirPath();
    int index = strPath.lastIndexOf('/');
    QString strCurrentDir = strPath.right(strPath.length() - index - 1);
    return strCurrentDir;

//    QString strPath = QDir::currentPath();
//    QStringList strList = strPath.split('/');
//    if (!strList.isEmpty()) {
//        return strList.last();
//    }
//    return "";
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
    const QString s_tmpDir = TMP_DIR;
    QString strUrl, strTmp;

    //ÿ�����´���Ŀ¼
    QDir dir = QDir::current();
    if (dir.exists(s_tmpDir)) {
        removeAllFiles(s_tmpDir);
    }
    dir.mkdir(s_tmpDir);

    int index = 0, count = 0;
    QString strFileName, strName, strPath;
    ui->pgsBarUpdate->setMaximum(fileList.size());
    ui->pgsBarUpdate->setValue(0);
    ui->labelSize->setText("0B");
    m_nSize = 0;
    for (auto& it : fileList) {
        if (m_bStopUpdate) {
            qApp->quit();
            return false;
        }

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
            dir.mkpath(s_tmpDir + "/" + strPath);
//            qDebug() << s_tmpDir + "/" + strPath;
        }
//        qDebug() << "name=" << strName << " path=" << strPath;

        ui->labelFileName->setText(strFileName);

        //����
        strTmp = s_tmpDir + "/" + strFileName;
        QFile file(strTmp);
        if (!file.open(QFile::WriteOnly)) {
            return false;
        }
        strUrl = m_settings.strUpdateUrl + "/" + strFileName;
        m_curl.Get(strUrl, &file);
        file.close();

        ui->pgsBarUpdate->setValue(++count);
        QCoreApplication::processEvents();
    }

    return true;
}

QString CAutoUpdate::removeSetComment(const QString &str)
{
    int index = str.indexOf('#');
    return str.left(index);
}

bool CAutoUpdate::canUpdate()
{
    if (m_settings.strUpdateUrl.isEmpty()) {
        QMessageBox::warning(this, "��ʾ", "�����ø��µ�ַupdate_url");
        return false;
    }

    return true;
}

void CAutoUpdate::removeAllFiles(const QString &strPath)
{
    QDir dir(strPath);
    QFileInfoList fileInfoList = dir.entryInfoList();
    foreach (QFileInfo info, fileInfoList) {
        if (info.fileName() == "." || info.fileName() == "..") {
            continue;
        }

        if (info.isFile()) {
            QFile::remove(info.filePath());
        } else if (info.isDir()) {
            removeAllFiles(info.filePath());
        }
    }
}

void CAutoUpdate::copyAllFiles(const QString &strSrcPath, const QString &strDstPath)
{
    QString strTmp, strPre;

    //src
    QQMAP kvMapSrc;
    QFileInfoList fileInfoListSrc;
    searchFileEx(fileInfoListSrc, strSrcPath);
    strPre = strSrcPath + "/";
    foreach (QFileInfo info, fileInfoListSrc) {
        strTmp = info.filePath().replace(strPre, "");
        kvMapSrc[strTmp] = info.absoluteFilePath();
//        qDebug() << "src " << strTmp << " -> " << info.absoluteFilePath();
    }

    QQMAP kvMapDst;
    QFileInfoList fileInfoListDst;
    searchFileEx(fileInfoListDst, strDstPath);
    strPre = strDstPath + "/";
    foreach (QFileInfo info, fileInfoListDst) {
        strTmp = info.filePath().replace(strPre, "");
        kvMapDst[strTmp] = info.absoluteFilePath();
//        qDebug() << "dst " << strTmp << " -> " << info.absoluteFilePath();
    }

    //�Ӹ����ļ���ʼ������ȥ
    int count = 0;
    QString strSimplePath, strAbsolutePath, strAbsolutePathDst;
    ui->pgsBarCopy->setMaximum(kvMapSrc.size());
    for (auto& it : kvMapSrc) {
        strSimplePath = it.first;
        strAbsolutePath = it.second;
        strAbsolutePathDst = kvMapDst[strSimplePath];
        if (QFile::exists(strAbsolutePath)) {
            QFile::remove(strAbsolutePathDst);
//            qDebug() << "remove " << strAbsolutePathDst << QFile::remove(strAbsolutePathDst);
        }
        QFile::copy(strAbsolutePath, strAbsolutePathDst);
//        qDebug() << "copy " << strSimplePath << QFile::copy(strAbsolutePath, strAbsolutePathDst);
        ui->pgsBarCopy->setValue(++count);
        ui->labelFileName->setText(strSimplePath);
    }
}

QString CAutoUpdate::convertFileSize(int size)
{
    static const int s_b = 1024;
    static const int s_kb = 1024 * 1024;
    static const int s_mb = 1024 * 1024 * 1024;

    QString strSize;
    if (size < s_b) {
        strSize = tr("%1B").arg(size);
    } else if (size < s_kb) {
        strSize = tr("%1KB").arg(1.0 * size / s_b, 0, 'f', 2);
    } else if (size < s_mb) {
        strSize = tr("%1MB").arg(1.0 * size / s_kb, 0, 'f', 2);
    }

    return strSize;
}

QString CAutoUpdate::toGbk(const QString &strIn)
{
    QTextCodec* codec = QTextCodec::codecForLocale();
    if (codec) {
        return codec->toUnicode(strIn.toLatin1());
    }
    return strIn;
}

void CAutoUpdate::slotTimeout()
{
    this->hide();

    m_timer.stop();
    m_bStopUpdate = false;

    ui->labelFileName->setText("������");

    if (!canUpdate()) {
        m_timer.start(m_settings.nUpdateInterval * 1000);
        return;
    }

    QString strTmp;

    //��ȡ���������Ϣ
    createLocalManifest(m_mapLocalManifest, m_settings.strUpdateDir);
//    createRemoteManifest(m_mapRemoteManifest, m_settings.strUpdateUrl + "/" + PROJECT_MANIFEST);

    if (!getRemoteManifest(m_mapRemoteManifest)) {
        m_pSysTrayIcon->showMessage("��ʾ", "��ȡ�汾��Ϣʧ�ܣ�", QSystemTrayIcon::Information);
        m_timer.start(m_settings.nUpdateInterval * 1000);
        return;
    }

    //�ȶ԰汾��Ϣ
    FileList diffList;
    compareLocalRemoteManifest(m_mapLocalManifest, m_mapRemoteManifest, diffList);
    if (diffList.empty()) {
        ui->labelFileName->setText("����Ѿ������°汾��");
        m_timer.start(m_settings.nUpdateInterval * 1000);
        return;
    }

    //�����ļ�
    m_pSysTrayIcon->showMessage("��ʾ", "��ʼ���ظ����ļ�������", QSystemTrayIcon::Information);
    if (!downloadDiffFiles(diffList)) {
        m_pSysTrayIcon->showMessage("��ʾ", "���ظ����ļ�ʧ�ܣ�", QSystemTrayIcon::Information);
        m_timer.start(m_settings.nUpdateInterval * 1000);
        return;
    }

    //ɱ������
    strTmp = tr("taskkill /im %1* /f").arg(m_settings.strApp);
    QProcess::execute(strTmp);

    //���������ļ�
    copyAllFiles(TMP_DIR, m_settings.strUpdateDir);

    //�������,���е�Ŀ��Ŀ¼���������л�ȥ�����ڶԷ�������������·������������Ҫ��ô����
    QString strCurrentDir = qApp->applicationDirPath();
    QDir::setCurrent(m_settings.strUpdateDir + "/");
    QProcess::startDetached(m_settings.strApp);
    QDir::setCurrent(strCurrentDir);

    //����
    ui->labelFileName->setText("�������");
    m_pSysTrayIcon->showMessage("��ʾ", "������ɣ�", QSystemTrayIcon::Information);

    //����������һ�θ���
    m_timer.start(m_settings.nUpdateInterval * 1000);

    this->hide();
}

void CAutoUpdate::slotActQuit()
{
    if (QMessageBox::warning(this, tr("�˳�"), tr("ȷ���˳��Զ����£�"), QMessageBox::Ok|QMessageBox::Cancel) == QMessageBox::Ok)
    {
        qApp->quit();
        m_bStopUpdate = true;
        m_pSysTrayIcon->showMessage("��ʾ", "��������ļ������˳������Եȣ�");
    }
}

void CAutoUpdate::slotActStop()
{
    m_bStopUpdate = true;
    m_pSysTrayIcon->showMessage("��ʾ", "�����¸��ļ�ֹͣ���أ�");
}

void CAutoUpdate::slotSysTrayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (QSystemTrayIcon::Trigger == reason)
    {
        this->activateWindow();
        this->showNormal();
    }
}

void CAutoUpdate::slotCurlSize(int size)
{
    m_nSize += size;
    ui->labelSize->setText(convertFileSize(m_nSize));
    QCoreApplication::processEvents();
}

