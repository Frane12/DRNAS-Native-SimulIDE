#include <QtGlobal>
#ifdef Q_OS_ANDROID
#include "mainwindow.h"
#include "circuitwidget.h"
#include "circuitview.h"
#include "editorwindow.h"
#include "componentlist.h"
#include <QtAndroidExtras/QtAndroid>
#include <QtAndroidExtras/QAndroidJniObject>
#include <QApplication>
#include <QDockWidget>
#include <QToolButton>
#include <QLineEdit>
#include <QPushButton>
#include <QResizeEvent>
#include <QScreen>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <QActionGroup>
#include <QtMath>

void MainWindow::setupAndroidUi()
{
    m_androidBar = new QToolBar(tr("Mobile controls"), this);
    m_androidBar->setObjectName("drnasMobileControls");
    m_androidBar->setMovable(false);
    m_androidBar->setFloatable(false);
    m_androidBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    m_androidBar->setIconSize(QSize(26, 26));
    m_androidBar->setStyleSheet("QToolButton { font-size:14px; min-width:56px; min-height:48px; padding:4px; }");
    addToolBar(Qt::TopToolBarArea, m_androidBar);

    m_androidComponentsDock = new QDockWidget(tr("Components / Files"), this);
    m_androidComponentsDock->setObjectName("drnasComponentsDock");
    m_androidComponentsDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::BottomDockWidgetArea);
    m_androidComponentsDock->setFeatures(QDockWidget::DockWidgetClosable);
    m_androidComponentsDock->setWidget(m_sidepanel);
    // Large touch-friendly close control, independent of the dock title decoration.
    auto closeParts = new QToolButton(m_androidComponentsDock);
    closeParts->setText(QString::fromUtf8("×"));
    closeParts->setToolTip(tr("Close components panel"));
    closeParts->setMinimumSize(44, 44);
    connect(closeParts, &QToolButton::clicked, m_androidComponentsDock, &QDockWidget::hide);
    m_androidComponentsDock->setTitleBarWidget(closeParts);
    addDockWidget(Qt::LeftDockWidgetArea, m_androidComponentsDock);
    m_androidComponentsDock->hide();
    auto parts = m_androidComponentsDock->toggleViewAction();
    parts->setText(tr("Components"));
    parts->setIcon(QIcon(":/ic2.png"));
    m_androidBar->addAction(parts);
    connect(m_androidComponentsDock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (visible) QTimer::singleShot(0, this, [this] { applyAndroidLayout(true); });
    });

    auto modes = new QActionGroup(this);
    auto circuit = m_androidBar->addAction(QIcon(":/zoomfit.svg"), tr("Circuit"));
    auto editor = m_androidBar->addAction(QIcon(":/new.svg"), tr("Editor"));
    circuit->setCheckable(true); editor->setCheckable(true);
    modes->addAction(circuit); modes->addAction(editor);
    circuit->setChecked(true);
    m_editor->hide();
    connect(circuit, &QAction::triggered, this, [this] {
        m_editor->hide(); m_circuitW->show();
    });
    connect(editor, &QAction::triggered, this, [this] {
        m_circuitW->hide(); m_editor->show();
        m_androidComponentsDock->hide();
    });
    auto closePanel = m_androidBar->addAction(tr("Close panel"));
    connect(closePanel, &QAction::triggered, this, [this] {
        m_androidComponentsDock->hide();
    });
    auto output = m_androidBar->addAction(QIcon(":/serialterm.png"), tr("Output"));
    output->setCheckable(true);
    auto lowerPanel = m_circuitW->splitter()->widget(1);
    lowerPanel->hide();
    connect(output, &QAction::toggled, this, [this, lowerPanel, circuit](bool visible) {
        circuit->trigger();
        lowerPanel->setVisible(visible);
        if (visible) m_circuitW->splitter()->setSizes({qMax(120, height()*3/4), qMax(100, height()/4)});
    });

    m_searchComponent->setFixedHeight(44);
    m_clearButton->setFixedSize(44, 44);
    m_clearButton->setIconSize(QSize(26, 26));
    m_sidepanel->setIconSize(QSize(28, 28));
    m_sidepanel->setStyleSheet("QTabBar::tab { min-height:40px; min-width:60px; }");
    m_components->setIconSize(QSize(32, 28));
    m_components->setIndentation(20);
    m_sidepanel->setStyleSheet(m_sidepanel->styleSheet() + QStringLiteral("QScrollBar:vertical { width: 24px; background: #e6e6e6; } QScrollBar::handle:vertical { background: #9098a2; min-height: 56px; border-radius: 7px; margin: 2px; } QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; } QScrollBar:horizontal { height: 24px; background: #e6e6e6; } QScrollBar::handle:horizontal { background: #9098a2; min-width: 56px; border-radius: 7px; margin: 2px; } QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0px; }"));
    for (auto bar : findChildren<QToolBar*>()) {
        if (bar == m_androidBar) continue;
        bar->setIconSize(QSize(30, 30));
        bar->setMovable(false);
        bar->setStyleSheet("QToolButton { min-width:40px; min-height:40px; padding:2px; }");
    }
    m_mainSplitter->setChildrenCollapsible(false);
    // Both primary simulation controls stay first, before overflow on phones.
    auto bar = findChild<QToolBar*>("m_circToolBar");
    if (bar) {
        auto start = m_circuitW->findChild<QAction*>("drnasStartSimulation");
        auto pause = m_circuitW->findChild<QAction*>("drnasPauseSimulation");
        if (start && pause) {
            bar->removeAction(start); bar->removeAction(pause);
            auto first = bar->actions().isEmpty() ? nullptr : bar->actions().first();
            bar->insertAction(first, start); bar->insertAction(first, pause);
        }
        auto plus = new QAction("+", bar);
        auto minus = new QAction(QString::fromUtf8("\xE2\x88\x92"), bar);
        plus->setToolTip(tr("Zoom in")); minus->setToolTip(tr("Zoom out"));
        auto before = bar->actions().size() > 2 ? bar->actions().at(2) : nullptr;
        bar->insertAction(before, plus); bar->insertAction(before, minus);
        connect(plus, &QAction::triggered, this, [] { CircuitView::self()->androidZoomBy(1.25); });
        connect(minus, &QAction::triggered, this, [] { CircuitView::self()->androidZoomBy(0.8); });
    }
    connect(qApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
        if (state != Qt::ApplicationActive) return;
        QtAndroid::androidActivity().callMethod<void>("requestImmersive");
        QTimer::singleShot(300, this, [this] { applyAndroidLayout(false); });
    });
    applyAndroidLayout(true);
    QtAndroid::androidActivity().callMethod<void>("requestImmersive");
}

void MainWindow::applyAndroidLayout(bool force)
{
    if (!m_androidComponentsDock) return;
    const QSize size = screen() ? screen()->geometry().size() : this->size();
    const bool landscape = size.width() > size.height();
    if (force || landscape != m_androidLandscape) {
        m_androidLandscape = landscape;
        const bool shown = !m_androidComponentsDock->isHidden();
        const QSignalBlocker blocker(m_androidComponentsDock);
        addDockWidget(landscape ? Qt::LeftDockWidgetArea : Qt::BottomDockWidgetArea, m_androidComponentsDock);
        m_androidComponentsDock->setVisible(shown);
        m_androidComponentsDock->setMinimumSize(0, 0);
        if (landscape) {
            m_androidComponentsDock->setMaximumSize(qMax(240, width()/2), QWIDGETSIZE_MAX);
            resizeDocks({m_androidComponentsDock}, {qBound(220, width()/3, 340)}, Qt::Horizontal);
        } else {
            m_androidComponentsDock->setMaximumSize(QWIDGETSIZE_MAX, qMax(120, height()/2));
            resizeDocks({m_androidComponentsDock}, {qMax(120, height()*2/5)}, Qt::Vertical);
        }
    }
    // Fill the display while keeping controls clear of a camera cutout.
    const auto activity = QtAndroid::androidActivity();
    const qreal dpr = screen() ? screen()->devicePixelRatio() : 1.0;
    setContentsMargins(qCeil(activity.callMethod<jint>("safeInsetLeft")/dpr),
                       qCeil(activity.callMethod<jint>("safeInsetTop")/dpr),
                       qCeil(activity.callMethod<jint>("safeInsetRight")/dpr),
                       qCeil(activity.callMethod<jint>("safeInsetBottom")/dpr));
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    if (!m_androidComponentsDock) return;
    QTimer::singleShot(0, this, [this] { applyAndroidLayout(false); });
    QTimer::singleShot(300, this, [this] { applyAndroidLayout(false); });
}
#endif
