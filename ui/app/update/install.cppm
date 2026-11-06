// Qt itself remains in the global module. No Qt definitions are exported.
module;
class QString;

export module genycaster.ui.update.install;

export namespace genycaster::ui::update {
    enum class InstallMode { Supervised, SystemInstaller, Unsupported };

    // The detail is optional; a failed handoff returns Unsupported and fills it.
    [[nodiscard]] InstallMode launchVerifiedUpdate(
        const QString& verifiedPackage,
        const QString& expectedSha256,
        QString* errorDetail);
}
