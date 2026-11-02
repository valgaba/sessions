#include "levelmeter.h"

#include <QPainter>
#include <QTimer>
#include <algorithm>
#include <cmath>

LevelMeter::LevelMeter(QWidget *parent) : QFrame(parent),
    m_barColor(palette().color(QPalette::Highlight)),
    m_peakColor(palette().color(QPalette::WindowText)) {
    setObjectName("LevelMeter");
    setMinimumSize(10, 5);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_animationTimer = new QTimer(this);
    m_animationTimer->setTimerType(Qt::PreciseTimer);
    m_animationTimer->setInterval(16);
    connect(m_animationTimer, &QTimer::timeout, this, &LevelMeter::animateLevel);
}

QColor LevelMeter::barColor() const { return m_barColor; }

void LevelMeter::setBarColor(const QColor &color) {
    if (color.isValid() && m_barColor != color) { m_barColor = color; update(); }
}

void LevelMeter::levelChanged(qreal level) {
    m_targetLevel = std::isfinite(level) ? std::clamp(level, qreal(0), qreal(1)) : 0;
    if (m_targetLevel > 0 && m_targetLevel >= m_peakLevel) {
        m_peakLevel = m_targetLevel;
        m_peakClock.start();
        update();
    }
    if (!m_animationTimer->isActive() && (m_level != m_targetLevel || m_peakLevel > m_level)) {
        m_animationClock.start();
        m_animationTimer->start();
    }
}

QColor LevelMeter::peakColor() const { return m_peakColor; }

void LevelMeter::setPeakColor(const QColor &color) {
    if (color.isValid() && m_peakColor != color) { m_peakColor = color; update(); }
}

void LevelMeter::animateLevel() {
    const qreal seconds = std::clamp(m_animationClock.restart() / 1000.0, 0.001, 0.1);
    const qreal response = m_targetLevel > m_level ? 0.022 : 0.160;
    m_level += (m_targetLevel - m_level) * (1.0 - std::exp(-seconds / response));
    if (std::abs(m_targetLevel - m_level) < 0.001) m_level = m_targetLevel;
    if (m_peakClock.isValid() && m_peakClock.elapsed() >= 500)
        m_peakLevel = std::max(m_level, m_peakLevel - seconds * 0.35);
    if (m_level == m_targetLevel && m_peakLevel - m_level < 0.001) {
        m_peakLevel = m_level;
        m_animationTimer->stop();
    }
    update();
}

void LevelMeter::paintEvent(QPaintEvent *event) {
    QFrame::paintEvent(event);
    const QRect area = contentsRect();
    if (area.isEmpty()) return;
    QPainter painter(this);
    QRect bar = area;
    bar.setWidth(qRound(bar.width() * m_level));
    if (!bar.isEmpty()) painter.fillRect(bar, m_barColor);
    if (m_peakLevel > 0) {
        const int markerWidth = std::min(2, area.width());
        const int x = area.left() + qRound((area.width() - markerWidth) * m_peakLevel);
        painter.fillRect(QRect(x, area.top(), markerWidth, area.height()), m_peakColor);
    }
}
