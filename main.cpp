#include "mainwindow.h"
#include <QApplication>
#include <QStyleFactory>
#include <QStyle>
#include <QFile>
#include <QDir>
#include <QIcon>
int smokeTest(QApplication &app);
int playbackTest(QApplication &app);
int main(int argc,char **argv){
    QApplication app(argc,argv);app.setApplicationName("Sessions");app.setOrganizationName("Sessions");
    app.setApplicationVersion("0.1.0");app.setWindowIcon(QIcon(":/icons/SessionsAppIcon.png"));
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
    MainWindow window;window.show();return app.exec();
}
