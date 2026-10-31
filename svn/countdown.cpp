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
#include <QLineEdit>

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
    if(totalTarget && totalTarget->timerOwner()==this)totalTarget->setTimerOwner(nullptr);
    if (validTarget() && target->timerOwner() == this) {
        target->setTimerOwner(nullptr); target->setTimerAppearance(false, false); emit textUpdated(target);
    }
}
void Countdown::setName(const QString &value) {
    const QString next=value.trimmed();if(next.isEmpty() || next==countdownName)return;
    countdownName = next; title->setToolTip(countdownName); updateTitle();emit configurationChanged();
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
    if(totalTarget==target)totalTarget.clear();
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
QStringList Countdown::outputFormats() { return {"MM:SS.C","HH:MM:SS.C","MM:SS","HH:MM:SS","SS.C","SS"}; }
bool Countdown::setOutputFormat(const QString &value, bool updateText) {
    const QString next=value.toUpper();
    if(!outputFormats().contains(next))return false;
    if(format==next)return true;
    format=next;lastPublished=-1;publish(updateText);emit configurationChanged();return true;
}
QString Countdown::formattedTime(qint64 milliseconds) const {
    const int tenths = int(milliseconds > 0 ? (milliseconds + 99) / 100 : -milliseconds / 100);
    const int seconds = tenths / 10;
    QString formatted=format;
    const bool hours=format.contains("HH"),minutes=format.contains("MM");
    formatted.replace("HH",QString::number(seconds/3600).rightJustified(2,'0'));
    formatted.replace("MM",QString::number(hours?seconds/60%60:seconds/60).rightJustified(2,'0'));
    formatted.replace("SS",QString::number(hours||minutes?seconds%60:seconds).rightJustified(2,'0'));
    formatted.replace("C",QString::number(tenths%10));
    return formatted;
}
void Countdown::setTotalText(CanvasItem *text, bool updateText) {
    if(totalTarget && totalTarget->timerOwner()==this)totalTarget->setTimerOwner(nullptr);
    totalTarget=text && text->isText() && composition && text->scene()==composition && text!=target ? text : nullptr;
    if(totalTarget && updateText)totalTarget->setTimerOwner(this);
    publish(updateText);emit configurationChanged();
}
void Countdown::publish(bool updateText) {
    const bool overdue = remainingMs <= 0;
    const bool hidden = currentState == State::Paused && (elapsed.elapsed() / 500) % 2;
    const int tenths = int(remainingMs > 0 ? (remainingMs + 99) / 100 : -remainingMs / 100);
    const QString formatted=formattedTime(remainingMs);
    if(updateText && totalTarget && composition && totalTarget->scene()==composition && totalTarget!=target) {
        if(lastPublished==-1)totalTarget->setTimerOwner(this);
        const QString total=formattedTime(qint64(duration)*1000);
        if(totalTarget->timerOwner()==this && (totalTarget->text!=total || totalTarget->timerHidden() || totalTarget->timerOverdue())) {
            totalTarget->setText(total);totalTarget->setTimerAppearance(false,false);emit textUpdated(totalTarget);
        }
    }
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
    auto *nameEdit=new QLineEdit(name());nameEdit->setObjectName("countdownName");nameEdit->selectAll();
    form->addRow("Nombre:",nameEdit);
    auto *seconds = new QSpinBox;
    seconds->setObjectName("countdownSeconds");
    seconds->setRange(1, 86400);
    seconds->setSuffix(" s"); seconds->setValue(duration);
    form->addRow("Tiempo (segundos):", seconds);
    auto *formats=new QComboBox;formats->setObjectName("countdownFormat");formats->addItems(outputFormats());formats->setCurrentText(format);
    form->addRow("Formato de salida:",formats);
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
    auto *totals=new QComboBox;totals->setObjectName("countdownTotalText");
    for(int i=0;i<texts->count();++i)totals->addItem(i==0?"Sin texto de tiempo total":texts->itemText(i));
    for(int i=1;i<choices.size();++i)if(choices[i]==totalTarget)totals->setCurrentIndex(i);
    form->addRow("Texto de tiempo total:",totals);
    layout->addLayout(form);
    auto *buttons = new QDialogButtonBox;
    auto *accept = buttons->addButton("Aceptar", QDialogButtonBox::AcceptRole);
    buttons->addButton("Cancelar", QDialogButtonBox::RejectRole);
    auto validate=[=]{accept->setEnabled(!nameEdit->text().trimmed().isEmpty() && (totals->currentIndex()==0 || totals->currentIndex()!=texts->currentIndex()));};
    connect(nameEdit,&QLineEdit::textChanged,&dialog,validate);
    connect(texts,&QComboBox::currentIndexChanged,&dialog,validate);
    connect(totals,&QComboBox::currentIndexChanged,&dialog,validate);validate();
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    if (dialog.exec() == QDialog::Accepted && !nameEdit->text().trimmed().isEmpty()) {
        setName(nameEdit->text());
        auto *item = choices.value(texts->currentIndex()).data();
        if(item && (!composition || item->scene()!=composition))item=nullptr;
        if(seconds->value()!=duration || item!=target.data()){setOutputFormat(formats->currentText(),false);configure(seconds->value(),item);}
        else setOutputFormat(formats->currentText());
        setTotalText(choices.value(totals->currentIndex()).data());
    }
}
