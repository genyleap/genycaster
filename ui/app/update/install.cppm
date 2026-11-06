// Qt's ABI remains globally attached; full Qt headers are private to implementations.
module;
#include "qt_fwd.hpp"

export module genycaster.ui.update.install;

export namespace genycaster::ui::update {
    enum class InstallMode { Supervised, SystemInstaller, Unsupported };

    // The detail is optional; a failed handoff returns Unsupported and fills it.
    [[nodiscard]] InstallMode launchVerifiedUpdate(
        const QString& verifiedPackage,
        const QString& expectedSha256,
        QString* errorDetail);
}
