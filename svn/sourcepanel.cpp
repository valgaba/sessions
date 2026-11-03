#include "sourcepanel.h"
#include <QtWidgets>
#include <QHash>
#include <QSet>
#include <QUuid>
#include <QTimer>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <algorithm>
#include <functional>

QIcon panelIcon(const QString &kind, bool active) {
    if(kind=="group")return QIcon(":/icons/folder.svg");
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

namespace {
bool isGroupRow(const QTreeWidgetItem *row) {
    return row && row->data(0,Qt::UserRole+1).toString()=="group";
}
}

class SourceTree : public QTreeWidget {
public:
    using QTreeWidget::QTreeWidget;
    std::function<void(const QStringList &, const QString &)> dropItems;
protected:
    void paintEvent(QPaintEvent *event) override {
        QTreeWidget::paintEvent(event);
        // Draw one outline around the whole row, including its tree branch,
        // instead of outlining each name/visibility/lock cell separately.
        QPainter painter(viewport());
        painter.setPen(QPen(QColor("#80a4ae"),1));painter.setBrush(Qt::NoBrush);
        for(auto *row:selectedItems()) {
            const QRect rect=visualItemRect(row);
            if(rect.isEmpty() || !viewport()->rect().intersects(rect))continue;
            painter.drawRect(QRect(0,rect.top(),viewport()->width()-1,rect.height()-1));
        }
    }
    void startDrag(Qt::DropActions actions) override {
        bool hasItems=false;
        for(auto *row:selectedItems())if(!isGroupRow(row)){hasItems=true;break;}
        if(hasItems)QTreeWidget::startDrag(actions);
    }
    void dragEnterEvent(QDragEnterEvent *event) override {
        if(event->source()==this){QTreeWidget::dragEnterEvent(event);event->setDropAction(Qt::MoveAction);event->accept();}
        else QTreeWidget::dragEnterEvent(event);
    }
    void dragMoveEvent(QDragMoveEvent *event) override {
        if(event->source()!=this){event->ignore();return;}
        QTreeWidget::dragMoveEvent(event);
        event->setDropAction(Qt::MoveAction);
        event->accept();
    }
    void dropEvent(QDropEvent *event) override {
        if(event->source()!=this){event->ignore();return;}
        QStringList ids;
        for(auto *row:selectedItems())if(!isGroupRow(row))ids.append(row->data(0,Qt::UserRole).toString());
        if(ids.isEmpty()){event->ignore();return;}
        auto *row=itemAt(event->position().toPoint());
        const QString groupId=row?(isGroupRow(row)?row->data(0,Qt::UserRole).toString():row->data(0,Qt::UserRole+2).toString()):QString();
        event->setDropAction(Qt::MoveAction);
        event->accept();
        QTimer::singleShot(0,this,[this,ids,groupId]{if(dropItems)dropItems(ids,groupId);});
    }
};

SourcePanel::SourcePanel(QGraphicsScene *value, QWidget *parent) : QWidget(parent), scene(value) {
    setObjectName("SourcesPanel"); setMinimumWidth(180);
    auto *layout = new QVBoxLayout(this); layout->setContentsMargins(0,0,0,0); layout->setSpacing(0);
    auto *title = new QLabel("Fuentes"); title->setObjectName("SourcesTitle"); layout->addWidget(title);
    list = new SourceTree; list->setObjectName("sourcesList"); list->setColumnCount(3);
    list->setHeaderHidden(true); list->setRootIsDecorated(true); list->setIndentation(16);
    list->setSelectionMode(QAbstractItemView::ExtendedSelection); list->setUniformRowHeights(true);
    list->setSelectionBehavior(QAbstractItemView::SelectRows);
    list->setDragEnabled(true); list->setAcceptDrops(true); list->setDropIndicatorShown(true);
    list->setDragDropMode(QAbstractItemView::DragDrop); list->setDefaultDropAction(Qt::MoveAction);
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
    connect(list,&QTreeWidget::itemSelectionChanged,this,[this]{selectCurrent();});
    connect(list,&QTreeWidget::itemClicked,this,[this](QTreeWidgetItem *row,int column){
        if(!row)return;
        if(isGroupRow(row)) {
            const QString id=row->data(0,Qt::UserRole).toString();
            if(column==1)toggleGroupVisibility(id); else if(column==2)toggleGroupLock(id);
            return;
        }
        if(column==1)toggleVisibility();else if(column==2)toggleLock();else selectCurrent();
    });
    connect(list,&QTreeWidget::itemDoubleClicked,this,[this](QTreeWidgetItem *row,int column){
        if(column!=0||!row)return;
        if(isGroupRow(row))renameGroup(row->data(0,Qt::UserRole).toString());
        else if(auto *item=currentItem())emit propertiesRequested(item);
    });
    connect(list,&QTreeWidget::itemExpanded,this,[this](QTreeWidgetItem *row){
        if(!isGroupRow(row))return;
        const QString id=row->data(0,Qt::UserRole).toString();
        for(auto &group:groupList)if(group.id==id)group.expanded=true;
    });
    connect(list,&QTreeWidget::itemCollapsed,this,[this](QTreeWidgetItem *row){
        if(!isGroupRow(row))return;
        const QString id=row->data(0,Qt::UserRole).toString();
        for(auto &group:groupList)if(group.id==id)group.expanded=false;
    });
    connect(list,&QWidget::customContextMenuRequested,this,&SourcePanel::showMenu);
    static_cast<SourceTree *>(list)->dropItems=[this](const QStringList &ids,const QString &groupId) {
        QSet<QString> groupIds;for(const auto &group:groupList)groupIds.insert(group.id);
        const QString target=groupIds.contains(groupId)?groupId:QString();
        bool changed=false;
        for(const QString &id:ids) {
            for(auto *base:scene->items())if(auto *item=dynamic_cast<CanvasItem *>(base);item && item->mediaId.isEmpty() && item->id==id) {
                if(item->groupId!=target){item->groupId=target;changed=true;}
                break;
            }
        }
        if(!changed)return;
        if(!target.isEmpty()) {
            for(auto &group:groupList)if(group.id==target)group.expanded=false;
            if(auto *row=groupRow(target))row->setExpanded(false);
        }
        signature.clear();emit edited();synchronize();
        if(auto *row=groupRow(target))row->setExpanded(false);
    };
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
    if (!list->currentItem() || isGroupRow(list->currentItem())) return nullptr;
    const QString id = list->currentItem()->data(0,Qt::UserRole).toString();
    for(auto *base:scene->items()) if(auto *item=dynamic_cast<CanvasItem *>(base);item && item->id==id)return item;
    return nullptr;
}
QString SourcePanel::currentGroupId() const {
    if (!list->currentItem()) return QString();
    if (isGroupRow(list->currentItem())) return list->currentItem()->data(0,Qt::UserRole).toString();
    if (auto *item=currentItem())
        for(const auto &group:groupList)if(group.id==item->groupId)return group.id;
    return QString();
}
QString SourcePanel::activeGroupId() const { return currentGroupId(); }
QList<CanvasItem *> SourcePanel::groupItems(const QString &groupId) const {
    QList<CanvasItem *> items;
    if(groupId.isEmpty())return items;
    for(auto *base:scene->items(Qt::DescendingOrder))if(auto *item=dynamic_cast<CanvasItem *>(base);item && item->mediaId.isEmpty() && item->groupId==groupId)items.append(item);
    return items;
}
QTreeWidgetItem *SourcePanel::groupRow(const QString &groupId) const {
    for(int i=0;i<list->topLevelItemCount();++i)if(isGroupRow(list->topLevelItem(i)) && list->topLevelItem(i)->data(0,Qt::UserRole).toString()==groupId)return list->topLevelItem(i);
    return nullptr;
}
void SourcePanel::setGroups(const QList<SourceGroup> &groups) {
    groupList.clear();
    QSet<QString> ids;
    for(const auto &group:groups) {
        const QString name=group.name.trimmed();
        if(group.id.isEmpty() || name.isEmpty() || ids.contains(group.id))continue;
        ids.insert(group.id);
        groupList.append({group.id,name,group.expanded});
    }
    signature.clear();
    synchronize();
}
void SourcePanel::synchronize() {
    QList<CanvasItem *> items; QStringList next;
    QString selectedId; int selectedCount=0;
    for(auto *base:scene->items(Qt::DescendingOrder)) if(auto *item=dynamic_cast<CanvasItem *>(base)) {
        if(!item->mediaId.isEmpty())continue;
        items.append(item);
        next << item->id << item->name << item->typeName() << QString::number(item->isVisible()) << QString::number(item->isLocked()) << item->groupId;
        if(item->isSelected()){selectedId=item->id;++selectedCount;}
    }
    for(const auto &group:groupList)next << QStringLiteral("@group") << group.id << group.name;
    QSignalBlocker blocker(list);
    QTreeWidgetItem *current=list->currentItem();
    QString currentId=current?current->data(0,Qt::UserRole).toString():QString();
    if(isGroupRow(current))currentId="group:"+currentId;
    if(!selectedId.isEmpty())currentId=selectedId;
    std::function<QTreeWidgetItem *(QTreeWidgetItem *)> findRow=[&](QTreeWidgetItem *parent)->QTreeWidgetItem *{
        const int count=parent?parent->childCount():list->topLevelItemCount();
        for(int i=0;i<count;++i) {
            auto *row=parent?parent->child(i):list->topLevelItem(i);
            if(row->data(0,Qt::UserRole).toString()==currentId)return row;
            if(auto *found=findRow(row))return found;
        }
        return nullptr;
    };
    if(next!=signature) {
        signature=next;
        QHash<QString,bool> expanded;
        for(int i=0;i<list->topLevelItemCount();++i)if(isGroupRow(list->topLevelItem(i)))expanded.insert(list->topLevelItem(i)->data(0,Qt::UserRole).toString(),list->topLevelItem(i)->isExpanded());
        for(auto &group:groupList)if(expanded.contains(group.id))group.expanded=expanded.value(group.id);
        list->clear();
        QSet<QString> groupIds;
        for(const auto &group:groupList)groupIds.insert(group.id);
        QHash<QString,QList<CanvasItem *>> grouped;
        QList<CanvasItem *> ungrouped;
        for(auto *item:items) {
            if(groupIds.contains(item->groupId))grouped[item->groupId].append(item);
            else ungrouped.append(item);
        }
        const auto addItemRow=[this](QTreeWidgetItem *parent,CanvasItem *item) {
            auto *row=parent?new QTreeWidgetItem(parent):new QTreeWidgetItem(list);
            row->setText(0,item->name); row->setData(0,Qt::UserRole,item->id); row->setData(0,Qt::UserRole+1,"item"); row->setData(0,Qt::UserRole+2,item->groupId);
            row->setFlags(row->flags() | Qt::ItemIsDragEnabled);
            row->setIcon(0,panelIcon(item->typeName())); row->setIcon(1,panelIcon("eye",item->isVisible())); row->setIcon(2,panelIcon("lock",item->isLocked()));
            row->setToolTip(0,item->name); row->setToolTip(1,item->isVisible()?"Ocultar":"Mostrar"); row->setToolTip(2,item->isLocked()?"Desbloquear":"Bloquear");
            row->setSizeHint(0,QSize(0,30));
        };
        for(const auto &group:groupList) {
            const auto children=grouped.value(group.id);
            auto *row=new QTreeWidgetItem(list);
            row->setText(0,group.name); row->setData(0,Qt::UserRole,group.id); row->setData(0,Qt::UserRole+1,"group");
            row->setFlags((row->flags() | Qt::ItemIsDropEnabled) & ~Qt::ItemIsDragEnabled);
            row->setIcon(0,panelIcon("group")); row->setSizeHint(0,QSize(0,30));
            row->setToolTip(0,group.name);
            bool allVisible=true, allLocked=!children.isEmpty();
            for(auto *item:children){allVisible=allVisible && item->isVisible();allLocked=allLocked && item->isLocked();}
            row->setIcon(1,panelIcon("eye",allVisible)); row->setIcon(2,panelIcon("lock",allLocked));
            row->setToolTip(1,allVisible?"Ocultar grupo":"Mostrar grupo"); row->setToolTip(2,allLocked?"Desbloquear grupo":"Bloquear grupo");
            for(auto *item:children)addItemRow(row,item);
            row->setExpanded(group.expanded);
        }
        for(auto *item:ungrouped)addItemRow(nullptr,item);
        if(currentId.startsWith("group:")) {
            auto *row=groupRow(currentId.mid(6));
            if(row)list->setCurrentItem(row);
        } else if(auto *row=findRow(nullptr))list->setCurrentItem(row);
    } else if(selectedCount==1) {
        if(auto *row=findRow(nullptr)) {
            if(row->isSelected())list->setCurrentItem(row,0,QItemSelectionModel::NoUpdate);
            else list->setCurrentItem(row);
        }
    }
}
void SourcePanel::selectCurrent() {
    { QSignalBlocker blocker(scene); scene->clearSelection();
      for(auto *row:list->selectedItems()) {
          if(isGroupRow(row))continue;
          const QString id=row->data(0,Qt::UserRole).toString();
          for(auto *base:scene->items())if(auto *item=dynamic_cast<CanvasItem *>(base);item && item->id==id && item->isVisible()){item->setSelected(true);break;}
      }
    }
    synchronize();
}
void SourcePanel::toggleVisibility() { if(auto *item=currentItem()){item->setVisible(!item->isVisible());emit edited();synchronize();} }
void SourcePanel::toggleLock() { if(auto *item=currentItem()){item->setLocked(!item->isLocked());emit edited();synchronize();} }
void SourcePanel::removeCurrent() { if(auto *item=currentItem()){delete item;emit edited();synchronize();} }
void SourcePanel::moveCurrent(int direction) {
    auto *item=currentItem(); if(!item)return;
    QList<CanvasItem *> siblings;
    for(auto *base:scene->items(Qt::DescendingOrder))if(auto *source=dynamic_cast<CanvasItem *>(base);source && source->mediaId.isEmpty() && source->groupId==item->groupId)siblings.append(source);
    const int from=siblings.indexOf(item),to=from+direction;
    if(from<0 || to<0 || to>=siblings.size())return;
    const qreal z=siblings[from]->zValue();
    siblings[from]->setZValue(siblings[to]->zValue()); siblings[to]->setZValue(z);
    emit edited();synchronize();
}
QString SourcePanel::suggestedGroupName() const {
    QSet<QString> names;
    for(const auto &group:groupList)names.insert(group.name.toCaseFolded());
    if(!names.contains(QStringLiteral("grupo").toCaseFolded()))return QStringLiteral("Grupo");
    int number=2;
    while(names.contains((QStringLiteral("Grupo ")+QString::number(number)).toCaseFolded()))++number;
    return QStringLiteral("Grupo ")+QString::number(number);
}
void SourcePanel::addGroup() {
    bool ok;
    const QString name=QInputDialog::getText(this,"Nuevo grupo","Nombre del grupo:",QLineEdit::Normal,suggestedGroupName(),&ok).trimmed();
    if(!ok || name.isEmpty())return;
    groupList.append({QUuid::createUuid().toString(QUuid::WithoutBraces),name,true});
    signature.clear(); emit edited(); synchronize();
    if(auto *row=groupRow(groupList.last().id))list->setCurrentItem(row);
}
void SourcePanel::renameGroup(const QString &groupId) {
    auto it=std::find_if(groupList.begin(),groupList.end(),[&](const SourceGroup &group){return group.id==groupId;});
    if(it==groupList.end())return;
    bool ok;
    const QString name=QInputDialog::getText(this,"Renombrar grupo","Nombre del grupo:",QLineEdit::Normal,it->name,&ok).trimmed();
    if(!ok || name.isEmpty() || name==it->name)return;
    it->name=name; signature.clear(); emit edited(); synchronize();
}
void SourcePanel::removeGroup(const QString &groupId) {
    for(auto *base:scene->items())if(auto *item=dynamic_cast<CanvasItem *>(base);item && item->groupId==groupId)item->groupId.clear();
    groupList.erase(std::remove_if(groupList.begin(),groupList.end(),[&](const SourceGroup &group){return group.id==groupId;}),groupList.end());
    signature.clear(); emit edited(); synchronize();
}
void SourcePanel::toggleGroupVisibility(const QString &groupId) {
    const auto items=groupItems(groupId);
    if(items.isEmpty())return;
    bool allVisible=true;for(auto *item:items)allVisible=allVisible && item->isVisible();
    for(auto *item:items)item->setVisible(!allVisible);
    emit edited(); synchronize();
}
void SourcePanel::toggleGroupLock(const QString &groupId) {
    const auto items=groupItems(groupId);
    if(items.isEmpty())return;
    bool allLocked=true;for(auto *item:items)allLocked=allLocked && item->isLocked();
    for(auto *item:items)item->setLocked(!allLocked);
    emit edited(); synchronize();
}
void SourcePanel::moveItemsToGroup(const QStringList &itemIds,const QString &groupId) {
    QSet<QString> ids;for(const auto &group:groupList)ids.insert(group.id);
    const QString destination=ids.contains(groupId)?groupId:QString();
    const QSet<QString> selected(itemIds.begin(),itemIds.end());
    bool changed=false;
    for(auto *base:scene->items())if(auto *item=dynamic_cast<CanvasItem *>(base);item && item->mediaId.isEmpty() && selected.contains(item->id) && item->groupId!=destination) {
        item->groupId=destination;changed=true;
    }
    if(!changed)return;
    for(auto &group:groupList)if(group.id==destination)group.expanded=false;
    if(auto *row=groupRow(destination))row->setExpanded(false);
    signature.clear(); emit edited(); synchronize();
    // Rebuilding the tree reparents the rows. Keep the whole moved selection.
    {
        QSignalBlocker blocker(list);list->clearSelection();
        for(QTreeWidgetItemIterator it(list);*it;++it)
            if(!isGroupRow(*it) && selected.contains((*it)->data(0,Qt::UserRole).toString()))(*it)->setSelected(true);
    }
    selectCurrent();
    if(auto *row=groupRow(destination))row->setExpanded(false);
}
void SourcePanel::addMoveToGroupMenu(QMenu *menu) {
    QStringList selectedIds;QSet<QString> memberships;
    for(auto *row:list->selectedItems())if(!isGroupRow(row)) {
        selectedIds.append(row->data(0,Qt::UserRole).toString());
        memberships.insert(row->data(0,Qt::UserRole+2).toString());
    }
    if(selectedIds.isEmpty())return;
    auto *move=menu->addMenu("Mover a grupo");
    const auto addDestination=[this,move,selectedIds,memberships](const QString &name,const QString &id) {
        auto *action=move->addAction(name);action->setCheckable(true);
        action->setChecked(memberships.size()==1 && memberships.contains(id));
        connect(action,&QAction::triggered,this,[this,selectedIds,id]{moveItemsToGroup(selectedIds,id);});
    };
    addDestination("Sin grupo",QString());
    for(const auto &group:groupList)addDestination(group.name,group.id);
}
void SourcePanel::showMenu(QPoint position) {
    if(auto *row=list->itemAt(position)) {
        if(row->isSelected())list->setCurrentItem(row,0,QItemSelectionModel::NoUpdate);
        else list->setCurrentItem(row);
    }
    QMenu menu(this);
    menu.addMenu(addMenu);
    auto *groupAction=menu.addAction(panelIcon("group"),"Añadir grupo…");
    groupAction->setObjectName("sourceAddGroup");
    connect(groupAction,&QAction::triggered,this,&SourcePanel::addGroup);
    menu.addSeparator();
    auto *currentRow=list->currentItem();
    if(currentRow && isGroupRow(currentRow)) {
        const QString groupId=currentRow->data(0,Qt::UserRole).toString();
        auto *rename=menu.addAction("Renombrar grupo…");rename->setObjectName("sourceRenameGroup");
        connect(rename,&QAction::triggered,this,[this,groupId]{renameGroup(groupId);});
        auto *remove=menu.addAction(QIcon(":/icons/Remove.svg"),"Eliminar grupo");remove->setObjectName("sourceDeleteGroup");
        connect(remove,&QAction::triggered,this,[this,groupId]{removeGroup(groupId);});
        menu.addSeparator();
        const auto items=groupItems(groupId);
        bool allVisible=true, allLocked=!items.isEmpty();
        for(auto *item:items){allVisible=allVisible && item->isVisible();allLocked=allLocked && item->isLocked();}
        auto *visible=menu.addAction(allVisible?"Ocultar grupo":"Mostrar grupo");
        visible->setObjectName("sourceGroupVisibility"); visible->setEnabled(!items.isEmpty());
        connect(visible,&QAction::triggered,this,[this,groupId]{toggleGroupVisibility(groupId);});
        auto *lock=menu.addAction(allLocked?"Desbloquear grupo":"Bloquear grupo");
        lock->setObjectName("sourceGroupLock"); lock->setEnabled(!items.isEmpty());
        connect(lock,&QAction::triggered,this,[this,groupId]{toggleGroupLock(groupId);});
        addMoveToGroupMenu(&menu);
        menu.exec(list->viewport()->mapToGlobal(position));
        return;
    }
    auto *item=currentItem();
    emit clipboardMenuRequested(&menu,item);
    if (!item) { menu.exec(list->viewport()->mapToGlobal(position)); return; }
    menu.addSeparator();
    auto *visible=menu.addAction(item->isVisible()?"Ocultar":"Mostrar"); connect(visible,&QAction::triggered,this,&SourcePanel::toggleVisibility);
    auto *lock=menu.addAction(item->isLocked()?"Desbloquear":"Bloquear");lock->setObjectName("sourceLock");connect(lock,&QAction::triggered,this,&SourcePanel::toggleLock);
    auto *remove=menu.addAction(QIcon(":/icons/Remove.svg"),"Eliminar");remove->setObjectName("sourceDelete");connect(remove,&QAction::triggered,this,&SourcePanel::removeCurrent);
    menu.addSeparator();
    QList<CanvasItem *> siblings;
    for(auto *base:scene->items(Qt::DescendingOrder))if(auto *source=dynamic_cast<CanvasItem *>(base);source && source->mediaId.isEmpty() && source->groupId==item->groupId)siblings.append(source);
    const int index=siblings.indexOf(item);
    auto *up=menu.addAction("Subir");up->setObjectName("sourceUp");up->setEnabled(index>0);connect(up,&QAction::triggered,this,[this]{moveCurrent(-1);});
    auto *down=menu.addAction("Bajar");down->setObjectName("sourceDown");down->setEnabled(index+1<siblings.size());connect(down,&QAction::triggered,this,[this]{moveCurrent(1);});
    addMoveToGroupMenu(&menu);
    menu.addSeparator();auto *properties=menu.addAction(QIcon(":/icons/settings.svg"),"Propiedades…");properties->setObjectName("sourceProperties");connect(properties,&QAction::triggered,this,[this]{if(auto *source=currentItem())emit propertiesRequested(source);});
    menu.exec(list->viewport()->mapToGlobal(position));
}
