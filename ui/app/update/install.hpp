#pragma once

#include <QString>

namespace genycaster::ui::update {

enum class InstallMode { Supervised, SystemInstaller, Unsupported };

struct InstallResult {
    InstallMode mode{InstallMode::Unsupported};
    QString detail{};
};

// Called only after UpdateController rehashes a trusted, compatible
// release artifact. Never modifies the running process or user data.
[[nodiscard]] InstallResult launchVerifiedUpdate(
    const QString& verifiedPackage,
    const QString& expectedSha256);

} // namespace genycaster::ui::update
