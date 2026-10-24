#!/usr/bin/env python3
"""Apply the phone UI to the pinned source, with exact checked anchors."""
from pathlib import Path
import shutil
import sys

root = Path(sys.argv[1]).resolve()
repo = Path(__file__).resolve().parent.parent
shutil.copytree(repo / 'android', root / 'android', dirs_exist_ok=True)
for source in (repo / 'mobile').iterdir():
    if source.suffix not in ('.cpp', '.h'): continue
    shutil.copy2(source, root / 'src' / source.name)


def replace(path, old, new):
    p = root / path
    text = p.read_text()
    if new in text:
        return
    if text.count(old) != 1:
        raise SystemExit(f'Mobile patch anchor missing or ambiguous in {path}: {old[:70]}')
    p.write_text(text.replace(old, new))


replace('SimulIDE.pri', '    OS = Android\n', '    OS = Android\n    QT += androidextras\n')
replace('SimulIDE.pri', '    ANDROID_VERSION_NAME = 0.1.0', '    ANDROID_VERSION_NAME = 0.6.0')
replace('SimulIDE.pri', '    ANDROID_VERSION_CODE = 1\n', '    ANDROID_VERSION_CODE = 6\n')
replace('src/mainwindow.h', '        void hideGui();', '''        void hideGui();
#ifdef Q_OS_ANDROID
        void setupAndroidUi();
#endif''')
replace('src/mainwindow.h', '        void closeEvent( QCloseEvent* event );', '''        void closeEvent( QCloseEvent* event );
#ifdef Q_OS_ANDROID
        void resizeEvent(QResizeEvent* event) override;
#endif''')
replace('src/mainwindow.h', '        void createWidgets();', '''        void createWidgets();
#ifdef Q_OS_ANDROID
        void applyAndroidLayout(bool force = false);
        QDockWidget* m_androidComponentsDock = nullptr;
        QToolBar* m_androidBar = nullptr;
        bool m_androidLandscape = false;
#endif''')
# Forward-declare globally, never accidentally declare MainWindow::QDockWidget.
replace('src/mainwindow.h', 'class ComponentList;', 'class QDockWidget;\nclass QToolBar;\nclass ComponentList;')
replace('src/mainwindow.cpp', '    setFontScale( scale );', '''#ifdef Q_OS_ANDROID
    scale = 1.35f; // logical pixels; Android/Qt already supplies display density
#endif
    setFontScale( scale );''')
replace('src/mainwindow.cpp', '    this->showMaximized();', '''#ifndef Q_OS_ANDROID
    this->showMaximized();
#endif''')
restore = '''    restoreGeometry( m_settings->value("geometry" ).toByteArray());
    restoreState(    m_settings->value("windowState" ).toByteArray());
    m_mainSplitter->restoreState( m_settings->value("Centralsplitter/geometry").toByteArray());
    CircuitWidget::self()->splitter()->restoreState( m_settings->value("Circsplitter/geometry").toByteArray());'''
replace('src/mainwindow.cpp', restore, '#ifndef Q_OS_ANDROID\n' + restore + '\n#endif')
replace('src/gui/circuitwidget/circuitwidget.cpp', '    powerCircAct->setStatusTip(tr("Start Simulation"));', '''    powerCircAct->setObjectName("drnasStartSimulation");
    powerCircAct->setStatusTip(tr("Start Simulation"));''')
for action, name in [('zoomFitAct', 'drnasZoomFit'), ('zoomSelAct', 'drnasZoomSelected')]:
    anchor = f'    {action}->setStatusTip('
    # Anchor the exact original status line without depending on translated text.
    path = root / 'src/gui/circuitwidget/circuitwidget.cpp'
    line = next(line for line in path.read_text().splitlines() if line.startswith(anchor))
    replace(str(path), line, f'    {action}->setObjectName("{name}");\n' + line)
replace('src/gui/circuitwidget/circuitwidget.cpp', '    pauseSimAct->setStatusTip(tr("Pause Simulation"));', '''    pauseSimAct->setObjectName("drnasPauseSimulation");
    pauseSimAct->setStatusTip(tr("Pause Simulation"));''')
replace('src/gui/circuitwidget/circuitview.h', '        void clear();', '''        void clear();
#ifdef Q_OS_ANDROID
        void androidZoomBy(qreal factor);
        void androidCancelWire();
        bool androidWireEvent(QMouseEvent* event);
        void androidWireFeedback(const QString& message, Pin* pin);
        QLabel* m_androidWireLabel = nullptr;
        QTimer* m_androidWireHintTimer = nullptr;
        QWidget* m_androidPinMarker = nullptr;
        QString m_androidTargetPin;
        QPoint m_androidWirePress;
        bool m_androidWireGesture = false;
        bool m_androidNewWireGesture = false;
        bool m_androidSkipWireRelease = false;
#endif''')
replace('src/gui/circuitwidget/circuitview.h', '        void wheelEvent( QWheelEvent* event ) override;', '''#ifdef Q_OS_ANDROID
        bool viewportEvent(QEvent* event) override;
        void keyPressEvent(QKeyEvent* event) override;
        qreal m_androidPinchScale = 1.5;
        QPointF m_androidLastPinchCenter;
        QSize m_androidGestureViewport;
        bool m_androidCameraGesture = false;
#endif
        void wheelEvent( QWheelEvent* event ) override;''')
replace('src/gui/circuitwidget/circuitview.cpp', '    setResizeAnchor( AnchorUnderMouse );', '''    setResizeAnchor( AnchorUnderMouse );
#ifdef Q_OS_ANDROID
    setResizeAnchor(AnchorViewCenter);
    setTransformationAnchor(AnchorViewCenter);
    viewport()->grabGesture(Qt::PinchGesture);
#endif''')
replace('src/gui/circuitwidget/circuitview.cpp', '''    centerOn( 0, 0 );
}''', '''    centerOn( 0, 0 );
#ifdef Q_OS_ANDROID
    androidZoomBy(1.5);
#endif
}''')
replace('src/main.cpp', '#include <QDirIterator>', '''#include <QDirIterator>
#include <QVBoxLayout>
#include <QtAndroidExtras/QtAndroid>
#include <QLabel>
#include <QTimer>''')
replace('src/main.cpp', '    QApplication app( argc, argv );', '''#ifdef Q_OS_ANDROID
    const jfloat displayScale = QtAndroid::androidActivity().callMethod<jfloat>("chooseDisplayScale", "()F");
    const qreal safeScale = qBound(qreal(0.6), qreal(displayScale), qreal(1.4));
    qputenv("QT_SCALE_FACTOR", QByteArray::number(safeScale));
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
    QApplication app( argc, argv );
#ifdef Q_OS_ANDROID
    // Preserve the established applicationName and private data location for upgrades.
    app.setApplicationDisplayName("DRNAS SimulIDE");
    QFont mobileFont = app.font();
    mobileFont.setPixelSize(16);
    app.setFont(mobileFont);
    app.setStyleSheet("QTreeView::item { min-height:36px; padding-top:2px; padding-bottom:2px; }"
                     "QMenu::item { min-height:36px; padding:6px 14px; }"
                     "QPushButton { min-height:36px; }"
                     "QComboBox { min-height:36px; }"
                     "QSplitter::handle { width:12px; height:12px; }");
    auto splash = new QWidget(nullptr, Qt::SplashScreen | Qt::FramelessWindowHint);
    splash->setObjectName("drnasWelcome");
    splash->setStyleSheet("QWidget#drnasWelcome { background:#170844; } QLabel { color:#F5EFCE; background:transparent; }");
    auto welcome = new QVBoxLayout(splash);
    welcome->setContentsMargins(24, 24, 24, 24);
    welcome->setSpacing(10);
    welcome->addStretch();
    auto logo = new QLabel(splash);
    logo->setAlignment(Qt::AlignCenter);
    logo->setPixmap(QPixmap("assets:/drnas_icon.png").scaled(112, 112, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    welcome->addWidget(logo);
    auto brand = new QLabel("DRNAS EMU", splash);
    brand->setAlignment(Qt::AlignCenter);
    brand->setStyleSheet("font-size:18px; font-weight:600; letter-spacing:3px;");
    welcome->addWidget(brand);
    auto title = new QLabel("SimulIDE", splash);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("font-size:32px; font-weight:700;");
    welcome->addWidget(title);
    welcome->addStretch();
    splash->showFullScreen();
    app.processEvents();
#endif''')
replace('src/main.cpp', '    window.show();', '''#ifdef Q_OS_ANDROID
    window.setupAndroidUi();
    window.showFullScreen();
    // The initial brand screen yields to the ready main window; no user action needed.
    splash->raise();
    QTimer::singleShot(900, &window, [splash] { splash->close(); splash->deleteLater(); });
#else
    window.show();
#endif''')
# Same source asset powers the Android launcher and in-app welcome screen.
shutil.copy2(repo / 'android/res/drawable-nodpi/drnas_icon.png', root / 'android/assets/drnas_icon.png')
print('Applied mobile fullscreen, rotation, touch density, branding and launcher assets.')

# Enlarge only the Android interaction target. Leave Pin::boundingRect and
# collision/auto-connect geometry unchanged for the circuit engine.
replace('src/gui/circuitwidget/circuitview.h', 'class Component;', 'class Pin;\nclass QLabel;\nclass QTimer;\nclass Component;')
for method in ['mousePressEvent', 'mouseMoveEvent', 'mouseReleaseEvent']:
    anchor = f'void CircuitView::{method}( QMouseEvent* event )\n{{'
    replace('src/gui/circuitwidget/circuitview.cpp', anchor, anchor + '\n#ifdef Q_OS_ANDROID\n    if (androidWireEvent(event)) return;\n#endif')
replace('src/mainwindow.cpp', 'void MainWindow::keyPressEvent( QKeyEvent* event)\n{', '''void MainWindow::keyPressEvent( QKeyEvent* event)
{
#ifdef Q_OS_ANDROID
    if ((event->key() == Qt::Key_Back || event->key() == Qt::Key_Escape) && Circuit::self()->is_constarted()) {
        CircuitView::self()->androidCancelWire();
        event->accept();
        return;
    }
#endif''')
replace('src/mainwindow.cpp', '#include \"circuitwidget.h\"', '#include \"circuitwidget.h\"\n#include \"circuitview.h\"')
