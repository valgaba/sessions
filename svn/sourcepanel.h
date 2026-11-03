#pragma once
#include <QWidget>
#include <QIcon>
#include <QList>
#include <QString>
#include <QStringList>
#include "canvas.h"
class QTreeWidget;
class QTreeWidgetItem;
class QMenu;
QIcon panelIcon(const QString &kind, bool active = true);

struct SourceGroup {
    QString id;
    QString name;
    bool expanded = true;
};

class SourcePanel : public QWidget {
    Q_OBJECT
public:
    explicit SourcePanel(QGraphicsScene *scene, QWidget *parent = nullptr);
    void synchronize();
    QList<SourceGroup> groups() const { return groupList; }
    void setGroups(const QList<SourceGroup> &groups);
    QString activeGroupId() const;
signals:
    void edited();
    void propertiesRequested(CanvasItem *item);
    void addRequested(QString type);
    void clipboardMenuRequested(QMenu *menu, CanvasItem *item);
    void copyRequested(CanvasItem *item, bool cut);
    void pasteRequested();
private:
    CanvasItem *currentItem() const;
    QString currentGroupId() const;
    QList<CanvasItem *> groupItems(const QString &groupId) const;
    QTreeWidgetItem *groupRow(const QString &groupId) const;
    void selectCurrent();
    void toggleVisibility();
    void toggleLock();
    void removeCurrent();
    void moveCurrent(int direction);
    void addGroup();
    void renameGroup(const QString &groupId);
    void removeGroup(const QString &groupId);
    void toggleGroupVisibility(const QString &groupId);
    void toggleGroupLock(const QString &groupId);
    void moveItemsToGroup(const QStringList &itemIds, const QString &groupId);
    void addMoveToGroupMenu(QMenu *menu);
    QString suggestedGroupName() const;
    void showMenu(QPoint position);
    QGraphicsScene *scene;
    QTreeWidget *list;
    QMenu *addMenu;
    QStringList signature;
    QList<SourceGroup> groupList;
};
