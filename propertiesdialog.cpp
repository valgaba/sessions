#include "propertiesdialog.h"
#include "canvas.h"
#include <QtWidgets>

PropertiesDialog::PropertiesDialog(CanvasItem *value, QWidget *parent)
    : QDialog(parent), item(value), font(value->font), color(value->color) {
    setObjectName("ItemPropertiesDialog");
    setWindowTitle(item->isShape() ? "Propiedades de la figura" : item->isImage() ? "Propiedades de la imagen" : "Propiedades del texto");
    resize(450, 300);
    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout;
    nameEdit = new QLineEdit(item->name);
    nameEdit->setObjectName("propertyName");
    form->addRow("Nombre", nameEdit);
    const QStringList labels{"X", "Y", "Ancho", "Alto"};
    const double values[]{item->x(), item->y(), item->itemSize().width(), item->itemSize().height()};
    for (int i = 0; i < 4; ++i) {
        fields[i] = new QDoubleSpinBox;
        fields[i]->setObjectName(QString("property%1").arg(i));
        fields[i]->setRange(i < 2 ? -16000 : 24, 16000);
        fields[i]->setDecimals(1);
        fields[i]->setSuffix(" px");
        fields[i]->setValue(values[i]);
        form->addRow(labels[i], fields[i]);
    }
    if (item->fixedAspect()) {
        connect(fields[2], qOverload<double>(&QDoubleSpinBox::valueChanged), fields[3], &QDoubleSpinBox::setValue);
        connect(fields[3], qOverload<double>(&QDoubleSpinBox::valueChanged), fields[2], &QDoubleSpinBox::setValue);
    }
    layout->addLayout(form);
    if (item->isText()) {
        textEdit = new QPlainTextEdit(item->text);
        textEdit->setObjectName("propertyText");
        layout->addWidget(new QLabel("Texto"));
        layout->addWidget(textEdit);
        auto *row = new QHBoxLayout;
        auto *fontButton = new QPushButton("Fuente…");
        auto *colorButton = new QPushButton("Color…");
        row->addWidget(fontButton); row->addWidget(colorButton); row->addStretch();
        layout->addLayout(row);
        connect(fontButton, &QPushButton::clicked, this, [this] {
            bool ok;
            QFont next = QFontDialog::getFont(&ok, font, this);
            if (ok) font = next;
        });
        connect(colorButton, &QPushButton::clicked, this, [this] {
            QColor next = QColorDialog::getColor(color, this);
            if (next.isValid()) color = next;
        });
    } else if (item->isShape()) {
        auto *colorButton = new QPushButton("Color de relleno…");
        colorButton->setObjectName("shapeColor");
        layout->addWidget(colorButton);
        connect(colorButton, &QPushButton::clicked, this, [this] {
            QColor next = QColorDialog::getColor(color, this, "Color de relleno");
            if (next.isValid()) color = next;
        });
    } else {
        layout->addWidget(new QLabel(QString("Imagen original: %1 × %2 px").arg(item->image.width()).arg(item->image.height())));
    }
    auto *buttons = new QDialogButtonBox;
    auto *accept = buttons->addButton("Aceptar", QDialogButtonBox::AcceptRole);
    connect(nameEdit, &QLineEdit::textChanged, accept, [this, accept] { accept->setEnabled(!nameEdit->text().trimmed().isEmpty()); });
    buttons->addButton("Cancelar", QDialogButtonBox::RejectRole);
    connect(buttons, &QDialogButtonBox::accepted, this, &PropertiesDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}
void PropertiesDialog::accept() {
    if (nameEdit->text().trimmed().isEmpty()) return;
    item->name = nameEdit->text().trimmed();
    item->setPos(fields[0]->value(), fields[1]->value());
    item->setItemSize({fields[2]->value(), fields[3]->value()});
    item->color = color;
    if (textEdit) {
        item->setText(textEdit->toPlainText());
        item->setTextFont(font);
    }
    item->update();
    QDialog::accept();
}
