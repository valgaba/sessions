#pragma once
#include <QObject>
#include <QString>
#include <QTimer>
#include <memory>

// Peak meter for a Windows render endpoint. LibVLC 3 does not expose decoded
// levels, so the meter reads the real mix of the selected output device through
// WASAPI, the same measurement a hardware VU would show.
class AudioMeter : public QObject {
    Q_OBJECT
public:
    explicit AudioMeter(QObject *parent = nullptr);
    ~AudioMeter() override;
    void setDeviceId(const QString &deviceId);
    QString deviceId() const { return currentDeviceId; }
    void setActive(bool active);
    bool isActive() const { return active; }
    bool isMetering() const;
signals:
    void levelsChanged(qreal left, qreal right);
private:
    void open();
    void close();
    void poll();
    QTimer timer;
    QString currentDeviceId;
    bool active = false;
    struct Private;
    std::unique_ptr<Private> d;
};
