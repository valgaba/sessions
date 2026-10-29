#include "countdown.h"
#include <QContextMenuEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QVBoxLayout>
#include <QSpinBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGraphicsScene>
#include <QPointer>
#include <algorithm>
#include <QEvent>
#include <QIcon>

Countdown::Countdown(QWidget *parent) : QWidget(parent) {
    setObjectName("Countdown");
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_StyledBackground);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(6);
    auto *header = new QHBoxLayout;
    auto *controls = new QHBoxLayout;
    title = new QLabel("Countdown");
    title->setTextFormat(Qt::PlainText);
    title->setObjectName("CountdownTitle");
    title->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    title->setMinimumWidth(65);
    title->installEventFilter(this);
    header->addWidget(title, 1);
    status = new QLabel("Detenido");
    status->setObjectName("CountdownStatus");
    header->addWidget(status);
    layout->addLayout(header);
    startButton = new QPushButton(QIcon(":/icons/Play.svg"),"Start");
    startButton->setObjectName("countdownStart");
    startButton->setCheckable(true);
    pauseButton = new QPushButton(QIcon(":/icons/Pause.svg"),"Pause");
    pauseButton->setObjectName("countdownPause");
    pauseButton->setCheckable(true);
    pauseButton->setEnabled(false);
    stopButton = new QPushButton(QIcon(":/icons/Stop.svg"),"Stop");
    stopButton->setObjectName("countdownStop");
    for (auto *button : {startButton, pauseButton, stopButton}) {
        button->setMinimumWidth(64);
        controls->addWidget(button);
    }
    controls->addStretch();
    layout->addLayout(controls);
    connect(startButton, &QPushButton::clicked, this, &Countdown::start);
    connect(pauseButton, &QPushButton::clicked, this, &Countdown::pause);
    connect(stopButton, &QPushButton::clicked, this, &Countdown::stop);
    setToolTip("Botón derecho: propiedades de Countdown");
    timer.setInterval(10);
    timer.setTimerType(Qt::PreciseTimer);
    connect(&timer, &QTimer::timeout, this, &Countdown::tick);
}
QString Countdown::name() const { return countdownName; }
Countdown::~Countdown() {
    if (validTarget() && target->timerOwner() == this) {
        target->setTimerOwner(nullptr); target->setTimerAppearance(false, false); emit textUpdated(target);
    }
}
void Countdown::setName(const QString &value) {
    countdownName = value.trimmed(); title->setToolTip(countdownName); updateTitle();
}
void Countdown::updateTitle() {
    title->setText(title->fontMetrics().elidedText(countdownName, Qt::ElideRight, title->contentsRect().width()));
}
bool Countdown::eventFilter(QObject *watched, QEvent *event) {
    if (watched == title && event->type() == QEvent::Resize) updateTitle();
    return QWidget::eventFilter(watched, event);
}
void Countdown::setCompositionScene(QGraphicsScene *scene) { composition = scene; }
bool Countdown::validTarget() const { return target && composition && target->scene() == composition && target->isText(); }
void Countdown::configure(int seconds, CanvasItem *text, bool updateText) {
    timer.stop();
    if (validTarget() && target != text && target->timerOwner() == this) {
        target->setTimerOwner(nullptr); target->setTimerAppearance(false, false); emit textUpdated(target);
    }
    disconnect(targetDestroyed);
    duration = std::clamp(seconds, 1, 86400);
    remainingMs = qint64(duration) * 1000;
    lastPublished = -1;
    target = text && text->isText() && composition && text->scene() == composition ? text : nullptr;
    setState(State::Stopped);
    if (target) targetDestroyed = connect(target, &QObject::destroyed, this, [this] {
        timer.stop(); target = nullptr; setState(State::Stopped);
        status->setText("Texto eliminado");
        emit configurationChanged();
    });
    publish(updateText);
    emit configurationChanged();
}
int Countdown::remainingSeconds() const { return int((qAbs(remainingMs) + 999) / 1000); }
void Countdown::publish(bool updateText) {
    const bool overdue = remainingMs <= 0;
    const bool hidden = currentState == State::Paused && (elapsed.elapsed() / 500) % 2;
    const int tenths = int(remainingMs > 0 ? (remainingMs + 99) / 100 : -remainingMs / 100);
    const int seconds = tenths / 10;
    const QString formatted = QString("%1:%2:%3.%4")
        .arg(seconds / 3600, 2, 10, QLatin1Char('0'))
        .arg(seconds / 60 % 60, 2, 10, QLatin1Char('0'))
        .arg(seconds % 60, 2, 10, QLatin1Char('0'))
        .arg(tenths % 10);
    status->setText(QString("%1 · %2").arg(currentState == State::Playing ? "En marcha" : currentState == State::Paused ? "En pausa" : "Detenido", formatted));
    if (!updateText || !validTarget()) return;
    if (lastPublished == -1) target->setTimerOwner(this);
    if (target->timerOwner() != this || (tenths == lastPublished && hidden == lastHidden && overdue == lastOverdue)) return;
    lastPublished = tenths;
    lastHidden = hidden; lastOverdue = overdue;
    target->setText(formatted);
    target->setTimerAppearance(overdue, hidden);
    emit textUpdated(target);
}
void Countdown::showTime() {
    lastPublished = -1;
    if (currentState == State::Playing) tick(); else publish();
}
void Countdown::start() {
    if (!validTarget()) {
        startButton->setChecked(false);
        status->setText("Selecciona un texto en Propiedades");
        return;
    }
    if (currentState == State::Playing) return;
    if (currentState == State::Stopped) remainingMs = qint64(duration) * 1000;
    segmentMs = remainingMs;
    lastPublished = -1;
    elapsed.start();
    setState(State::Playing);
    publish();
    timer.start();
}
void Countdown::tick() {
    if (!validTarget()) { timer.stop(); setState(State::Stopped); return; }
    if (currentState == State::Playing) remainingMs = segmentMs - elapsed.elapsed();
    publish();
}
void Countdown::pause() {
    if (currentState != State::Playing) return;
    tick();
    if (currentState == State::Playing) { elapsed.restart(); setState(State::Paused); publish(); }
}
void Countdown::stop() {
    timer.stop();
    remainingMs = qint64(duration) * 1000;
    lastPublished = -1;
    setState(State::Stopped);
    publish();
}
void Countdown::setState(State value) {
    if (value == currentState) return;
    currentState = value;
    startButton->setChecked(value == State::Playing);
    startButton->setEnabled(value != State::Playing);
    pauseButton->setChecked(value == State::Paused);
    pauseButton->setEnabled(value == State::Playing);
    status->setText(value == State::Playing ? "En marcha" : value == State::Paused ? "En pausa" : "Detenido");
    emit stateChanged(value);
}
void Countdown::contextMenuEvent(QContextMenuEvent *event) {
    emit contextMenuRequested(this, event->globalPos());
    event->accept();
}
void Countdown::showProperties() {
    QDialog dialog(this);
    dialog.setObjectName("CountdownProperties");
    dialog.setWindowTitle("Propiedades de " + name());
    auto *layout = new QVBoxLayout(&dialog);
    auto *form = new QFormLayout;
    auto *seconds = new QSpinBox;
    seconds->setObjectName("countdownSeconds");
    seconds->setRange(1, 86400);
    seconds->setSuffix(" s"); seconds->setValue(duration);
    form->addRow("Tiempo (segundos):", seconds);
    auto *texts = new QComboBox;
    texts->setObjectName("countdownText");
    texts->addItem("Selecciona un texto…");
    QList<QPointer<CanvasItem>> choices;
    choices.append(QPointer<CanvasItem>());
    if (composition) for (auto *base : composition->items(Qt::AscendingOrder)) {
        auto *item = dynamic_cast<CanvasItem *>(base);
        if (!item || !item->isText()) continue;
        choices.append(item);
        texts->addItem(item->name);
        texts->setItemData(texts->count() - 1, item->text, Qt::ToolTipRole);
        if (item == target) texts->setCurrentIndex(texts->count() - 1);
    }
    form->addRow("Texto asociado:", texts);
    layout->addLayout(form);
    auto *buttons = new QDialogButtonBox;
    auto *accept = buttons->addButton("Aceptar", QDialogButtonBox::AcceptRole);
    buttons->addButton("Cancelar", QDialogButtonBox::RejectRole);
    accept->setEnabled(texts->currentIndex() > 0);
    connect(texts, qOverload<int>(&QComboBox::currentIndexChanged), accept, [accept](int index) { accept->setEnabled(index > 0); });
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    if (dialog.exec() == QDialog::Accepted) {
        auto *item = choices.value(texts->currentIndex()).data();
        if (item && composition && item->scene() == composition) configure(seconds->value(), item);
    }
}
