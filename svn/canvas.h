#pragma once
#include <QGraphicsObject>
#include <QGraphicsView>
#include <QImage>
#include <QUuid>
#include <QPointer>
#include <QMenu>

class CanvasItem : public QGraphicsObject {
    Q_OBJECT
public:
    enum class Shape { None, Circle, Square, Rectangle, Line };
    Q_ENUM(Shape)
    explicit CanvasItem(const QString &text);
    explicit CanvasItem(const QImage &image);
    explicit CanvasItem(Shape shape);
    QRectF boundingRect() const override;
    void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget *) override;
    QPainterPath shape() const override;
    QSizeF itemSize() const { return size; }
    void setItemSize(QSizeF value);
    QRectF selectionRect() const;
    void setText(const QString &value);
    void setTextFont(const QFont &value);
    void setVideoFrame(const QImage &frame);
    void setLocked(bool locked);
    bool isLocked() const { return locked; }
    void setTimerAppearance(bool overdue, bool hidden);
    bool timerOverdue() const { return overdueText; }
    bool timerHidden() const { return hiddenText; }
    void setTimerOwner(QObject *owner) { countdownOwner = owner; }
    QObject *timerOwner() const { return countdownOwner.data(); }
    QString text;
    QString name;
    QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QString mediaId;
    QImage image;
    QColor color = Qt::white;
    QFont font = QFont("Arial", 32);
    bool isImage() const { return !image.isNull(); }
    bool isText() const { return !isImage() && geometryType == Shape::None; }
    bool isShape() const { return geometryType != Shape::None; }
    bool fixedAspect() const { return geometryType == Shape::Circle || geometryType == Shape::Square; }
    Shape shapeType() const { return geometryType; }
    QString typeName() const;
signals:
    void edited();
    void editRequested(CanvasItem *item);
protected:
    QVariant itemChange(GraphicsItemChange, const QVariant &) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *) override;
private:
    bool locked = false;
    bool overdueText = false;
    bool hiddenText = false;
    QPointer<QObject> countdownOwner;
    Shape geometryType = Shape::None;
    QSizeF size{320, 100};
    int cornerAt(QPointF pos) const;
    QRectF handle(int corner) const;
    int resizing = -1;
    QPointF opposite;
    QSizeF resizeOriginalSize;
    QRectF resizeOriginalRect;
};

class CanvasView : public QGraphicsView {
    Q_OBJECT
public:
    explicit CanvasView(QWidget *parent = nullptr);
    void fitCanvas();
    void setZoom(qreal factor);
    void zoomIn();
    void zoomOut();
    bool isCanvasFitted() const { return autoFit; }
signals:
    void zoomChanged(qreal factor);
    void clipboardMenuRequested(QMenu *menu, CanvasItem *item, QPointF position);
    void copyRequested(bool cut);
    void pasteRequested();
    void addTextRequested(QPointF pos);
    void addImageRequested(QPointF pos);
    void addShapeRequested(CanvasItem::Shape shape, QPointF pos);
    void deleteRequested();
    void bringToFrontRequested(CanvasItem *item);
    void editRequested(CanvasItem *item);
protected:
    void resizeEvent(QResizeEvent *) override;
    void drawBackground(QPainter *, const QRectF &) override;
    void contextMenuEvent(QContextMenuEvent *) override;
    void wheelEvent(QWheelEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
private:
    bool autoFit = true;
    bool fitting = false;
};
