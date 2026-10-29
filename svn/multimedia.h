#pragma once
#include <QWidget>
#include <QTimer>
#include <memory>
#include <QImage>
#include <QUuid>
#include <QElapsedTimer>
class QLabel;
class QPushButton;
class QSlider;
struct MediaPlayback;
struct VideoFrames;

class Multimedia : public QWidget {
    Q_OBJECT
public:
    explicit Multimedia(QWidget *parent = nullptr);
    ~Multimedia() override;
    QString name() const { return mediaName; }
    QString filePath() const { return path; }
    QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QImage currentFrame() const { return lastFrame; }
    bool hasCurrentFrame() const { return !awaitingFreshFrame && !lastFrame.isNull(); }
    void configure(const QString &name, const QString &file);
    qint64 position() const;
    qint64 duration() const;
    bool isPlaying() const;
    bool isPaused() const;
    bool hasVideo() const;
    bool isStillImage() const { return imageMode; }
    static bool isImageFile(const QString &file);
    static bool backendAvailable();
public slots:
    void play();
    void cue();
    void pause();
    void stop();
    void seek(qint64 milliseconds);
    void showProperties();
signals:
    void configurationChanged();
    void frameReady(const QImage &image);
    void playRequested();
    void cueRequested();
    void playbackFinished();
    void playbackStopped();
    void contextMenuRequested(Multimedia *media, QPoint globalPosition);
protected:
    void contextMenuEvent(QContextMenuEvent *event) override;
private:
    void stopPlayback(bool clearOutput);
    bool startPlayback();
    void receiveFrame();
    void updatePlayback();
    void updateTime(qint64 position, qint64 duration);
    QString mediaName = "Multimedia";
    QString path;
    QLabel *title;
    QLabel *status;
    QLabel *time;
    QPushButton *playButton;
    QPushButton *cueButton;
    QPushButton *pauseButton;
    QPushButton *stopButton;
    QSlider *slider;
    QTimer poll;
    std::unique_ptr<MediaPlayback> playback;
    std::unique_ptr<VideoFrames> frames;
    QImage lastFrame;
    QImage firstFrame;
    bool cueMode = false;
    bool imageMode = false;
    bool imageActive = false;
    bool finished = false;
    bool acceptFrames = false;
    bool awaitingFreshFrame = true;
    qint64 finishedTime = 0;
    mutable qint64 knownDuration = 0;
    qint64 shownPosition = 0;
    qint64 seekTarget = 0;
    bool seeking = false;
    QElapsedTimer seekWait;
    quint64 frameGeneration = 0;
};
