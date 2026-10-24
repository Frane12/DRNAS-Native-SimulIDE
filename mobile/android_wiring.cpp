#include <QtGlobal>
#ifdef Q_OS_ANDROID
#include "circuitview.h"
#include "circuit.h"
#include "pin.h"
#include "connector.h"
#include "connectorline.h"
#include <QApplication>
#include <QGraphicsSceneMouseEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QTimer>
#include <QLineF>
#include <QtAndroidExtras/QtAndroid>

namespace {
// Measure in viewport logical pixels, so the target stays usable at every zoom.
Pin* nearestFreePin(CircuitView* view, const QPoint& point)
{
    const int radius = 22;
    const QPolygonF area = view->mapToScene(QRect(point-QPoint(radius,radius), QSize(2*radius+1,2*radius+1)));
    Pin* best = nullptr;
    qreal bestDistance = radius;
    auto circuit = Circuit::self();
    for (auto item : view->scene()->items(area.boundingRect(), Qt::IntersectsItemBoundingRect)) {
        if (item->type() != Pin::Type) continue;
        auto pin = static_cast<Pin*>(item);
        if (!pin->isVisible() || pin->unused() || pin->connector()) continue;
        if (circuit->is_constarted()) {
            if (circuit->getNewConnector()->startPin() == pin) continue;
            if (circuit->getNewConnector()->isBus() != pin->isBus()) continue;
        }
        const qreal distance = QLineF(QPointF(point), view->viewportTransform().map(pin->scenePos())).length();
        if (distance < bestDistance || (best && qFuzzyCompare(distance, bestDistance) && pin->pinId() < best->pinId())) {
            best = pin;
            bestDistance = distance;
        }
    }
    return best;
}

bool activatePin(CircuitView* view, Pin* pin)
{
    QGraphicsSceneMouseEvent click(QEvent::GraphicsSceneMousePress);
    click.setButton(Qt::LeftButton);
    click.setButtons(Qt::LeftButton);
    click.setScenePos(pin->scenePos());
    click.setPos(QPointF(0, 0));
    click.setScreenPos(view->viewport()->mapToGlobal(view->mapFromScene(pin->scenePos())));
    click.setAccepted(false);
    view->scene()->sendEvent(pin, &click);
    return click.isAccepted();
}

void pinHaptic()
{
    QtAndroid::runOnAndroidThread([] {
        auto window = QtAndroid::androidActivity().callObjectMethod("getWindow", "()Landroid/view/Window;");
        auto decor = window.callObjectMethod("getDecorView", "()Landroid/view/View;");
        // KEYBOARD_TAP respects the user's haptic setting; no vibration permission.
        decor.callMethod<jboolean>("performHapticFeedback", "(I)Z", 3);
    });
}
}

void CircuitView::androidWireFeedback(const QString& message, Pin* pin)
{
    if (!m_androidWireLabel) {
        m_androidWireLabel = new QLabel(viewport());
        m_androidWireLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        m_androidWireLabel->setWordWrap(true);
        m_androidWireLabel->setStyleSheet("background:rgba(23,8,68,225); color:white; border-radius:6px; padding:8px; font-size:15px;");
        m_androidPinMarker = new QWidget(viewport());
        m_androidPinMarker->setAttribute(Qt::WA_TransparentForMouseEvents);
        m_androidPinMarker->setFixedSize(38, 38);
        m_androidPinMarker->setStyleSheet("background:rgba(30,200,220,35); border:3px solid #00cce5; border-radius:19px;");
    }
    m_androidWireLabel->setFixedWidth(qMax(80, qMin(380, viewport()->width()-16)));
    m_androidWireLabel->setText(message);
    m_androidWireLabel->adjustSize();
    m_androidWireLabel->move(8, 8);
    m_androidWireLabel->show(); m_androidWireLabel->raise();
    if (pin) {
        const QPoint point = mapFromScene(pin->scenePos());
        m_androidPinMarker->move(point-QPoint(19,19));
        m_androidPinMarker->show(); m_androidPinMarker->raise();
        if (m_androidTargetPin != pin->pinId()) pinHaptic();
        m_androidTargetPin = pin->pinId();
    } else {
        m_androidPinMarker->hide();
        m_androidTargetPin.clear();
    }
}

void CircuitView::androidCancelWire()
{
    if (!m_circuit || !m_circuit->is_constarted()) return;
    m_circuit->deleteNewConnector(); // Existing cancel path also rolls back the open undo step.
    m_androidWireGesture = false;
    androidWireFeedback(tr("Wire cancelled"), nullptr);
    QTimer::singleShot(1200, this, [this] {
        if (m_androidWireLabel && !m_circuit->is_constarted()) m_androidWireLabel->hide();
    });
}

bool CircuitView::androidWireEvent(QMouseEvent* event)
{
    if (!m_circuit || event->modifiers() != Qt::NoModifier) return false;
    const bool press = event->type() == QEvent::MouseButtonPress;
    const bool release = event->type() == QEvent::MouseButtonRelease;
    if ((press || release) && event->button() != Qt::LeftButton) return false;
    if (release && m_androidSkipWireRelease) {
        m_androidSkipWireRelease = false;
        m_androidWireGesture = false;
        event->accept(); return true;
    }
    if (press) {
        m_androidSkipWireRelease = false;
        Pin* pin = nearestFreePin(this, event->pos());
        if (!pin && !m_circuit->is_constarted()) return false; // Normal component dragging.
        // Existing wire junctions keep their upstream click/split behavior.
        // Do not turn a tap on an existing wire into an empty route corner.
        auto hit = itemAt(event->pos());
        if (!pin && hit && hit->type() == ConnectorLine::Type) return false;
        // A deliberate press on the component body still moves the component.
        if (pin && !m_circuit->is_constarted() && itemAt(event->pos()) == pin->component()
            && !pin->contains(pin->mapFromScene(mapToScene(event->pos())))) return false;
        m_androidWireGesture = true;
        m_androidNewWireGesture = !m_circuit->is_constarted();
        m_androidWirePress = event->pos();
        if (pin) {
            const QString id = pin->pinId();
            if (!activatePin(this, pin)) { m_androidWireGesture=false; return false; }
            if (m_circuit->is_constarted())
                androidWireFeedback(tr("Wire started: %1. Drag or tap another pin. Cancel stops the wire.").arg(id), pin);
            else {
                androidWireFeedback(tr("Connected to %1").arg(id), pin);
                QTimer::singleShot(1000, this, [this] {
                    if (!m_circuit->is_constarted() && m_androidWireLabel) {
                        m_androidWireLabel->hide(); m_androidPinMarker->hide();
                    }
                });
            }
        }
        event->accept();
        return true;
    }
    if (!m_androidWireGesture) return false;
    // A tap on the source arms the wire. Do not silently connect to an adjacent
    // dense-IC pin just because the occupied source is no longer eligible.
    if (release && m_androidNewWireGesture
        && (event->pos()-m_androidWirePress).manhattanLength() < QApplication::startDragDistance()) {
        m_androidWireGesture = false;
        event->accept(); return true;
    }
    if (m_circuit->is_constarted()) {
        Pin* target = nearestFreePin(this, event->pos());
        const QPointF end = target ? target->scenePos() : mapToScene(event->pos());
        m_circuit->getNewConnector()->updateConRoute(nullptr, end);
        if (target) androidWireFeedback(tr("Pin: %1 — release to connect").arg(target->pinId()), target);
        else androidWireFeedback(tr("Drawing wire — move onto a free pin, or tap Cancel"), nullptr);
        if (release) {
            if (target) {
                const QString id = target->pinId();
                if (activatePin(this, target) && !m_circuit->is_constarted()) {
                    androidWireFeedback(tr("Connected to %1").arg(id), target);
                    QTimer::singleShot(1000, this, [this] {
                        if (!m_circuit->is_constarted() && m_androidWireLabel) {
                            m_androidWireLabel->hide(); m_androidPinMarker->hide();
                        }
                    });
                }
            } else if (!m_androidNewWireGesture || (event->pos()-m_androidWirePress).manhattanLength() >= QApplication::startDragDistance()) {
                m_circuit->getNewConnector()->incActLine();
            }
        }
    }
    if (release) m_androidWireGesture = false;
    event->accept();
    return true;
}
void CircuitView::keyPressEvent(QKeyEvent* event)
{
    if ((event->key() == Qt::Key_Back || event->key() == Qt::Key_Escape)
        && m_circuit && m_circuit->is_constarted()) {
        androidCancelWire(); event->accept(); return;
    }
    QGraphicsView::keyPressEvent(event);
}
#endif
