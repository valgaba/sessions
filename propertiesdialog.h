#pragma once
#include <QDialog>
#include <QFont>
#include <QColor>
class CanvasItem;
class QDoubleSpinBox;
class QPlainTextEdit;
class QLineEdit;
class PropertiesDialog : public QDialog {
    Q_OBJECT
public:
    explicit PropertiesDialog(CanvasItem *item, QWidget *parent = nullptr);
    void accept() override;
private:
    CanvasItem *item;
    QDoubleSpinBox *fields[4];
    QPlainTextEdit *textEdit = nullptr;
    QLineEdit *nameEdit;
    QFont font;
    QColor color;
};
