#include "mainwindow.h"
#include "widgetcontainer.h"
#include "propertiesdialog.h"
#include "outputview.h"
#include "countdown.h"
#include "sourcepanel.h"
#include "multimedia.h"
#include "aboutdialog.h"
#include <QtWidgets>
#include <QtTest/QTest>
#include <cstdio>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

int smokeTest(QApplication &app) {
    int failures=0;
    auto check=[&](bool ok,const char *label){std::printf("%s %s\n",ok?"PASS":"FAIL",label);std::fflush(stdout);if(!ok)++failures;};
    MainWindow w;w.setFadeDuration(0);w.show();app.processEvents();w.view->fitCanvas();app.processEvents();
    check(!app.windowIcon().pixmap(32,32).isNull() && !w.windowIcon().isNull(),"Sessions application icon is available to windows");
    check(!QIcon(":/icons/Play.svg").pixmap(16,16).isNull() && !QIcon(":/icons/ActionCopy.svg").pixmap(16,16).isNull(),"original Radit SVG icons render through deployed icon engine");
    auto *aboutAction=w.findChild<QAction *>("aboutSessions");
    if(aboutAction)aboutAction->trigger();
    app.processEvents();
    QPointer<AboutDialog> about=w.findChild<AboutDialog *>();
    check(aboutAction && about && about->isVisible() && about->findChild<QLabel *>("AboutName")->text()=="sessions","Help menu opens Sessions about dialog in Radit layout");
    if(about) {
        check(about->grab().save(QApplication::applicationDirPath()+"/../build/about-sessions.png"),"save about dialog preview");
        QTest::keyClick(about,Qt::Key_Escape);app.processEvents();
        check(!about || !about->isVisible(),"about dialog closes with Escape");
    }
    check(w.findChild<QSplitter *>("mainSplitter")->count()==2,"two vertical work areas");
    check(w.findChild<QSplitter *>("topSplitter")->count() == 2, "composition and output share upper area");
    check(w.outputScene != w.canvas && w.outputScene->items().isEmpty(), "output starts empty and independent");
    app.processEvents();
    auto initialOutput = w.outputPreview->grab().toImage();
    check(initialOutput.pixelColor(initialOutput.width()/2, initialOutput.height()/2) == QColor(Qt::black), "output canvas starts black");
    QPoint canvasCenter = w.view->mapFromScene(w.canvas->sceneRect().center());
    check(w.view->viewport()->grab().toImage().pixelColor(canvasCenter) == QColor(Qt::black), "composition canvas starts black");
    auto *bottomPanels = w.findChild<QSplitter *>("bottomSplitter");
    auto *container = bottomPanels->widget(0)->findChild<WidgetContainer *>();
    auto *actionsTitle = bottomPanels->widget(0)->findChild<QLabel *>("ActionsTitle");
    check(actionsTitle && actionsTitle->text() == "Acciones" && !bottomPanels->widget(0)->findChild<QToolButton *>(), "actions panel keeps title without bottom toolbar");
    auto *mainSplit = w.findChild<QSplitter *>("mainSplitter");
    check(bottomPanels && bottomPanels->count() == 3 && bottomPanels->width() == mainSplit->widget(0)->width() && qAbs(bottomPanels->height() - mainSplit->widget(0)->height()) <= 2, "upper and lower areas start with equal height");
    w.resize(1200, 950); app.processEvents();
    check(qAbs(mainSplit->widget(0)->height() - bottomPanels->height()) <= 2, "equal heights are preserved on window resize");
    check(container && container->widgetCount() == 0, "lower container starts empty");
    auto addCountdown = [&](const QString &name, bool accept) {
        QTimer::singleShot(0, [&] {
            if (auto *menu = qobject_cast<QMenu *>(app.activePopupWidget())) {
                QTimer::singleShot(30, [&] {
                    if (auto *dialog = qobject_cast<QInputDialog *>(app.activeModalWidget())) {
                        dialog->setTextValue(name);
                        if (accept) dialog->accept(); else dialog->reject();
                    }
                });
                QTest::mouseClick(menu, Qt::LeftButton, Qt::NoModifier, menu->actionGeometry(menu->actions().first()).center());
            }
        });
        auto *surface = container->widget();
        QContextMenuEvent event(QContextMenuEvent::Mouse, QPoint(10,10), surface->mapToGlobal(QPoint(10,10)));
        QApplication::sendEvent(surface, &event);
    };
    addCountdown("Descartado", false);
    check(container->widgetCount() == 0, "cancel name does not create Countdown");
    addCountdown("  ", true);
    check(container->widgetCount() == 0, "empty name does not create Countdown");
    addCountdown("Inicio del evento", true);
    auto *countdown = w.findChild<Countdown *>("Countdown");
    check(container && countdown && container->widgetCount() == 1 && container->isAncestorOf(countdown), "Countdown is hosted in lower container");
    check(countdown && countdown->name() == "Inicio del evento" && countdown->findChild<QLabel *>("CountdownTitle")->toolTip() == "Inicio del evento", "Countdown retains entered name and shows full title in tooltip");
    auto *timerText = w.addItem(new CanvasItem("Texto del temporizador"), {30,30});
    countdown->configure(3661, timerText);
    check(countdown->outputFormat()=="MM:SS.C" && timerText->text=="61:01.0","Countdown defaults to minutes seconds tenths with total minutes beyond one hour");
    countdown->setOutputFormat("HH:MM:SS.c");
    check(timerText->text == "01:01:01.0", "configuration immediately displays hours minutes seconds and tenths");
    countdown->setOutputFormat("SS.C");check(timerText->text=="3661.0","seconds-only format preserves the complete configured duration");
    check(!countdown->setOutputFormat("invalid") && countdown->outputFormat()=="SS.C","Countdown rejects an unsupported output format");
    countdown->setOutputFormat("HH:MM:SS.C");
    countdown->configure(3, timerText);
    auto *play = countdown->findChild<QPushButton *>("countdownStart");
    auto *pause = countdown->findChild<QPushButton *>("countdownPause");
    auto *stop = countdown->findChild<QPushButton *>("countdownStop");
    auto *dragTitle = countdown->findChild<QLabel *>("CountdownTitle");
    container->selectCountdown(nullptr);
    QTest::mouseClick(dragTitle,Qt::LeftButton);
    check(container->selectedCountdown() == countdown, "title press retains drag source instead of propagating to background");
    check(countdown->acceptDrops() && dragTitle->acceptDrops() && play->acceptDrops(), "Countdown accepts insertion drops over its title body and buttons");
    QTest::mouseClick(play, Qt::LeftButton);
    check(countdown->state() == Countdown::State::Playing && pause->isEnabled() && timerText->text == "00:00:03.0", "Countdown Start writes configured time");
    QTest::mouseClick(pause, Qt::LeftButton);
    check(countdown->state() == Countdown::State::Paused && play->isEnabled(), "Countdown Pause pauses");
    QTest::mouseClick(play, Qt::LeftButton);
    QTest::mouseClick(stop, Qt::LeftButton);
    check(countdown->state() == Countdown::State::Stopped && !pause->isEnabled(), "Countdown Stop resets state");
    bool propertiesOpened = false;
    QTimer::singleShot(0, [&] {
        if (auto *menu = qobject_cast<QMenu *>(app.activePopupWidget())) {
            QTimer::singleShot(30, [&] {
                if (auto *dialog = qobject_cast<QDialog *>(app.activeModalWidget()); dialog && dialog->objectName() == "CountdownProperties") {
                    propertiesOpened = true;
                    dialog->findChild<QSpinBox *>("countdownSeconds")->setValue(2);
                    auto *texts = dialog->findChild<QComboBox *>("countdownText");
                    check(texts->count() == 2, "Countdown properties lists composition texts");
                    check(texts->itemText(1) == timerText->name, "Countdown identifies associated text by persistent name");
                    texts->setCurrentIndex(1);
                    dialog->grab().save(QApplication::applicationDirPath() + "/../build/properties-countdown.png");
                    dialog->accept();
                }
            });
            auto *propertiesAction = menu->findChild<QAction *>("actionProperties");
            if (propertiesAction) QTest::mouseClick(menu, Qt::LeftButton, Qt::NoModifier, menu->actionGeometry(propertiesAction).center());
        }
    });
    QContextMenuEvent countdownMenu(QContextMenuEvent::Mouse, QPoint(20,20), countdown->mapToGlobal(QPoint(20,20)));
    QApplication::sendEvent(countdown, &countdownMenu);
    check(propertiesOpened && countdown->durationSeconds() == 2 && countdown->associatedText() == timerText && timerText->text == "00:00:02.0", "Countdown properties immediately updates associated text");
    QTest::mouseClick(play, Qt::LeftButton);
    QTest::qWait(1250);
    check(countdown->remainingSeconds() == 1 && QRegularExpression("^00:00:00\\.[0-9]$").match(timerText->text).hasMatch(), "Countdown displays changing tenths");
    QTest::mouseClick(pause, Qt::LeftButton);
    const QString pausedTime = timerText->text;
    QTest::qWait(250);
    check(timerText->text == pausedTime && countdown->state() == Countdown::State::Paused, "Countdown pause freezes remaining tenths");
    QTest::qWait(300);
    check(timerText->timerHidden() && timerText->text == pausedTime, "paused text blinks off without changing time");
    QTest::qWait(500);
    check(!timerText->timerHidden(), "paused text blinks on again");
    QTest::mouseClick(play, Qt::LeftButton);
    check(!timerText->timerHidden(), "resume restores visible text immediately");
    QTest::qWait(1100);
    check(QRegularExpression("^00:00:00\\.[1-9]$").match(timerText->text).hasMatch() && countdown->state() == Countdown::State::Playing && timerText->timerOverdue(), "Countdown continues positively in red after zero");
    QTest::mouseClick(pause, Qt::LeftButton);
    const QString overtime = timerText->text;
    QTest::qWait(550);
    check(timerText->text == overtime && timerText->timerHidden() && timerText->timerOverdue(), "overtime pauses and blinks while retaining red");
    QTest::mouseClick(play, Qt::LeftButton); QTest::qWait(1100);
    check(timerText->text > overtime && timerText->timerOverdue(), "overtime resumes counting upwards");
    QTest::mouseClick(stop, Qt::LeftButton);
    check(timerText->text == "00:00:02.0" && !timerText->timerOverdue() && !timerText->timerHidden() && timerText->color == QColor(Qt::white), "Countdown Stop restores configured duration and normal appearance");
    QTest::mouseClick(play, Qt::LeftButton);
    delete timerText;
    check(countdown->state() == Countdown::State::Stopped && !countdown->associatedText(), "deleting associated text safely stops Countdown");
    auto *futureWidget = new QLabel("Widget de prueba");
    container->addWidget(futureWidget);
    check(container->widgetCount() == 2 && container->isAncestorOf(futureWidget), "container accepts widgets");
    container->removeWidget(futureWidget);
    check(container->widgetCount() == 1 && !futureWidget->parent(), "container removes widgets");
    delete futureWidget;
    // Exercise the real context menu and insertion dialog before testing drags.
    QPoint insertion=w.view->mapFromScene(QPointF(80,70));
    QTimer::singleShot(0,[&]{
        if(auto *menu=qobject_cast<QMenu *>(app.activePopupWidget())) {
            QTimer::singleShot(30,[&]{
                if(auto *dialog=qobject_cast<QDialog *>(app.activeModalWidget()); dialog && dialog->objectName() == "AddTextDialog") {
                    dialog->findChild<QLineEdit *>("elementName")->setText("Reloj principal");
                    dialog->findChild<QPlainTextEdit *>("elementText")->setPlainText("Texto desde menú contextual");
                    dialog->accept();
                }
            });
            if(auto *addText=menu->findChild<QAction *>("addText"))QTest::mouseClick(menu,Qt::LeftButton,Qt::NoModifier,menu->actionGeometry(addText).center());else menu->close();
        }
    });
    QContextMenuEvent context(QContextMenuEvent::Mouse,insertion,w.view->viewport()->mapToGlobal(insertion));
    QApplication::sendEvent(w.view->viewport(),&context);
    check(w.canvas->items().size()==1,"right click menu inserts text at cursor");
    check(static_cast<CanvasItem *>(w.canvas->items().first())->name == "Reloj principal", "adding text requests and assigns its name");
    w.removeSelection();
    auto *text=w.addItem(new CanvasItem("Sessions\nTexto de prueba"),{100,80});
    countdown->configure(2, text);
    auto *view=w.view;auto *vp=view->viewport();
    auto drag=[&](QPoint start,QPoint end){
        QTest::mousePress(vp,Qt::LeftButton,Qt::NoModifier,start);
        QMouseEvent move(QEvent::MouseMove,QPointF(end),QPointF(vp->mapToGlobal(end)),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);
        QApplication::sendEvent(vp,&move);
        QTest::mouseRelease(vp,Qt::LeftButton,Qt::NoModifier,end);app.processEvents();
    };
    check(text->selectionRect().width() < text->itemSize().width() * .8 && text->selectionRect().height() < text->itemSize().height(), "text selection fits visible countdown instead of full layout box");
    auto start=view->mapFromScene(text->mapToScene(text->selectionRect().center()));
    drag(start,start+QPoint(40,25));
    check(text->x()>130 && text->y()>100,"move text with mouse");
    QPointF anchor=text->mapToScene(text->selectionRect().topLeft());
    start=view->mapFromScene(text->mapToScene(text->selectionRect().bottomRight()));
    drag(start,start+QPoint(60,30));
    check(text->itemSize().width()>360 && text->itemSize().height()>120 && QLineF(text->mapToScene(text->selectionRect().topLeft()),anchor).length()<1,"resize text using fitted corner preserves opposite visible corner");
    auto before=text->mapToScene(text->selectionRect().bottomRight());
    start=view->mapFromScene(text->mapToScene(text->selectionRect().topLeft()));drag(start,start-QPoint(20,10));
    auto after=text->mapToScene(text->selectionRect().bottomRight());
    check(QLineF(before,after).length()<1,"top left resize preserves opposite corner");
    auto openProperties = [&](CanvasItem *item, bool accept) {
        bool opened = false;
        QTimer::singleShot(0, [&] {
            if (auto *menu = qobject_cast<QMenu *>(app.activePopupWidget())) {
                auto *action = menu->findChild<QAction *>("itemProperties");
                if (!action) { menu->close(); return; }
                QTimer::singleShot(30, [&] {
                    if (auto *dialog = qobject_cast<PropertiesDialog *>(app.activeModalWidget())) {
                        opened = true;
                        dialog->findChild<QDoubleSpinBox *>("property2")->setValue(440);
                        dialog->grab().save(QApplication::applicationDirPath() + "/../build/properties-" + (item->isImage() ? "image" : "text") + ".png");
                        if (accept) dialog->accept(); else dialog->reject();
                    }
                });
                QTest::mouseClick(menu, Qt::LeftButton, Qt::NoModifier, menu->actionGeometry(action).center());
            }
        });
        QPoint at = view->mapFromScene(item->mapToScene(item->selectionRect().center()));
        QContextMenuEvent event(QContextMenuEvent::Mouse, at, vp->mapToGlobal(at));
        QApplication::sendEvent(vp, &event);
        return opened;
    };
    check(openProperties(text, true) && text->itemSize().width() == 440, "right click text properties edits size");
    QImage image(240,140,QImage::Format_ARGB32);image.fill(QColor("#2878b8"));
    {QPainter p(&image);p.setPen(Qt::white);p.setFont(QFont("Arial",20));p.drawText(image.rect(),Qt::AlignCenter,"IMAGEN");}
    auto *picture=w.addItem(new CanvasItem(image),{650,150});
    picture->name = "Logo del evento";
    start=view->mapFromScene(picture->pos()+QPointF(picture->itemSize().width(),picture->itemSize().height()));
    drag(start,start+QPoint(25,15));check(picture->itemSize().width()>480,"resize image using corner");
    auto originalSize = picture->itemSize();
    check(openProperties(picture, false) && picture->itemSize() == originalSize, "cancel image properties preserves size");
    check(openProperties(picture, true) && picture->itemSize().width() == 440, "right click image properties edits size");
    QTemporaryDir temp(QApplication::applicationDirPath()+"/../build/test-XXXXXX");
    check(temp.isValid() && w.writeProject(temp.filePath("roundtrip.sessions")),"save project with embedded image");
    MainWindow restored;
    check(restored.readProject(temp.filePath("roundtrip.sessions")) && restored.canvas->items().size()==2,"reopen project");
    auto *savedCountdown = restored.findChild<Countdown *>("Countdown");
    check(savedCountdown && savedCountdown->name() == "Inicio del evento", "Countdown name survives project round trip");
    check(savedCountdown && savedCountdown->durationSeconds() == 2 && savedCountdown->associatedText() && savedCountdown->associatedText()->id == text->id, "Countdown duration and text association survive save and reopen");
    check(savedCountdown && savedCountdown->outputFormat()=="HH:MM:SS.C","Countdown output format survives save and reopen");
    bool namesSaved = false;
    for (auto *base : restored.canvas->items()) if (auto *item = dynamic_cast<CanvasItem *>(base); item && item->isImage()) namesSaved = item->name == "Logo del evento";
    check(namesSaved && savedCountdown->associatedText()->name == text->name, "text and image names survive save and reopen");
    bool preserved=false;for(auto *base:restored.canvas->items())if(auto *item=dynamic_cast<CanvasItem *>(base);item && item->isImage())preserved=item->image==image;
    check(preserved,"embedded image survives round trip");
    QFile bad(temp.filePath("bad.sessions"));bad.open(QIODevice::WriteOnly);bad.write("{\"format\":\"sessions\",\"version\":1,\"items\":[{}]}");bad.close();
    check(!restored.readProject(bad.fileName()) && restored.canvas->items().size()==2,"invalid file preserves current design");
    w.canvas->clearSelection();picture->setSelected(true);QTest::keyClick(vp,Qt::Key_Delete);
    check(w.canvas->items().size()==1,"Delete removes selected element");
    picture=w.addItem(new CanvasItem(image),{650,150});picture->setSelected(false);text->setSelected(true);
    auto *launch = w.findChild<QPushButton *>("launchComposition");
    check(launch->cursor().shape() == Qt::PointingHandCursor, "launch button uses hand cursor instead of splitter resize cursor");
    QTest::mouseClick(launch, Qt::LeftButton);
    check(w.outputScene->items().size() == 2, "launch sends composition to output");
    CanvasItem *outputText = nullptr;
    bool cleanOutput = true;
    for (auto *base : w.outputScene->items()) {
        auto *item = dynamic_cast<CanvasItem *>(base);
        cleanOutput &= item && !item->isSelected() && !(item->flags() & QGraphicsItem::ItemIsSelectable);
        if (item && !item->isImage()) outputText = item;
    }
    check(cleanOutput, "output has no selection handles or editor interaction");
    QPointF launchedPosition = text->pos();
    text->setPos(text->pos() + QPointF(20, 20));
    check(outputText && outputText->pos() == launchedPosition, "editing does not alter launched output");
    QTest::mouseClick(launch, Qt::LeftButton);
    bool updated = false;
    for (auto *base : w.outputScene->items()) if (auto *item = dynamic_cast<CanvasItem *>(base); item && !item->isImage()) updated = item->pos() == text->pos();
    check(updated, "launch again updates output");
    countdown->start();
    bool live = false;
    for (auto *base : w.outputScene->items()) if (auto *item = dynamic_cast<CanvasItem *>(base); item && item->id == text->id) live = item->text == "00:00:02.0";
    check(live, "Countdown updates launched output text live");
    countdown->pause();
    QTest::qWait(550);
    CanvasItem *liveText = nullptr;
    for (auto *base : w.outputScene->items()) if (auto *item = dynamic_cast<CanvasItem *>(base); item && item->id == text->id) liveText = item;
    check(liveText && liveText->timerHidden() && text->timerHidden(), "pause blinking propagates to launched output");
    countdown->start(); QTest::qWait(2150);
    check(liveText && liveText->timerOverdue() && !liveText->timerHidden() && liveText->text == text->text, "positive overtime and red appearance propagate to output");
    countdown->stop();
    check(liveText && !liveText->timerOverdue() && !liveText->timerHidden(), "Stop restores output appearance");
    check(!w.showExternalOutput(nullptr), "invalid monitor is rejected");
    check(w.showExternalOutput(QGuiApplication::primaryScreen()), "output window opens fullscreen on selected available screen");
    app.processEvents();
    check(w.externalOutput->isFullScreen() && w.externalOutput->isVisible(), "external output is fullscreen");
    QTest::keyClick(w.externalOutput, Qt::Key_Escape);
    check(!w.externalOutput->isVisible(), "Escape closes external output");
    std::printf("INFO available screens: %lld\n", static_cast<long long>(QGuiApplication::screens().size()));
    auto insertShape = [&](const QString &actionName, const QString &name) -> CanvasItem * {
        QTimer::singleShot(0, [&] {
            if (auto *menu = qobject_cast<QMenu *>(app.activePopupWidget())) {
                auto *action = menu->findChild<QAction *>(actionName);
                if (!action) { menu->close(); return; }
                QTimer::singleShot(30, [&] {
                    if (auto *dialog = qobject_cast<QInputDialog *>(app.activeModalWidget())) {
                        dialog->setTextValue(name); dialog->accept();
                    }
                });
                auto *shapes=menu->findChild<QMenu *>("basicShapesMenu");
                if(shapes){menu->setActiveAction(shapes->menuAction());QTest::keyClick(menu,Qt::Key_Right);QTest::mouseClick(shapes,Qt::LeftButton,Qt::NoModifier,shapes->actionGeometry(action).center());}
                else menu->close();
            }
        });
        QPoint point = view->mapFromScene(QPointF(1200,680));
        QContextMenuEvent event(QContextMenuEvent::Mouse, point, vp->mapToGlobal(point));
        QApplication::sendEvent(vp, &event);
        for (auto *base : w.canvas->items()) if (auto *item = dynamic_cast<CanvasItem *>(base); item && item->name == name) return item;
        return nullptr;
    };
    auto *circle = insertShape("addCircle", "Círculo de prueba");
    if (circle) circle->setPos(80,450);
    auto *square = insertShape("addSquare", "Cuadrado de prueba");
    if (square) square->setPos(350,470);
    auto *rectangle = insertShape("addRectangle", "Rectángulo de prueba");
    if(rectangle)rectangle->setPos(650,500);
    auto *line=insertShape("addLine","Línea de prueba");
    check(line && line->shapeType()==CanvasItem::Shape::Line && line->itemSize()==QSizeF(320,1),"basic shapes submenu creates a named horizontal line");
    if(line){line->setPos(100,400);line->setItemSize({400,1});line->setSelected(false);check(line->shape().contains(QPointF(200,0)) && !line->shape().contains(QPointF(200,20)),"line hit testing follows its stroke rather than a rectangular fill");}
    check(circle && square && rectangle, "right click inserts named circle square and rectangle");
    if (!circle || !square || !rectangle) return 1;
    circle->setPos(80,450); circle->setSelected(true);
    auto cornerBefore = circle->mapToScene(circle->selectionRect().topLeft());
    start = view->mapFromScene(circle->mapToScene(circle->selectionRect().bottomRight()));
    drag(start,start+QPoint(10,15));
    check(circle->itemSize().width() == circle->itemSize().height() && QLineF(cornerBefore,circle->mapToScene(circle->selectionRect().topLeft())).length()<1, "circle corner resizing preserves proportions and opposite corner");
    PropertiesDialog shapeProperties(circle, &w);
    shapeProperties.grab().save(QApplication::applicationDirPath()+"/../build/properties-shape.png");
    shapeProperties.findChild<QDoubleSpinBox *>("property3")->setValue(240);
    check(shapeProperties.findChild<QDoubleSpinBox *>("property2")->value() == 240 && shapeProperties.findChild<QPushButton *>("shapeColor") && !shapeProperties.findChild<QPlainTextEdit *>("propertyText"), "shape properties offers fill and keeps circle dimensions equal");
    shapeProperties.accept();
    square->setPos(350,470); square->setItemSize({200,120}); square->color=QColor("#d5ad5d");
    rectangle->setPos(650,500); rectangle->setItemSize({280,140}); rectangle->color=QColor("#a76b8f");
    check(square->itemSize() == QSizeF(200,200) && rectangle->itemSize() == QSizeF(280,140), "square stays square while rectangle permits independent dimensions");
    circle->setSelected(false);
    check(!circle->shape().contains(QPointF(1,1)) && circle->shape().contains(circle->selectionRect().center()), "circle hit testing follows its visible shape");
    circle->setSelected(true);
    for (int panel = 2; panel < 3; ++panel) {
        auto *reserved = qobject_cast<WidgetContainer *>(bottomPanels->widget(panel));
        auto *surface = reserved->widget();
        QContextMenuEvent event(QContextMenuEvent::Mouse, QPoint(10,10), surface->mapToGlobal(QPoint(10,10)));
        QApplication::sendEvent(surface, &event);
        check(!reserved->countdownCreationEnabled() && !app.activePopupWidget() && reserved->widgetCount() == 0, "reserved lower panel has no Countdown creation menu");
    }
    check(w.writeProject(temp.filePath("shapes.sessions")), "save design with basic shapes");
    MainWindow shapeRestored;
    bool loadedShapes = shapeRestored.readProject(temp.filePath("shapes.sessions"));
    int shapeCount = 0;
    for (auto *base : shapeRestored.canvas->items()) if (auto *item = dynamic_cast<CanvasItem *>(base); item && item->isShape()) ++shapeCount;
    check(loadedShapes && shapeCount == 4, "reopen all four basic shape types including a one-pixel high line");
    auto *restoredPanels = shapeRestored.findChild<QSplitter *>("bottomSplitter");
    check(restoredPanels->widget(0)->findChild<Countdown *>() && !restoredPanels->widget(1)->findChild<Countdown *>() && !restoredPanels->widget(2)->findChild<Countdown *>(), "reopened Countdowns stay in left panel only");
    QFile oldProject(temp.filePath("shapes.sessions")); oldProject.open(QIODevice::ReadOnly);
    auto legacyRoot = QJsonDocument::fromJson(oldProject.readAll()).object(); oldProject.close();
    auto legacyWidgets = legacyRoot["widgets"].toArray();
    auto legacyCountdown = legacyWidgets.first().toObject(); legacyCountdown["panel"] = 2;
    legacyWidgets[0] = legacyCountdown; legacyRoot["widgets"] = legacyWidgets;
    QFile legacyFile(temp.filePath("legacy-panels.sessions")); legacyFile.open(QIODevice::WriteOnly);
    legacyFile.write(QJsonDocument(legacyRoot).toJson()); legacyFile.close();
    check(shapeRestored.readProject(legacyFile.fileName()) && restoredPanels->widget(0)->findChild<Countdown *>() && !restoredPanels->widget(2)->findChild<Countdown *>(), "older right panel Countdown is preserved in left panel");
    w.launchComposition();
    QImage outputFrame(1280,720,QImage::Format_ARGB32); outputFrame.fill(Qt::black);
    { QPainter painter(&outputFrame); w.outputScene->render(&painter); }
    check(outputFrame.pixelColor(200,570) == circle->color && outputFrame.pixelColor(450,570) == square->color && outputFrame.pixelColor(790,570) == rectangle->color, "launched output renders circle square rectangle and fill colors");
    Countdown onlyText;
    onlyText.setCompositionScene(w.canvas); onlyText.configure(10,rectangle);
    check(!onlyText.associatedText(), "Countdown cannot associate with geometric shapes");
    auto countdownMenuAction = [&](Countdown *item, const QString &actionName) {
        bool invoked = false;
        QTimer::singleShot(0, [&] {
            if (auto *menu = qobject_cast<QMenu *>(app.activePopupWidget())) {
                auto *action = menu->findChild<QAction *>(actionName);
                if (!action || !action->isEnabled()) { menu->close(); return; }
                invoked = true;
                if (actionName == "copyAction") menu->grab().save(QApplication::applicationDirPath()+"/../build/countdown-menu.png");
                QTest::mouseClick(menu,Qt::LeftButton,Qt::NoModifier,menu->actionGeometry(action).center());
            }
        });
        QContextMenuEvent event(QContextMenuEvent::Mouse,QPoint(20,10),item->mapToGlobal(QPoint(20,10)));
        QApplication::sendEvent(item,&event);
        return invoked;
    };
    countdown->start(); QTest::qWait(250); countdown->pause();
    const QString currentTime = text->text;
    check(countdownMenuAction(countdown,"copyAction") && countdownMenuAction(countdown,"pasteAction"), "classic menu copies and pastes Countdown");
    auto *copy = container->selectedCountdown();
    check(copy && copy != countdown && copy->durationSeconds() == countdown->durationSeconds() && copy->associatedText() == text && copy->state() == Countdown::State::Stopped && text->text == currentTime, "paste preserves configuration without resetting source countdown text");
    if (!copy || copy == countdown) return 1;
    check(copy->outputFormat()==countdown->outputFormat(),"copied Countdown retains its output format");
    check(countdownMenuAction(copy,"moveActionUp") && container->widgetsInOrder().first() == copy && countdown->state() == Countdown::State::Paused, "moving Countdown up preserves timer state");
    check(w.writeProject(temp.filePath("ordered.sessions")), "save reordered Countdown list");
    MainWindow ordered;
    bool orderLoaded = ordered.readProject(temp.filePath("ordered.sessions"));
    auto *orderedPanel = ordered.findChild<QSplitter *>("bottomSplitter")->widget(0)->findChild<WidgetContainer *>();
    check(orderLoaded && orderedPanel->widgetCount() == 2 && qobject_cast<Countdown *>(orderedPanel->widgetsInOrder().first())->name() == copy->name(), "visual Countdown order survives save and reopen");
    check(countdownMenuAction(copy,"moveActionDown") && container->widgetsInOrder().last() == copy, "moving Countdown down changes visual order");
    QPointer<Countdown> cutCopy(copy);
    check(countdownMenuAction(copy,"cutAction") && !cutCopy && container->widgetCount() == 1, "cut removes Countdown and cancels its timer");
    container->pasteCountdown(); copy = container->selectedCountdown();
    check(container->widgetCount() == 2 && copy && copy != countdown, "cut Countdown can be pasted again");
    QPointer<Countdown> removedCopy(copy);
    check(countdownMenuAction(copy,"deleteAction") && !removedCopy && container->widgetCount() == 1 && countdown->associatedText() == text, "delete removes Countdown without deleting its composition text");
    container->selectCountdown(countdown);
    w.activateWindow(); countdown->setFocus(); app.processEvents();
    QTest::keyClick(container,Qt::Key_C,Qt::ControlModifier);
    QTest::keyClick(container,Qt::Key_V,Qt::ControlModifier);
    check(container->widgetCount() == 2 && container->selectedCountdown() != countdown, "Ctrl C and Ctrl V operate on selected Countdown");
    QTest::keyClick(container,Qt::Key_Delete);
    check(container->widgetCount() == 1 && countdown->associatedText() == text, "Delete shortcut removes selected Countdown only");
    addCountdown("Cuenta alternativa", true);
    auto *alternative = container->selectedCountdown();
    alternative->configure(90, text, false);
    QTest::mouseClick(alternative->findChild<QLabel *>("CountdownTitle"), Qt::LeftButton);
    check(text->text == "01:30.0" && alternative->state() == Countdown::State::Stopped && countdown->state() == Countdown::State::Paused, "selecting Countdown publishes its time without starting or resetting it");
    QTest::qWait(550);
    check(text->text == "01:30.0" && !text->timerHidden(), "previous paused Countdown cannot overwrite the selected Countdown text");
    QTest::mouseClick(countdown->findChild<QLabel *>("CountdownTitle"), Qt::LeftButton);
    check(text->text == currentTime && countdown->state() == Countdown::State::Paused, "selecting paused Countdown restores its frozen time");
    container->selectCountdown(alternative); container->deleteCountdown();
    container->selectCountdown(countdown);
    app.processEvents();
    auto *sources = w.findChild<SourcePanel *>();
    auto *sourceList = sources ? sources->findChild<QTreeWidget *>("sourcesList") : nullptr;
    check(sources && bottomPanels->widget(1) == sources && sourceList && sourceList->topLevelItemCount() == w.canvas->items().size(), "middle panel lists all composition sources");
    if (!sourceList) return 1;
    auto rowFor = [&](const QString &id) -> QTreeWidgetItem * {
        for(int i=0;i<sourceList->topLevelItemCount();++i)if(sourceList->topLevelItem(i)->data(0,Qt::UserRole).toString()==id)return sourceList->topLevelItem(i);
        return nullptr;
    };
    auto *managed = w.addItem(new CanvasItem(CanvasItem::Shape::Rectangle),{400,300});
    managed->name = "Fuente de prueba"; managed->setZValue(50); sources->synchronize();
    check(rowFor(managed->id) && rowFor(managed->id)->text(0) == managed->name, "new source and name appear in sources panel");
    auto clickSource = [&](int column) {
        auto *row = rowFor(managed->id); sourceList->scrollToItem(row); app.processEvents();
        const QRect rect = sourceList->visualItemRect(row);
        QTest::mouseClick(sourceList->viewport(),Qt::LeftButton,Qt::NoModifier,QPoint(sourceList->header()->sectionViewportPosition(column)+12,rect.center().y()));
        app.processEvents();
    };
    clickSource(0);
    check(managed->isSelected(), "source row selects corresponding canvas object");
    clickSource(1);
    check(!managed->isVisible() && rowFor(managed->id), "eye control hides source and retains its row");
    w.launchComposition();
    bool hiddenInOutput=false;
    for(auto *base:w.outputScene->items())if(auto *item=dynamic_cast<CanvasItem *>(base);item && item->id==managed->id)hiddenInOutput=!item->isVisible();
    check(hiddenInOutput,"hidden sources stay hidden in launched output");
    clickSource(1); clickSource(2);
    check(managed->isVisible() && managed->isLocked() && !managed->flags().testFlag(QGraphicsItem::ItemIsMovable), "eye restores visibility and lock prevents moving source");
    auto originalRect=managed->itemSize(); auto originalPosition=managed->pos();
    auto corner=w.view->mapFromScene(managed->mapToScene(managed->selectionRect().bottomRight()));
    QTest::mousePress(w.view->viewport(),Qt::LeftButton,Qt::NoModifier,corner);
    QTest::mouseMove(w.view->viewport(),corner+QPoint(25,25),30);
    QTest::mouseRelease(w.view->viewport(),Qt::LeftButton,Qt::NoModifier,corner+QPoint(25,25));
    check(managed->itemSize()==originalRect && managed->pos()==originalPosition,"locked source cannot be resized or moved with mouse");
    sourceList->setCurrentItem(rowFor(managed->id));
    bool sourceMenuOrder=false,sourceShapeSubmenu=false;
    auto sourceMenuAction = [&](const QString &name) {
        QTimer::singleShot(0,[&] {
            if(auto *menu=qobject_cast<QMenu *>(app.activePopupWidget())) {
                const auto entries=menu->actions();auto *lock=menu->findChild<QAction *>("sourceLock");auto *remove=menu->findChild<QAction *>("sourceDelete");auto *properties=menu->findChild<QAction *>("sourceProperties");
                sourceMenuOrder=lock && remove && properties && entries.indexOf(remove)==entries.indexOf(lock)+1 && entries.last()==properties && entries.at(entries.size()-2)->isSeparator();
                auto *add=sources->findChild<QMenu *>("sourceAddMenu");auto *shapes=sources->findChild<QMenu *>("sourceShapesMenu");
                sourceShapeSubmenu=add && shapes && add->actions().size()==3 && add->actions().last()->menu()==shapes && shapes->actions().size()==4;
                menu->grab().save(QApplication::applicationDirPath()+"/../build/sources-menu.png");
                if(auto *action=menu->findChild<QAction *>(name);action && action->isEnabled())QTest::mouseClick(menu,Qt::LeftButton,Qt::NoModifier,menu->actionGeometry(action).center());
                else menu->close();
            }
        });
        QPoint point=sourceList->visualItemRect(sourceList->currentItem()).center();
        QContextMenuEvent event(QContextMenuEvent::Mouse,point,sourceList->viewport()->mapToGlobal(point));
        QApplication::sendEvent(sourceList->viewport(),&event);
    };
    sourceMenuAction("sourceDown");
    check(sourceMenuOrder,"Sources menu places Delete immediately below Lock and Properties last after a separator");
    check(sourceShapeSubmenu,"Sources Add menu groups circle square rectangle and line in Basic shapes");
    check(sourceList->indexOfTopLevelItem(rowFor(managed->id))==1,"down control moves source down one layer");
    sourceMenuAction("sourceUp");
    check(sourceList->indexOfTopLevelItem(rowFor(managed->id))==0,"up control moves source up one layer");
    clickSource(1);
    check(w.writeProject(temp.filePath("sources.sessions")),"save source visibility lock and order");
    MainWindow sourcesRestored;
    bool sourceLoaded=sourcesRestored.readProject(temp.filePath("sources.sessions"));
    CanvasItem *restoredSource=nullptr;
    for(auto *base:sourcesRestored.canvas->items())if(auto *item=dynamic_cast<CanvasItem *>(base);item && item->id==managed->id)restoredSource=item;
    check(sourceLoaded && restoredSource && !restoredSource->isVisible() && restoredSource->isLocked() && sourcesRestored.canvas->items().first()==restoredSource,"source visibility lock and layer order survive reopening");
    clickSource(1); clickSource(2);
    text->setSelected(true); sources->synchronize();
    check(sourceList->currentItem() && sourceList->currentItem()->data(0,Qt::UserRole).toString()==text->id,"canvas selection is reflected in sources panel");
    sourceList->setCurrentItem(rowFor(managed->id));
    QPointer<CanvasItem> deletedSource=managed;
    sourceMenuAction("sourceDelete");
    check(!deletedSource && sourceList->topLevelItemCount()==w.canvas->items().size(),"delete control removes source from composition and list");
    bool mediaAdded=false;
    QTimer::singleShot(0,[&] {
        if(auto *menu=qobject_cast<QMenu *>(app.activePopupWidget())) {
            QTimer::singleShot(30,[&]{if(auto *dialog=qobject_cast<QInputDialog *>(app.activeModalWidget())){dialog->setTextValue("Vídeo de prueba");dialog->accept();mediaAdded=true;}});
            auto *action=menu->findChild<QAction *>("addMultimedia");
            if(action)QTest::mouseClick(menu,Qt::LeftButton,Qt::NoModifier,menu->actionGeometry(action).center());else menu->close();
        }
    });
    QContextMenuEvent addMedia(QContextMenuEvent::Mouse,QPoint(15,15),container->widget()->mapToGlobal(QPoint(15,15)));
    QApplication::sendEvent(container->widget(),&addMedia);
    auto *media=container->findChild<Multimedia *>();
    check(mediaAdded && media && media->name()=="Vídeo de prueba","Actions context menu creates named Multimedia widget");
    if(!media)return 1;
    check(Multimedia::backendAvailable(),"LibVLC 3 backend is available");
    const QString audioFile=QDir::cleanPath(QApplication::applicationDirPath()+"/../build/media-test.wav");
    const QString videoFile=QDir::cleanPath(QApplication::applicationDirPath()+"/../../svn/tests/fixtures/media-test.mp4");
    // Five seconds of silent PCM audio; keep the playback test self-contained.
    { QFile wave(audioFile); if(wave.open(QIODevice::WriteOnly)) {
        QDataStream stream(&wave);stream.setByteOrder(QDataStream::LittleEndian);
        stream.writeRawData("RIFF",4);stream<<quint32(36+44100*5*2);stream.writeRawData("WAVEfmt ",8);
        stream<<quint32(16)<<quint16(1)<<quint16(1)<<quint32(44100)<<quint32(88200)<<quint16(2)<<quint16(16);
        stream.writeRawData("data",4);stream<<quint32(44100*5*2);wave.write(QByteArray(44100*5*2,'\0'));
    } }
    auto waitFor=[&](auto condition,int timeout=5000){QElapsedTimer deadline;deadline.start();while(!condition() && deadline.elapsed()<timeout)QTest::qWait(50);return condition();};
    media->configure("Audio",audioFile);
    auto *loopButton=media->findChild<QPushButton *>("multimediaLoop");
    app.processEvents();
    check(loopButton && loopButton->isCheckable() && loopButton->isEnabled() && !loopButton->isChecked() && !loopButton->icon().isNull() && loopButton->text().isEmpty(),"Multimedia offers a Radit style icon Loop toggle, off by default");
    check(loopButton && loopButton->isVisible() && loopButton->toolTip().startsWith("Loop") && loopButton->mapTo(media,QPoint(0,0)).x()+loopButton->width()<=media->width(),"Loop toggle is labelled by its tooltip and fits inside the action card");
    auto swatch=[&](QPushButton *button){const QImage image=button->grab().toImage();return image.pixelColor(qRound(3.0*image.width()/button->width()),qRound(3.0*image.height()/button->height()));};
    const QColor loopIdle=swatch(loopButton);
    QTest::mouseClick(loopButton,Qt::LeftButton);
    check(media->loops() && loopButton->isChecked() && swatch(loopButton)==QColor("#39385b") && loopIdle==QColor("#262c3b"),"Loop toggle keeps Radit's checked palette when enabled");
    QTest::mouseClick(loopButton,Qt::LeftButton);
    check(!media->loops() && !loopButton->isChecked(),"Loop toggle returns to its normal palette when disabled");
    QTest::mouseClick(media->findChild<QPushButton *>("multimediaPlay"),Qt::LeftButton);
    check(waitFor([&]{return media->isPlaying() && media->position()>150 && media->duration()>4000;}),"Multimedia plays audio and reports position and duration");
    QTest::mouseClick(media->findChild<QPushButton *>("multimediaPause"),Qt::LeftButton);
    check(waitFor([&]{return media->isPaused();}),"Multimedia Pause freezes playback");
    const qint64 pausedPosition=media->position();QTest::qWait(300);
    check(qAbs(media->position()-pausedPosition)<100,"paused audio position remains stable");
    auto *mediaSlider=media->findChild<QSlider *>("multimediaSlider");
    mediaSlider->setValue(600);QMetaObject::invokeMethod(mediaSlider,"sliderReleased");
    check(waitFor([&]{return media->position()>=2800 && media->position()<=3200;}),"Multimedia slider seeks forwards in paused audio");
    mediaSlider->setValue(100);QMetaObject::invokeMethod(mediaSlider,"sliderReleased");
    check(waitFor([&]{return media->position()>=350 && media->position()<=650;}),"Multimedia slider seeks backwards in paused audio");
    QTest::mouseClick(media->findChild<QPushButton *>("multimediaPlay"),Qt::LeftButton);
    check(waitFor([&]{return media->isPlaying() && media->position()>650;}),"Multimedia Play resumes after seeking");
    const qint64 loadedDuration=media->duration();
    QTest::mouseClick(media->findChild<QPushButton *>("multimediaStop"),Qt::LeftButton);
    check(!media->isPlaying() && !media->isPaused() && media->position()==0 && media->findChild<QLabel *>("MultimediaTime")->text().startsWith("00:00:00 /"),"Multimedia Stop resets playback and time display");
    check(loadedDuration>0 && media->duration()==loadedDuration,"media duration survives decoder stopping and discarding timing metadata");
    media->configure("Vídeo de prueba",videoFile);
    countdown->stop();
    check(waitFor([&]{return !media->thumbnail().isNull();}),"video first-frame thumbnail is prepared without Cue or Play");
    auto *thumbnailButton=media->findChild<QPushButton *>("multimediaPlay");
    check(thumbnailButton && thumbnailButton->text().isEmpty() && !thumbnailButton->icon().isNull() && !media->isPlaying() && !media->hasCurrentFrame() && media->position()==0,"thumbnail replaces the Play button and leaves playback and output untouched");
    media->grab().save(QApplication::applicationDirPath()+"/../build/multimedia-thumbnail.png");
    QImage beforeCue(1280,720,QImage::Format_RGB32);beforeCue.fill(Qt::black);
    {QPainter painter(&beforeCue);w.outputScene->render(&painter);}
    QTest::mouseClick(media->findChild<QPushButton *>("multimediaCue"),Qt::LeftButton);
    check(waitFor([&]{return !media->currentFrame().isNull() && media->isPlaying() && media->position()>150;}),"Cue plays video in composition");
    const qint64 cuePosition=media->position();
    check(waitFor([&]{return media->position()>cuePosition && media->findChild<QPushButton *>("multimediaPlay")->isEnabled();},1500),"Cue keeps playing and allows switching to output with Play");
    CanvasItem *videoSource=nullptr;
    for(auto *base:w.canvas->items())if(auto *item=dynamic_cast<CanvasItem *>(base);item && item->mediaId==media->id)videoSource=item;
    check(videoSource && videoSource->itemSize()==w.canvas->sceneRect().size(),"Cue adds video as a full canvas source");
    sources->synchronize();
    check(videoSource && !rowFor(videoSource->id),"Multimedia video is excluded from Sources list");
    QImage afterCue(1280,720,QImage::Format_RGB32);afterCue.fill(Qt::black);
    {QPainter painter(&afterCue);w.outputScene->render(&painter);}
    check(beforeCue==afterCue,"Cue leaves launched output unchanged");
    w.launchComposition();
    bool videoBeforePlay=false;
    for(auto *base:w.outputScene->items())if(auto *item=dynamic_cast<CanvasItem *>(base);item && item->mediaId==media->id)videoBeforePlay=true;
    check(!videoBeforePlay,"launch composition cannot send a cued video before Play");
    QImage compositionVideo(1280,720,QImage::Format_RGB32);compositionVideo.fill(Qt::black);
    {QPainter painter(&compositionVideo);w.canvas->render(&painter);}
    auto isBlue=[](QColor color){return color.blue()>180 && color.red()<50 && color.green()<50;};
    check(isBlue(compositionVideo.pixelColor(1200,680)),"Cue video frame is visibly rendered in composition");
    QTest::mouseClick(media->findChild<QPushButton *>("multimediaPlay"),Qt::LeftButton);
    check(waitFor([&]{return media->hasVideo() && media->position()>150;}),"Multimedia continues video playback after switching to output");
    QImage playingVideo(1280,720,QImage::Format_RGB32);playingVideo.fill(Qt::black);
    {QPainter painter(&playingVideo);w.outputScene->render(&painter);}
    check(isBlue(playingVideo.pixelColor(1200,680)),"Play launches video and renders decoded frames in output");
    check(mediaSlider->maximum()==1000 && mediaSlider->inherits("Slider"),"Multimedia uses Radit Slider class and normalized range");
    check(!media->findChild<QWidget *>("MultimediaPreview"),"Multimedia card contains controls without video image");
    auto videoInOutput=[&] {
        for(auto *base:w.outputScene->items())if(auto *item=dynamic_cast<CanvasItem *>(base);item && item->mediaId==media->id)return true;
        return false;
    };
    auto compositionIsBlack=[&] {
        QImage image(1280,720,QImage::Format_RGB32);image.fill(Qt::black);
        {QPainter painter(&image);w.canvas->render(&painter);}
        return image.pixelColor(1200,680)==QColor(Qt::black);
    };
    const int otherOutputItems=w.outputScene->items().size()-1;
    QTest::mouseClick(media->findChild<QPushButton *>("multimediaStop"),Qt::LeftButton);
    check(!videoInOutput() && w.outputScene->items().size()==otherOutputItems && !media->isPlaying() && media->position()==0,"Stop removes playing video from output while retaining other composition elements");
    check(videoSource && !videoSource->isVisible() && compositionIsBlack(),"Stop also clears video from composition leaving its background black");
    w.launchComposition();
    check(!videoInOutput(),"launch cannot restore a stopped video without Play");
    media->play();waitFor([&]{return media->isPlaying();});
    media->pause();waitFor([&]{return media->isPaused();});
    QTest::mouseClick(media->findChild<QPushButton *>("multimediaStop"),Qt::LeftButton);
    check(!videoInOutput() && !media->isPaused() && videoSource && !videoSource->isVisible() && videoSource->scene()==w.canvas,"Stop also hides paused video in composition while preserving its placement");
    media->play();
    check(waitFor([&]{return media->isPlaying() && videoInOutput() && mediaSlider->isEnabled() && media->position()>150;}),"Play sends a stopped video back to output");
    media->seek(media->duration()-250);
    check(waitFor([&]{return media->findChild<QLabel *>("MultimediaStatus")->text()=="Finalizado" && !media->isPlaying();}),"video reaches end and reports finished");
    const QImage preparedFrame=media->currentFrame();
    QTest::qWait(1200);
    check(!media->isPlaying() && !media->isPaused() && media->position()==media->duration() && mediaSlider->value()==mediaSlider->maximum() && media->currentFrame()==preparedFrame,"finished video retains end time slider and last frame without restarting");
    check(videoInOutput() && videoSource && videoSource->isVisible() && !compositionIsBlack(),"finished video retains its frame in output and composition");
    w.launchComposition();bool relaunchedVideo=false;
    for(auto *base:w.outputScene->items())if(auto *item=dynamic_cast<CanvasItem *>(base);item && item->mediaId==media->id)relaunchedVideo=true;
    check(relaunchedVideo && media->position()==media->duration() && media->currentFrame()==preparedFrame,"Lanzar preserves the finished video without restarting it");
    QTest::mouseClick(media->findChild<QPushButton *>("multimediaPlay"),Qt::LeftButton);
    check(waitFor([&]{return media->isPlaying() && media->position()<1500;}),"finished video restarts only when Play is pressed explicitly");
    QTest::mouseClick(loopButton,Qt::LeftButton);
    check(media->loops() && loopButton->isChecked(),"Loop toggle activates repeat for the configured video");
    media->seek(media->duration()-250);
    check(waitFor([&]{return media->isPlaying() && media->position()<media->duration()/2 && media->findChild<QLabel *>("MultimediaStatus")->text()!="Finalizado";},4000),"looping video restarts at the beginning instead of finishing");
    check(waitFor([&]{return videoInOutput() && mediaSlider->value()<mediaSlider->maximum() && media->position()>0 && media->position()<media->duration()/2;}),"loop restart keeps the action in output and rewinds the slider");
    media->seek(media->duration()-250);
    check(waitFor([&]{return media->isPlaying() && media->position()<media->duration()/2;},6000),"loop keeps repeating while it stays enabled");
    QTest::mouseClick(loopButton,Qt::LeftButton);
    check(!media->loops() && !loopButton->isChecked(),"Loop toggle can be disabled during playback");
    media->seek(media->duration()-250);
    check(waitFor([&]{return media->findChild<QLabel *>("MultimediaStatus")->text()=="Finalizado";},5000),"disabling Loop restores the finished state at end of file");
    media->play();waitFor([&]{return media->isPlaying();});
    media->pause();waitFor([&]{return media->isPaused();});
    media->setLoop(true);
    check(w.writeProject(temp.filePath("multimedia.sessions")),"save design with mixed Countdown and Multimedia actions");
    MainWindow mediaRestored;bool loadedMedia=mediaRestored.readProject(temp.filePath("multimedia.sessions"));
    auto *restoredMedia=mediaRestored.findChild<Multimedia *>();
    auto *restoredActions=mediaRestored.findChild<QSplitter *>("bottomSplitter")->widget(0)->findChild<WidgetContainer *>();
    check(loadedMedia && restoredMedia && restoredActions->widgetsInOrder().last()==restoredMedia && restoredMedia->name()==media->name() && restoredMedia->filePath()==videoFile && !restoredMedia->isPlaying() && !restoredMedia->isPaused(),"Multimedia name file and order restore without automatic playback");
    bool bindingRestored=false;
    if(restoredMedia)for(auto *base:mediaRestored.canvas->items())if(auto *item=dynamic_cast<CanvasItem *>(base);item && item->mediaId==restoredMedia->id)bindingRestored=true;
    check(bindingRestored,"saved video source retains its Multimedia binding");
    check(restoredMedia && restoredMedia->loops() && restoredMedia->findChild<QPushButton *>("multimediaLoop")->isChecked(),"Loop mode survives saving and reopening the design");
    { QJsonObject root{{"format","sessions"},{"version",1},{"items",QJsonArray()},
        {"widgets",QJsonArray{QJsonObject{{"type","multimedia"},{"name","Bucle no válido"},{"file",videoFile},{"loop","sí"}}}}};
      QFile invalidLoop(temp.filePath("invalid-loop.sessions"));invalidLoop.open(QIODevice::WriteOnly);invalidLoop.write(QJsonDocument(root).toJson());invalidLoop.close(); }
    MainWindow invalidLoopWindow;
    check(!invalidLoopWindow.readProject(temp.filePath("invalid-loop.sessions")) && invalidLoopWindow.findChild<Multimedia *>()==nullptr,"a design with a non boolean Loop value is rejected");
    media->setLoop(false);
    media->cue();
    check(waitFor([&]{return media->isPlaying() && media->position()>100 && videoSource->isVisible();}),"Cue restores hidden video when explicitly requested");
    QTest::mouseClick(media->findChild<QPushButton *>("multimediaStop"),Qt::LeftButton);
    check(!videoSource->isVisible() && compositionIsBlack() && !videoInOutput(),"Stop during Cue clears video from composition and output");
    media->cue();waitFor([&]{return media->isPlaying() && media->position()>100 && mediaSlider->isEnabled();});
    media->seek(media->duration()-250);
    check(waitFor([&]{return media->findChild<QLabel *>("MultimediaStatus")->text()=="Finalizado";}),"Cue playback also reaches a stable finished state");
    QTest::qWait(1000);
    check(mediaSlider->value()==mediaSlider->maximum() && media->duration()>=4800 && media->position()==media->duration() && !media->isPlaying(),"Cue end retains the final position without replaying");
    check(videoSource->isVisible() && !compositionIsBlack() && !videoInOutput(),"Cue completion retains its last frame in composition only");
    media->stop();media->configure("Vídeo de prueba",temp.filePath("missing.mp4"));media->play();
    check(!media->isPlaying() && media->findChild<QLabel *>("MultimediaStatus")->text()=="Archivo no encontrado","missing media file produces a readable error without playback");
    media->configure("Vídeo de prueba",videoFile);
    auto dropFiles=[&](QWidget *surface,const QList<QUrl> &urls) {
        QMimeData mime;mime.setUrls(urls);QPoint point(20,qMax(1,surface->height()-2));
        QDragEnterEvent enter(point,Qt::CopyAction,&mime,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(surface,&enter);
        if(!enter.isAccepted())return false;
        QDropEvent drop(QPointF(point),Qt::CopyAction,&mime,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(surface,&drop);return drop.isAccepted();
    };
    const int beforeDrop=container->widgetCount();
    check(dropFiles(container->widget(),{QUrl::fromLocalFile(audioFile),QUrl::fromLocalFile(videoFile)}) && container->widgetCount()==beforeDrop+2 && !app.activeModalWidget(),"dropping audio and video creates Multimedia actions directly without dialogs");
    auto actionOrder=container->widgetsInOrder();
    auto *droppedAudio=qobject_cast<Multimedia *>(actionOrder.value(beforeDrop));
    auto *droppedVideo=qobject_cast<Multimedia *>(actionOrder.value(beforeDrop+1));
    check(droppedAudio && droppedVideo && droppedAudio->filePath()==audioFile && droppedVideo->filePath()==videoFile && droppedVideo->name()==QFileInfo(videoFile).completeBaseName() && !droppedAudio->isPlaying() && !droppedVideo->isPlaying(),"dropped media preserves names file order and stopped state");
    check(!dropFiles(container->widget(),{QUrl::fromLocalFile(temp.path()),QUrl::fromLocalFile(temp.filePath("sources.sessions")),QUrl("https://example.com/video.mp4")}) && container->widgetCount()==beforeDrop+2,"non-media folders and remote URLs are rejected by Actions file drop");
    auto *reservedContainer=qobject_cast<WidgetContainer *>(bottomPanels->widget(2));
    check(!dropFiles(reservedContainer->widget(),{QUrl::fromLocalFile(videoFile)}) && reservedContainer->widgetCount()==0,"reserved panel does not create media on file drop");
    check(w.writeProject(temp.filePath("dropped-media.sessions")),"save directly dropped Multimedia actions");
    MainWindow droppedRestored;bool dropLoaded=droppedRestored.readProject(temp.filePath("dropped-media.sessions"));
    check(dropLoaded && droppedRestored.findChildren<Multimedia *>().size()==3,"directly dropped media actions survive project reopen");
    sources->synchronize();
    check(videoSource && !rowFor(videoSource->id),"Multimedia remains excluded from Sources after playback and drops");
    media->cue();waitFor([&]{return media->isPlaying() && videoSource->isVisible();});
    droppedVideo->cue();
    CanvasItem *secondVideoSource=nullptr;
    bool secondPreview=waitFor([&]{
        for(auto *base:w.canvas->items())if(auto *item=dynamic_cast<CanvasItem *>(base);item && item->mediaId==droppedVideo->id)secondVideoSource=item;
        return secondVideoSource && secondVideoSource->isVisible() && droppedVideo->isPlaying();
    });
    check(secondPreview && !videoSource->isVisible(),"Cue switches composition preview without leaving a previous video behind");
    droppedVideo->stop();QTest::qWait(150);
    check(!videoSource->isVisible() && secondVideoSource && !secondVideoSource->isVisible() && compositionIsBlack(),"Stop clears composition without revealing an older video");
    media->stop();
    auto actionMenu=[&](QWidget *target,const QString &actionName,QStringList *entries=nullptr) {
        bool invoked=false;
        QTimer::singleShot(0,[&]{
            if(auto *menu=qobject_cast<QMenu *>(app.activePopupWidget())) {
                if(entries)for(auto *action:menu->actions())entries->append(action->isSeparator()?"---":action->text());
                if(actionName.isEmpty()){invoked=true;menu->close();}
                else if(auto *action=menu->findChild<QAction *>(actionName);action && action->isEnabled()) {
                    invoked=true;QTest::mouseClick(menu,Qt::LeftButton,Qt::NoModifier,menu->actionGeometry(action).center());
                } else menu->close();
            }
        });
        QWidget *surface=target?target:container->widget();
        QContextMenuEvent context(QContextMenuEvent::Mouse,QPoint(15,15),surface->mapToGlobal(QPoint(15,15)));
        QApplication::sendEvent(surface,&context);return invoked;
    };
    QStringList multimediaMenu,backgroundMenu,timerMenu;
    actionMenu(droppedAudio,{},&multimediaMenu);actionMenu(nullptr,{},&backgroundMenu);actionMenu(countdown,{},&timerMenu);
    check(multimediaMenu==backgroundMenu && multimediaMenu==timerMenu && multimediaMenu.size()==12,"Multimedia Countdown and Actions background expose one identical menu");
    actionMenu(droppedAudio,{});
    check(container->selectedAction()==droppedAudio && droppedAudio->property("selected").toBool(),"right click selects Multimedia and background retains the selected action");
    droppedAudio->play();waitFor([&]{return droppedAudio->isPlaying() && droppedAudio->position()>100;});droppedAudio->pause();waitFor([&]{return droppedAudio->isPaused();});
    const qint64 originalMediaPosition=droppedAudio->position();
    droppedAudio->setLoop(true);
    check(actionMenu(droppedAudio,"copyAction") && actionMenu(nullptr,"pasteAction"),"common menu copies Multimedia and pastes from Actions background");
    auto *mediaCopy=qobject_cast<Multimedia *>(container->selectedAction());
    check(mediaCopy && mediaCopy!=droppedAudio && mediaCopy->id!=droppedAudio->id && mediaCopy->filePath()==droppedAudio->filePath() && mediaCopy->loops() && !mediaCopy->isPlaying() && droppedAudio->isPaused() && droppedAudio->position()==originalMediaPosition,"pasted Multimedia gets independent identity and Loop mode without disturbing source playback");
    droppedAudio->setLoop(false);
    if(!mediaCopy)return 1;
    bool mediaPropertiesOpened=false;
    QTimer propertiesCloser;propertiesCloser.setInterval(30);
    QObject::connect(&propertiesCloser,&QTimer::timeout,[&]{if(auto *dialog=qobject_cast<QDialog *>(app.activeModalWidget());dialog && dialog->objectName()=="MultimediaProperties"){mediaPropertiesOpened=true;dialog->reject();propertiesCloser.stop();}});
    propertiesCloser.start();
    check(actionMenu(mediaCopy,"actionProperties") && mediaPropertiesOpened,"common properties action opens Multimedia properties");
    propertiesCloser.stop();
    QPointer<Multimedia> cutMedia(mediaCopy);
    check(actionMenu(mediaCopy,"cutAction") && !cutMedia && actionMenu(nullptr,"pasteAction"),"common menu cuts and pastes Multimedia");
    mediaCopy=qobject_cast<Multimedia *>(container->selectedAction());
    check(mediaCopy && mediaCopy->filePath()==audioFile && actionMenu(nullptr,"moveActionUp") && container->selectedAction()==mediaCopy,"Actions background reorders the selected Multimedia");
    QPointer<Multimedia> removedMedia(mediaCopy);
    check(actionMenu(nullptr,"deleteAction") && !removedMedia && droppedAudio->isPaused(),"Actions background deletes selected Multimedia without affecting another action");
    droppedAudio->stop();
    const QString stillFile=temp.filePath("Imagen fija.png");
    QImage still(640,360,QImage::Format_RGB32);still.fill(QColor("#e04090"));still.save(stillFile);
    int beforeStill=container->widgetCount();
    check(dropFiles(container->widget(),{QUrl::fromLocalFile(stillFile)}) && container->widgetCount()==beforeStill+1,"dropping an image creates a stopped Multimedia action without dialogs");
    auto *stillAction=qobject_cast<Multimedia *>(container->widgetsInOrder().value(beforeStill));
    if(!stillAction)return 1;
    check(stillAction->isStillImage() && stillAction->name()=="Imagen fija" && !stillAction->isPlaying() && stillAction->currentFrame().isNull(),"image action is named from file and does not display automatically");
    check(stillAction->findChild<QPushButton *>("multimediaPause")->isHidden() && stillAction->findChild<QSlider *>("multimediaSlider")->isHidden() && stillAction->findChild<QLabel *>("MultimediaTime")->isHidden(),"still image card has Cue Play Stop without playback timeline");
    auto imageSource=[&](QGraphicsScene *scene)->CanvasItem *{
        for(auto *base:scene->items())if(auto *item=dynamic_cast<CanvasItem *>(base);item && item->mediaId==stillAction->id)return item;
        return nullptr;
    };
    QTest::mouseClick(stillAction->findChild<QPushButton *>("multimediaCue"),Qt::LeftButton);
    auto *stillSource=imageSource(w.canvas);
    check(stillSource && stillSource->isVisible() && !imageSource(w.outputScene),"image Cue displays composition only");
    sources->synchronize();
    check(stillSource && !rowFor(stillSource->id),"image actions remain excluded from Sources");
    w.launchComposition();
    check(!imageSource(w.outputScene),"Lanzar does not send a cued image before Play");
    QTest::mouseClick(stillAction->findChild<QPushButton *>("multimediaPlay"),Qt::LeftButton);
    check(imageSource(w.outputScene) && imageSource(w.outputScene)->isVisible() && stillAction->isPlaying(),"image Play sends the image to output");
    QImage stillOutput(1280,720,QImage::Format_RGB32);stillOutput.fill(Qt::black);
    {QPainter painter(&stillOutput);w.outputScene->render(&painter);}
    check(stillOutput.pixelColor(1200,680)==QColor("#e04090"),"output renders the fixed image with its original colors");
    QTest::qWait(200);
    check(stillAction->isPlaying() && imageSource(w.outputScene),"fixed image stays on output until explicitly stopped");
    check(w.writeProject(temp.filePath("image-actions.sessions")),"save a project containing a fixed image action");
    MainWindow stillRestored;bool stillLoaded=stillRestored.readProject(temp.filePath("image-actions.sessions"));
    Multimedia *restoredStill=nullptr;
    for(auto *entry:stillRestored.findChildren<Multimedia *>())if(entry->filePath()==stillFile)restoredStill=entry;
    bool restoredHidden=true;
    for(auto *base:stillRestored.canvas->items())if(auto *item=dynamic_cast<CanvasItem *>(base);item && item->mediaId==stillAction->id && item->isVisible())restoredHidden=false;
    check(stillLoaded && restoredStill && restoredStill->isStillImage() && !restoredStill->isPlaying() && restoredHidden,"reopened image action retains file and starts stopped with hidden composition image");
    QTest::mouseClick(stillAction->findChild<QPushButton *>("multimediaStop"),Qt::LeftButton);
    check(stillSource && !stillSource->isVisible() && !imageSource(w.outputScene) && !stillAction->isPlaying(),"image Stop removes it from both composition and output");
    stillAction->play();
    check(imageSource(w.outputScene) && stillSource->isVisible(),"image Play works directly after Stop without Cue");
    media->play();
    check(waitFor([&]{return videoInOutput() && media->isPlaying();}) && !imageSource(w.outputScene) && !stillSource->isVisible() && !stillAction->isPlaying(),"playing a video replaces a fixed image safely");
    stillAction->play();
    check(imageSource(w.outputScene) && !videoInOutput() && stillSource->isVisible(),"playing a fixed image replaces video output");
    media->stop();
    check(imageSource(w.outputScene) && stillSource->isVisible(),"stopping an inactive video preserves the current fixed image");
    stillAction->stop();stillAction->cue();stillAction->stop();
    check(!stillSource->isVisible() && !imageSource(w.outputScene),"Stop during image Cue clears composition without sending it to output");
    check(actionMenu(stillAction,"copyAction") && actionMenu(nullptr,"pasteAction"),"fixed image actions use the shared Actions copy and paste menu");
    auto *stillCopy=qobject_cast<Multimedia *>(container->selectedAction());
    check(stillCopy && stillCopy!=stillAction && stillCopy->isStillImage() && stillCopy->filePath()==stillFile && !stillCopy->isPlaying(),"copied image action has independent identity and remains stopped");
    if(stillCopy)container->moveWidget(stillCopy,0);
    check(stillCopy && container->widgetsInOrder().first()==stillCopy,"fixed image action can be relocated among other Actions");
    {
        MainWindow transition;transition.setFadeDuration(0);
        QImage green(640,360,QImage::Format_RGB32);green.fill(Qt::green);const QString greenFile=temp.filePath("green.png");green.save(greenFile);
        QFile fadeProject(temp.filePath("fades.sessions"));fadeProject.open(QIODevice::WriteOnly);
        fadeProject.write(QJsonDocument(QJsonObject{{"format","sessions"},{"version",1},{"items",QJsonArray()},
            {"widgets",QJsonArray{QJsonObject{{"type","multimedia"},{"name","Vídeo azul"},{"file",videoFile}},QJsonObject{{"type","multimedia"},{"name","Imagen verde"},{"file",greenFile}}}}}).toJson());fadeProject.close();
        check(transition.readProject(fadeProject.fileName()),"load transition fixture with video and fixed image");
        auto actions=transition.findChildren<Multimedia *>();Multimedia *blueVideo=nullptr,*greenImage=nullptr;
        for(auto *entry:actions){if(entry->filePath()==videoFile)blueVideo=entry;else if(entry->filePath()==greenFile)greenImage=entry;}
        if(!blueVideo || !greenImage)return 1;
        auto outputColor=[&]{QImage frame(1280,720,QImage::Format_RGB32);frame.fill(Qt::black);QPainter painter(&frame);transition.outputScene->render(&painter);return frame.pixelColor(640,360);};
        auto overlays=[&]{int count=0;for(auto *item:transition.outputScene->items())if(item->data(0).toString()=="outputFadeOverlay")++count;return count;};
        blueVideo->play();waitFor([&]{return blueVideo->isPlaying() && blueVideo->duration()>0 && isBlue(outputColor());});
        blueVideo->seek(blueVideo->duration()-250);
        check(waitFor([&]{return blueVideo->findChild<QLabel *>("MultimediaStatus")->text()=="Finalizado";}) && isBlue(outputColor()),"completed video retains its final frame before a transition");
        transition.setFadeDuration(500);greenImage->play();
        check(isBlue(outputColor()) && overlays()==1,"new image starts the fade from the retained final video frame");
        QTest::qWait(200);QColor blend=outputColor();
        check(blend.blue()>20 && blend.green()>20 && blend.blue()<240 && blend.green()<240,"video to image transition blends both images without a black frame");
        QTest::qWait(400);
        check(outputColor()==QColor(Qt::green) && overlays()==0,"crossfade ends on the new fixed image and removes its overlay");
        greenImage->stop();check(outputColor()==QColor(Qt::green) && overlays()==1,"Stop starts fading from the visible image");
        QTest::qWait(200);blend=outputColor();
        check(blend.green()>20 && blend.green()<240 && blend.blue()==0 && blend.red()==0,"Stop progressively fades the image toward black");
        QTest::qWait(400);check(outputColor()==QColor(Qt::black) && overlays()==0,"Stop fade completes on black");
        greenImage->play();QTest::qWait(120);QColor interrupted=outputColor();greenImage->stop();
        check(outputColor()==interrupted && overlays()==1,"Stop during a crossfade continues from the current blended image without a flash");
        greenImage->play();QTest::qWait(600);
        check(outputColor()==QColor(Qt::green) && overlays()==0,"Play during a Stop fade replaces the transition safely");
        blueVideo->play();
        check(outputColor()==QColor(Qt::green) && overlays()==0,"restarted video waits for a fresh decoded frame before beginning its fade");
        check(waitFor([&]{return overlays()==1;}),"fresh video frame starts the transition from the current image");
        QTest::qWait(600);check(isBlue(outputColor()) && overlays()==0,"image to video fade completes on the playing video");
        greenImage->play();QTest::qWait(600);
        blueVideo->cue();QTest::qWait(200);
        check(outputColor()==QColor(Qt::green) && overlays()==0,"Cue does not change or fade the current output");
        blueVideo->stop();check(outputColor()==QColor(Qt::green) && overlays()==0,"stopping a Cue preview does not fade another action on output");
        transition.setFadeDuration(0);greenImage->stop();
        check(outputColor()==QColor(Qt::black) && overlays()==0,"zero duration applies Stop immediately without an overlay");
        bool fadeDialogOpened=false;
        QTimer::singleShot(0,[&]{if(auto *dialog=qobject_cast<QDialog *>(app.activeModalWidget());dialog && dialog->objectName()=="FadeOptions"){
            dialog->grab().save(QApplication::applicationDirPath()+"/../build/fade-options.png");
            auto *seconds=dialog->findChild<QDoubleSpinBox *>("fadeSeconds");fadeDialogOpened=seconds && seconds->value()==0 && seconds->maximum()==10;if(seconds)seconds->setValue(2);dialog->reject();}});
        if(auto *action=transition.findChild<QAction *>("fadeOptions"))action->trigger();
        check(fadeDialogOpened && transition.fadeDuration()==0,"Options offers fade duration in seconds and Cancel preserves its previous value");
    }
    auto *zoomSelector=w.findChild<QSlider *>("compositionZoom");
    auto *zoomLabel=w.findChild<QLabel *>("compositionZoomLevel");
    auto *zoomBar=w.findChild<QWidget *>("CompositionZoomBar");
    check(!w.findChild<QStatusBar *>() && zoomBar && zoomBar->parentWidget()==w.view->parentWidget(),"zoom controls belong to composition footer and the global bottom status bar is removed");
    auto sendZoomWheel=[&](int delta,Qt::KeyboardModifiers modifiers){
        const QPoint point=w.view->viewport()->rect().center();QTest::mouseMove(w.view->viewport(),point);
        QWheelEvent wheel(QPointF(point),QPointF(w.view->viewport()->mapToGlobal(point)),QPoint(),QPoint(0,delta),Qt::NoButton,modifiers,Qt::NoScrollPhase,false);
        QApplication::sendEvent(w.view->viewport(),&wheel);
    };
    w.view->setZoom(8);check(qAbs(w.view->transform().m11()-8)<0.00001 && zoomSelector && zoomLabel && zoomLabel->text()=="800 %","composition supports zoom beyond the previous 500 percent limit and displays its level");
    w.view->setZoom(32);sendZoomWheel(120,Qt::ControlModifier);
    check(qAbs(w.view->transform().m11()-32)<0.00001,"composition zoom clamps precisely at 3200 percent");
    auto *precise=w.addItem(new CanvasItem(CanvasItem::Shape::Rectangle),{100,100});precise->setItemSize({60,60});
    w.view->centerOn(precise->mapToScene(precise->selectionRect().bottomRight()));app.processEvents();
    const QPoint fineCorner=w.view->mapFromScene(precise->mapToScene(precise->selectionRect().bottomRight()));
    drag(fineCorner,fineCorner+QPoint(4,4));
    check(precise->itemSize().width()>60 && precise->itemSize().width()<61 && qAbs(precise->x()-100)<.01 && qAbs(precise->y()-100)<.01,"3200 percent zoom allows subpixel object resizing while preserving the opposite corner");
    delete precise;
    w.view->setZoom(.05);sendZoomWheel(-120,Qt::ControlModifier);
    check(qAbs(w.view->transform().m11()-.05)<0.00001,"composition zoom clamps precisely at 5 percent");
    w.view->setZoom(1);sendZoomWheel(120,Qt::ControlModifier|Qt::ShiftModifier);
    check(qAbs(w.view->transform().m11()-1.02)<0.00001,"Ctrl Shift wheel offers fine two-percent zoom steps");
    sendZoomWheel(0,Qt::ControlModifier);check(qAbs(w.view->transform().m11()-1.02)<0.00001,"empty wheel movement does not change zoom");
    w.view->setZoom(4);w.resize(w.width()+50,w.height()+40);app.processEvents();
    check(qAbs(w.view->transform().m11()-4)<0.00001,"manual zoom survives resizing the window");
    if(zoomSelector)zoomSelector->setValue(1000);
    check(qAbs(w.view->transform().m11()-32)<0.00001,"composition zoom slider reaches 3200 percent");
    if(zoomSelector)zoomSelector->setValue(500);
    const qreal midpoint=.05*std::sqrt(640.0);
    check(qAbs(w.view->transform().m11()-midpoint)<0.00001,"composition zoom slider provides a useful logarithmic range");
    if(auto *more=w.findChild<QToolButton *>("compositionZoomIn"))QTest::mouseClick(more,Qt::LeftButton);
    check(qAbs(w.view->transform().m11()-midpoint*1.15)<0.00001,"magnifying glass zoom-in button changes composition zoom");
    if(auto *less=w.findChild<QToolButton *>("compositionZoomOut"))QTest::mouseClick(less,Qt::LeftButton);
    check(qAbs(w.view->transform().m11()-midpoint)<0.00001,"magnifying glass zoom-out button changes composition zoom");
    w.view->fitCanvas();const qreal fitBefore=w.view->transform().m11();w.resize(w.width()+100,w.height()+100);app.processEvents();
    check(w.view->isCanvasFitted() && w.view->transform().m11()!=fitBefore && zoomSelector,"fit mode continues adapting to available space after using the zoom slider");
    {
        MainWindow editor;editor.setFadeDuration(0);editor.show();editor.activateWindow();app.processEvents();editor.view->fitCanvas();
        auto *panel=editor.findChild<SourcePanel *>("SourcesPanel");auto *rows=panel->findChild<QTreeWidget *>("sourcesList");
        auto selectSource=[&](CanvasItem *item){panel->synchronize();for(int i=0;i<rows->topLevelItemCount();++i)if(rows->topLevelItem(i)->data(0,Qt::UserRole).toString()==item->id){rows->setCurrentItem(rows->topLevelItem(i));rows->scrollToItem(rows->topLevelItem(i));app.processEvents();break;}};
        auto clipboardMenu=[&](bool fromSources,const QString &actionName,QPointF scenePoint=QPointF(1100,650)) {
            bool invoked=false;
            QTimer::singleShot(0,[&]{if(auto *menu=qobject_cast<QMenu *>(app.activePopupWidget())){
                if(auto *action=menu->findChild<QAction *>(actionName);action && action->isEnabled()){invoked=true;QTest::mouseClick(menu,Qt::LeftButton,Qt::NoModifier,menu->actionGeometry(action).center());}else menu->close();
            }});
            QWidget *surface=fromSources?rows->viewport():editor.view->viewport();
            QPoint point=fromSources?(rows->currentItem()?rows->visualItemRect(rows->currentItem()).center():QPoint(5,5)):editor.view->mapFromScene(scenePoint);
            QContextMenuEvent context(QContextMenuEvent::Mouse,point,surface->mapToGlobal(point));QApplication::sendEvent(surface,&context);return invoked;
        };
        check(!clipboardMenu(true,"itemPaste") && !clipboardMenu(false,"itemPaste"),"Sources and composition disable Paste when the element clipboard is empty");
        auto *original=editor.addItem(new CanvasItem("Texto copiado"),{80,100});original->name="Etiqueta";original->color=QColor("#d5ad5d");original->setTextFont(QFont("Arial",27,QFont::Bold));original->setItemSize({240,90});selectSource(original);
        check(clipboardMenu(true,"itemCopy") && clipboardMenu(false,"itemPaste",{800,500}),"copy from Sources pastes through the composition context menu");
        CanvasItem *duplicate=nullptr;for(auto *base:editor.canvas->selectedItems())duplicate=dynamic_cast<CanvasItem *>(base);
        check(duplicate && duplicate!=original && duplicate->id!=original->id && duplicate->text==original->text && duplicate->font==original->font && duplicate->color==original->color && duplicate->itemSize()==original->itemSize(),"pasted text preserves appearance and has an independent identifier");
        if(!duplicate)return 1;
        const QPointF pastePoint=editor.view->mapToScene(editor.view->mapFromScene(QPointF(800,500)));
        check(QLineF(duplicate->mapToScene(duplicate->selectionRect().topLeft()),pastePoint).length()<.01,"composition context Paste places the copied item at the pointer");
        const QString duplicateId=duplicate->id;QPointer<CanvasItem> removed=duplicate;const QPointF duplicateCenter=duplicate->mapToScene(duplicate->selectionRect().center());
        check(clipboardMenu(false,"itemCut",duplicateCenter) && !removed && clipboardMenu(true,"itemPaste"),"cut from composition removes the item and pastes through Sources");
        CanvasItem *moved=nullptr;for(auto *base:editor.canvas->items())if(auto *item=dynamic_cast<CanvasItem *>(base);item && item->id==duplicateId)moved=item;
        check(moved && moved->text==original->text && editor.canvas->items().size()==2,"first paste after Cut preserves the moved element identity");
        QImage pixels(80,40,QImage::Format_RGB32);pixels.fill(QColor("#a76b8f"));
        auto *photo=editor.addItem(new CanvasItem(pixels),{50,300});photo->name="Foto";photo->setZValue(2);
        auto *stroke=editor.addItem(new CanvasItem(CanvasItem::Shape::Line),{300,300});stroke->name="Trazo";stroke->setItemSize({350,1});stroke->color=Qt::red;stroke->setZValue(3);
        editor.canvas->clearSelection();photo->setSelected(true);stroke->setSelected(true);
        check(clipboardMenu(false,"itemCopy",photo->mapToScene(photo->selectionRect().center())) && clipboardMenu(true,"itemPaste"),"composition copies multiple selected elements and Sources pastes the group");
        CanvasItem *photoCopy=nullptr,*strokeCopy=nullptr;
        for(auto *base:editor.canvas->selectedItems())if(auto *item=dynamic_cast<CanvasItem *>(base)){if(item->isImage())photoCopy=item;else if(item->shapeType()==CanvasItem::Shape::Line)strokeCopy=item;}
        check(photoCopy && strokeCopy && photoCopy->image==pixels && photoCopy->id!=photo->id && strokeCopy->id!=stroke->id && strokeCopy->itemSize()==stroke->itemSize() && strokeCopy->color==stroke->color && strokeCopy->pos()-photoCopy->pos()==stroke->pos()-photo->pos() && strokeCopy->zValue()>photoCopy->zValue(),"group paste preserves embedded image line geometry relative positions and layer order");
        stroke->setVisible(false);stroke->setLocked(true);selectSource(stroke);
        QSet<QString> previousIds;for(auto *base:editor.canvas->items())if(auto *item=dynamic_cast<CanvasItem *>(base))previousIds.insert(item->id);
        check(clipboardMenu(true,"itemCopy") && clipboardMenu(true,"itemPaste"),"Sources can copy and paste a hidden locked source");
        CanvasItem *hiddenCopy=nullptr;for(auto *base:editor.canvas->items())if(auto *item=dynamic_cast<CanvasItem *>(base);item && !previousIds.contains(item->id))hiddenCopy=item;
        check(hiddenCopy && hiddenCopy->isLocked() && !hiddenCopy->isVisible(),"pasting preserves source visibility and lock settings");
        auto *timer=new Countdown;timer->setName("Temporizador vinculado");timer->setCompositionScene(editor.canvas);timer->setOutputFormat("HH:MM:SS.C",false);timer->configure(12,original);
        for(auto *container:editor.findChildren<WidgetContainer *>())if(container->countdownCreationEnabled()){container->addWidget(timer);break;}
        timer->start();QTest::qWait(100);timer->pause();const QString originalId=original->id;QPointer<CanvasItem> cutOriginal=original;selectSource(original);
        check(clipboardMenu(true,"itemCut") && !cutOriginal && !timer->associatedText() && timer->state()==Countdown::State::Stopped,"cutting an associated text stops Countdown and leaves no dangling association");
        check(clipboardMenu(false,"itemPaste",{400,150}) && timer->associatedText() && timer->associatedText()->id==originalId && timer->state()==Countdown::State::Stopped && timer->outputFormat()=="HH:MM:SS.C","pasting a cut text restores its Countdown association without starting it");
        auto *restoredText=timer->associatedText();if(!restoredText)return 1;
        editor.canvas->clearSelection();restoredText->setSelected(true);editor.view->setFocus();app.processEvents();const int beforeKeys=editor.canvas->items().size();
        QTest::keyClick(editor.view->viewport(),Qt::Key_C,Qt::ControlModifier);QTest::keyClick(editor.view->viewport(),Qt::Key_V,Qt::ControlModifier);
        check(editor.canvas->items().size()==beforeKeys+1 && timer->associatedText()==restoredText,"composition Ctrl C and Ctrl V duplicate text without copying its Countdown ownership");
        selectSource(restoredText);rows->setFocus();app.processEvents();QTest::keyClick(rows,Qt::Key_C,Qt::ControlModifier);QTest::keyClick(rows,Qt::Key_V,Qt::ControlModifier);
        check(editor.canvas->items().size()==beforeKeys+2,"Sources Ctrl C and Ctrl V use the shared element clipboard");
        const QString copiedProject=temp.filePath("copied-elements.sessions");MainWindow reload;
        check(editor.writeProject(copiedProject) && reload.readProject(copiedProject) && reload.canvas->items().size()==editor.canvas->items().size(),"copied images shapes texts and restored Countdown binding survive project reopening");
    }
    {
        MainWindow defaults;defaults.show();app.processEvents();
        auto addWithDefault=[&](MainWindow &window,const QString &label)->QString {
            QString proposed;
            QTimer::singleShot(0,[&]{
                if(auto *dialog=qobject_cast<QInputDialog *>(app.activeModalWidget())){proposed=dialog->textValue();dialog->accept();}
                else if(auto *dialog=qobject_cast<QDialog *>(app.activeModalWidget());dialog && dialog->objectName()=="AddTextDialog") {
                    auto *name=dialog->findChild<QLineEdit *>("elementName");proposed=name->text();
                    auto *buttons=dialog->findChild<QDialogButtonBox *>();bool enabled=false;
                    for(auto *button:buttons->buttons())if(buttons->buttonRole(button)==QDialogButtonBox::AcceptRole)enabled=button->isEnabled();
                    check(enabled,"default text name enables adding without typing a custom name");dialog->accept();
                }
            });
            for(auto *action:window.findChildren<QAction *>())if(action->text()==label){action->trigger();break;}
            return proposed;
        };
        check(addWithDefault(defaults,"Añadir texto…")=="texto1" && addWithDefault(defaults,"Añadir texto…")=="texto2","new texts receive consecutive editable default names");
        auto *existing=defaults.addItem(new CanvasItem("Texto existente"),{350,100});existing->name="TEXTO3";
        check(addWithDefault(defaults,"Añadir texto…")=="texto4","default names avoid existing names regardless of letter case");
        check(addWithDefault(defaults,"Añadir rectángulo…")=="rectángulo1" && addWithDefault(defaults,"Añadir rectángulo…")=="rectángulo2","basic shapes receive numbered default names in their existing name dialog");
        const QString defaultsFile=temp.filePath("default-names.sessions");MainWindow reloadedNames;
        check(defaults.writeProject(defaultsFile) && reloadedNames.readProject(defaultsFile) && addWithDefault(reloadedNames,"Añadir texto…")=="texto5","numbered names remain unique after reopening a saved composition");
    }
    auto renameCountdown=[&](Countdown *timer,const QString &next,bool accept,bool *allowed=nullptr){
        QTimer::singleShot(0,[&]{if(auto *dialog=qobject_cast<QDialog *>(app.activeModalWidget());dialog && dialog->objectName()=="CountdownProperties"){
            auto *edit=dialog->findChild<QLineEdit *>("countdownName");
            if(!edit){dialog->reject();return;}
            edit->setText(next);bool enabled=false;
            for(auto *button:dialog->findChild<QDialogButtonBox *>()->buttons())if(dialog->findChild<QDialogButtonBox *>()->buttonRole(button)==QDialogButtonBox::AcceptRole)enabled=button->isEnabled();
            if(allowed)*allowed=enabled;
            dialog->grab().save(QApplication::applicationDirPath()+"/../build/properties-countdown-name.png");
            if(accept && enabled)dialog->accept();else dialog->reject();
        }});
        timer->showProperties();
    };
    Countdown unassigned;unassigned.setName("countdown1");bool nameAllowed=false;
    renameCountdown(&unassigned,"Cuenta sin asociación",true,&nameAllowed);
    check(nameAllowed && unassigned.name()=="Cuenta sin asociación" && !unassigned.associatedText(),"Countdown properties can rename an action before associating a text");
    renameCountdown(&unassigned,"   ",true,&nameAllowed);
    check(!nameAllowed && unassigned.name()=="Cuenta sin asociación","Countdown properties reject an empty name");
    renameCountdown(&unassigned,"Nombre descartado",false);
    check(unassigned.name()=="Cuenta sin asociación","Cancel discards Countdown name changes");
    countdown->start();QTest::qWait(150);countdown->pause();const auto frozenSeconds=countdown->remainingSeconds();auto *linkedText=countdown->associatedText();
    const QString frozenText=linkedText?linkedText->text:QString();
    renameCountdown(countdown,"  Cuenta principal  ",true);
    check(countdown->name()=="Cuenta principal" && countdown->findChild<QLabel *>("CountdownTitle")->toolTip()=="Cuenta principal" && countdown->state()==Countdown::State::Paused && countdown->remainingSeconds()==frozenSeconds && countdown->associatedText()==linkedText && linkedText && linkedText->text==frozenText,"renaming a paused Countdown updates its title without resetting time state or association");
    MainWindow renamedReload;const QString renamedProject=temp.filePath("renamed-countdown.sessions");
    bool nameSaved=w.writeProject(renamedProject) && renamedReload.readProject(renamedProject);bool nameRestored=false;
    for(auto *timer:renamedReload.findChildren<Countdown *>())if(timer->name()=="Cuenta principal")nameRestored=true;
    check(nameSaved && nameRestored,"Countdown name edited in properties survives saving and reopening");
    countdown->stop();
    w.activateWindow();app.processEvents();
    {
        auto *total=w.addItem(new CanvasItem("Total"),{500,400});total->name="Tiempo total";
        countdown->configure(3661,countdown->associatedText());countdown->setOutputFormat("HH:MM:SS.C");
        QTimer::singleShot(0,[&]{if(auto *dialog=qobject_cast<QDialog *>(app.activeModalWidget())) {
            auto *combo=dialog->findChild<QComboBox *>("countdownTotalText");
            if(combo)combo->setCurrentIndex(combo->findText("Tiempo total"));
            dialog->accept();
        }});countdown->showProperties();
        check(countdown->totalText()==total && total->text=="01:01:01.0","Countdown properties associate a separate text with the complete formatted duration");
        countdown->start();QTest::qWait(220);countdown->pause();QTest::qWait(550);
        check(total->text=="01:01:01.0" && !total->timerHidden() && !total->timerOverdue(),"total duration stays fixed and visible while remaining time advances or blinks");
        countdown->stop();countdown->setOutputFormat("MM:SS");countdown->configure(125,countdown->associatedText());
        check(total->text=="02:05","changing duration and format updates the total text immediately");
        container->selectAction(countdown);container->copyAction();container->pasteAction();
        auto *totalCopy=qobject_cast<Countdown *>(container->selectedAction());
        check(totalCopy && totalCopy!=countdown && totalCopy->totalText()==total,"copied Countdown retains its total text association");
        if(totalCopy && totalCopy!=countdown)container->deleteAction();
        MainWindow loadedTotal;const auto saved=temp.filePath("total-text.sessions");
        bool savedTotal=w.writeProject(saved) && loadedTotal.readProject(saved),foundTotal=false;
        for(auto *timer:loadedTotal.findChildren<Countdown *>())if(timer->name()==countdown->name() && timer->totalText() && timer->totalText()->text=="02:05")foundTotal=true;
        check(savedTotal && foundTotal,"total text binding and formatting survive a project roundtrip");
        QPointer<CanvasItem> deletedTotal=total;delete total;countdown->showTime();
        check(!deletedTotal && !countdown->totalText(),"deleting the total text safely releases its association");
    }
    {
        // Use an isolated scene to verify actual mixed frames and action chaining.
        MainWindow mixed;mixed.setFadeDuration(0);mixed.resize(1800,900);mixed.show();
        const QString blueFile=temp.filePath("mix-blue.png"),redFile=temp.filePath("mix-red.png");
        QImage color(320,180,QImage::Format_RGB32);color.fill(Qt::blue);color.save(blueFile);color.fill(Qt::red);color.save(redFile);
        QJsonArray actions;
        for(const auto &file:{blueFile,redFile,videoFile,audioFile})actions.append(QJsonObject{{"type","multimedia"},{"name",QFileInfo(file).baseName()},{"file",file}});
        const auto project=temp.filePath("mix-fixture.sessions");
        {QFile file(project);file.open(QIODevice::WriteOnly);file.write(QJsonDocument(QJsonObject{{"format","sessions"},{"version",1},{"items",QJsonArray()},{"widgets",actions}}).toJson());}
        check(mixed.readProject(project),"load isolated Multimedia mixing fixture");
        auto *host=mixed.findChild<QSplitter *>("bottomSplitter")->widget(0)->findChild<WidgetContainer *>();const auto ordered=host->widgetsInOrder();
        auto *blue=qobject_cast<Multimedia *>(ordered.value(0)),*red=qobject_cast<Multimedia *>(ordered.value(1));
        auto *clip=qobject_cast<Multimedia *>(ordered.value(2)),*audio=qobject_cast<Multimedia *>(ordered.value(3));
        if(blue && red && clip && audio) {
            auto renderColor=[&]{QImage frame(1280,720,QImage::Format_RGB32);frame.fill(Qt::black);QPainter p(&frame);mixed.outputScene->render(&p);p.end();return frame.pixelColor(640,360);};
            blue->play();auto *mixToggle=red->findChild<QPushButton *>("multimediaMix");
            QTest::mouseClick(mixToggle,Qt::LeftButton);app.processEvents();
            check(mixToggle->isCheckable() && mixToggle->isChecked() && red->mixes() && !red->isPlaying() && renderColor().blue()>240,"Mix toggles its mode without starting playback or changing output");
            red->play();app.processEvents();const QColor blended=renderColor();
            check(blue->isPlaying() && red->isPlaying() && blended.red()>110 && blended.blue()>110 && blended.green()<10,"Mix keeps both images active and blends their output equally");
            red->stop();app.processEvents();
            check(blue->isPlaying() && renderColor().blue()>240 && renderColor().red()<10,"stopping one mixed image preserves the other at full opacity");
            clip->setMix(true);clip->play();check(waitFor([&]{return clip->isPlaying() && clip->hasCurrentFrame();}),"Play with Mix enabled starts a video alongside an existing image");
            audio->setMix(true);audio->play();check(waitFor([&]{return audio->isPlaying();}) && clip->isPlaying(),"Play with Mix enabled plays audio and video simultaneously");
            audio->stop();check(clip->isPlaying(),"stopping mixed audio leaves video playing");
            QTest::mouseClick(mixToggle,Qt::LeftButton);app.processEvents();
            check(!red->mixes() && clip->isPlaying() && renderColor().blue()>0,"disabling Mix changes only the mode without interrupting other playback");
            red->play();app.processEvents();check(!clip->isPlaying() && !blue->isPlaying() && renderColor().red()>240,"Play with Mix disabled replaces mixed contents with the selected object");
            audio->cue();check(waitFor([&]{return audio->isPlaying();}) && !audio->isAudioMuted(),"audio Cue plays unmuted on the default audio device");audio->stop();
            clip->setPurge(true);audio->setNext(true);host->selectAction(clip);host->copyAction();host->pasteAction();
            auto *copyMedia=qobject_cast<Multimedia *>(host->selectedAction());
            check(copyMedia && copyMedia->purges() && copyMedia->mixes(),"copying Multimedia retains Purge and Mix modes");if(copyMedia)host->deleteAction();
            // Restore the intended order after testing the action clipboard.
            host->moveWidget(audio,host->widgetCount());
            const auto flagsProject=temp.filePath("media-flags.sessions");MainWindow flags;
            bool flagsSaved=mixed.writeProject(flagsProject) && flags.readProject(flagsProject);bool purgeSaved=false,nextSaved=false,mixSaved=false;
            for(auto *item:flags.findChildren<Multimedia *>()){if(item->filePath()==videoFile){purgeSaved=item->purges();mixSaved=item->mixes();}if(item->filePath()==audioFile)nextSaved=item->playsNext();}
            check(flagsSaved && purgeSaved && nextSaved && mixSaved,"Purge Next and Mix modes survive saving and reopening");
            clip->play();check(waitFor([&]{return clip->duration()>0 && clip->isPlaying();}),"start video for automatic Next and Purge");
            QPointer<Multimedia> removedClip=clip;clip->seek(clip->duration()-600);
            check(waitFor([&]{return !removedClip && audio->isPlaying();},6000),"completion purges the finished action and starts the next enabled Multimedia");audio->stop();
            app.processEvents();auto *mini=red->findChild<QPushButton *>("multimediaPlay");auto *cue=red->findChild<QPushButton *>("multimediaCue");
            check(mini->width()>=64 && cue->width()>=64 && mini->y()==cue->y() && mini->y()>red->findChild<QPushButton *>("multimediaNext")->y() && red->findChild<QLabel *>("MultimediaStatus")->isHidden(),"larger Multimedia playback controls occupy a second row below the header without a status label");
            mixed.grab().save(QApplication::applicationDirPath()+"/../build/multimedia-header.png");
        } else check(false,"mixing fixture contains four valid Multimedia actions");
    }
    check(w.grab().save(QApplication::applicationDirPath()+"/../build/preview.png"),"save interface preview");
    return failures ? 1 : 0;
}

int mediaLoadTest(QApplication &app) {
    const auto args=app.arguments();const int option=args.indexOf("--media-load-test");
    if(option<0 || option+1>=args.size() || !QFileInfo(args[option+1]).isFile())return 2;
    for(int i=0;i<8;++i) {
        Multimedia media;media.configure("Prueba MOV",args[option+1]);
        QElapsedTimer clock;clock.start();
        while(media.thumbnail().isNull() && clock.elapsed()<7000)QTest::qWait(10);
        std::printf("LOAD %d thumbnail=%dx%d playing=%d\n",i,media.thumbnail().width(),media.thumbnail().height(),media.isPlaying());std::fflush(stdout);
        if(media.thumbnail().isNull() || media.isPlaying())return 1;
        if(i<3) {
            media.cue();clock.restart();
            while((!media.isPlaying() || !media.hasCurrentFrame()) && clock.elapsed()<7000)QTest::qWait(10);
            if(!media.isPlaying() || !media.hasCurrentFrame())return 1;
            media.stop();media.play();clock.restart();
            while((!media.isPlaying() || !media.hasCurrentFrame()) && clock.elapsed()<7000)QTest::qWait(10);
            if(!media.isPlaying() || !media.hasCurrentFrame())return 1;
            media.stop();
            std::printf("TRANSPORT %d Cue/Stop/Play/Stop OK\n",i);std::fflush(stdout);
        }
    }
    return 0;
}

int playbackTest(QApplication &app) {
    const auto args=app.arguments();const int option=args.indexOf("--playback-test");
    if(option<0 || option+1>=args.size() || !QFileInfo(args[option+1]).isFile()){std::printf("PLAYBACK invalid arguments: %s\n",qPrintable(args.join(" | ")));return 2;}
    QTemporaryDir temporary(QApplication::applicationDirPath()+"/../build/playback-XXXXXX");
    QFile project(temporary.filePath("playback.sessions"));if(!project.open(QIODevice::WriteOnly)){std::printf("PLAYBACK cannot write fixture\n");return 2;}
    project.write(QJsonDocument(QJsonObject{{"format","sessions"},{"version",1},{"items",QJsonArray()},
        {"widgets",QJsonArray{QJsonObject{{"type","multimedia"},{"name","Prueba de reproducción"},{"file",QFileInfo(args[option+1]).absoluteFilePath()}}}}}).toJson());project.close();
    MainWindow window;window.show();app.processEvents();
    if(!window.readProject(project.fileName())){std::printf("PLAYBACK cannot load fixture: %s\n",qPrintable(project.fileName()));return 2;}
    auto *media=window.findChild<Multimedia *>();if(!media){std::printf("PLAYBACK missing widget\n");return 2;}
    QElapsedTimer clock;clock.start();qint64 first=-1,last=-1,maxGap=0,lastPulse=0,maxPulse=0;
    int frameCount=0,uniqueFrames=0;bool launched=false,finished=false;
    quint64 previousHash=0;qint64 lastChange=-1,maxFreeze=0,nextReport=1000;QImage lastImage;
    QObject::connect(media,&Multimedia::frameReady,&window,[&](const QImage &frame){
        qint64 now=clock.elapsed();if(first<0)first=now;
        lastImage=frame;
        if(last>=0)maxGap=qMax(maxGap,now-last);
        last=now;++frameCount;
        quint64 hash=1469598103934665603ull;
        for(int y=frame.height()/16;y<frame.height();y+=qMax(1,frame.height()/8))
            for(int x=frame.width()/16;x<frame.width();x+=qMax(1,frame.width()/8))hash=(hash^frame.pixel(x,y))*1099511628211ull;
        if(hash!=previousHash){if(lastChange>=0)maxFreeze=qMax(maxFreeze,now-lastChange);lastChange=now;++uniqueFrames;previousHash=hash;}
    });
    QTimer pulse;pulse.setInterval(16);QObject::connect(&pulse,&QTimer::timeout,&window,[&]{
        qint64 now=clock.elapsed();if(lastPulse)maxPulse=qMax(maxPulse,now-lastPulse);lastPulse=now;
    });pulse.start();
    media->cue();
    while(clock.elapsed()<qMax(qint64(20000),media->duration()+10000)) {
        QTest::qWait(10);
        if(clock.elapsed()>=nextReport){std::printf("SAMPLE wall=%lld media=%lld frames=%d unique=%d playing=%d\n",static_cast<long long>(clock.elapsed()),static_cast<long long>(media->position()),frameCount,uniqueFrames,media->isPlaying()?1:0);nextReport+=1000;}
        if(!launched && media->position()>=media->duration()/2 && media->duration()>0){std::printf("SWITCH wall=%lld\n",static_cast<long long>(clock.elapsed()));media->play();launched=true;}
        if(media->findChild<QLabel *>("MultimediaStatus")->text()=="Finalizado"){finished=true;break;}
        if(media->findChild<QLabel *>("MultimediaStatus")->text().contains("Error"))break;
    }
    std::printf("PLAYBACK frames=%d first_ms=%lld last_ms=%lld max_frame_gap_ms=%lld max_ui_gap_ms=%lld duration_ms=%lld finished=%d\n",
        frameCount,static_cast<long long>(first),static_cast<long long>(last),static_cast<long long>(maxGap),static_cast<long long>(maxPulse),static_cast<long long>(media->duration()),finished?1:0);
    std::printf("MOTION unique=%d max_freeze_ms=%lld\n",uniqueFrames,static_cast<long long>(maxFreeze));
    bool retainedOutput=false;
    for(auto *base:window.outputScene->items())if(auto *item=dynamic_cast<CanvasItem *>(base);item && item->mediaId==media->id && item->isVisible() && item->image==lastImage)retainedOutput=true;
    QTest::qWait(500);
    const bool prepared=finished && media->position()==media->duration() && media->currentFrame()==lastImage && !media->isPlaying() && !media->isPaused() && retainedOutput;
    std::printf("FINISHED last_frame_retained=%d output_retained=%d\n",prepared?1:0,retainedOutput?1:0);
    return prepared && frameCount>10 && maxGap<500 && maxPulse<500 && maxFreeze<500 && last-first<media->duration()+1500?0:1;
}
