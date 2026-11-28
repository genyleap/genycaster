/*!
 * @file        ui/app/platform/instance.cppm
 * @brief       Desktop single-instance and deep-link handoff.
 *
 * @details
 * Coordinates one process per operating-system user, forwards safe incoming URLs and activates the existing Qt window.
 *
 * @author      Kambiz Asadzadeh
 * @since       07 Nov 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;
#include "qt_fwd.hpp"
#include <functional>
#include <memory>

export module genycaster.ui.desktop.instance;

export namespace genycaster::ui {
/**
 * @brief OS-user-scoped broker for activation and safe deep-link forwarding.
 *
 * A second executable instance asks the primary Qt window to activate rather
 * than creating another writable session. IPC remains local to the same user;
 * forwarded links are validated before being dispatched to navigation.
 */
class DesktopSingleInstance final {
public:
    /** @brief Result of acquiring the per-user instance lock. */
    enum class Result { Primary, ActivatedExisting, Error };
    DesktopSingleInstance();
    ~DesktopSingleInstance();
    DesktopSingleInstance(const DesktopSingleInstance&) = delete;
    DesktopSingleInstance& operator=(const DesktopSingleInstance&) = delete;

    /**
     * @brief Acquire exclusive instance ownership or contact the primary.
     * @param requestedUrl Optional incoming HTTPS link to forward.
     * @return Primary, activated existing instance, or a startup error.
     */
    [[nodiscard]] Result acquire(const QString& requestedUrl);

    /** @brief Bind the created QML window after engine startup. */
    void setWindow(QQuickWindow* window);

    /** @brief Restore through the same desktop lifecycle owner as the tray. */
    void setActivationHandler(std::function<void()> handler);

    /** @brief Register navigation handoff after runtime/setup becomes ready.
     *  The callback must not outlive the referenced runtime controllers.
     */
    void setOpenUrlHandler(std::function<void(const QString&)> handler);
    [[nodiscard]] QString errorString() const;
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
}
