#include "mainwindow.h"
#include "fontloader.h"
#include "multimedia.h"
#include <QApplication>
#include <QStyleFactory>
#include <QStyle>
#include <QFile>
#include <QDir>
#include <QIcon>
#include <QSplashScreen>
#include <QPixmap>
#include <QThread>
#include <QEventLoop>
#include <QLabel>
#include <QSlider>
#include <QEvent>
#include <memory>
int smokeTest(QApplication &app);
int playbackTest(QApplication &app);
int mediaLoadTest(QApplication &app);
int main(int argc,char **argv){
    QApplication app(argc,argv);app.setApplicationName("Sessions");app.setOrganizationName("Sessions");
    app.setApplicationVersion("1.0.0");app.setWindowIcon(QIcon(":/icons/SessionsAppIcon.png"));
    const bool testMode=app.arguments().contains("--smoke-test")||app.arguments().contains("--playback-test")||app.arguments().contains("--media-load-test");
    // Prepare the media engine alongside fonts and styles while the splash
    // remains visible and its event loop stays responsive.
    QSplashScreen splash(QPixmap(":/icons/splashscreen.png"));
    QLabel startupStatus(&splash);
    startupStatus.setGeometry(16,splash.height()-48,splash.width()-32,18);
    startupStatus.setStyleSheet("background: transparent; border: none; padding: 0; color: #80a4ae; font-family: Tahoma; font-size: 11px;");
    startupStatus.setText("Preparando multimedia…");
    QSlider startupProgress(Qt::Horizontal,&splash);
    startupProgress.setObjectName("splashLoading");
    startupProgress.setGeometry(16,splash.height()-28,180,16);
    startupProgress.setRange(0,5);startupProgress.setValue(0);
    startupProgress.setFocusPolicy(Qt::NoFocus);
    startupProgress.setAttribute(Qt::WA_TransparentForMouseEvents);
    startupProgress.setAccessibleName("Preparación del motor multimedia");
    startupProgress.setStyleSheet(
        "QSlider { background: transparent; border: none; }"
        "QSlider::groove:horizontal { border: 1px solid #686b74; height: 6px; background: transparent; border-radius: 3px; }"
        "QSlider::sub-page:horizontal { background: #80a4ae; border-radius: 3px; }"
        "QSlider::handle:horizontal { background: #80a4ae; width: 10px; margin: -3px 0; border: 1px solid #80a4ae; border-radius: 3px; }");
    std::unique_ptr<QThread> mediaWarmup;
    if(!testMode){
        splash.setCursor(Qt::WaitCursor);
        splash.show();
        QCoreApplication::processEvents();
        mediaWarmup.reset(QThread::create([&]{
            Multimedia::warmUpBackend([&](int phase,const QString &text){
                QMetaObject::invokeMethod(&splash,[&,phase,text]{
                    startupProgress.setValue(phase);startupStatus.setText(text);
                    startupProgress.setAccessibleDescription(text);
                },Qt::QueuedConnection);
            });
        }));
        mediaWarmup->start();
    }
    QString fontDirectory=QDir(app.applicationDirPath()).filePath("font");
    if(!QDir(fontDirectory).exists())fontDirectory=QDir(app.applicationDirPath()).filePath("fonts");
    FontLoader::load(fontDirectory);
    app.setStyle(QStyleFactory::create("Windows"));app.setFont(QFont("Tahoma",9));
    QPalette palette = app.style()->standardPalette();
    palette.setColor(QPalette::Window, QColor("#181e2c"));
    palette.setColor(QPalette::Button, QColor("#262c3b"));
    palette.setColor(QPalette::WindowText, QColor("#8a8d96"));
    palette.setColor(QPalette::ButtonText, QColor("#80a4ae"));
    palette.setColor(QPalette::Base, QColor("#252b39"));
    palette.setColor(QPalette::Text, QColor("#d4e1e4"));
    palette.setColor(QPalette::Highlight, QColor("#4e4d7a"));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    app.setPalette(palette);
    QFile raditStyle(QDir(app.applicationDirPath()).filePath("radit.qss"));
    if (!raditStyle.open(QIODevice::ReadOnly)) {
        raditStyle.setFileName(":/radit.qss");
        raditStyle.open(QIODevice::ReadOnly);
    }
    QFile sessionsStyle(":/sessions.qss");
    sessionsStyle.open(QIODevice::ReadOnly);
    app.setStyleSheet(QString::fromUtf8(raditStyle.readAll()) + "\n" + QString::fromUtf8(sessionsStyle.readAll()));
    if(app.arguments().contains("--smoke-test"))return smokeTest(app);
    if(app.arguments().contains("--playback-test"))return playbackTest(app);
    if(app.arguments().contains("--media-load-test"))return mediaLoadTest(app);
    if(mediaWarmup) {
        QEventLoop waiting;
        QObject::connect(mediaWarmup.get(),&QThread::finished,&waiting,&QEventLoop::quit);
        if(mediaWarmup->isRunning())waiting.exec();
        mediaWarmup->wait();
        QCoreApplication::sendPostedEvents(&splash,QEvent::MetaCall);
    }
    MainWindow window;
    if(!testMode)splash.finish(&window);
    window.show();
    return app.exec();
}
