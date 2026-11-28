#pragma once

#include <QDebug>
#include <QEvent>
#include <QLoggingCategory>
#include <QObject>
#include <QPointer>
#include <QScopedValueRollback>
#include <QString>
#include <QThread>
#include <QTimerEvent>

namespace genycaster::ui::diagnostics {

inline const QLoggingCategory& guiEventTraceLog()
{
    static const QLoggingCategory category{"genycaster.render.gui", QtWarningMsg};
    return category;
}

// Qt's public notify boundary measures event delivery, including nested
// delivery, synchronous QML bindings and waits. Never inspect worker
// receivers or event payloads, never restart an event loop, and never
// use the receiver's raw pointer after delivery (it may be deleted).
template<class Base, double (*ClockMs)() noexcept>
class GuiEventTraceApplication final : public Base {
public:
    using Base::Base;

    bool notify(QObject* receiver, QEvent* event) override
    {
        if (!guiEventTraceLog().isDebugEnabled() || !QThread::isMainThread()
            || !receiver || !event || m_notifying)
            return Base::notify(receiver, event);

        const QScopedValueRollback<bool> scope{m_notifying, true};
        const auto type = event->type();
        const auto timerId = type == QEvent::Timer
            ? static_cast<QTimerEvent*>(event)->timerId() : 0;
        const QPointer<QObject> guard{receiver};
        const QPointer<QObject> owner{receiver->parent()};
        const auto start = ClockMs();
        const bool delivered = Base::notify(receiver, event);
        const auto elapsed = ClockMs() - start;
        if (elapsed >= 16.0)
            qCDebug(guiEventTraceLog()) << "guiEvent" << static_cast<int>(type)
                << "receiver" << (guard ? guard->metaObject()->className() : "<destroyed>")
                << "ownerClass" << (owner ? owner->metaObject()->className() : "<none>")
                << "timerId" << timerId
                << "startMs" << QString::number(start, 'f', 3)
                << "durationMs" << elapsed;
        return delivered;
    }

private:
    bool m_notifying{false}; // main-thread only
};

} // namespace genycaster::ui::diagnostics
