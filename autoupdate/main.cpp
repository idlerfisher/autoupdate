#include <QApplication>
#include "qtsingleapplication.h"
#include <QTextCodec>
#include "cautoupdate.h"
#include "ccurl.h"

int main(int argc, char *argv[])
{
    CCurl::GlobalInit();

//    QApplication a(argc, argv);
    QtSingleApplication a(argc, argv);
    if (a.IsRunning()) {
        return 0;
    }

    QTextCodec::setCodecForLocale(QTextCodec::codecForLocale());
    QTextCodec::setCodecForCStrings(QTextCodec::codecForLocale());
    QTextCodec::setCodecForTr(QTextCodec::codecForLocale());

    //�������������Ϊ:����
    QTranslator translator;
    translator.load(a.applicationDirPath() + "/qt_zh_CN.qm");
    a.installTranslator(&translator);

    CAutoUpdate w;

    //�Զ�����
    if (true) {
        w.show();

        CCurl::GlobalCleanup();

        return a.exec();
    }

    //����project.manifest
    return 1;
}
