#include "sourcepanel.h"
#include <QtWidgets>

QIcon panelIcon(const QString &kind, bool active) {
    if(kind=="delete")return QIcon(":/icons/Remove.svg");
    QPixmap pixmap(20,20); pixmap.fill(Qt::transparent);
    QPainter p(&pixmap); p.setRenderHint(QPainter::Antialiasing);
    const QColor color(active ? "#d4e1e4" : "#686b74");
    p.setPen(QPen(color,1.6));
    if (kind == "eye") {
        QPainterPath path; path.moveTo(2,10); path.quadTo(10,0,18,10); path.quadTo(10,20,2,10);
        p.drawPath(path); p.setBrush(color); p.drawEllipse(QPointF(10,10),2.5,2.5);
        if (!active) { p.setPen(QPen(color,2)); p.drawLine(3,18,17,2); }
    } else if (kind == "lock") {
        p.drawRoundedRect(QRectF(5,9,10,8),1,1);
        p.drawArc(QRectF(active ? 6 : 9,2,8,12),0,180*16);
    } else if (kind == "text") {
        QFont font("Arial",9); font.setBold(true); p.setFont(font); p.drawText(pixmap.rect(),Qt::AlignCenter,"ab");
    } else if (kind == "circle") p.drawEllipse(QRectF(3,3,14,14));
    else if (kind == "square") p.drawRect(QRectF(3,3,14,14));
    else if (kind == "rectangle") p.drawRect(QRectF(2,5,16,10));
    else if (kind == "line") p.drawLine(3,16,17,4);
    else if (kind == "image") {
        p.drawRect(QRectF(2,3,16,14)); p.drawEllipse(QPointF(6,7),1,1);
        p.drawPolyline(QPolygonF{QPointF(3,15),QPointF(9,9),QPointF(12,12),QPointF(15,8),QPointF(18,13)});
    } else if (kind == "delete") {
        p.drawLine(3,5,17,5); p.drawLine(7,2,13,2); p.drawRect(QRectF(5,5,10,12));
        p.drawLine(8,8,8,14); p.drawLine(12,8,12,14);
    } else {
        QFont font("Arial",14); p.setFont(font); p.drawText(pixmap.rect(),Qt::AlignCenter,kind);
    }
    return QIcon(pixmap);
}

SourcePanel::SourcePanel(QGraphicsScene *value, QWidget *parent) : QWidget(parent), scene(value) {
    setObjectName("SourcesPanel"); setMinimumWidth(180);
    auto *layout = new QVBoxLayout(this); layout->setContentsMargins(0,0,0,0); layout->setSpacing(0);
    auto *title = new QLabel("Fuentes"); title->setObjectName("SourcesTitle"); layout->addWidget(title);
    list = new QTreeWidget; list->setObjectName("sourcesList"); list->setColumnCount(3);
    list->setHeaderHidden(true); list->setRootIsDecorated(false); list->setIndentation(0);
    list->setSelectionMode(QAbstractItemView::SingleSelection); list->setUniformRowHeights(true);
    list->header()->setStretchLastSection(false); list->header()->setSectionResizeMode(0,QHeaderView::Stretch);
    for (int column : {1,2}) { list->header()->setSectionResizeMode(column,QHeaderView::Fixed); list->setColumnWidth(column,28); }
    list->setContextMenuPolicy(Qt::CustomContextMenu); layout->addWidget(list,1);
    addMenu = new QMenu("Añadir fuente",this);addMenu->setIcon(QIcon(":/icons/Add.svg"));
    addMenu->setObjectName("sourceAddMenu");
    auto *shapesMenu=new QMenu("Formas básicas",addMenu);shapesMenu->setObjectName("sourceShapesMenu");shapesMenu->setIcon(QIcon(":/icons/Add.svg"));
    for (auto pair : {qMakePair(QString("Texto…"),QString("text")),qMakePair(QString("Imagen…"),QString("image")),qMakePair(QString("Círculo…"),QString("circle")),qMakePair(QString("Cuadrado…"),QString("square")),qMakePair(QString("Rectángulo…"),QString("rectangle")),qMakePair(QString("Línea…"),QString("line"))}) {
        auto *action = (pair.second=="text" || pair.second=="image"?addMenu:shapesMenu)->addAction(panelIcon(pair.second),pair.first);
        action->setObjectName("sourceAdd_"+pair.second);
        connect(action,&QAction::triggered,this,[this,pair]{emit addRequested(pair.second);});
    }
    addMenu->addMenu(shapesMenu);
    connect(list,&QTreeWidget::currentItemChanged,this,[this]{selectCurrent();});
    connect(list,&QTreeWidget::itemClicked,this,[this](QTreeWidgetItem *,int column){if(column==1)toggleVisibility();else if(column==2)toggleLock();else selectCurrent();});
    connect(list,&QTreeWidget::itemDoubleClicked,this,[this](QTreeWidgetItem *,int column){if(column==0)if(auto *item=currentItem())emit propertiesRequested(item);});
    connect(list,&QWidget::customContextMenuRequested,this,&SourcePanel::showMenu);
    connect(scene,&QGraphicsScene::changed,this,[this]{synchronize();});
    connect(scene,&QGraphicsScene::selectionChanged,this,[this]{synchronize();});
    auto *removeKey = new QShortcut(QKeySequence::Delete,list); removeKey->setContext(Qt::WidgetWithChildrenShortcut);
    connect(removeKey,&QShortcut::activated,this,&SourcePanel::removeCurrent);
    for(auto key:{QKeySequence::Copy,QKeySequence::Cut,QKeySequence::Paste}) {
        auto *shortcut=new QShortcut(QKeySequence(key),list);shortcut->setContext(Qt::WidgetWithChildrenShortcut);
        connect(shortcut,&QShortcut::activated,this,[this,key]{if(key==QKeySequence::Paste)emit pasteRequested();else if(auto *item=currentItem())emit copyRequested(item,key==QKeySequence::Cut);});
    }
    synchronize();
}
CanvasItem *SourcePanel::currentItem() const {
    if (!list->currentItem()) return nullptr;
    const QString id = list->currentItem()->data(0,Qt::UserRole).toString();
    for(auto *base:scene->items()) if(auto *item=dynamic_cast<CanvasItem *>(base);item && item->id==id)return item;
    return nullptr;
}
void SourcePanel::synchronize() {
    QList<CanvasItem *> items; QStringList next;
    QString selectedId;
    for(auto *base:scene->items(Qt::DescendingOrder)) if(auto *item=dynamic_cast<CanvasItem *>(base)) {
        if(!item->mediaId.isEmpty())continue;
        items.append(item);
        next << item->id << item->name << item->typeName() << QString::number(item->isVisible()) << QString::number(item->isLocked());
        if(item->isSelected())selectedId=item->id;
    }
    QSignalBlocker blocker(list);
    QString currentId = list->currentItem() ? list->currentItem()->data(0,Qt::UserRole).toString() : QString();
    if(!selectedId.isEmpty())currentId=selectedId;
    if(next!=signature) {
        signature=next; list->clear();
        for(auto *item:items) {
            auto *row=new QTreeWidgetItem(list); row->setText(0,item->name); row->setData(0,Qt::UserRole,item->id);
            row->setIcon(0,panelIcon(item->typeName())); row->setIcon(1,panelIcon("eye",item->isVisible())); row->setIcon(2,panelIcon("lock",item->isLocked()));
            row->setToolTip(0,item->name); row->setToolTip(1,item->isVisible()?"Ocultar":"Mostrar"); row->setToolTip(2,item->isLocked()?"Desbloquear":"Bloquear");
            row->setSizeHint(0,QSize(0,30));
        }
    }
    for(int i=0;i<list->topLevelItemCount();++i)if(list->topLevelItem(i)->data(0,Qt::UserRole).toString()==currentId)list->setCurrentItem(list->topLevelItem(i));
}
void SourcePanel::selectCurrent() {
    auto *item=currentItem();
    { QSignalBlocker blocker(scene); scene->clearSelection(); if(item && item->isVisible())item->setSelected(true); }
    synchronize();
}
void SourcePanel::toggleVisibility() { if(auto *item=currentItem()){item->setVisible(!item->isVisible());emit edited();synchronize();} }
void SourcePanel::toggleLock() { if(auto *item=currentItem()){item->setLocked(!item->isLocked());emit edited();synchronize();} }
void SourcePanel::removeCurrent() { if(auto *item=currentItem()){delete item;emit edited();synchronize();} }
void SourcePanel::moveCurrent(int direction) {
    auto *item=currentItem(); if(!item)return;
    QList<CanvasItem *> items; for(auto *base:scene->items(Qt::DescendingOrder))if(auto *source=dynamic_cast<CanvasItem *>(base))items.append(source);
    int from=items.indexOf(item),to=from+direction; if(to<0 || to>=items.size())return;
    items.swapItemsAt(from,to); for(int i=0;i<items.size();++i)items[i]->setZValue(items.size()-i);
    emit edited();synchronize();
}
void SourcePanel::showMenu(QPoint position) {
    if(auto *row=list->itemAt(position))list->setCurrentItem(row);
    auto *item=currentItem();
    QMenu menu(this);
    menu.addMenu(addMenu);
    menu.addSeparator();
    emit clipboardMenuRequested(&menu,item);
    if (!item) { menu.exec(list->viewport()->mapToGlobal(position)); return; }
    menu.addSeparator();
    auto *visible=menu.addAction(item->isVisible()?"Ocultar":"Mostrar"); connect(visible,&QAction::triggered,this,&SourcePanel::toggleVisibility);
    auto *lock=menu.addAction(item->isLocked()?"Desbloquear":"Bloquear");lock->setObjectName("sourceLock");connect(lock,&QAction::triggered,this,&SourcePanel::toggleLock);
    auto *remove=menu.addAction(QIcon(":/icons/Remove.svg"),"Eliminar");remove->setObjectName("sourceDelete");connect(remove,&QAction::triggered,this,&SourcePanel::removeCurrent);
    menu.addSeparator();
    int index=list->indexOfTopLevelItem(list->currentItem());
    auto *up=menu.addAction("Subir");up->setObjectName("sourceUp");up->setEnabled(index>0);connect(up,&QAction::triggered,this,[this]{moveCurrent(-1);});
    auto *down=menu.addAction("Bajar");down->setObjectName("sourceDown");down->setEnabled(index+1<list->topLevelItemCount());connect(down,&QAction::triggered,this,[this]{moveCurrent(1);});
    menu.addSeparator();auto *properties=menu.addAction(QIcon(":/icons/settings.svg"),"Propiedades…");properties->setObjectName("sourceProperties");connect(properties,&QAction::triggered,this,[this]{if(auto *source=currentItem())emit propertiesRequested(source);});
    menu.exec(list->viewport()->mapToGlobal(position));
}
