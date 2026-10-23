#include <QtGlobal>
#ifdef Q_OS_ANDROID
#include <QApplication>
#include <QDialog>
#include <QEvent>
#include <QKeyEvent>
#include <QLayout>
#include <QPushButton>
#include <QTimer>
#include <QVariant>
#include <QPointer>

// Keep upstream closeEvent/reject behavior, including any unsaved-work prompt.
class DrnasDialogControls : public QObject
{
public:
    explicit DrnasDialogControls(QObject* parent) : QObject(parent) {}
protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        auto dialog = qobject_cast<QDialog*>(watched);
        if (!dialog) return false;
        if (event->type() == QEvent::Show && !dialog->property("drnasCloseInstalled").toBool()) {
            dialog->setProperty("drnasCloseInstalled", true);
            auto button = new QPushButton(QString::fromUtf8("× Close"), dialog);
            button->setObjectName("drnasDialogClose");
            button->setFixedSize(120, 48);
            button->setStyleSheet("font-size:16px; font-weight:600;");
            QObject::connect(button, &QPushButton::clicked, dialog, &QWidget::close);
            if (dialog->layout()) {
                const bool fixedHeight = dialog->minimumHeight() == dialog->maximumHeight();
                const int previousHeight = dialog->height();
                const auto margins = dialog->layout()->contentsMargins();
                dialog->layout()->setContentsMargins(margins.left(), margins.top()+56,
                                                     margins.right(), margins.bottom());
                // Property dialogs use a fixed content height upstream. Make
                // room for the header instead of covering their final row.
                if (fixedHeight) dialog->setFixedHeight(previousHeight + 56);
            }
            button->move(qMax(0, dialog->width()-124), 4);
            button->show(); button->raise();
        }
        if (event->type() == QEvent::Resize || event->type() == QEvent::Show) {
            if (auto button = dialog->findChild<QPushButton*>("drnasDialogClose", Qt::FindDirectChildrenOnly)) {
                button->move(qMax(0, dialog->width()-124), 4);
                button->raise();
            }
        }
        if (event->type() == QEvent::KeyPress) {
            auto key = static_cast<QKeyEvent*>(event);
            if (key->key() == Qt::Key_Back || key->key() == Qt::Key_Escape) {
                dialog->close();
                return true;
            }
        }
        return false;
    }
};

void installDrnasDialogControls()
{
    qApp->setStyleSheet(qApp->styleSheet() + QStringLiteral("QTabBar::close-button { width:32px; height:32px; }"));
    auto controls = new DrnasDialogControls(qApp);
    qApp->installEventFilter(controls);
}
#endif
