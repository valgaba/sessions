#include "vumeter.h"
#include "levelmeter.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>

VuMeter::VuMeter(QWidget *parent) : QWidget(parent) {
    setObjectName("VuMeter");
    setAccessibleName(tr("Medidor de nivel estéreo"));
    setFixedWidth(96);
    setFixedHeight(34);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(3, 2, 3, 2);
    layout->setSpacing(2);
    const auto addChannel = [this, layout](const QString &name) {
        auto *row = new QHBoxLayout;
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(4);
        auto *label = new QLabel(name, this);
        label->setFixedSize(16, 12);
        label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        auto *bar = new LevelMeter(this);
        bar->setFixedHeight(9);
        bar->setAccessibleName(name == "L" ? tr("Canal izquierdo") : tr("Canal derecho"));
        row->addWidget(label);
        row->addWidget(bar, 1);
        layout->addLayout(row);
        return bar;
    };
    m_left = addChannel("L");
    m_right = addChannel("R");
    m_idleTimer = new QTimer(this);
    m_idleTimer->setSingleShot(true);
    m_idleTimer->setInterval(250);
    connect(m_idleTimer, &QTimer::timeout, this, &VuMeter::reset);
}

QSize VuMeter::sizeHint() const { return QSize(96, 34); }

void VuMeter::setLevels(qreal left, qreal right) {
    m_left->levelChanged(std::clamp<qreal>(left, 0, 1));
    m_right->levelChanged(std::clamp<qreal>(right, 0, 1));
    m_idleTimer->start();
}

void VuMeter::reset() {
    m_idleTimer->stop();
    if (m_left) m_left->levelChanged(0);
    if (m_right) m_right->levelChanged(0);
}
