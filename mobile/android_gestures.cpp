#include <QtGlobal>
#ifdef Q_OS_ANDROID
#include "circuitview.h"
#include "touch_camera_math.h"
#include <QGestureEvent>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsItem>
#include <QPinchGesture>
#include <QtMath>
#include <QLabel>

void CircuitView::androidZoomBy(qreal factor)
{
    const qreal current = qMax(qreal(0.001), transform().m11());
    const qreal next = qBound(qreal(0.25), current * factor, qreal(8.0));
    scale(next/current, next/current);
    m_scale = next;
}

bool CircuitView::viewportEvent(QEvent* event)
{
    if (event->type() == QEvent::TouchBegin) m_androidCameraGesture = false;
    if (event->type() == QEvent::TouchCancel) {
        m_androidCameraGesture = false;
        m_androidWireGesture = false;
        m_androidSkipWireRelease = true;
    }
    if (event->type() == QEvent::Gesture) {
        auto gestures = static_cast<QGestureEvent*>(event);
        if (auto pinch = static_cast<QPinchGesture*>(gestures->gesture(Qt::PinchGesture))) {
            // Qt's pinch recognizer also reports parallel two-finger motion;
            // its centerPoint is in global logical screen coordinates.
            const QPointF globalCenter = pinch->centerPoint();
            if (pinch->state() == Qt::GestureStarted) {
                // Finish the original item's single-finger grab without
                // sending a scene release that would add a wire corner.
                if (scene() && scene()->mouseGrabberItem()) {
                    auto grabbed = scene()->mouseGrabberItem();
                    QGraphicsSceneMouseEvent release(QEvent::GraphicsSceneMouseRelease);
                    release.setButton(Qt::LeftButton);
                    release.setButtons(Qt::NoButton);
                    const QPointF point = mapToScene(viewport()->rect().center());
                    release.setScenePos(point);
                    release.setPos(grabbed->mapFromScene(point));
                    scene()->sendEvent(grabbed, &release);
                    if (scene()->mouseGrabberItem() == grabbed) grabbed->ungrabMouse();
                }
                m_androidPinchScale = transform().m11();
                m_androidLastPinchCenter = globalCenter;
                m_androidGestureViewport = viewport()->size();
                m_androidCameraGesture = true;
                m_androidWireGesture = false;
                m_androidSkipWireRelease = true;
                if (m_androidWireLabel) m_androidWireLabel->hide();
                if (m_androidPinMarker) m_androidPinMarker->hide();
                m_androidTargetPin.clear();
            }
            if (pinch->state() == Qt::GestureStarted || pinch->state() == Qt::GestureUpdated) {
                // A resize/rotation must not turn old screen coordinates into
                // a camera jump. Begin a new anchor at the current midpoint.
                if (m_androidGestureViewport != viewport()->size()) {
                    m_androidLastPinchCenter = globalCenter;
                    m_androidGestureViewport = viewport()->size();
                    if (pinch->totalScaleFactor() > 0)
                        m_androidPinchScale = transform().m11()/pinch->totalScaleFactor();
                }
                const QPointF origin = viewport()->mapToGlobal(QPoint(0,0));
                const QPointF previous = m_androidLastPinchCenter-origin;
                const QPointF current = globalCenter-origin;
                const QPointF middle = viewport()->rect().center();
                const QPointF oldCenter = mapToScene(viewport()->rect().center());
                const qreal oldScale = qMax(qreal(0.001), transform().m11());
                const qreal nextScale = qBound(qreal(0.25), m_androidPinchScale*pinch->totalScaleFactor(), qreal(8.0));
                const auto center = DrnasTouch::cameraCenter(
                    {oldCenter.x(),oldCenter.y()}, {middle.x(),middle.y()},
                    {previous.x(),previous.y()}, {current.x(),current.y()}, oldScale,nextScale);
                androidZoomBy(nextScale/oldScale);
                centerOn(center.x,center.y);
                m_androidLastPinchCenter = globalCenter;
            } else if (pinch->state() == Qt::GestureFinished || pinch->state() == Qt::GestureCanceled) {
                m_androidCameraGesture = false;
                m_androidSkipWireRelease = true;
            }
            gestures->accept(pinch);
            return true;
        }
    }
    return QGraphicsView::viewportEvent(event);
}
#endif
