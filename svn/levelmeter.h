#pragma once
#include <QColor>
#include <QElapsedTimer>
#include <QFrame>

class QTimer;

// Horizontal level bar adapted from Radit's LevelMeter (GPL v3).
class LevelMeter : public QFrame {
    Q_OBJECT
    Q_PROPERTY(QColor barColor READ barColor WRITE setBarColor)
    Q_PROPERTY(QColor peakColor READ peakColor WRITE setPeakColor)
public:
    explicit LevelMeter(QWidget *parent = nullptr);
    QColor barColor() const;
    void setBarColor(const QColor &color);
    QColor peakColor() const;
    void setPeakColor(const QColor &color);
public slots:
    void levelChanged(qreal level);
protected:
    void paintEvent(QPaintEvent *event) override;
private:
    void animateLevel();
    qreal m_level = 0.0;
    qreal m_targetLevel = 0.0;
    qreal m_peakLevel = 0.0;
    QTimer *m_animationTimer = nullptr;
    QElapsedTimer m_animationClock;
    QElapsedTimer m_peakClock;
    QColor m_barColor;
    QColor m_peakColor;
};
