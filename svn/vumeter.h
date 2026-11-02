#pragma once
#include <QWidget>

class LevelMeter;
class QTimer;

// Compact stereo peak meter adapted from Radit (GPL v3). Levels are linear
// peaks (0 empty, 1 full) read from the selected Windows output endpoint.
class VuMeter : public QWidget {
    Q_OBJECT
public:
    explicit VuMeter(QWidget *parent = nullptr);
    QSize sizeHint() const override;
public slots:
    void setLevels(qreal left, qreal right);
    void reset();
private:
    LevelMeter *m_left = nullptr;
    LevelMeter *m_right = nullptr;
    QTimer *m_idleTimer = nullptr;
};
