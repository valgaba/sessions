#include "widgetcontainer.h"
#include <QFrame>
#include <QVBoxLayout>
#include <QMenu>
#include "countdown.h"
#include "multimedia.h"
#include <QtWidgets>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeData>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDropEvent>

namespace {
constexpr auto moveMime = "application/x-sessions-action-move";
QStringList mediaFiles(const QMimeData *mime) {
    QStringList files;
    const QSet<QString> formats{"mp4","mov","m4v","mkv","avi","webm","wmv","mpg","mpeg","mts","m2ts","ts","mxf","ogv","3gp","mp3","wav","flac","ogg","opus","aac","m4a","wma","aif","aiff"};
    for(const auto &url:mime->urls()) {
        if(!url.isLocalFile())continue;
        QFileInfo file(url.toLocalFile());
        if(file.isFile() && (formats.contains(file.suffix().toLower()) || Multimedia::isImageFile(file.filePath())) && !files.contains(file.absoluteFilePath()))files.append(file.absoluteFilePath());
    }
    return files;
}
QJsonObject actionClipboard;
bool clipboardAction(QJsonObject *result = nullptr) {
    const auto &root = actionClipboard;
    if (root["version"].toInt() != 1 || !root["name"].isString() || root["name"].toString().trimmed().isEmpty())return false;
    if(root["type"]=="countdown") {
        if(!root["seconds"].isDouble() || root["seconds"].toDouble()!=root["seconds"].toInt() || root["seconds"].toInt()<1 || root["seconds"].toInt()>86400 || !root["textId"].isString())return false;
    } else if(root["type"]!="multimedia" || !root["file"].isString())return false;
    if (result) *result = root;
    return true;
}
}

WidgetContainer::WidgetContainer(QWidget *parent) : QScrollArea(parent) {
    setObjectName("Container");
    setWidgetResizable(true);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *contents = new QFrame;
    contents->setObjectName("Contents");
    contentsLayout = new QVBoxLayout(contents);
    contentsLayout->setContentsMargins(4, 4, 4, 4);
    contentsLayout->setSpacing(4);
    contentsLayout->addStretch();
    setWidget(contents);
    dropLine = new QFrame(contents);
    dropLine->setObjectName("CountdownDropLine"); dropLine->setStyleSheet("background: #80a4ae; border: none;");
    dropLine->setAttribute(Qt::WA_TransparentForMouseEvents); dropLine->hide();
    setFocusPolicy(Qt::StrongFocus);
    dragScroll.setInterval(35);
    connect(&dragScroll, &QTimer::timeout, this, [this] {
        if (!dragged) { dragScroll.stop(); return; }
        QPoint point = viewport()->mapFromGlobal(QCursor::pos());
        if (!viewport()->rect().contains(point)) return;
        int step = point.y() < 22 ? -10 : point.y() > viewport()->height()-22 ? 10 : 0;
        if (!step) return;
        verticalScrollBar()->setValue(verticalScrollBar()->value()+step);
        showDropLine(insertionIndexAt(widget()->mapFromGlobal(QCursor::pos()).y()));
    });
    for (QWidget *surface : {viewport(), static_cast<QWidget *>(contents)}) {
        surface->setAcceptDrops(true); surface->installEventFilter(this);
        surface->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(surface, &QWidget::customContextMenuRequested, this, [this, surface](QPoint pos) {
            showAddMenu(surface->mapToGlobal(pos));
        });
    }
    auto shortcut = [this](const QKeySequence &key, auto callback) {
        auto *binding = new QShortcut(key, this);
        binding->setContext(Qt::WidgetWithChildrenShortcut);
        connect(binding, &QShortcut::activated, this, callback);
    };
    shortcut(QKeySequence::Copy, [this] { copyAction(); });
    shortcut(QKeySequence::Cut, [this] { copyAction(true); });
    shortcut(QKeySequence::Paste, [this] { pasteAction(); });
    shortcut(QKeySequence::Delete, [this] { deleteAction(); });
    shortcut(QKeySequence("Alt+Up"), [this] { if (selected) moveWidget(selected,contentsLayout->indexOf(selected)-1); });
    shortcut(QKeySequence("Alt+Down"), [this] { if (selected) moveWidget(selected,contentsLayout->indexOf(selected)+2); });
}
void WidgetContainer::showAddMenu(QPoint globalPos) {
    showActionsMenu(nullptr, globalPos);
}
void WidgetContainer::showActionsMenu(QWidget *action, QPoint globalPos) {
    if (!allowCountdown) return;
    if (action) selectAction(action,true);
    if (selected) selected->setFocus(Qt::OtherFocusReason); else setFocus(Qt::OtherFocusReason);
    QMenu menu(this);
    auto *add = menu.addAction("Add Countdown"); add->setObjectName("addCountdown");
    auto *addMedia = menu.addAction("Add Multimedia"); addMedia->setObjectName("addMultimedia");
    menu.addSeparator();
    auto *cut = menu.addAction("Cortar\tCtrl+X"); cut->setObjectName("cutAction");
    auto *copy = menu.addAction("Copiar\tCtrl+C"); copy->setObjectName("copyAction");
    auto *paste = menu.addAction("Pegar\tCtrl+V"); paste->setObjectName("pasteAction");
    auto *remove = menu.addAction("Eliminar\tSupr"); remove->setObjectName("deleteAction");
    cut->setEnabled(selected); copy->setEnabled(selected); remove->setEnabled(selected);
    paste->setEnabled(clipboardAction());
    menu.addSeparator();
    auto *up = menu.addAction("Subir\tAlt+↑"); up->setObjectName("moveActionUp");
    auto *down = menu.addAction("Bajar\tAlt+↓"); down->setObjectName("moveActionDown");
    int index = selected ? contentsLayout->indexOf(selected) : -1;
    up->setEnabled(index > 0); down->setEnabled(index >= 0 && index < widgetCount()-1);
    menu.addSeparator();
    auto *properties = menu.addAction("Propiedades…"); properties->setObjectName("actionProperties"); properties->setEnabled(selected);
    add->setIcon(QIcon(":/icons/Add.svg"));addMedia->setIcon(QIcon(":/icons/audiofile.svg"));
    cut->setIcon(QIcon(":/icons/ActionCut.svg"));copy->setIcon(QIcon(":/icons/ActionCopy.svg"));paste->setIcon(QIcon(":/icons/ActionPaste.svg"));
    remove->setIcon(QIcon(":/icons/Remove.svg"));properties->setIcon(QIcon(":/icons/settings.svg"));
    up->setIcon(QIcon(QIcon(":/icons/MoveLeft.svg").pixmap(16,16).transformed(QTransform().rotate(90))));
    down->setIcon(QIcon(QIcon(":/icons/MoveRight.svg").pixmap(16,16).transformed(QTransform().rotate(90))));
    auto *chosen = menu.exec(globalPos);
    if (chosen == add) emit addCountdownRequested();
    else if (chosen == addMedia) emit addMultimediaRequested();
    else if (chosen == copy) copyAction();
    else if (chosen == cut) copyAction(true);
    else if (chosen == paste) pasteAction();
    else if (chosen == remove) deleteAction();
    else if (chosen == up && selected) moveWidget(selected,contentsLayout->indexOf(selected)-1);
    else if (chosen == down && selected) moveWidget(selected,contentsLayout->indexOf(selected)+2);
    else if (chosen == properties) {
        if(auto *timer=qobject_cast<Countdown *>(selected.data()))timer->showProperties();
        else if(auto *media=qobject_cast<Multimedia *>(selected.data()))media->showProperties();
    }
}
void WidgetContainer::setCountdownCreationEnabled(bool enabled) {
    allowCountdown = enabled;
    for (auto *surface : {viewport(), widget()})
        surface->setContextMenuPolicy(enabled ? Qt::CustomContextMenu : Qt::PreventContextMenu);
}
void WidgetContainer::clearWidgets() {
    selectCountdown(nullptr); pressed.clear(); dragScroll.stop(); dropLine->hide();
    while (contentsLayout->count() > 1) {
        auto *item = contentsLayout->takeAt(0);
        delete item->widget();
        delete item;
    }
}
void WidgetContainer::addWidget(QWidget *widget) {
    if (!widget || contentsLayout->indexOf(widget) >= 0) return;
    contentsLayout->insertWidget(contentsLayout->count() - 1, widget);
    widget->show();
    if(auto *media=qobject_cast<Multimedia *>(widget)) {
        connect(media,&Multimedia::contextMenuRequested,this,[this](Multimedia *item,QPoint position){showActionsMenu(item,position);});
        media->setAcceptDrops(allowCountdown);media->installEventFilter(this);
        media->setToolTip("Arrastra el nombre o el fondo para reubicar. Botón derecho: opciones.");
        for(auto *child:media->findChildren<QWidget *>()) {
            child->setAcceptDrops(allowCountdown);child->installEventFilter(this);
            if(qobject_cast<QLabel *>(child))child->setCursor(Qt::OpenHandCursor);
        }
    }
    if (auto *countdown = qobject_cast<Countdown *>(widget)) {
        countdown->setAcceptDrops(allowCountdown);
        countdown->installEventFilter(this);
        countdown->setToolTip("Arrastra el nombre o el fondo para reubicar. Botón derecho: opciones.");
        for (auto *child : countdown->findChildren<QWidget *>()) {
            child->setAcceptDrops(allowCountdown);
            child->installEventFilter(this);
            if (qobject_cast<QLabel *>(child)) child->setCursor(Qt::OpenHandCursor);
        }
        connect(countdown, &Countdown::contextMenuRequested, this, [this](Countdown *item,QPoint position){showActionsMenu(item,position);});
    }
}
void WidgetContainer::removeWidget(QWidget *widget) {
    if (!widget || contentsLayout->indexOf(widget) < 0) return;
    if (widget == selected) selectCountdown(nullptr);
    contentsLayout->removeWidget(widget);
    widget->hide();
    widget->setParent(nullptr);
}
int WidgetContainer::widgetCount() const { return contentsLayout->count() - 1; }
QList<QWidget *> WidgetContainer::widgetsInOrder() const {
    QList<QWidget *> result;
    for (int index=0; index<widgetCount(); ++index) result.append(contentsLayout->itemAt(index)->widget());
    return result;
}
Countdown *WidgetContainer::selectedCountdown() const { return qobject_cast<Countdown *>(selected.data()); }
void WidgetContainer::selectCountdown(Countdown *countdown, bool updateText) { selectAction(countdown,updateText); }
void WidgetContainer::selectAction(QWidget *countdown, bool updateText) {
    if (countdown && contentsLayout->indexOf(countdown) < 0) return;
    if (selected == countdown) { if (auto *timer=qobject_cast<Countdown *>(countdown);timer && updateText)timer->showTime(); return; }
    auto refresh = [](QWidget *item, bool active) {
        if (!item) return;
        item->setProperty("selected",active); item->style()->unpolish(item); item->style()->polish(item); item->update();
    };
    refresh(selected,false); selected = countdown; refresh(selected,true);
    emit selectionChanged();
    if (auto *timer=qobject_cast<Countdown *>(countdown);timer && updateText)timer->showTime();
}
void WidgetContainer::moveCountdown(Countdown *countdown, int insertionIndex) {
    moveWidget(countdown,insertionIndex);
}
void WidgetContainer::moveWidget(QWidget *countdown, int insertionIndex) {
    if (!allowCountdown || !countdown) return;
    int from = contentsLayout->indexOf(countdown);
    if (from < 0) return;
    insertionIndex = qBound(0,insertionIndex,widgetCount());
    if (from < insertionIndex) --insertionIndex;
    if (from == insertionIndex) return;
    delete contentsLayout->takeAt(from);
    contentsLayout->insertWidget(insertionIndex,countdown);
    selectAction(countdown);
    emit contentsChanged();
}
bool WidgetContainer::copyCountdown(bool cut) { return copyAction(cut); }
void WidgetContainer::pasteCountdown() { pasteAction(); }
void WidgetContainer::deleteCountdown() { deleteAction(); }
bool WidgetContainer::copyAction(bool cut) {
    if (!allowCountdown || !selected) return false;
    if(auto *timer=qobject_cast<Countdown *>(selected.data()))
        actionClipboard = QJsonObject{{"version",1},{"type","countdown"},{"name",timer->name()},{"seconds",timer->durationSeconds()},
            {"textId",timer->associatedText()?timer->associatedText()->id:QString()},{"cut",cut}};
    else if(auto *media=qobject_cast<Multimedia *>(selected.data()))
        actionClipboard = QJsonObject{{"version",1},{"type","multimedia"},{"name",media->name()},{"file",media->filePath()},{"cut",cut}};
    else return false;
    if (cut) deleteAction();
    return true;
}
void WidgetContainer::pasteAction() {
    if (!allowCountdown) return;
    QJsonObject root;
    if (!clipboardAction(&root)) return;
    int index = selected ? contentsLayout->indexOf(selected)+1 : widgetCount();
    QString name = root["name"].toString() + (root["cut"].toBool() ? QString() : QString(" (copia)"));
    if(root["type"]=="multimedia")emit pasteMultimediaRequested(name,root["file"].toString(),index);
    else emit pasteCountdownRequested(name,root["seconds"].toInt(),root["textId"].toString(),index);
}
void WidgetContainer::deleteAction() {
    if (!allowCountdown || !selected) return;
    auto *item = selected.data(); selectCountdown(nullptr);
    if(auto *media=qobject_cast<Multimedia *>(item))media->stop();
    delete item; emit contentsChanged();
}
int WidgetContainer::insertionIndexAt(int y) const {
    for (int index=0; index<widgetCount(); ++index)
        if (y < contentsLayout->itemAt(index)->widget()->geometry().center().y()) return index;
    return widgetCount();
}
void WidgetContainer::showDropLine(int index) {
    int y = index < widgetCount() ? contentsLayout->itemAt(index)->widget()->y()-2
        : widgetCount() ? contentsLayout->itemAt(widgetCount()-1)->widget()->geometry().bottom()+2 : 4;
    dropLine->setGeometry(4,qMax(0,y),widget()->width()-8,2); dropLine->show(); dropLine->raise();
}
bool WidgetContainer::eventFilter(QObject *watched, QEvent *event) {
    if (!allowCountdown) return QScrollArea::eventFilter(watched,event);
    auto *surface = qobject_cast<QWidget *>(watched);
    if (surface && (event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove || event->type() == QEvent::Drop)) {
        auto *drop = static_cast<QDropEvent *>(event);
        if(drop->mimeData()->hasUrls()) {
            const auto files=mediaFiles(drop->mimeData());
            if(files.isEmpty()){drop->ignore();return true;}
            const int index=insertionIndexAt(surface->mapTo(widget(),drop->position().toPoint()).y());
            if(event->type()==QEvent::Drop){dropLine->hide();emit mediaFilesDropped(files,index);}
            else showDropLine(index);
            drop->setDropAction(Qt::CopyAction);drop->accept();return true;
        }
        if (dragged && drop->source() == this && drop->mimeData()->hasFormat(moveMime)) {
            int index = insertionIndexAt(surface->mapTo(widget(),drop->position().toPoint()).y());
            if (event->type() == QEvent::Drop) { dragScroll.stop(); moveWidget(dragged,index); dropLine->hide(); }
            else {
                showDropLine(index);
                dragScroll.start();
                int y = surface->mapTo(viewport(),drop->position().toPoint()).y();
                if (y < 20) verticalScrollBar()->setValue(verticalScrollBar()->value()-12);
                else if (y > viewport()->height()-20) verticalScrollBar()->setValue(verticalScrollBar()->value()+12);
            }
            drop->setDropAction(Qt::MoveAction); drop->accept(); return true;
        }
    }
    if (event->type() == QEvent::DragLeave) { dragScroll.stop(); dropLine->hide(); }
    QWidget *item = nullptr;
    for (auto *parent = surface; parent && parent != widget(); parent = parent->parentWidget())
        if(qobject_cast<Countdown *>(parent) || qobject_cast<Multimedia *>(parent)){item=parent;break;}
    if (event->type() == QEvent::MouseButtonPress) {
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() == Qt::LeftButton) {
            selectAction(item, true);
            pressed = (qobject_cast<QAbstractButton *>(surface) || qobject_cast<QAbstractSlider *>(surface)) ? nullptr : item;
            dragStart = mouse->globalPosition().toPoint();
            if (pressed) {
                pressed->setFocus(Qt::MouseFocusReason);
                // Consume the press so QWidget propagation cannot clear the
                // drag source again when the event reaches the container.
                mouse->accept();
                return true;
            }
        }
    } else if (event->type() == QEvent::MouseButtonRelease) pressed.clear();
    else if (event->type() == QEvent::MouseMove && pressed) {
        auto *mouse = static_cast<QMouseEvent *>(event);
        if ((mouse->buttons() & Qt::LeftButton) && (mouse->globalPosition().toPoint()-dragStart).manhattanLength() >= QApplication::startDragDistance()) {
            dragged = pressed; pressed.clear();
            QDrag drag(this); auto *mime = new QMimeData; mime->setData(moveMime,"move"); drag.setMimeData(mime);
            drag.setPixmap(dragged->grab()); drag.setHotSpot(QPoint(16,16));
            drag.exec(Qt::MoveAction); dragScroll.stop(); dragged.clear(); dropLine->hide(); return true;
        }
    }
    return QScrollArea::eventFilter(watched,event);
}
