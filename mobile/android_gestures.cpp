#include <QtGlobal>
#ifdef Q_OS_ANDROID
#include "circuitview.h"
#include <QGestureEvent>
#include <QPinchGesture>
#include <QtMath>

void CircuitView::androidZoomBy(qreal factor)
{
    const qreal current = qMax(qreal(0.001), transform().m11());
    const qreal next = qBound(qreal(0.25), current * factor, qreal(8.0));
    scale(next/current, next/current);
    m_scale = next;
}

bool CircuitView::viewportEvent(QEvent* event)
{
    if (event->type() == QEvent::Gesture) {
        auto gestures = static_cast<QGestureEvent*>(event);
        if (auto pinch = static_cast<QPinchGesture*>(gestures->gesture(Qt::PinchGesture))) {
            if (pinch->state() == Qt::GestureStarted) m_androidPinchScale = transform().m11();
            const qreal desired = m_androidPinchScale * pinch->totalScaleFactor();
            androidZoomBy(desired / qMax(qreal(0.001), transform().m11()));
            gestures->accept(pinch);
            return true;
        }
    }
    return QGraphicsView::viewportEvent(event);
}
#endif
