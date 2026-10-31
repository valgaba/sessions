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
#include <cmath>

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
    name = value == Shape::Circle ? "Círculo" : value == Shape::Square ? "Cuadrado" : value==Shape::Line?"Línea":"Rectángulo";
    size = value==Shape::Line?QSizeF(320,1):value == Shape::Rectangle ? QSizeF(320,180) : QSizeF(200,200);
    color = QColor("#80a4ae");
}
QString CanvasItem::typeName() const {
    if (isImage()) return "image";
    switch (geometryType) {
    case Shape::Circle: return "circle";
    case Shape::Square: return "square";
    case Shape::Rectangle: return "rectangle";
    case Shape::Line: return "line";
    default: return "text";
    }
}
QPainterPath CanvasItem::shape() const {
    if(geometryType==Shape::Line) {
        QPainterPath center;center.moveTo(0,0);center.lineTo(size.width()-1,size.height()-1);
        QPainterPathStroker stroke;stroke.setWidth(10);auto path=stroke.createStroke(center);
        if(isSelected() && !locked){path.addRect(handle(0));path.addRect(handle(3));}
        return path;
    }
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
    qreal extent=10;
    if(scene() && !scene()->views().isEmpty())extent/=qMax(qreal(1),scene()->views().first()->transform().m11());
    return QRectF(QPointF((c & 1) ? r.right()-extent/2 : r.left()-extent/2, (c & 2) ? r.bottom()-extent/2 : r.top()-extent/2), QSizeF(extent,extent));
}
int CanvasItem::cornerAt(QPointF p) const {
    if(geometryType==Shape::Line){if(isSelected() && !locked){if(handle(0).contains(p))return 0;if(handle(3).contains(p))return 3;}return -1;}
    if (isSelected() && !locked) for (int c=0; c<4; ++c) if (handle(c).contains(p)) return c;
    return -1;
}
void CanvasItem::setItemSize(QSizeF value) {
    const double minimum=geometryType==Shape::Line?1.0:24.0;
    value = QSizeF(std::clamp(value.width(), minimum, 16000.0), std::clamp(value.height(), minimum, 16000.0));
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
            const QRectF frame((r.width()-fitted.width())/2,(r.height()-fitted.height())/2,fitted.width(),fitted.height());
            // Paint each pixel once: a black fill underneath a translucent
            // image would darken the other sources in a mixed output.
            if(frame.top()>0) {
                p->fillRect(QRectF(0,0,r.width(),frame.top()),Qt::black);
                p->fillRect(QRectF(0,frame.bottom(),r.width(),r.height()-frame.bottom()),Qt::black);
            } else if(frame.left()>0) {
                p->fillRect(QRectF(0,0,frame.left(),r.height()),Qt::black);
                p->fillRect(QRectF(frame.right(),0,r.width()-frame.right(),r.height()),Qt::black);
            }
            p->drawImage(frame,image);
        } else p->drawImage(r, image);
    }
    else if (isShape()) {
        p->setPen(Qt::NoPen); p->setBrush(color);
        if(geometryType==Shape::Line){p->setPen(QPen(color,3,Qt::SolidLine,Qt::RoundCap));p->drawLine(QPointF(0,0),QPointF(size.width()-1,size.height()-1));}
        else if (geometryType == Shape::Circle) p->drawEllipse(r); else p->drawRect(r);
    } else {
        p->save(); p->setClipRect(r); p->scale(size.width()/320.0, size.height()/100.0);
        p->setFont(font); p->setPen(overdueText ? QColor(Qt::red) : color);
        if (hiddenText) p->setOpacity(0);
        p->drawText(QRectF(4,4,312,92), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap, text); p->restore();
    }
    if (isSelected() && !locked) {
        QPen pen(QColor("#1558a6"), 1, Qt::DashLine); pen.setCosmetic(true);
        p->setPen(pen); p->setBrush(Qt::NoBrush); p->drawRect(selectionRect());
        pen.setStyle(Qt::SolidLine);p->setPen(pen); p->setBrush(Qt::white);
        for (int c=0;c<4;++c) if(geometryType!=Shape::Line || c==0 || c==3)p->drawRect(handle(c));
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
        const qreal minimum=geometryType==Shape::Line?1.0:24.0;
        qreal w = std::clamp((resizing & 1) ? p.x()-opposite.x() : opposite.x()-p.x(), minimum, 16000.0);
        qreal h = std::clamp((resizing & 2) ? p.y()-opposite.y() : opposite.y()-p.y(), minimum, 16000.0);
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
void CanvasView::fitCanvas() {
    if(!scene() || fitting)return;
    autoFit=true;fitting=true;
    fitInView(scene()->sceneRect().adjusted(-24,-24,24,24), Qt::KeepAspectRatio);
    fitting=false;emit zoomChanged(transform().m11());
}
void CanvasView::setZoom(qreal factor) {
    if(!std::isfinite(factor))return;
    autoFit=false;
    const qreal current=transform().m11();
    factor=std::clamp(factor,0.05,32.0);
    if(current>0)scale(factor/current,factor/current);
    emit zoomChanged(transform().m11());
}
void CanvasView::zoomIn(){setZoom(transform().m11()*1.15);}
void CanvasView::zoomOut(){setZoom(transform().m11()/1.15);}
void CanvasView::resizeEvent(QResizeEvent *event) {
    QGraphicsView::resizeEvent(event);
    if (autoFit && !fitting) fitCanvas();
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
    emit clipboardMenuRequested(&menu,item,point);
    menu.addSeparator();
    QAction *text = nullptr, *image = nullptr, *edit = nullptr, *remove = nullptr, *front = nullptr;
    QAction *circle = nullptr, *square = nullptr, *rectangle = nullptr, *line = nullptr;
    if (item) {
        edit = menu.addAction("Propiedades…");
        edit->setObjectName("itemProperties");
        front = menu.addAction("Traer al frente");
        menu.addSeparator();
        remove = menu.addAction("Eliminar selección");
    } else {
        text = menu.addAction("Añadir texto…");
        text->setObjectName("addText");
        image = menu.addAction("Añadir imagen…");
        image->setObjectName("addImage");
        menu.addSeparator();
        auto *shapes=menu.addMenu(QIcon(":/icons/Add.svg"),"Formas básicas");shapes->setObjectName("basicShapesMenu");
        circle = shapes->addAction("Círculo…"); circle->setObjectName("addCircle");
        square = shapes->addAction("Cuadrado…"); square->setObjectName("addSquare");
        rectangle = shapes->addAction("Rectángulo…"); rectangle->setObjectName("addRectangle");
        line=shapes->addAction("Línea…");line->setObjectName("addLine");
    }
    if(edit)edit->setIcon(QIcon(":/icons/settings.svg"));
    if(remove)remove->setIcon(QIcon(":/icons/Remove.svg"));
    if(front)front->setIcon(QIcon(":/icons/Next.svg"));
    if(text)text->setIcon(QIcon(":/icons/Add.svg"));
    if(image)image->setIcon(QIcon(":/icons/Load.svg"));
    for(auto *action:{circle,square,rectangle,line})if(action)action->setIcon(QIcon(":/icons/Add.svg"));
    auto *chosen = menu.exec(e->globalPos());
    if (!chosen) return;
    if (chosen == text) emit addTextRequested(point);
    else if (chosen == image) emit addImageRequested(point);
    else if (chosen == circle) emit addShapeRequested(CanvasItem::Shape::Circle, point);
    else if (chosen == square) emit addShapeRequested(CanvasItem::Shape::Square, point);
    else if (chosen == rectangle) emit addShapeRequested(CanvasItem::Shape::Rectangle, point);
    else if (chosen == line) emit addShapeRequested(CanvasItem::Shape::Line, point);
    else if (chosen == edit) emit editRequested(item);
    else if (chosen == front) emit bringToFrontRequested(item);
    else if (chosen == remove) emit deleteRequested();
}
void CanvasView::wheelEvent(QWheelEvent *e) {
    if (e->modifiers() & Qt::ControlModifier) {
        const int delta=e->angleDelta().y()!=0?e->angleDelta().y():e->pixelDelta().y();
        if(delta!=0)setZoom(transform().m11()*std::pow(e->modifiers() & Qt::ShiftModifier?1.02:1.15,delta/120.0));
        e->accept(); return;
    }
    QGraphicsView::wheelEvent(e);
}
void CanvasView::keyPressEvent(QKeyEvent *e) {
    if(e->matches(QKeySequence::Copy)){emit copyRequested(false);e->accept();return;}
    if(e->matches(QKeySequence::Cut)){emit copyRequested(true);e->accept();return;}
    if(e->matches(QKeySequence::Paste)){emit pasteRequested();e->accept();return;}
    if (e->key() == Qt::Key_Delete) { emit deleteRequested(); e->accept(); return; }
    QGraphicsView::keyPressEvent(e);
}
