#include "mainwindow.h"
#include "widgetcontainer.h"
#include "propertiesdialog.h"
#include "outputview.h"
#include "countdown.h"
#include "sourcepanel.h"
#include "multimedia.h"
#include "aboutdialog.h"
#include <QtWidgets>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QBuffer>
#include <QSaveFile>
#include <QScreen>
#include <QWindow>
#include <cmath>

// Radit options use a flat title strip instead of the native Windows frame.
class RaditOptionsDialog : public QDialog {
public:
    explicit RaditOptionsDialog(QWidget *parent):QDialog(parent) {
        setWindowFlags(Qt::Dialog|Qt::FramelessWindowHint);
        setAttribute(Qt::WA_StyledBackground);
    }
protected:
    void mousePressEvent(QMouseEvent *event) override {
        if(event->button()==Qt::LeftButton && event->position().y()<26 && windowHandle()) {
            windowHandle()->startSystemMove();event->accept();return;
        }
        QDialog::mousePressEvent(event);
    }
};

MainWindow::MainWindow() {
    setFadeDuration(QSettings().value("output/fadeDurationMs",1000).toInt());
    fadeAnimation=new QVariantAnimation(this);
    fadeAnimation->setStartValue(1.0);fadeAnimation->setEndValue(0.0);
    fadeAnimation->setEasingCurve(QEasingCurve::Linear);
    connect(fadeAnimation,&QVariantAnimation::valueChanged,this,[this](const QVariant &value){if(fadeOverlay)fadeOverlay->setOpacity(value.toReal());});
    connect(fadeAnimation,&QVariantAnimation::finished,this,[this]{delete fadeOverlay;fadeOverlay=nullptr;});
    resize(1200,800); setWindowTitle("Sessions — Sin título[*]");
    auto *splitter = new QSplitter(Qt::Vertical, this);
    splitter->setObjectName("mainSplitter"); splitter->setChildrenCollapsible(false);
    setCentralWidget(splitter);
    auto *top = new QSplitter(Qt::Horizontal);
    splitter->addWidget(top);
    top->setObjectName("topSplitter"); top->setChildrenCollapsible(false); top->setHandleWidth(94);
    top->setStyleSheet("QSplitter#topSplitter::handle { background: #181e2c; border: none; }");
    auto *composition = new QWidget;
    auto *compositionLayout = new QVBoxLayout(composition);
    compositionLayout->setContentsMargins(4,4,4,4);
    auto *compositionHeader = new QHBoxLayout;
    compositionHeader->addWidget(new QLabel("COMPOSICIÓN  ·  1280 × 720"));
    compositionHeader->addStretch();
    auto *launch = new QPushButton(QIcon(":/icons/Next.svg"),"Lanzar");
    launch->setObjectName("launchComposition");
    launch->setCursor(Qt::PointingHandCursor);
    launch->setToolTip("Enviar la composición a la salida (Ctrl+Intro)");
    launch->setShortcut(QKeySequence("Ctrl+Return"));
    compositionLayout->addLayout(compositionHeader);
    canvas = new QGraphicsScene(this); canvas->setSceneRect(0,0,1280,720);
    canvas->setBackgroundBrush(Qt::black);
    view = new CanvasView; view->setScene(canvas); compositionLayout->addWidget(view);
    auto *zoomRow=new QWidget;zoomRow->setObjectName("CompositionZoomBar");zoomRow->setFixedWidth(284);
    auto *zoomLayout=new QHBoxLayout(zoomRow);zoomLayout->setContentsMargins(0,0,0,0);zoomLayout->setSpacing(5);zoomLayout->addStretch();
    auto *zoomLevel=new QLabel;zoomLevel->setObjectName("compositionZoomLevel");zoomLevel->setMinimumWidth(56);zoomLevel->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
    auto *less=new QToolButton;less->setObjectName("compositionZoomOut");less->setIcon(QIcon(":/icons/zoom-out.svg"));less->setIconSize(QSize(18,18));less->setToolTip("Alejar");less->setAccessibleName("Alejar composición");less->setFixedSize(24,24);zoomLayout->addWidget(less);
    auto *zoom=new QSlider(Qt::Horizontal);zoom->setObjectName("compositionZoom");zoom->setRange(0,1000);zoom->setSingleStep(3);zoom->setPageStep(25);zoom->setFixedSize(150,26);zoom->setAccessibleName("Zoom de composición");zoom->setToolTip("Zoom de composición · Ctrl+rueda · Ctrl+Mayús+rueda para ajuste fino");zoomLayout->addWidget(zoom);
    auto *more=new QToolButton;more->setObjectName("compositionZoomIn");more->setIcon(QIcon(":/icons/zoom-in.svg"));more->setIconSize(QSize(18,18));more->setToolTip("Acercar");more->setAccessibleName("Acercar composición");more->setFixedSize(24,24);zoomLayout->addWidget(more);
    zoomLayout->addWidget(zoomLevel);
    auto *zoomFooter=new QHBoxLayout;zoomFooter->setContentsMargins(0,0,0,0);zoomFooter->addStretch();zoomFooter->addWidget(zoomRow);compositionLayout->addLayout(zoomFooter);
    connect(less,&QToolButton::clicked,view,&CanvasView::zoomOut);connect(more,&QToolButton::clicked,view,&CanvasView::zoomIn);
    connect(zoom,&QSlider::valueChanged,this,[this](int value){view->setZoom(.05*std::pow(640.0,value/1000.0));});
    connect(view,&CanvasView::zoomChanged,this,[zoom,zoomLevel](qreal factor){
        const QSignalBlocker blocker(zoom);zoom->setValue(qRound(1000*std::log(factor/.05)/std::log(640.0)));
        zoomLevel->setText(QString::number(factor*100,'f',factor<1?1:0)+" %");
    });
    top->addWidget(composition);
    outputScene = new QGraphicsScene(this);
    outputScene->setSceneRect(canvas->sceneRect()); outputScene->setBackgroundBrush(Qt::black);
    auto *output = new QWidget;
    auto *outputLayout = new QVBoxLayout(output); outputLayout->setContentsMargins(4,4,4,4);
    outputLayout->addWidget(new QLabel("SALIDA  ·  1280 × 720"));
    outputPreview = new OutputView(outputScene); outputPreview->setObjectName("outputPreview");
    outputLayout->addWidget(outputPreview, 1);
    outputPreview->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(outputPreview, &QWidget::customContextMenuRequested, this, &MainWindow::showOutputMenu);
    top->addWidget(output); top->setSizes({600,600});
    auto *transferHandle = top->handle(1);
    transferHandle->setObjectName("transferHandle");
    transferHandle->setCursor(Qt::ArrowCursor);
    auto *transferLayout = new QVBoxLayout(transferHandle);
    transferLayout->setContentsMargins(6, 0, 6, 0);
    transferLayout->addStretch();
    transferLayout->addWidget(launch);
    transferLayout->addStretch();
    externalOutput = new OutputView(outputScene, this);
    externalOutput->setObjectName("externalOutput");
    externalOutput->setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    externalOutput->setWindowTitle("Sessions — Salida exterior");
    externalOutput->hide();
    connect(launch, &QPushButton::clicked, this, &MainWindow::launchComposition);
    connect(qApp, &QGuiApplication::screenAdded, this, [this] { updateScreens(); });
    connect(qApp, &QGuiApplication::screenRemoved, this, [this] {
        externalOutput->hide(); updateScreens();
    });
    updateScreens();
    auto *bottomPanels = new QSplitter(Qt::Horizontal);
    bottomPanels->setObjectName("bottomSplitter");
    bottomPanels->setChildrenCollapsible(false); bottomPanels->setHandleWidth(1);
    bottomPanels->setMinimumHeight(70);
    for (int panel = 0; panel < 3; ++panel) {
        if (panel == 1) {
            sources = new SourcePanel(canvas); bottomPanels->addWidget(sources); bottomPanels->setStretchFactor(panel, 1);
            connect(sources,&SourcePanel::edited,this,&MainWindow::changed);
            connect(sources,&SourcePanel::propertiesRequested,this,&MainWindow::editText);
            connect(sources,&SourcePanel::clipboardMenuRequested,this,[this](QMenu *menu,CanvasItem *item){addClipboardActions(menu,item?QList<CanvasItem *>{item}:QList<CanvasItem *>{});});
            connect(sources,&SourcePanel::copyRequested,this,[this](CanvasItem *item,bool cut){copyItems({item},cut);});
            connect(sources,&SourcePanel::pasteRequested,this,[this]{pasteItems();});
            connect(sources,&SourcePanel::addRequested,this,[this](const QString &type) {
                const QPointF position(80,80);
                if(type=="text")emit view->addTextRequested(position);
                else if(type=="image")emit view->addImageRequested(position);
                else emit view->addShapeRequested(type=="circle"?CanvasItem::Shape::Circle:type=="square"?CanvasItem::Shape::Square:type=="line"?CanvasItem::Shape::Line:CanvasItem::Shape::Rectangle,position);
            });
            continue;
        }
        auto *container = new WidgetContainer;
        container->setCountdownCreationEnabled(panel == 0);
        countdownContainers.append(container);
        if (panel == 0) {
            auto *actions = new QWidget; actions->setObjectName("ActionsPanel");
            actions->setAcceptDrops(true);actions->installEventFilter(container);
            auto *layout = new QVBoxLayout(actions); layout->setContentsMargins(0,0,0,0); layout->setSpacing(0);
            auto *title = new QLabel("Acciones"); title->setObjectName("ActionsTitle"); layout->addWidget(title);
            layout->addWidget(container,1);
            bottomPanels->addWidget(actions);
        } else bottomPanels->addWidget(container);
        bottomPanels->setStretchFactor(panel, 1);
        if (panel == 0) connect(container, &WidgetContainer::addCountdownRequested, this, [this] {
            bool ok;
            QString name = QInputDialog::getText(this, "Add Countdown", "Nombre:", QLineEdit::Normal, suggestedElementName("countdown"), &ok).trimmed();
            if (!ok || name.isEmpty()) return;
            auto *countdown = createCountdown(name);
            bottom->selectCountdown(countdown);
            changed();
        });
    }
    bottom = countdownContainers.first();
    connect(bottom,&WidgetContainer::mediaFilesDropped,this,[this](const QStringList &files,int index) {
        for(const auto &file:files) {
            auto *media=createMultimedia(QFileInfo(file).completeBaseName(),file);
            bottom->moveWidget(media,index++);
        }
        changed();
    });
    connect(bottom,&WidgetContainer::addMultimediaRequested,this,[this] {
        bool ok;QString name=QInputDialog::getText(this,"Add Multimedia","Nombre:",QLineEdit::Normal,suggestedElementName("multimedia"),&ok).trimmed();
        if(!ok || name.isEmpty())return;
        createMultimedia(name);changed();
    });
    connect(bottom, &WidgetContainer::contentsChanged, this, &MainWindow::changed);
    connect(bottom,&WidgetContainer::pasteMultimediaRequested,this,[this](const QString &name,const QString &file,bool loop,int index,bool purge,bool next,bool mix){
        auto *media=createMultimedia(name,file,{},loop);media->setPurge(purge);media->setNext(next);media->setMix(mix);bottom->moveWidget(media,index);bottom->selectAction(media);changed();
    });
    connect(bottom, &WidgetContainer::pasteCountdownRequested, this, [this](const QString &name, int seconds, const QString &textId, int index, const QString &format,const QString &totalTextId) {
        auto *countdown = createCountdown(name,seconds,textId,false,format,totalTextId);
        bottom->moveCountdown(countdown,index);
        bottom->selectCountdown(countdown);
        changed();
    });
    bottomPanels->setSizes({400,400,400});
    splitter->addWidget(bottomPanels);
    splitter->setHandleWidth(1);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({385,385});
    menuBar()->setObjectName("menubar");
    auto *file=menuBar()->addMenu("&Archivo"); auto *insert=menuBar()->addMenu("&Insertar"); auto *visual=menuBar()->addMenu("&Ver");
    auto *options=menuBar()->addMenu("&Opciones");
    auto *fadeOptions=options->addAction(QIcon(":/icons/settings.svg"),"Tiempo de fundido…");fadeOptions->setObjectName("fadeOptions");
    connect(fadeOptions,&QAction::triggered,this,&MainWindow::showFadeOptions);
    auto action=[this](QMenu *menu,const QString &label,QKeySequence key,auto fn){
        static const QHash<QString,QString> icons{{"Nuevo","New"},{"Abrir…","Load"},{"Guardar","save"},{"Guardar como…","saveas"},{"Salir","eject"},
            {"Añadir texto…","Add"},{"Añadir imagen…","Load"},{"Añadir círculo…","Add"},{"Añadir cuadrado…","Add"},{"Añadir rectángulo…","Add"},{"Ajustar lienzo","Zoom"},{"Tamaño real","Zoom"}};
        auto *a=menu->addAction(QIcon(":/icons/"+icons.value(label)+".svg"),label);a->setShortcut(key);connect(a,&QAction::triggered,this,fn);return a;
    };
    action(file,"Nuevo",QKeySequence::New,[this]{if(canDiscard()){for(auto *container:countdownContainers)container->clearWidgets(); canvas->clear(); filename.clear();dirty=false;setWindowModified(false);setWindowTitle("Sessions — Sin título[*]");refresh();}});
    action(file,"Abrir…",QKeySequence::Open,[this]{if(!canDiscard())return;QString p=QFileDialog::getOpenFileName(this,"Abrir diseño",{},"Diseño Sessions (*.sessions)");if(!p.isEmpty() && !readProject(p))QMessageBox::warning(this,"Abrir","No se ha podido abrir el diseño.");});
    action(file,"Guardar",QKeySequence::Save,[this]{save();});
    action(file,"Guardar como…",QKeySequence::SaveAs,[this]{save(true);});
    action(file,"Salir",QKeySequence::Quit,[this]{close();});
    auto addText = [this](QPointF p) {
        QDialog dialog(this);
        dialog.setObjectName("AddTextDialog"); dialog.setWindowTitle("Añadir texto"); dialog.resize(420,240);
        auto *layout = new QVBoxLayout(&dialog);
        auto *form = new QFormLayout;
        auto *name = new QLineEdit(suggestedElementName("texto")); name->setObjectName("elementName");name->selectAll();
        auto *text = new QPlainTextEdit("Nuevo texto"); text->setObjectName("elementText");
        form->addRow("Nombre:", name); form->addRow("Texto:", text); layout->addLayout(form);
        auto *buttons = new QDialogButtonBox;
        auto *accept = buttons->addButton("Añadir", QDialogButtonBox::AcceptRole);
        buttons->addButton("Cancelar", QDialogButtonBox::RejectRole); accept->setEnabled(false);
        auto validate = [=] { accept->setEnabled(!name->text().trimmed().isEmpty() && !text->toPlainText().isEmpty()); };
        connect(name, &QLineEdit::textChanged, &dialog, validate); connect(text, &QPlainTextEdit::textChanged, &dialog, validate);
        validate();
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject); layout->addWidget(buttons);
        if (dialog.exec() != QDialog::Accepted || name->text().trimmed().isEmpty()) return;
        auto *item = new CanvasItem(text->toPlainText()); item->name = name->text().trimmed(); addItem(item,p);
    };
    auto addImage = [this](QPointF p) {
        QString path = QFileDialog::getOpenFileName(this,"Añadir imagen",{},"Imágenes (*.png *.jpg *.jpeg *.bmp *.gif);;Todos los archivos (*)");
        if (path.isEmpty()) return;
        QImage image(path);
        if (image.isNull()) { QMessageBox::warning(this,"Imagen","No se ha podido leer la imagen."); return; }
        bool ok;
        QString name = QInputDialog::getText(this,"Añadir imagen","Nombre:",QLineEdit::Normal,suggestedElementName("imagen"),&ok).trimmed();
        if (!ok || name.isEmpty()) return;
        auto *item = new CanvasItem(image); item->name = name; addItem(item,p);
    };
    action(insert,"Añadir texto…",QKeySequence("Ctrl+T"),[this,addText]{addText({80,80});});
    action(insert,"Añadir imagen…",QKeySequence("Ctrl+I"),[this,addImage]{addImage({80,80});});
    auto addShape = [this](CanvasItem::Shape shape, QPointF pos) {
        QString label = shape == CanvasItem::Shape::Circle ? "Círculo" : shape == CanvasItem::Shape::Square ? "Cuadrado" : shape==CanvasItem::Shape::Line?"Línea":"Rectángulo";
        bool ok;
        QString name = QInputDialog::getText(this, "Añadir " + label.toLower(), "Nombre:", QLineEdit::Normal, suggestedElementName(label.toLower()), &ok).trimmed();
        if (!ok || name.isEmpty()) return;
        auto *item = new CanvasItem(shape); item->name = name; addItem(item,pos);
    };
    insert->addSeparator();
    auto *shapesMenu=insert->addMenu(QIcon(":/icons/Add.svg"),"Formas básicas");
    action(shapesMenu,"Añadir círculo…",{},[addShape]{addShape(CanvasItem::Shape::Circle,{80,80});});
    action(shapesMenu,"Añadir cuadrado…",{},[addShape]{addShape(CanvasItem::Shape::Square,{80,80});});
    action(shapesMenu,"Añadir rectángulo…",{},[addShape]{addShape(CanvasItem::Shape::Rectangle,{80,80});});
    auto *lineAction=shapesMenu->addAction(QIcon(":/icons/Add.svg"),"Añadir línea…");connect(lineAction,&QAction::triggered,this,[addShape]{addShape(CanvasItem::Shape::Line,{80,80});});
    connect(view, &CanvasView::addShapeRequested, this, addShape);
    connect(view,&CanvasView::addTextRequested,this,addText); connect(view,&CanvasView::addImageRequested,this,addImage);
    connect(view,&CanvasView::deleteRequested,this,&MainWindow::removeSelection);
    connect(view,&CanvasView::clipboardMenuRequested,this,[this](QMenu *menu,CanvasItem *,QPointF position){addClipboardActions(menu,selectedCanvasItems(),position,true);});
    connect(view,&CanvasView::copyRequested,this,[this](bool cut){copyItems(selectedCanvasItems(),cut);});
    connect(view,&CanvasView::pasteRequested,this,[this]{pasteItems();});
    connect(view,&CanvasView::editRequested,this,&MainWindow::editText);
    connect(view, &CanvasView::bringToFrontRequested, this, [this](CanvasItem *item) {
        qreal z = 0;
        for (auto *other : canvas->items()) z = qMax(z, other->zValue());
        item->setZValue(z + 1);
        changed();
    });
    action(visual,"Ajustar lienzo",QKeySequence("Ctrl+0"),[this]{view->fitCanvas();});
    action(visual,"Tamaño real",QKeySequence("Ctrl+1"),[this]{view->setZoom(1);});
    auto *zoomIn=visual->addAction(QIcon(":/icons/Zoom.svg"),"Acercar");zoomIn->setShortcut(QKeySequence::ZoomIn);connect(zoomIn,&QAction::triggered,view,&CanvasView::zoomIn);
    auto *zoomOut=visual->addAction(QIcon(":/icons/Zoom.svg"),"Alejar");zoomOut->setShortcut(QKeySequence::ZoomOut);connect(zoomOut,&QAction::triggered,view,&CanvasView::zoomOut);
    auto *help=menuBar()->addMenu("A&yuda");
    auto *about=help->addAction(QIcon(":/icons/Info.svg"),"Acerca de Sessions…");about->setObjectName("aboutSessions");
    connect(about,&QAction::triggered,this,[this]{(new AboutDialog(this))->show();});
    refresh();
    QTimer::singleShot(0,view,&CanvasView::fitCanvas);

     this->showMaximized();
}
MainWindow::~MainWindow() {
    fadeDurationMs=0;cancelOutputFade();
    disconnect(canvas, nullptr, sources, nullptr);
    for (auto *container : countdownContainers) container->clearWidgets();
    delete externalOutput;
    delete outputPreview;
    // The scene can emit selectionChanged while child widgets are being destroyed.
    disconnect(canvas, nullptr, this, nullptr);
    for (auto *base : canvas->items())
        if (auto *item = dynamic_cast<CanvasItem *>(base)) disconnect(item, nullptr, this, nullptr);
}
void MainWindow::launchComposition() {
    const QImage previous=pendingMediaFade && fadeDurationMs>0?captureOutput():QImage();
    pendingMediaFade=false;cancelOutputFade();
    outputScene->clear();
    for (auto *base : canvas->items(Qt::AscendingOrder)) {
        auto *source = dynamic_cast<CanvasItem *>(base);
        if (!source) continue;
        if(!source->mediaId.isEmpty()) {
            bool present=false;for(const auto &media:outputMedia)if(media && media->id==source->mediaId)present=true;
            if(!present)continue;
        }
        auto *copy = source->isShape() ? new CanvasItem(source->shapeType()) : source->isImage() ? new CanvasItem(source->image) : new CanvasItem(source->text);
        copy->id = source->id;
        copy->mediaId = source->mediaId;
        copy->name = source->name;
        copy->setTextFont(source->font); copy->color = source->color;
        copy->setTimerAppearance(source->timerOverdue(), source->timerHidden());
        copy->setVisible(source->mediaId.isEmpty()?source->isVisible():true);
        copy->setItemSize(source->itemSize()); copy->setPos(source->pos()); copy->setZValue(source->zValue());
        copy->setFlags(QGraphicsItem::GraphicsItemFlags());
        outputScene->addItem(copy);
    }
    balanceMediaLayers();startOutputFade(previous);
}
void MainWindow::setFadeDuration(int milliseconds) {fadeDurationMs=qBound(0,milliseconds,10000);}
void MainWindow::showFadeOptions() {
    RaditOptionsDialog dialog(this);dialog.setObjectName("FadeOptions");dialog.setWindowTitle("Tiempo de fundido");
    auto *root=new QVBoxLayout(&dialog);root->setContentsMargins(1,1,1,1);root->setSpacing(0);
    auto *bar=new QFrame;bar->setObjectName("FadeTitleBar");bar->setFixedHeight(25);
    auto *barLayout=new QHBoxLayout(bar);barLayout->setContentsMargins(7,0,5,0);
    auto *title=new QLabel(dialog.windowTitle());title->setObjectName("PanelTitle");title->setAttribute(Qt::WA_TransparentForMouseEvents);barLayout->addWidget(title,1);
    auto *close=new QPushButton(QIcon(":/icons/Close-hover.svg"),QString());close->setObjectName("FadeClose");close->setFixedSize(15,15);close->setIconSize(QSize(15,15));close->setToolTip("Cerrar");close->setAccessibleName("Cerrar");barLayout->addWidget(close);
    connect(close,&QPushButton::clicked,&dialog,&QDialog::reject);root->addWidget(bar);
    auto *content=new QWidget;content->setMinimumWidth(418);auto *layout=new QVBoxLayout(content);layout->setContentsMargins(12,10,12,10);layout->setSpacing(10);root->addWidget(content);
    auto *group=new QFrame;group->setObjectName("FadeSettingsFrame");auto *groupLayout=new QVBoxLayout(group);groupLayout->setContentsMargins(10,8,10,9);groupLayout->setSpacing(9);
    auto *heading=new QLabel("Transiciones de salida");heading->setObjectName("FadeHeading");groupLayout->addWidget(heading);
    auto *form=new QFormLayout;form->setHorizontalSpacing(12);form->setVerticalSpacing(8);
    auto *seconds=new QDoubleSpinBox;seconds->setObjectName("fadeSeconds");seconds->setRange(0,10);seconds->setDecimals(2);seconds->setSingleStep(0.1);seconds->setSuffix(" s");seconds->setValue(fadeDurationMs/1000.0);
    seconds->setButtonSymbols(QAbstractSpinBox::NoButtons);
    form->addRow("Duración:",seconds);groupLayout->addLayout(form);
    auto *hint=new QLabel("Fundido al cambiar de vídeo o imagen y al pulsar Stop.\n0 s aplica el cambio inmediato.");hint->setWordWrap(true);groupLayout->addWidget(hint);layout->addWidget(group);
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);layout->addWidget(buttons);
    buttons->button(QDialogButtonBox::Ok)->setText("Aceptar");buttons->button(QDialogButtonBox::Cancel)->setText("Cancelar");
    dialog.setMinimumWidth(420);root->setSizeConstraint(QLayout::SetFixedSize);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()==QDialog::Accepted){setFadeDuration(qRound(seconds->value()*1000));QSettings().setValue("output/fadeDurationMs",fadeDurationMs);}
}
QImage MainWindow::captureOutput() const {
    QImage image(outputScene->sceneRect().size().toSize(),QImage::Format_RGB32);image.fill(Qt::black);
    QPainter painter(&image);painter.setRenderHints(QPainter::Antialiasing|QPainter::SmoothPixmapTransform);
    outputScene->render(&painter);return image;
}
void MainWindow::cancelOutputFade() {
    fadeAnimation->stop();delete fadeOverlay;fadeOverlay=nullptr;
}
void MainWindow::startOutputFade(const QImage &previous) {
    cancelOutputFade();
    if(previous.isNull() || fadeDurationMs==0)return;
    fadeOverlay=outputScene->addPixmap(QPixmap::fromImage(previous));
    fadeOverlay->setPos(outputScene->sceneRect().topLeft());fadeOverlay->setZValue(1e9);
    fadeOverlay->setData(0,"outputFadeOverlay");
    fadeAnimation->setDuration(fadeDurationMs);fadeAnimation->start();
}
void MainWindow::updateScreens() {
    auto screens = QGuiApplication::screens();
    if (!outputScreen || !screens.contains(outputScreen.data())) {
        outputScreen = QGuiApplication::primaryScreen();
        for (auto *screen : screens) if (screen != this->screen()) { outputScreen = screen; break; }
    }
    outputPreview->setToolTip("Botón derecho: seleccionar pantalla, mostrar o cerrar la salida exterior");
}
void MainWindow::showOutputMenu(QPoint pos) {
    updateScreens();
    QMenu menu(this);
    auto *displays = menu.addMenu("Seleccionar pantalla");
    displays->setObjectName("outputDisplays");
    auto *group = new QActionGroup(&menu);
    group->setExclusive(true);
    const auto screens = QGuiApplication::screens();
    for (auto *screen : screens) {
        auto *action = displays->addAction(QString("%1 · %2 × %3%4").arg(screen->name()).arg(screen->size().width()).arg(screen->size().height())
            .arg(screen == QGuiApplication::primaryScreen() ? " (principal)" : ""));
        action->setCheckable(true);
        action->setChecked(screen == outputScreen);
        group->addAction(action);
        QPointer<QScreen> target(screen);
        connect(action, &QAction::triggered, this, [this, target] {
            if (!target) return;
            outputScreen = target;
            if (externalOutput->isVisible()) showExternalOutput(target);
        });
    }
    menu.addSeparator();
    auto *show = menu.addAction("Mostrar en pantalla exterior");
    show->setIcon(QIcon(":/icons/Play.svg"));
    show->setEnabled(screens.size() > 1 && outputScreen);
    if (screens.size() < 2) {
        auto *hint = menu.addAction("Conecta una segunda pantalla para activar la salida");
        hint->setEnabled(false);
    }
    auto *hide = menu.addAction("Cerrar salida exterior");
    hide->setIcon(QIcon(":/icons/Close.svg"));
    hide->setEnabled(externalOutput->isVisible());
    auto *chosen = menu.exec(outputPreview->mapToGlobal(pos));
    if (chosen == show) showExternalOutput(outputScreen);
    else if (chosen == hide) externalOutput->hide();
}
bool MainWindow::showExternalOutput(QScreen *screen) {
    if (!screen || !QGuiApplication::screens().contains(screen)) return false;
    externalOutput->hide();
    externalOutput->winId();
    externalOutput->windowHandle()->setScreen(screen);
    externalOutput->setGeometry(screen->geometry());
    externalOutput->showFullScreen();
    externalOutput->setFocus();
    return true;
}
void MainWindow::refresh() {
    sources->synchronize();
}
Countdown *MainWindow::createCountdown(const QString &name, int seconds, const QString &textId, bool updateText, const QString &format, const QString &totalTextId) {
    auto *countdown = new Countdown;
    countdown->setName(name);
    countdown->setCompositionScene(canvas);
    countdown->setOutputFormat(format,false);
    CanvasItem *target = nullptr;
    for (auto *base : canvas->items()) if (auto *item = dynamic_cast<CanvasItem *>(base); item && item->id == textId && item->isText()) target = item;
    countdown->configure(seconds, target, updateText);
    for(auto *base:canvas->items())if(auto *item=dynamic_cast<CanvasItem *>(base);item && item->id==totalTextId)countdown->setTotalText(item,updateText);
    connect(countdown, &Countdown::configurationChanged, this, &MainWindow::changed);
    connect(countdown, &Countdown::textUpdated, this, [this](CanvasItem *source) {
        for (auto *base : outputScene->items()) if (auto *item = dynamic_cast<CanvasItem *>(base); item && item->id == source->id) {
            item->setText(source->text);
            item->setTimerAppearance(source->timerOverdue(), source->timerHidden());
        }
        changed();
    });
    bottom->addWidget(countdown);
    return countdown;
}
CanvasItem *MainWindow::prepareVideo(Multimedia *media,const QImage &frame) {
    if(frame.isNull())return nullptr;
    for(auto *base:canvas->items())if(auto *item=dynamic_cast<CanvasItem *>(base);item && item->mediaId==media->id){item->setVideoFrame(frame);item->setVisible(previewMedia==media);return item;}
    auto *item=new CanvasItem(frame);item->mediaId=media->id;item->name=media->name();item->setItemSize(canvas->sceneRect().size());
    qreal lowest=0;for(auto *base:canvas->items())lowest=qMin(lowest,base->zValue());item->setZValue(lowest-1);
    item->setVisible(previewMedia==media);
    return addItem(item,canvas->sceneRect().topLeft());
}
Multimedia *MainWindow::createMultimedia(const QString &name,const QString &file,const QString &id,bool loop) {
    auto *media=new Multimedia;if(!id.isEmpty())media->id=id;media->configure(name,file);if(loop)media->setLoop(true);bottom->addWidget(media);
    auto selectPreview=[this,media]{
        previewMedia=media;
        for(auto *base:canvas->items())if(auto *item=dynamic_cast<CanvasItem *>(base);item && !item->mediaId.isEmpty())item->setVisible(item->mediaId==media->id && !media->currentFrame().isNull());
    };
    auto hideCompositionVideo=[this,media]{
        if(previewMedia==media)previewMedia.clear();
        for(auto *base:canvas->items())if(auto *item=dynamic_cast<CanvasItem *>(base);item && item->mediaId==media->id)item->setVisible(false);
    };
    connect(media,&Multimedia::playbackStopped,this,hideCompositionVideo);
    connect(media,&Multimedia::cueRequested,this,[this,media,selectPreview]{outputMedia.removeAll(media);if(activeMedia==media){activeMedia.clear();pendingMediaFade=false;}selectPreview();});
    connect(media,&Multimedia::playbackStopped,this,[this,media]{
        bool present=false;
        for(auto *base:outputScene->items())if(auto *item=dynamic_cast<CanvasItem *>(base);item && item->mediaId==media->id)present=true;
        const QImage previous=present && fadeDurationMs>0?captureOutput():QImage();
        if(activeMedia==media){activeMedia.clear();pendingMediaFade=false;}
        outputMedia.removeAll(media);
        for(auto *base:outputScene->items())if(auto *item=dynamic_cast<CanvasItem *>(base);item && item->mediaId==media->id)delete item;
        balanceMediaLayers();if(present)startOutputFade(previous);
    });
    connect(media,&Multimedia::playRequested,this,[this,media]{launchMedia(media,media->mixes());});
    connect(media,&Multimedia::frameReady,this,[this,media](const QImage &frame){
        auto *source=prepareVideo(media,frame);if(!source || !outputMedia.contains(media))return;
        if(pendingMediaFade){launchComposition();return;}
        bool found=false;
        for(auto *base:outputScene->items())if(auto *item=dynamic_cast<CanvasItem *>(base);item && item->mediaId==media->id){item->setVideoFrame(frame);found=true;}
        if(!found)launchComposition();
    });
    connect(media,&Multimedia::playbackFinished,this,[this,media]{
        if(media->isCue() || !media->isFinished())return;
        const auto ordered=bottom->widgetsInOrder();const int index=ordered.indexOf(media);
        QPointer<Multimedia> next;
        for(int i=index+1;index>=0 && i<ordered.size();++i)if(auto *candidate=qobject_cast<Multimedia *>(ordered[i])) {if(candidate->playsNext())next=candidate;break;}
        if(next)next->play();
        if(media->purges()) {
            outputMedia.removeAll(media);if(activeMedia==media)activeMedia.clear();if(previewMedia==media)previewMedia.clear();
            bottom->removeWidget(media);media->deleteLater();changed();
        }
    },Qt::QueuedConnection);
    connect(media,&Multimedia::configurationChanged,this,&MainWindow::changed);return media;
}
void MainWindow::launchMedia(Multimedia *media,bool mix) {
    outputMedia.removeAll(QPointer<Multimedia>());
    if(!mix) {
        if(activeMedia!=media || outputMedia.size()>1)pendingMediaFade=true;
        for(const auto &other:outputMedia)if(other && other!=media)other->pause();
        outputMedia.clear();
    }
    if(!outputMedia.contains(media))outputMedia.append(media);
    activeMedia=media;previewMedia=media;
    for(auto *base:canvas->items())if(auto *item=dynamic_cast<CanvasItem *>(base);item && !item->mediaId.isEmpty())item->setVisible(item->mediaId==media->id && !media->currentFrame().isNull());
    if(media->hasCurrentFrame() && prepareVideo(media,media->currentFrame()))launchComposition();
}
void MainWindow::balanceMediaLayers() {
    qreal highest=-1;for(auto *base:outputScene->items())if(auto *item=dynamic_cast<CanvasItem *>(base);item && !item->mediaId.isEmpty())highest=qMax(highest,item->zValue());
    int count=0;
    for(const auto &media:outputMedia)if(media)for(auto *base:outputScene->items())if(auto *item=dynamic_cast<CanvasItem *>(base);item && item->mediaId==media->id) {
        item->setZValue(highest-outputMedia.size()+count);item->setOpacity(1.0/(++count));
    }
}
void MainWindow::changed(){dirty=true;setWindowModified(true);refresh();}
QString MainWindow::suggestedElementName(const QString &base) const {
    QSet<QString> names;
    for(auto *entry:canvas->items())if(auto *item=dynamic_cast<CanvasItem *>(entry))names.insert(item->name.toCaseFolded());
    for(auto *countdown:findChildren<Countdown *>())names.insert(countdown->name().toCaseFolded());
    for(auto *media:findChildren<Multimedia *>())names.insert(media->name().toCaseFolded());
    int number=1;while(names.contains((base+QString::number(number)).toCaseFolded()))++number;
    return base+QString::number(number);
}
CanvasItem *MainWindow::addItem(CanvasItem *item,QPointF pos){
    canvas->clearSelection(); canvas->addItem(item);item->setPos(pos);
    connect(item,&CanvasItem::edited,this,&MainWindow::changed);
    connect(item,&CanvasItem::editRequested,this,&MainWindow::editText);
    item->setSelected(true);changed();return item;
}
void MainWindow::removeSelection(){ auto items=canvas->selectedItems();if(items.isEmpty())return;for(auto *item:items)delete item;changed(); }
namespace {
CanvasItem *clipboardClone(const CanvasItem *source,bool preserveId) {
    auto *copy=source->isShape()?new CanvasItem(source->shapeType()):source->isImage()?new CanvasItem(source->image):new CanvasItem(source->text);
    if(preserveId)copy->id=source->id;
    copy->name=source->name;copy->color=source->color;copy->setTextFont(source->font);
    copy->setItemSize(source->itemSize());copy->setPos(source->pos());copy->setZValue(source->zValue());
    copy->setVisible(source->isVisible());copy->setLocked(source->isLocked());return copy;
}
}
QList<CanvasItem *> MainWindow::selectedCanvasItems() const {
    QList<CanvasItem *> items;
    for(auto *base:canvas->items(Qt::AscendingOrder))if(auto *item=dynamic_cast<CanvasItem *>(base);item && item->isSelected() && item->mediaId.isEmpty())items.append(item);
    return items;
}
void MainWindow::addClipboardActions(QMenu *menu,const QList<CanvasItem *> &items,const QPointF &position,bool usePosition) {
    QList<QPointer<CanvasItem>> live;
    for(auto *item:items)if(item && item->scene()==canvas && item->mediaId.isEmpty())live.append(item);
    auto *copy=menu->addAction(QIcon(":/icons/ActionCopy.svg"),"Copiar\tCtrl+C");copy->setObjectName("itemCopy");copy->setEnabled(!live.isEmpty());
    auto *cut=menu->addAction(QIcon(":/icons/ActionCut.svg"),"Cortar\tCtrl+X");cut->setObjectName("itemCut");cut->setEnabled(!live.isEmpty());
    auto *paste=menu->addAction(QIcon(":/icons/ActionPaste.svg"),"Pegar\tCtrl+V");paste->setObjectName("itemPaste");paste->setEnabled(!itemClipboard.isEmpty());
    auto performCopy=[this,live](bool cut){QList<CanvasItem *> items;for(const auto &item:live)if(item)items.append(item.data());copyItems(items,cut);};
    connect(copy,&QAction::triggered,this,[performCopy]{performCopy(false);});connect(cut,&QAction::triggered,this,[performCopy]{performCopy(true);});
    connect(paste,&QAction::triggered,this,[this,position,usePosition]{pasteItems(position,usePosition);});
}
void MainWindow::copyItems(const QList<CanvasItem *> &requested,bool cut) {
    QList<CanvasItem *> items;
    for(auto *item:requested)if(item && item->scene()==canvas && item->mediaId.isEmpty() && !items.contains(item))items.append(item);
    if(items.isEmpty())return;
    itemClipboard.clear();cutTextAssociations.clear();cutTotalAssociations.clear();clipboardIsCut=cut;pasteCount=0;
    for(auto *item:items)itemClipboard.append(std::shared_ptr<CanvasItem>(clipboardClone(item,true)));
    if(!cut)return;
    for(auto *countdown:findChildren<Countdown *>())if(auto *text=countdown->associatedText();text && items.contains(text)) {
        cutTextAssociations[text->id].append(countdown);
        countdown->configure(countdown->durationSeconds(),nullptr,false);
    }
    for(auto *countdown:findChildren<Countdown *>())if(auto *text=countdown->totalText();text && items.contains(text)) {
        cutTotalAssociations[text->id].append(countdown);countdown->setTotalText(nullptr,false);
    }
    for(auto *item:items)delete item;
    changed();
}
void MainWindow::pasteItems(const QPointF &position,bool usePosition) {
    if(itemClipboard.isEmpty())return;
    QRectF bounds;for(const auto &snapshot:itemClipboard)bounds=bounds.united(snapshot->mapRectToScene(snapshot->selectionRect()));
    ++pasteCount;const QPointF delta=usePosition?position-bounds.topLeft():clipboardIsCut?QPointF():QPointF(20*pasteCount,20*pasteCount);
    qreal z=0;QSet<QString> ids;for(auto *base:canvas->items())if(auto *item=dynamic_cast<CanvasItem *>(base)){z=qMax(z,item->zValue());ids.insert(item->id);}
    QList<CanvasItem *> pasted;
    for(const auto &snapshot:itemClipboard) {
        const bool preserve=clipboardIsCut && !ids.contains(snapshot->id);
        auto *copy=clipboardClone(snapshot.get(),preserve);if(!preserve)copy->name+=" (copia)";
        copy->setZValue(++z);addItem(copy,snapshot->pos()+delta);ids.insert(copy->id);pasted.append(copy);
        if(preserve && copy->isText())for(const auto &countdown:cutTextAssociations.value(snapshot->id))if(countdown && !countdown->associatedText())countdown->configure(countdown->durationSeconds(),copy);
        if(preserve && copy->isText())for(const auto &countdown:cutTotalAssociations.value(snapshot->id))if(countdown && !countdown->totalText())countdown->setTotalText(copy);
    }
    canvas->clearSelection();for(auto *item:pasted)item->setSelected(true);
    clipboardIsCut=false;cutTextAssociations.clear();cutTotalAssociations.clear();changed();
}
void MainWindow::editText(CanvasItem *item) {
    if (!item) return;
    PropertiesDialog dialog(item, this);
    if (dialog.exec() == QDialog::Accepted) changed();
}
bool MainWindow::writeProject(const QString &path){
    QJsonArray items;
    for(auto *base:canvas->items(Qt::AscendingOrder))if(auto *item=dynamic_cast<CanvasItem *>(base)){
        QJsonObject o{{"id",item->id},{"name",item->name},{"x",item->x()},{"y",item->y()},{"w",item->itemSize().width()},{"h",item->itemSize().height()},{"z",item->zValue()},
                      {"text",item->text},{"font",item->font.toString()},{"color",item->color.name()},{"type",item->typeName()},
                      {"visible",item->isVisible()},{"locked",item->isLocked()},{"mediaId",item->mediaId}};
        if(item->isImage()){QByteArray bytes;QBuffer buffer(&bytes);buffer.open(QIODevice::WriteOnly);if(!item->image.save(&buffer,"PNG"))return false;o["data"]=QString::fromLatin1(bytes.toBase64());}
        items.append(o);
    }
    QJsonArray widgets;
    for (int panel = 0; panel < countdownContainers.size(); ++panel)
        for (auto *base : countdownContainers[panel]->widgetsInOrder()) {
            if (auto *media=qobject_cast<Multimedia *>(base))widgets.append(QJsonObject{{"type","multimedia"},{"id",media->id},{"name",media->name()},{"file",media->filePath()},{"loop",media->loops()},{"purge",media->purges()},{"next",media->playsNext()},{"mix",media->mixes()},{"panel",0}});
            else if (auto *countdown = qobject_cast<Countdown *>(base))
            widgets.append(QJsonObject{{"type", "countdown"}, {"name", countdown->name()}, {"seconds", countdown->durationSeconds()}, {"panel",panel},
                {"textId", countdown->associatedText() ? countdown->associatedText()->id : QString()},{"format",countdown->outputFormat()},
                {"totalTextId",countdown->totalText()?countdown->totalText()->id:QString()}});
        }
    QSaveFile file(path);if(!file.open(QIODevice::WriteOnly))return false;
    auto bytes=QJsonDocument(QJsonObject{{"format","sessions"},{"version",1},{"items",items},{"widgets",widgets}}).toJson();
    if(file.write(bytes)!=bytes.size() || !file.commit())return false;
    filename=path;dirty=false;setWindowModified(false);setWindowTitle("Sessions — "+QFileInfo(path).fileName()+"[*]");return true;
}
bool MainWindow::readProject(const QString &path){
    QFile file(path);if(!file.open(QIODevice::ReadOnly) || file.size()>128*1024*1024)return false;
    QJsonParseError error;auto doc=QJsonDocument::fromJson(file.readAll(),&error);auto root=doc.object();
    if(error.error!=QJsonParseError::NoError || root["format"]!="sessions" || root["version"].toInt()!=1 || !root["items"].isArray())return false;
    QJsonArray countdownData;
    if (root.contains("widgets")) {
        if (!root["widgets"].isArray()) return false;
        for (auto value : root["widgets"].toArray()) {
            auto widget = value.toObject();
            if ((widget["type"] != "countdown" && widget["type"] != "multimedia") || !widget["name"].isString() || widget["name"].toString().trimmed().isEmpty()) return false;
            if(widget["type"]=="multimedia" && !widget["file"].isString())return false;
            if(widget.contains("loop") && !widget["loop"].isBool())return false;
            for(auto key:{"purge","next","mix"})if(widget.contains(key) && !widget[key].isBool())return false;
            if(widget.contains("totalTextId") && !widget["totalTextId"].isString())return false;
            if(!widget["totalTextId"].toString().isEmpty() && widget["totalTextId"]==widget["textId"])return false;
            if(widget.contains("id") && (!widget["id"].isString() || widget["id"].toString().isEmpty()))return false;
            if (widget.contains("seconds") && (!widget["seconds"].isDouble() || widget["seconds"].toDouble() != widget["seconds"].toInt() || widget["seconds"].toInt() < 1 || widget["seconds"].toInt() > 86400)) return false;
            if (widget.contains("textId") && !widget["textId"].isString()) return false;
            if(widget.contains("format") && (!widget["format"].isString() || !Countdown::outputFormats().contains(widget["format"].toString().toUpper())))return false;
            if (widget.contains("panel") && (!widget["panel"].isDouble() || widget["panel"].toDouble() != widget["panel"].toInt() || widget["panel"].toInt() < 0 || widget["panel"].toInt() > 2)) return false;
            countdownData.append(widget);
        }
    }
    QList<CanvasItem *> pending;
    auto fail=[&pending]{qDeleteAll(pending);return false;};
    QSet<QString> ids;
    for(auto value:root["items"].toArray()){
        auto o=value.toObject();
        for(auto key:{"x","y","w","h","z"})if(!o[key].isDouble() || !std::isfinite(o[key].toDouble()) || std::abs(o[key].toDouble())>16000)return fail();
        const double minimum=o["type"]=="line"?1:24;
        if(o["w"].toDouble()<minimum || o["h"].toDouble()<minimum)return fail();
        CanvasItem *item=nullptr;
        if(o["type"]=="image"){QImage image;image.loadFromData(QByteArray::fromBase64(o["data"].toString().toLatin1()),"PNG");if(image.isNull())return fail();item=new CanvasItem(image);}
        else if(o["type"]=="text")item=new CanvasItem(o["text"].toString());
        else if(o["type"]=="circle")item=new CanvasItem(CanvasItem::Shape::Circle);
        else if(o["type"]=="square")item=new CanvasItem(CanvasItem::Shape::Square);
        else if(o["type"]=="rectangle")item=new CanvasItem(CanvasItem::Shape::Rectangle);
        else if(o["type"]=="line")item=new CanvasItem(CanvasItem::Shape::Line);
        else return fail();
        pending.append(item);item->setPos(o["x"].toDouble(),o["y"].toDouble());item->setItemSize({o["w"].toDouble(),o["h"].toDouble()});item->setZValue(o["z"].toDouble());
        if(o.contains("mediaId") && !o["mediaId"].isString())return fail();
        item->mediaId=o["mediaId"].toString();
        for (auto key : {"visible","locked"}) if(o.contains(key) && !o[key].isBool())return fail();
        item->setVisible(item->mediaId.isEmpty() && o["visible"].toBool(true)); item->setLocked(o["locked"].toBool(false));
        if (o.contains("name")) {
            if (!o["name"].isString() || o["name"].toString().trimmed().isEmpty()) return fail();
            item->name = o["name"].toString().trimmed();
        }
        if (o.contains("id")) {
            if (!o["id"].isString() || o["id"].toString().isEmpty()) return fail();
            item->id = o["id"].toString();
        }
        if (ids.contains(item->id)) return fail();
        ids.insert(item->id);
        QFont loadedFont = item->font; loadedFont.fromString(o["font"].toString()); item->setTextFont(loadedFont);
        QColor c(o["color"].toString());if(c.isValid())item->color=c;
    }
    for (auto value : countdownData) {
        for(auto key:{"textId","totalTextId"}) {
        QString textId = value.toObject()[key].toString();
        if (textId.isEmpty()) continue;
        bool found = false;
        for (auto *item : pending) if (item->isText() && item->id == textId) found = true;
        if (!found) return fail();
        }
    }
    for (auto *container : countdownContainers) container->clearWidgets();
    canvas->clear();for(auto *item:pending)addItem(item,item->pos());canvas->clearSelection();
    for (auto value : countdownData) {
        auto widget = value.toObject();
        if(widget["type"]=="multimedia") {
            QString file=widget["file"].toString();if(!file.isEmpty() && QFileInfo(file).isRelative())file=QFileInfo(path).dir().absoluteFilePath(file);
            auto *media=createMultimedia(widget["name"].toString(),file,widget["id"].toString(),widget["loop"].toBool(false));
            media->setPurge(widget["purge"].toBool());media->setNext(widget["next"].toBool());media->setMix(widget["mix"].toBool());
        } else createCountdown(widget["name"].toString(), widget["seconds"].toInt(60), widget["textId"].toString(),true,widget["format"].toString("MM:SS.C"),widget["totalTextId"].toString());
    }
    filename=path;dirty=false;setWindowModified(false);setWindowTitle("Sessions — "+QFileInfo(path).fileName()+"[*]");refresh();return true;
}
bool MainWindow::save(bool saveAs){
    QString path=filename;if(saveAs || path.isEmpty())path=QFileDialog::getSaveFileName(this,"Guardar diseño",filename,"Diseño Sessions (*.sessions)");
    if(path.isEmpty())return false;
    if(!path.endsWith(".sessions",Qt::CaseInsensitive))path+=".sessions";
    if(writeProject(path))return true;
    QMessageBox::warning(this,"Guardar","No se ha podido guardar el diseño.");return false;
}
bool MainWindow::canDiscard(){
    if(!dirty)return true;
    auto answer=QMessageBox::question(this,"Cambios sin guardar","¿Quieres guardar los cambios del diseño?",QMessageBox::Save|QMessageBox::Discard|QMessageBox::Cancel);
    return answer==QMessageBox::Discard || (answer==QMessageBox::Save && save());
}
void MainWindow::closeEvent(QCloseEvent *e) {
    if (canDiscard()) {
        externalOutput->hide();
        e->accept();
    } else e->ignore();
}
