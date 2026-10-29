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
            connect(sources,&SourcePanel::addRequested,this,[this](const QString &type) {
                const QPointF position(80,80);
                if(type=="text")emit view->addTextRequested(position);
                else if(type=="image")emit view->addImageRequested(position);
                else emit view->addShapeRequested(type=="circle"?CanvasItem::Shape::Circle:type=="square"?CanvasItem::Shape::Square:CanvasItem::Shape::Rectangle,position);
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
            QString name = QInputDialog::getText(this, "Add Countdown", "Nombre:", QLineEdit::Normal, "Countdown", &ok).trimmed();
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
        bool ok;QString name=QInputDialog::getText(this,"Add Multimedia","Nombre:",QLineEdit::Normal,"Multimedia",&ok).trimmed();
        if(!ok || name.isEmpty())return;
        createMultimedia(name);changed();
    });
    connect(bottom, &WidgetContainer::contentsChanged, this, &MainWindow::changed);
    connect(bottom,&WidgetContainer::pasteMultimediaRequested,this,[this](const QString &name,const QString &file,int index){
        auto *media=createMultimedia(name,file);bottom->moveWidget(media,index);bottom->selectAction(media);changed();
    });
    connect(bottom, &WidgetContainer::pasteCountdownRequested, this, [this](const QString &name, int seconds, const QString &textId, int index) {
        auto *countdown = createCountdown(name,seconds,textId,false);
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
        auto *name = new QLineEdit; name->setObjectName("elementName");
        auto *text = new QPlainTextEdit("Nuevo texto"); text->setObjectName("elementText");
        form->addRow("Nombre:", name); form->addRow("Texto:", text); layout->addLayout(form);
        auto *buttons = new QDialogButtonBox;
        auto *accept = buttons->addButton("Añadir", QDialogButtonBox::AcceptRole);
        buttons->addButton("Cancelar", QDialogButtonBox::RejectRole); accept->setEnabled(false);
        auto validate = [=] { accept->setEnabled(!name->text().trimmed().isEmpty() && !text->toPlainText().isEmpty()); };
        connect(name, &QLineEdit::textChanged, &dialog, validate); connect(text, &QPlainTextEdit::textChanged, &dialog, validate);
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
        QString name = QInputDialog::getText(this,"Añadir imagen","Nombre:",QLineEdit::Normal,QFileInfo(path).completeBaseName(),&ok).trimmed();
        if (!ok || name.isEmpty()) return;
        auto *item = new CanvasItem(image); item->name = name; addItem(item,p);
    };
    action(insert,"Añadir texto…",QKeySequence("Ctrl+T"),[this,addText]{addText({80,80});});
    action(insert,"Añadir imagen…",QKeySequence("Ctrl+I"),[this,addImage]{addImage({80,80});});
    auto addShape = [this](CanvasItem::Shape shape, QPointF pos) {
        QString label = shape == CanvasItem::Shape::Circle ? "Círculo" : shape == CanvasItem::Shape::Square ? "Cuadrado" : "Rectángulo";
        bool ok;
        QString name = QInputDialog::getText(this, "Añadir " + label.toLower(), "Nombre:", QLineEdit::Normal, label, &ok).trimmed();
        if (!ok || name.isEmpty()) return;
        auto *item = new CanvasItem(shape); item->name = name; addItem(item,pos);
    };
    insert->addSeparator();
    action(insert,"Añadir círculo…",{},[addShape]{addShape(CanvasItem::Shape::Circle,{80,80});});
    action(insert,"Añadir cuadrado…",{},[addShape]{addShape(CanvasItem::Shape::Square,{80,80});});
    action(insert,"Añadir rectángulo…",{},[addShape]{addShape(CanvasItem::Shape::Rectangle,{80,80});});
    connect(view, &CanvasView::addShapeRequested, this, addShape);
    connect(view,&CanvasView::addTextRequested,this,addText); connect(view,&CanvasView::addImageRequested,this,addImage);
    connect(view,&CanvasView::deleteRequested,this,&MainWindow::removeSelection);
    connect(view,&CanvasView::editRequested,this,&MainWindow::editText);
    connect(view, &CanvasView::bringToFrontRequested, this, [this](CanvasItem *item) {
        qreal z = 0;
        for (auto *other : canvas->items()) z = qMax(z, other->zValue());
        item->setZValue(z + 1);
        changed();
    });
    action(visual,"Ajustar lienzo",QKeySequence("Ctrl+0"),[this]{view->fitCanvas();});
    action(visual,"Tamaño real",QKeySequence("Ctrl+1"),[this]{view->resetTransform();});
    auto *help=menuBar()->addMenu("A&yuda");
    auto *about=help->addAction(QIcon(":/icons/Info.svg"),"Acerca de Sessions…");about->setObjectName("aboutSessions");
    connect(about,&QAction::triggered,this,[this]{(new AboutDialog(this))->show();});
    statusBar()->showMessage("Listo — botón derecho sobre el lienzo para empezar");
    refresh();
    QTimer::singleShot(0,view,&CanvasView::fitCanvas);
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
        if(!source->mediaId.isEmpty() && (!activeMedia || activeMedia->id!=source->mediaId))continue;
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
    startOutputFade(previous);
    statusBar()->showMessage("Composición enviada a salida — los siguientes cambios se enviarán al volver a lanzar");
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
    statusBar()->showMessage(QString("%1 elemento(s) — botón derecho para añadir o abrir propiedades").arg(canvas->items().size()));
}
Countdown *MainWindow::createCountdown(const QString &name, int seconds, const QString &textId, bool updateText) {
    auto *countdown = new Countdown;
    countdown->setName(name);
    countdown->setCompositionScene(canvas);
    CanvasItem *target = nullptr;
    for (auto *base : canvas->items()) if (auto *item = dynamic_cast<CanvasItem *>(base); item && item->id == textId && item->isText()) target = item;
    countdown->configure(seconds, target, updateText);
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
Multimedia *MainWindow::createMultimedia(const QString &name,const QString &file,const QString &id) {
    auto *media=new Multimedia;if(!id.isEmpty())media->id=id;media->configure(name,file);bottom->addWidget(media);
    auto selectPreview=[this,media]{
        previewMedia=media;
        for(auto *base:canvas->items())if(auto *item=dynamic_cast<CanvasItem *>(base);item && !item->mediaId.isEmpty())item->setVisible(item->mediaId==media->id && !media->currentFrame().isNull());
    };
    auto hideCompositionVideo=[this,media]{
        if(previewMedia==media)previewMedia.clear();
        for(auto *base:canvas->items())if(auto *item=dynamic_cast<CanvasItem *>(base);item && item->mediaId==media->id)item->setVisible(false);
    };
    connect(media,&Multimedia::playbackStopped,this,hideCompositionVideo);
    connect(media,&Multimedia::cueRequested,this,[this,media,selectPreview]{if(activeMedia==media){activeMedia.clear();pendingMediaFade=false;}selectPreview();});
    connect(media,&Multimedia::playbackStopped,this,[this,media]{
        bool present=false;
        for(auto *base:outputScene->items())if(auto *item=dynamic_cast<CanvasItem *>(base);item && item->mediaId==media->id)present=true;
        const QImage previous=present && fadeDurationMs>0?captureOutput():QImage();
        if(activeMedia==media){activeMedia.clear();pendingMediaFade=false;}
        for(auto *base:outputScene->items())if(auto *item=dynamic_cast<CanvasItem *>(base);item && item->mediaId==media->id)delete item;
        if(present)startOutputFade(previous);
    });
    connect(media,&Multimedia::playRequested,this,[this,media,selectPreview]{
        if(activeMedia!=media)pendingMediaFade=true;
        if(activeMedia && activeMedia!=media)activeMedia->pause();
        activeMedia=media;
        selectPreview();
        // A restarted video may still cache its previous final frame. Wait for
        // a fresh decoded frame before replacing the current output.
        if(media->hasCurrentFrame() && prepareVideo(media,media->currentFrame()))launchComposition();
    });
    connect(media,&Multimedia::frameReady,this,[this,media](const QImage &frame){
        auto *source=prepareVideo(media,frame);if(!source || activeMedia!=media)return;
        if(pendingMediaFade){launchComposition();return;}
        bool found=false;
        for(auto *base:outputScene->items())if(auto *item=dynamic_cast<CanvasItem *>(base);item && item->mediaId==media->id){item->setVideoFrame(frame);found=true;}
        if(!found)launchComposition();
    });
    connect(media,&Multimedia::configurationChanged,this,&MainWindow::changed);return media;
}
void MainWindow::changed(){dirty=true;setWindowModified(true);refresh();}
CanvasItem *MainWindow::addItem(CanvasItem *item,QPointF pos){
    canvas->clearSelection(); canvas->addItem(item);item->setPos(pos);
    connect(item,&CanvasItem::edited,this,&MainWindow::changed);
    connect(item,&CanvasItem::editRequested,this,&MainWindow::editText);
    item->setSelected(true);changed();return item;
}
void MainWindow::removeSelection(){ auto items=canvas->selectedItems();if(items.isEmpty())return;for(auto *item:items)delete item;changed(); }
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
            if (auto *media=qobject_cast<Multimedia *>(base))widgets.append(QJsonObject{{"type","multimedia"},{"id",media->id},{"name",media->name()},{"file",media->filePath()},{"panel",0}});
            else if (auto *countdown = qobject_cast<Countdown *>(base))
            widgets.append(QJsonObject{{"type", "countdown"}, {"name", countdown->name()}, {"seconds", countdown->durationSeconds()}, {"panel",panel},
                {"textId", countdown->associatedText() ? countdown->associatedText()->id : QString()}});
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
            if(widget.contains("id") && (!widget["id"].isString() || widget["id"].toString().isEmpty()))return false;
            if (widget.contains("seconds") && (!widget["seconds"].isDouble() || widget["seconds"].toDouble() != widget["seconds"].toInt() || widget["seconds"].toInt() < 1 || widget["seconds"].toInt() > 86400)) return false;
            if (widget.contains("textId") && !widget["textId"].isString()) return false;
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
        if(o["w"].toDouble()<24 || o["h"].toDouble()<24)return fail();
        CanvasItem *item=nullptr;
        if(o["type"]=="image"){QImage image;image.loadFromData(QByteArray::fromBase64(o["data"].toString().toLatin1()),"PNG");if(image.isNull())return fail();item=new CanvasItem(image);}
        else if(o["type"]=="text")item=new CanvasItem(o["text"].toString());
        else if(o["type"]=="circle")item=new CanvasItem(CanvasItem::Shape::Circle);
        else if(o["type"]=="square")item=new CanvasItem(CanvasItem::Shape::Square);
        else if(o["type"]=="rectangle")item=new CanvasItem(CanvasItem::Shape::Rectangle);
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
        QString textId = value.toObject()["textId"].toString();
        if (textId.isEmpty()) continue;
        bool found = false;
        for (auto *item : pending) if (item->isText() && item->id == textId) found = true;
        if (!found) return fail();
    }
    for (auto *container : countdownContainers) container->clearWidgets();
    canvas->clear();for(auto *item:pending)addItem(item,item->pos());canvas->clearSelection();
    for (auto value : countdownData) {
        auto widget = value.toObject();
        if(widget["type"]=="multimedia") {
            QString file=widget["file"].toString();if(!file.isEmpty() && QFileInfo(file).isRelative())file=QFileInfo(path).dir().absoluteFilePath(file);
            createMultimedia(widget["name"].toString(),file,widget["id"].toString());
        } else createCountdown(widget["name"].toString(), widget["seconds"].toInt(60), widget["textId"].toString());
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
