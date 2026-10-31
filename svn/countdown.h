#pragma once
#include <QWidget>
#include <QPointer>
#include <QTimer>
#include <QElapsedTimer>
#include <QStringList>
#include "canvas.h"

class QLabel;
class QPushButton;

class Countdown : public QWidget {
    Q_OBJECT
public:
    enum class State { Stopped, Playing, Paused };
    Q_ENUM(State)
    explicit Countdown(QWidget *parent = nullptr);
    ~Countdown() override;
    QString name() const;
    void setName(const QString &name);
    State state() const { return currentState; }
    void setCompositionScene(QGraphicsScene *scene);
    void configure(int seconds, CanvasItem *text, bool updateText = true);
    int durationSeconds() const { return duration; }
    int remainingSeconds() const;
    QString outputFormat() const { return format; }
    static QStringList outputFormats();
    bool setOutputFormat(const QString &value, bool updateText = true);
    CanvasItem *associatedText() const { return target.data(); }
    CanvasItem *totalText() const { return totalTarget.data(); }
    void setTotalText(CanvasItem *text, bool updateText = true);
public slots:
    void start();
    void pause();
    void stop();
    void showProperties();
    void showTime();
signals:
    void stateChanged(Countdown::State state);
    void configurationChanged();
    void textUpdated(CanvasItem *text);
    void contextMenuRequested(Countdown *countdown, QPoint globalPos);
protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
private:
    void setState(State state);
    void tick();
    void publish(bool updateText = true);
    QString formattedTime(qint64 milliseconds) const;
    bool validTarget() const;
    void updateTitle();
    QString countdownName = "Countdown";
    QString format = "MM:SS.C";
    State currentState = State::Stopped;
    QLabel *status;
    QLabel *title;
    QPushButton *startButton;
    QPushButton *pauseButton;
    QPushButton *stopButton;
    QPointer<QGraphicsScene> composition;
    QPointer<CanvasItem> target;
    QPointer<CanvasItem> totalTarget;
    QMetaObject::Connection targetDestroyed;
    QTimer timer;
    QElapsedTimer elapsed;
    int duration = 60;
    qint64 remainingMs = 60000;
    qint64 segmentMs = 60000;
    int lastPublished = -1;
    bool lastHidden = false;
    bool lastOverdue = false;
};
