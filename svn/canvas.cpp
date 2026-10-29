#include "canvas.h"
#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include <QMenu>
#include <QContextMenuEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QFontMetricsF>
#include <QPainterPath>
#include <algorithm>

CanvasItem::CanvasItem(const QString &value) : text(value), name(value.simplified().left(50)) {
    if (name.isEmpty()) name = "Texto";
    setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges);
    setAcceptHoverEvents(true);
}
CanvasItem::CanvasItem(const QImage &value) : CanvasItem(QString()) {
    image = value;
    name = "Imagen";
    size = value.size().scaled(480, 300, Qt::KeepAspectRatio);
}
CanvasItem::CanvasItem(Shape value) : CanvasItem(QString()) {
    geometryType = value;
    name = value == Shape::Circle ? "Círculo" : value == Shape::Square ? "Cuadrado" : "Rectángulo";
    size = value == Shape::Rectangle ? QSizeF(320,180) : QSizeF(200,200);
    color = QColor("#80a4ae");
}
QString CanvasItem::typeName() const {
    if (isImage()) return "image";
    switch (geometryType) {
    case Shape::Circle: return "circle";
    case Shape::Square: return "square";
    case Shape::Rectangle: return "rectangle";
    default: return "text";
    }
}
QPainterPath CanvasItem::shape() const {
    if (geometryType != Shape::Circle) return QGraphicsObject::shape();
    QPainterPath path;
    path.addEllipse(selectionRect());
    if (isSelected() && !locked) for (int corner=0; corner<4; ++corner) path.addRect(handle(corner));
    return path;
}
QRectF CanvasItem::selectionRect() const {
    if (!isText()) return QRectF(QPointF(), size);
    const QFontMetricsF metrics(font);
    QRectF bounds = metrics.boundingRect(QRectF(4,4,312,92), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap, text);
    if (text.isEmpty()) bounds = QRectF(4,4,24,metrics.height());
    bounds = bounds.adjusted(-2,-2,2,2).intersected(QRectF(0,0,320,100));
    return QRectF(bounds.x()*size.width()/320.0, bounds.y()*size.height()/100.0,
                  bounds.width()*size.width()/320.0, bounds.height()*size.height()/100.0);
}
QRectF CanvasItem::boundingRect() const { return selectionRect().adjusted(-6,-6,6,6); }
void CanvasItem::setText(const QString &value) {
    if (text == value) return;
    prepareGeometryChange(); text = value; update();
}
void CanvasItem::setTextFont(const QFont &value) {
    if (font == value) return;
    prepareGeometryChange(); font = value; update();
}
void CanvasItem::setVideoFrame(const QImage &frame) {
    if (frame.isNull()) return;
    image = frame; update();
}
void CanvasItem::setTimerAppearance(bool overdue, bool hidden) {
    overdueText = overdue; hiddenText = hidden; update();
}
void CanvasItem::setLocked(bool value) {
    locked = value; resizing = -1; setFlag(ItemIsMovable, !value); update();
}
QRectF CanvasItem::handle(int c) const {
    const QRectF r = selectionRect();
    return QRectF(QPointF((c & 1) ? r.right()-5 : r.left()-5, (c & 2) ? r.bottom()-5 : r.top()-5), QSizeF(10,10));
}
int CanvasItem::cornerAt(QPointF p) const {
    if (isSelected() && !locked) for (int c=0; c<4; ++c) if (handle(c).contains(p)) return c;
    return -1;
}
void CanvasItem::setItemSize(QSizeF value) {
    value = QSizeF(std::clamp(value.width(), 24.0, 16000.0), std::clamp(value.height(), 24.0, 16000.0));
    if (fixedAspect()) value.setHeight(value.width());
    if (value == size) return;
    prepareGeometryChange(); size = value; update(); emit edited();
}
void CanvasItem::paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *) {
    const QRectF r(QPointF(), size);
    p->setRenderHint(QPainter::Antialiasing);
    if (isImage()) {
        p->setRenderHint(QPainter::SmoothPixmapTransform);
        if (!mediaId.isEmpty()) {
            QSizeF fitted=image.size(); fitted.scale(r.size(),Qt::KeepAspectRatio);
            p->fillRect(r,Qt::black);p->drawImage(QRectF((r.width()-fitted.width())/2,(r.height()-fitted.height())/2,fitted.width(),fitted.height()),image);
        } else p->drawImage(r, image);
    }
    else if (isShape()) {
        p->setPen(Qt::NoPen); p->setBrush(color);
        if (geometryType == Shape::Circle) p->drawEllipse(r); else p->drawRect(r);
    } else {
        p->save(); p->setClipRect(r); p->scale(size.width()/320.0, size.height()/100.0);
        p->setFont(font); p->setPen(overdueText ? QColor(Qt::red) : color);
        if (hiddenText) p->setOpacity(0);
        p->drawText(QRectF(4,4,312,92), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap, text); p->restore();
    }
    if (isSelected() && !locked) {
        QPen pen(QColor("#1558a6"), 1, Qt::DashLine); pen.setCosmetic(true);
        p->setPen(pen); p->setBrush(Qt::NoBrush); p->drawRect(selectionRect());
        p->setPen(QColor("#1558a6")); p->setBrush(Qt::white);
        for (int c=0;c<4;++c) p->drawRect(handle(c));
    }
}
QVariant CanvasItem::itemChange(GraphicsItemChange change, const QVariant &value) {
    if (change == ItemPositionHasChanged) emit edited();
    return QGraphicsObject::itemChange(change, value);
}
void CanvasItem::mousePressEvent(QGraphicsSceneMouseEvent *e) {
    if (e->button() == Qt::LeftButton && (resizing = cornerAt(e->pos())) >= 0) {
        resizeOriginalSize = size;
        resizeOriginalRect = selectionRect();
        opposite = pos() + QPointF((resizing & 1) ? resizeOriginalRect.left() : resizeOriginalRect.right(),
                                   (resizing & 2) ? resizeOriginalRect.top() : resizeOriginalRect.bottom());
        e->accept(); return;
    }
    QGraphicsObject::mousePressEvent(e);
}
void CanvasItem::mouseMoveEvent(QGraphicsSceneMouseEvent *e) {
    if (resizing >= 0) {
        QPointF p = e->scenePos();
        qreal w = std::clamp((resizing & 1) ? p.x()-opposite.x() : opposite.x()-p.x(), 24.0, 16000.0);
        qreal h = std::clamp((resizing & 2) ? p.y()-opposite.y() : opposite.y()-p.y(), 24.0, 16000.0);
        if (fixedAspect()) {
            qreal side = std::abs(w - resizeOriginalRect.width()) >= std::abs(h - resizeOriginalRect.height()) ? w : h;
            w = h = side;
        }
        setItemSize({w * resizeOriginalSize.width() / resizeOriginalRect.width(),
                     h * resizeOriginalSize.height() / resizeOriginalRect.height()});
        const QRectF r = selectionRect();
        setPos(opposite - QPointF((resizing & 1) ? r.left() : r.right(), (resizing & 2) ? r.top() : r.bottom()));
        e->accept(); return;
    }
    QGraphicsObject::mouseMoveEvent(e);
}
void CanvasItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *e) {
    if (resizing >= 0) { resizing = -1; e->accept(); return; }
    QGraphicsObject::mouseReleaseEvent(e);
}
void CanvasItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *e) {
    emit editRequested(this); e->accept();
}
void CanvasItem::hoverMoveEvent(QGraphicsSceneHoverEvent *e) {
    int c = cornerAt(e->pos());
    setCursor(locked ? Qt::ArrowCursor : c < 0 ? Qt::SizeAllCursor : (c==0 || c==3) ? Qt::SizeFDiagCursor : Qt::SizeBDiagCursor);
    QGraphicsObject::hoverMoveEvent(e);
}
CanvasView::CanvasView(QWidget *parent) : QGraphicsView(parent) {
    setObjectName("canvasView"); setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    setBackgroundBrush(QColor("#92999f")); setDragMode(RubberBandDrag);
    setTransformationAnchor(AnchorUnderMouse);
}
void CanvasView::fitCanvas() { fitInView(scene()->sceneRect().adjusted(-24,-24,24,24), Qt::KeepAspectRatio); }
void CanvasView::resizeEvent(QResizeEvent *event) {
    QGraphicsView::resizeEvent(event);
    if (scene()) fitCanvas();
}
void CanvasView::drawBackground(QPainter *p, const QRectF &rect) {
    p->fillRect(rect, palette().window());
    p->fillRect(scene()->sceneRect(), Qt::black);
    QPen pen(QColor("#626a72")); pen.setCosmetic(true); p->setPen(pen);p->drawRect(scene()->sceneRect());
}
void CanvasView::contextMenuEvent(QContextMenuEvent *e) {
    QPointF point = mapToScene(e->pos());
    auto *item = dynamic_cast<CanvasItem *>(itemAt(e->pos()));
    if (item && !item->isSelected()) { scene()->clearSelection(); item->setSelected(true); }
    QMenu menu(this);
    QAction *text = nullptr, *image = nullptr, *edit = nullptr, *remove = nullptr, *front = nullptr;
    QAction *circle = nullptr, *square = nullptr, *rectangle = nullptr;
    if (item) {
        edit = menu.addAction("Propiedades…");
        edit->setObjectName("itemProperties");
        front = menu.addAction("Traer al frente");
        menu.addSeparator();
        remove = menu.addAction("Eliminar selección");
    } else {
        text = menu.addAction("Añadir texto…");
        image = menu.addAction("Añadir imagen…");
        menu.addSeparator();
        circle = menu.addAction("Añadir círculo…"); circle->setObjectName("addCircle");
        square = menu.addAction("Añadir cuadrado…"); square->setObjectName("addSquare");
        rectangle = menu.addAction("Añadir rectángulo…"); rectangle->setObjectName("addRectangle");
    }
    if(edit)edit->setIcon(QIcon(":/icons/settings.svg"));
    if(remove)remove->setIcon(QIcon(":/icons/Remove.svg"));
    if(front)front->setIcon(QIcon(":/icons/Next.svg"));
    if(text)text->setIcon(QIcon(":/icons/Add.svg"));
    if(image)image->setIcon(QIcon(":/icons/Load.svg"));
    for(auto *action:{circle,square,rectangle})if(action)action->setIcon(QIcon(":/icons/Add.svg"));
    auto *chosen = menu.exec(e->globalPos());
    if (!chosen) return;
    if (chosen == text) emit addTextRequested(point);
    else if (chosen == image) emit addImageRequested(point);
    else if (chosen == circle) emit addShapeRequested(CanvasItem::Shape::Circle, point);
    else if (chosen == square) emit addShapeRequested(CanvasItem::Shape::Square, point);
    else if (chosen == rectangle) emit addShapeRequested(CanvasItem::Shape::Rectangle, point);
    else if (chosen == edit) emit editRequested(item);
    else if (chosen == front) emit bringToFrontRequested(item);
    else if (chosen == remove) emit deleteRequested();
}
void CanvasView::wheelEvent(QWheelEvent *e) {
    if (e->modifiers() & Qt::ControlModifier) {
        qreal factor = e->angleDelta().y() > 0 ? 1.15 : 1/1.15;
        qreal next = transform().m11()*factor;
        if (next >= .1 && next <= 5) scale(factor, factor);
        e->accept(); return;
    }
    QGraphicsView::wheelEvent(e);
}
void CanvasView::keyPressEvent(QKeyEvent *e) {
    if (e->key() == Qt::Key_Delete) { emit deleteRequested(); e->accept(); return; }
    QGraphicsView::keyPressEvent(e);
}
