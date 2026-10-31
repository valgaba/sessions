#pragma once
#include <QMainWindow>
#include <QPointer>
#include <memory>
#include <QHash>
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
    Countdown *createCountdown(const QString &name, int seconds = 60, const QString &textId = {}, bool updateText = true, const QString &format = "MM:SS.C", const QString &totalTextId = {});
    Multimedia *createMultimedia(const QString &name, const QString &file = {}, const QString &id = {}, bool loop = false);
    CanvasItem *prepareVideo(Multimedia *media, const QImage &frame);
    QImage captureOutput() const;
    void startOutputFade(const QImage &previous);
    void cancelOutputFade();
    void showFadeOptions();
    QString suggestedElementName(const QString &base) const;
    void addClipboardActions(QMenu *menu, const QList<CanvasItem *> &items, const QPointF &position = {}, bool usePosition = false);
    void copyItems(const QList<CanvasItem *> &items, bool cut);
    void pasteItems(const QPointF &position = {}, bool usePosition = false);
    QList<CanvasItem *> selectedCanvasItems() const;
    QList<std::shared_ptr<CanvasItem>> itemClipboard;
    QHash<QString,QList<QPointer<Countdown>>> cutTextAssociations;
    QHash<QString,QList<QPointer<Countdown>>> cutTotalAssociations;
    bool clipboardIsCut = false;
    int pasteCount = 0;
    QVariantAnimation *fadeAnimation;
    QGraphicsPixmapItem *fadeOverlay = nullptr;
    int fadeDurationMs = 1000;
    bool pendingMediaFade = false;
    QPointer<Multimedia> activeMedia;
    QPointer<Multimedia> previewMedia;
    QList<QPointer<Multimedia>> outputMedia;
    void launchMedia(Multimedia *media, bool mix);
    void balanceMediaLayers();
    bool save(bool saveAs = false);
    bool canDiscard();
    QString filename;
    bool dirty = false;
    QPointer<QScreen> outputScreen;
    WidgetContainer *bottom;
    SourcePanel *sources;
    QList<WidgetContainer *> countdownContainers;
};
