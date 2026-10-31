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
struct ThumbnailProbe;

class Multimedia : public QWidget {
    Q_OBJECT
public:
    explicit Multimedia(QWidget *parent = nullptr);
    ~Multimedia() override;
    QString name() const { return mediaName; }
    QString filePath() const { return path; }
    QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QImage currentFrame() const { return lastFrame; }
    QImage thumbnail() const { return thumbnailImage; }
    bool hasCurrentFrame() const { return !awaitingFreshFrame && !lastFrame.isNull(); }
    void configure(const QString &name, const QString &file);
    qint64 position() const;
    qint64 duration() const;
    bool isPlaying() const;
    bool isPaused() const;
    bool hasVideo() const;
    bool isAudioMuted() const;
    bool isStillImage() const { return imageMode; }
    bool loops() const { return looping; }
    bool purges() const;
    bool playsNext() const;
    bool mixes() const;
    bool isCue() const { return cueMode; }
    bool isFinished() const { return finished; }
    void setPurge(bool enabled);
    void setNext(bool enabled);
    void setMix(bool enabled);
    void setLoop(bool enabled);
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
    void restartForLoop();
    void receiveFrame();
    void updatePlayback();
    void updateTime(qint64 position, qint64 duration);
    void updateThumbnail(const QImage &image);
    QString mediaName = "Multimedia";
    QString path;
    QLabel *title;
    QLabel *status;
    QLabel *time;
    QPushButton *playButton;
    QPushButton *cueButton;
    QPushButton *pauseButton;
    QPushButton *stopButton;
    QPushButton *loopButton;
    QPushButton *purgeButton;
    QPushButton *nextButton;
    QPushButton *mixButton;
    QSlider *slider;
    QTimer poll;
    std::unique_ptr<MediaPlayback> playback;
    std::unique_ptr<VideoFrames> frames;
    std::unique_ptr<ThumbnailProbe> thumbnailProbe;
    QImage thumbnailImage;
    QImage lastFrame;
    QImage firstFrame;
    bool cueMode = false;
    bool looping = false;
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
