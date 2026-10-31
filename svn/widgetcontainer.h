#pragma once
#include <QScrollArea>
#include <QPointer>
#include <QTimer>
class Countdown;
class Multimedia;
class QFrame;
class QVBoxLayout;

// Scrollable widget host using Radit's Container/Contents style selectors.
class WidgetContainer : public QScrollArea {
    Q_OBJECT
public:
    explicit WidgetContainer(QWidget *parent = nullptr);
    void addWidget(QWidget *widget);
    void removeWidget(QWidget *widget);
    int widgetCount() const;
    void clearWidgets();
    void setCountdownCreationEnabled(bool enabled);
    bool countdownCreationEnabled() const { return allowCountdown; }
    QList<QWidget *> widgetsInOrder() const;
    Countdown *selectedCountdown() const;
    QWidget *selectedAction() const { return selected.data(); }
    void selectAction(QWidget *action, bool updateText = false);
    void selectCountdown(Countdown *countdown, bool updateText = false);
    void moveCountdown(Countdown *countdown, int insertionIndex);
    void moveWidget(QWidget *widget, int insertionIndex);
    bool copyCountdown(bool cut = false);
    void pasteCountdown();
    void deleteCountdown();
    bool copyAction(bool cut = false);
    void pasteAction();
    void deleteAction();
signals:
    void selectionChanged();
    void addCountdownRequested();
    void addMultimediaRequested();
    void mediaFilesDropped(QStringList files, int insertionIndex);
    void pasteCountdownRequested(QString name, int seconds, QString textId, int insertionIndex, QString format, QString totalTextId);
    void pasteMultimediaRequested(QString name, QString file, bool loop, int insertionIndex, bool purge, bool next, bool mix);
    void contentsChanged();
protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
private:
    void showAddMenu(QPoint globalPos);
    void showActionsMenu(QWidget *action, QPoint globalPos);
    int insertionIndexAt(int y) const;
    void showDropLine(int index);
    QVBoxLayout *contentsLayout;
    bool allowCountdown = true;
    QPointer<QWidget> selected;
    QPointer<QWidget> dragged;
    QPointer<QWidget> pressed;
    QPoint dragStart;
    QFrame *dropLine;
    QTimer dragScroll;
};
