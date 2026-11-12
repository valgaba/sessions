#pragma once
#include <QWidget>
#include <QPainter>
#include <QVariant>

// Same 100 x 10 colour marker and polygon geometry as Radit's FrameColorItemMax.
class PlayerColorIndicator : public QWidget {
public:
    explicit PlayerColorIndicator(QWidget *parent) : QWidget(parent) {
        setObjectName("playerColorIndicator");
        setFixedSize(100,10);
        setAttribute(Qt::WA_TransparentForMouseEvents);
    }
protected:
    void paintEvent(QPaintEvent *) override {
        const QColor color(parentWidget()->property("playerColor").toString());
        if(!color.isValid())return;
        QPainter painter(this);painter.setPen(Qt::NoPen);painter.setBrush(color);
        painter.drawPolygon(QPolygonF{QPointF(0,0),QPointF(50,0),QPointF(40,10),QPointF(0,10)});
        painter.drawPolygon(QPolygonF{QPointF(55,0),QPointF(65,0),QPointF(55,10),QPointF(45,10)});
        painter.drawPolygon(QPolygonF{QPointF(70,0),QPointF(80,0),QPointF(70,10),QPointF(60,10)});
    }
};
