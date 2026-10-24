#include <QtGlobal>
#ifdef Q_OS_ANDROID
#include "mainwindow.h"
#include "circuitwidget.h"
#include "circuit.h"
#include "circuitview.h"
#include "editorwindow.h"
#include "componentlist.h"
#include <QtAndroidExtras/QtAndroid>
#include <QtAndroidExtras/QAndroidJniObject>
#include <QApplication>
#include <QDockWidget>
#include <QToolButton>
#include <QHBoxLayout>
#include <QLabel>
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
#include <QGridLayout>
#include <QVBoxLayout>
#include <QMenu>
#include <QWidgetAction>
#include <QStringList>
#include <QtMath>

void installDrnasDialogControls();

void MainWindow::setupAndroidUi()
{
    installDrnasDialogControls();
    m_androidBar = new QToolBar(tr("Mobile controls"), this);
    m_androidBar->setObjectName("drnasMobileControls");
    m_androidBar->setMovable(false);
    m_androidBar->setFloatable(false);
    m_androidBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    m_androidBar->setIconSize(QSize(22, 22));
    m_androidBar->setStyleSheet("QToolBar { spacing:0px; padding:0px; border:0px; } QToolButton { font-size:11px; min-width:0px; min-height:38px; padding:2px; }");
    addToolBar(Qt::TopToolBarArea, m_androidBar);

    m_androidComponentsDock = new QDockWidget(tr("Components / Files"), this);
    m_androidComponentsDock->setObjectName("drnasComponentsDock");
    m_androidComponentsDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::BottomDockWidgetArea);
    m_androidComponentsDock->setFeatures(QDockWidget::DockWidgetClosable);
    m_androidComponentsDock->setWidget(m_sidepanel);
    // Large touch-friendly close control, independent of the dock title decoration.
    auto dockTitle = new QWidget(m_androidComponentsDock);
    auto dockTitleLayout = new QHBoxLayout(dockTitle);
    dockTitleLayout->setContentsMargins(8, 2, 4, 2);
    dockTitleLayout->addWidget(new QLabel(tr("Components / Files"), dockTitle));
    dockTitleLayout->addStretch();
    auto closeParts = new QToolButton(dockTitle);
    closeParts->setText(QString::fromUtf8("× Close"));
    closeParts->setToolTip(tr("Close components panel"));
    closeParts->setMinimumSize(44, 44);
    connect(closeParts, &QToolButton::clicked, m_androidComponentsDock, &QDockWidget::hide);
    dockTitleLayout->addWidget(closeParts);
    m_androidComponentsDock->setTitleBarWidget(dockTitle);
    addDockWidget(Qt::LeftDockWidgetArea, m_androidComponentsDock);
    m_androidComponentsDock->hide();
    auto parts = m_androidComponentsDock->toggleViewAction();
    parts->setText(tr("Parts"));
    parts->setIcon(QIcon(":/ic2.png"));
    m_androidBar->addAction(parts);
    connect(m_androidComponentsDock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        Q_UNUSED(visible);
        QTimer::singleShot(0, this, [this] { applyAndroidLayout(true); });
    });

    auto circuit = m_androidBar->addAction(QIcon(":/zoomfit.svg"), tr("Circuit"));
    auto editor = m_androidBar->addAction(QIcon(":/new.svg"), tr("Editor"));
    editor->setCheckable(true);
    m_editor->hide();
    connect(circuit, &QAction::triggered, this, [this, editor] {
        editor->setChecked(false);
        m_editor->hide(); m_circuitW->show();
        QTimer::singleShot(0, this, [this] { applyAndroidLayout(false); });
    });
    connect(editor, &QAction::triggered, this, [this](bool checked) {
        m_circuitW->setVisible(!checked); m_editor->setVisible(checked);
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

    auto closePanels = m_androidBar->addAction(QIcon(":/remove.svg"), tr("Close"));
    connect(closePanels, &QAction::triggered, this, [this, circuit, output] {
        m_androidComponentsDock->hide();
        output->setChecked(false);
        circuit->trigger();
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
    // A wrapping grid keeps camera and cancel controls visible at every width.
    // Reuse upstream QActions so simulation, selection and menus keep their state.
    auto bar = findChild<QToolBar*>("m_circToolBar");
    if (bar) {
        auto controls = new QWidget(m_circuitW);
        controls->setObjectName("drnasCircuitControls");
        controls->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        auto grid = new QGridLayout(controls);
        grid->setContentsMargins(2, 2, 2, 2);
        grid->setSpacing(2);
        controls->setStyleSheet("QToolButton { min-width:0px; min-height:0px; padding:2px; font-size:20px; }");
        auto more = new QMenu(tr("More circuit commands"), controls);
        const QStringList primaryNames = {"drnasStartSimulation", "drnasPauseSimulation",
                                          "drnasZoomFit", "drnasZoomSelected"};
        QLabel* status = nullptr;
        for (auto action : bar->actions()) {
            if (primaryNames.contains(action->objectName())) continue;
            // Toolbar widget actions do not render usefully in a menu. Expose
            // their existing recent-files/help menus, omit desktop spacers.
            if (auto widgetAction = qobject_cast<QWidgetAction*>(action)) {
                if (auto label = qobject_cast<QLabel*>(widgetAction->defaultWidget())) {
                    status = label;
                    status->setParent(controls);
                    status->setObjectName("drnasCircuitStatus");
                    status->setMaximumWidth(QWIDGETSIZE_MAX);
                    status->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
                    status->setVisible(!status->text().trimmed().isEmpty());
                }
                if (auto button = qobject_cast<QToolButton*>(widgetAction->defaultWidget())) {
                    if (button->menu()) {
                        auto submenu = more->addMenu(button->toolTip());
                        submenu->addActions(button->menu()->actions());
                    }
                }
            } else more->addAction(action);
        }
        auto addButton = [controls](QAction* action) {
            auto button = new QToolButton(controls);
            button->setProperty("drnasControl", true);
            button->setToolButtonStyle(Qt::ToolButtonIconOnly);
            button->setDefaultAction(action);
            button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            return button;
        };
        for (const auto& name : primaryNames.mid(0, 2)) {
            if (auto action = m_circuitW->findChild<QAction*>(name)) addButton(action);
        }
        auto cancelWire = new QAction(QIcon(":/remove.svg"), tr("Cancel wire"), controls);
        cancelWire->setEnabled(false);
        addButton(cancelWire);
        connect(cancelWire, &QAction::triggered, this, [] { CircuitView::self()->androidCancelWire(); });
        auto wireState = new QTimer(controls);
        connect(wireState, &QTimer::timeout, controls, [cancelWire, status] {
            if (status) status->setVisible(!status->text().trimmed().isEmpty());
            cancelWire->setEnabled(Circuit::self() && Circuit::self()->is_constarted());
        });
        wireState->start(100);
        auto plus = new QAction("+", controls);
        auto minus = new QAction(QString::fromUtf8("−"), controls);
        plus->setToolTip(tr("Zoom in")); minus->setToolTip(tr("Zoom out"));
        addButton(plus); addButton(minus);
        connect(plus, &QAction::triggered, this, [] { CircuitView::self()->androidZoomBy(1.25); });
        connect(minus, &QAction::triggered, this, [] { CircuitView::self()->androidZoomBy(0.8); });
        for (const auto& name : primaryNames.mid(2)) {
            if (auto action = m_circuitW->findChild<QAction*>(name)) addButton(action);
        }
        auto moreButton = addButton(new QAction(QString::fromUtf8("…"), controls));
        moreButton->setToolTip(tr("Files, settings and more"));
        moreButton->setMenu(more);
        moreButton->setPopupMode(QToolButton::InstantPopup);
        if (auto layout = qobject_cast<QVBoxLayout*>(m_circuitW->layout()))
            layout->insertWidget(0, controls);
        bar->hide();
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
    // Use actual available widget width, including display scaling and docks.
    if (auto controls = m_circuitW->findChild<QWidget*>("drnasCircuitControls")) {
        auto grid = qobject_cast<QGridLayout*>(controls->layout());
        const auto buttons = controls->findChildren<QToolButton*>(QString(), Qt::FindDirectChildrenOnly);
        const int available = qMax(1, m_circuitW->width() - 4);
        const int maxColumns = qMax(1, (available + 2) / 42);
        const int rows = qMax(1, (buttons.size() + maxColumns - 1) / maxColumns);
        const int columns = qMax(1, (buttons.size() + rows - 1) / rows);
        if (controls->property("columns").toInt() != columns || force) {
            controls->setProperty("columns", columns);
            while (auto item = grid->takeAt(0)) delete item;
            for (int column = 0; column < 8; ++column) grid->setColumnStretch(column, 0);
            for (int i = 0; i < buttons.size(); ++i) {
                buttons[i]->setIconSize(QSize(22, 22));
                buttons[i]->setFixedHeight(40);
                grid->addWidget(buttons[i], i / columns, i % columns);
            }
            for (int column = 0; column < columns; ++column) grid->setColumnStretch(column, 1);
            if (auto status = controls->findChild<QLabel*>("drnasCircuitStatus"))
                grid->addWidget(status, rows, 0, 1, columns);
        }
    }
    // Full fit is the default; optional cutout margins are set at startup.
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
