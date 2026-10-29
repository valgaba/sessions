#pragma once
#include <QWidget>
#include <QIcon>
#include "canvas.h"
class QTreeWidget;
class QMenu;
QIcon panelIcon(const QString &kind, bool active = true);

class SourcePanel : public QWidget {
    Q_OBJECT
public:
    explicit SourcePanel(QGraphicsScene *scene, QWidget *parent = nullptr);
    void synchronize();
signals:
    void edited();
    void propertiesRequested(CanvasItem *item);
    void addRequested(QString type);
private:
    CanvasItem *currentItem() const;
    void selectCurrent();
    void toggleVisibility();
    void toggleLock();
    void removeCurrent();
    void moveCurrent(int direction);
    void showMenu(QPoint position);
    QGraphicsScene *scene;
    QTreeWidget *list;
    QMenu *addMenu;
    QStringList signature;
};
