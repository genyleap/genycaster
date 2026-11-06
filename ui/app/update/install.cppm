module;

#include <QString>

export module genycaster.ui.update.install;

export namespace genycaster::ui::update {
    enum class InstallMode { Supervised, SystemInstaller, Unsupported };
    struct InstallResult {
        InstallMode mode{InstallMode::Unsupported};
        QString detail{};
    };
    // A verified OS-compatible artifact only. This function does not bypass
    // system installation permissions or replace a running executable.
    [[nodiscard]] InstallResult launchVerifiedUpdate(
        const QString& verifiedPackage, const QString& expectedSha256);
}
