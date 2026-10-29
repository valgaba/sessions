#include "outputview.h"
#include <QGraphicsScene>
#include <QPainter>
#include <QKeyEvent>

OutputView::OutputView(QGraphicsScene *value, QWidget *parent) : QWidget(parent), scene(value) {
    setMinimumSize(160, 90);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_OpaquePaintEvent);
    connect(scene, &QGraphicsScene::changed, this, qOverload<>(&QWidget::update));
}
void OutputView::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    QSizeF size = scene->sceneRect().size();
    size.scale(QSizeF(width(), height()), Qt::KeepAspectRatio);
    QRectF target((width() - size.width()) / 2, (height() - size.height()) / 2, size.width(), size.height());
    painter.setClipRect(target);
    scene->render(&painter, target, scene->sceneRect(), Qt::KeepAspectRatio);
}
void OutputView::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape && isWindow()) { hide(); event->accept(); return; }
    QWidget::keyPressEvent(event);
}
