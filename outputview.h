#pragma once
#include <QWidget>
class QGraphicsScene;

// A read-only view of the launched scene, shared by preview and external output.
class OutputView : public QWidget {
    Q_OBJECT
public:
    explicit OutputView(QGraphicsScene *scene, QWidget *parent = nullptr);
protected:
    void paintEvent(QPaintEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
private:
    QGraphicsScene *scene;
};
