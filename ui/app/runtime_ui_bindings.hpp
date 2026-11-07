/*!
 * @file        ui/app/runtime_ui_bindings.hpp
 * @brief       Qt runtime-to-interface composition bindings.
 *
 * @details
 * Owns GUI-facing controller connections for a composed runtime while
 * preserving MOC compatibility and core module isolation.
 *
 * @author      Kambiz Asadzadeh
 * @since       20 Oct 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

#pragma once

#include <memory>
#include <string>
#include <QString>

// Qt application composition boundary: C++/Qt QObject implementations are
// owned by their existing MOC-compatible headers, not by the domain layer.
class PresentationBridge;
class RichComposerController;
class OpenProofConnectionsController;
class MediaUploadController;
class MediaPlaybackCoordinator;
// The importer must import genycaster.ui.runtime before this header;
// redeclaring a named-module type in the global module is not valid C++26.

namespace genycaster::ui::bindings {

// Fixture wiring is used only when the application is launched in its
// explicitly enabled verification mode.
void applyProfileVerificationFixture(PresentationBridge& bridge);

// Own the GUI-facing application event graph separately from the entrypoint.
// Signal connections retain their prior QObject context and lifetime.
void connectRuntime(PresentationBridge& bridge, RichComposerController& richComposer,
                    OpenProofConnectionsController& connections,
                    const std::shared_ptr<runtime::Runtime>& runtime,
                    bool authenticationAvailable, std::string siwfDomain,
                    std::string siwfUri, const QString& browserSnapchainOrigin);

#if FARCASTER_HAS_MEDIA_UPLOAD
void connectMediaUpload(MediaUploadController& controller,
                        const std::shared_ptr<runtime::Runtime>& runtime);
#endif

#if defined(Q_OS_WASM)
void connectBrowserMediaUpload(MediaUploadController& controller,
                               PresentationBridge& bridge,
                               RichComposerController& richComposer);
#endif

void connectMediaPlayback(MediaPlaybackCoordinator& coordinator,
                          const std::shared_ptr<runtime::Runtime>& runtime);

/**
 * @brief Borrowed Qt controllers and startup identity for one runtime handoff.
 *
 * Every controller reference remains owned by the application entry point.
 * The destination runtime is assigned on the GUI thread only after successful
 * provider construction. This carries no signer secret and never changes the
 * user's selected Read or Write provider.
 */
struct RuntimeAttachment final {
    PresentationBridge& presentationBridge;
    RichComposerController& richComposerController;
    OpenProofConnectionsController& openProofConnections;
    MediaUploadController& mediaUploadController;
    MediaPlaybackCoordinator& mediaPlayback;
    std::shared_ptr<runtime::Runtime>& runtime;
    std::string siwfDomain;
    std::string siwfUri;
    QString browserSnapchainOrigin;
    QString startupIdentity;
    bool authenticationAvailable{false};
    bool openExploreForTest{false};
};

/**
 * @brief Publish a fully constructed runtime to its existing GUI controllers.
 *
 * @param attachment Borrowed controller references and session-routing values.
 * @param readyRuntime Successfully constructed runtime, transferred to the
 *        application owner before controller connections are established.
 *
 * Call only on the GUI thread, from the receiver-bound queued bootstrap
 * completion; do not invoke after QML roots have been dismantled. Publishing
 * runtimeReady occurs only after all controller bindings are installed.
 */
void attachRuntime(RuntimeAttachment attachment,
                   std::shared_ptr<runtime::Runtime> readyRuntime);

} // namespace genycaster::ui::bindings
