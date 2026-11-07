#pragma once
#include <QWidget>
#include <QTimer>
#include <memory>
#include <functional>
#include <QImage>
#include <QUuid>
#include <QElapsedTimer>
class QLabel;
class QPushButton;
class QSlider;
class QProgressBar;
struct MediaPlayback;
struct VideoFrames;
struct ThumbnailProbe;

struct AudioOutputDevice {
    QString id;
    QString name;
};

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
    qint64 displayPosition() const;
    qint64 duration() const;
    bool isPlaying() const;
    bool isPaused() const;
    bool hasVideo() const;
    bool isAudioMuted() const;
    bool isStillImage() const { return imageMode; }
    bool isAudioOnly() const;
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
    static void warmUpBackend(const std::function<void(int,const QString &)> &progress = {});
    static QList<AudioOutputDevice> audioDevices();
    static bool volumeControlAvailable();
    QString currentAudioDevice() const;
    int currentAudioVolume() const;
    void setAudioDevices(const QString &composition, const QString &output);
    void setAudioVolumes(int composition, int output);
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
    void applyAudioDevice(const QString &deviceId);
    void applyAudioVolume(int volume);
    void restartForLoop();
    void receiveFrame();
    void updatePlayback();
    void updateTimeline();
    void updateTime(qint64 position, qint64 duration);
    void updateThumbnail(const QImage &image);
    void setLoading(bool loading);
    void setTransportActive(QPushButton *active);
    QString mediaName = "Multimedia";
    QString path;
    QString compositionAudioDevice;
    QString outputAudioDevice;
    QString appliedAudioDevice;
    bool audioDeviceApplied = false;
    int compositionVolume = 100;
    int outputVolume = 100;
    int appliedVolume = -1;
    bool audioVolumeApplied = false;
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
    QProgressBar *loadingBar;
    QTimer poll;
    QTimer timelineTimer;
    std::unique_ptr<MediaPlayback> playback;
    std::unique_ptr<VideoFrames> frames;
    std::unique_ptr<ThumbnailProbe> thumbnailProbe;
    QImage thumbnailImage;
    QImage lastFrame;
    QImage firstFrame;
    bool cueMode = false;
    bool preparingFile = false;
    int pendingTransport = 0; // 1: Play, 2: Cue, once preparation finishes.
    bool looping = false;
    bool imageMode = false;
    bool imageActive = false;
    bool finished = false;
    bool acceptFrames = false;
    bool awaitingFreshFrame = true;
    qint64 finishedTime = 0;
    mutable qint64 knownDuration = 0;
    qint64 shownPosition = 0;
    mutable QElapsedTimer displayTick;
    mutable qint64 displaySample = 0;
    mutable qreal displayTime = 0;
    mutable qreal displayCorrection = 0;
    qint64 seekTarget = 0;
    bool seeking = false;
    QElapsedTimer seekWait;
    quint64 frameGeneration = 0;
    quint64 preparationGeneration = 0;
};
