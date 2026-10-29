#pragma once
#include <QMainWindow>
#include <QPointer>
#include "canvas.h"
class QCloseEvent;
class OutputView;
class QScreen;
class WidgetContainer;
class Countdown;
class SourcePanel;
class Multimedia;
class QVariantAnimation;
class QGraphicsPixmapItem;
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow();
    ~MainWindow() override;
    CanvasView *view;
    QGraphicsScene *canvas;
    QGraphicsScene *outputScene;
    OutputView *outputPreview;
    OutputView *externalOutput;
    CanvasItem *addItem(CanvasItem *item, QPointF pos);
    bool writeProject(const QString &path);
    bool readProject(const QString &path);
    void removeSelection();
    void launchComposition();
    bool showExternalOutput(QScreen *screen);
    int fadeDuration() const { return fadeDurationMs; }
    void setFadeDuration(int milliseconds);
protected:
    void closeEvent(QCloseEvent *) override;
private:
    void refresh();
    void changed();
    void editText(CanvasItem *item);
    void updateScreens();
    void showOutputMenu(QPoint pos);
    Countdown *createCountdown(const QString &name, int seconds = 60, const QString &textId = {}, bool updateText = true);
    Multimedia *createMultimedia(const QString &name, const QString &file = {}, const QString &id = {});
    CanvasItem *prepareVideo(Multimedia *media, const QImage &frame);
    QImage captureOutput() const;
    void startOutputFade(const QImage &previous);
    void cancelOutputFade();
    void showFadeOptions();
    QVariantAnimation *fadeAnimation;
    QGraphicsPixmapItem *fadeOverlay = nullptr;
    int fadeDurationMs = 1000;
    bool pendingMediaFade = false;
    QPointer<Multimedia> activeMedia;
    QPointer<Multimedia> previewMedia;
    bool save(bool saveAs = false);
    bool canDiscard();
    QString filename;
    bool dirty = false;
    QPointer<QScreen> outputScreen;
    WidgetContainer *bottom;
    SourcePanel *sources;
    QList<WidgetContainer *> countdownContainers;
};
