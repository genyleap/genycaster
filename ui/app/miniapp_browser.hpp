/*!
 * @file        ui/app/miniapp_browser.hpp
 * @brief       Platform Mini App browser abstraction.
 *
 * @details
 * Defines isolated native browser lifecycle, origin-aware host messaging
 * and Mini App presentation operations without exporting WebKit types.
 *
 * @author      Kambiz Asadzadeh
 * @since       24 Sep 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

// Textual compatibility boundary: Objective-C++/WebKit adapter is deliberately
// outside C++ named modules and Qt MOC. No platform types escape this boundary.
#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QUrl>
#include <functional>
#include <memory>

namespace farcaster::platform::miniapp {

/** @brief Physical presentation edges, unaffected by text/layout direction. */
enum class PresentationMode { Window, Bottom, Left, Right };

/** @brief Selects whether the isolated browser exposes the Farcaster Mini App bridge. */
enum class BrowserRole { MiniApp, Media };

/** @brief GUI-thread callbacks; the browser detaches them before destruction. */
struct BrowserCallbacks final {
    std::function<void(const QJsonObject&, const QString&)> message;
    std::function<void()> closed;
    std::function<void()> retry;
    std::function<void()> minimize;

    /** @brief Reports native window activation without changing document lifetime. */
    std::function<void()> activated;
    std::function<void()> share;
    std::function<void()> navigationStarted;
    std::function<void(const QString&)> failed;
};

/**
 * @brief Owns an isolated in-app window and nonpersistent native browser.
 *
 * All calls are GUI-thread-only. Same-origin main-frame messages are delivered
 * to the host; replies are scoped to the originating document token. No external
 * browser fallback or native permission prompt is initiated by web content.
 */
class Browser {
public:
    virtual ~Browser() = default;

    /**
     * @brief Loads one document, optionally identifying the embedding client.
     *
     * Web media providers such as YouTube require a Referer-equivalent client
     * identity in desktop WebViews. Mini Apps pass an empty referrer and retain
     * their existing same-origin navigation contract.
     */
    virtual void load(const QUrl& url, const QUrl& referrer = {}) = 0;
    virtual void status(const QString& message, bool failed) = 0;
    virtual void ready(bool disableNativeGestures) = 0;
    virtual void setBackVisible(bool visible) = 0;
    virtual void reply(const QJsonObject& value, const QString& document) = 0;
    virtual void ethereumProviderEvent(
        const QString& event, const QJsonArray& params, const QString& document) = 0;

    /** @brief Emits a Farcaster Mini App client event into the matching document. */
    virtual void miniAppEvent(const QJsonObject& event, const QString& document) = 0;

    /** @brief Shows or hides the native host without destroying the WebView document. */
    virtual void setVisible(bool visible) = 0;

    /**
     * @brief Returns a renderer process identifier only when a public platform API exposes one.
     *
     * A return value of zero means process-level telemetry is unavailable. Implementations must
     * not use private WebKit selectors or infer renderer ownership from process enumeration.
     */
    [[nodiscard]] virtual qlonglong processIdentifier() const noexcept = 0;
};

/** @brief Creates and displays a native host; callback ownership transfers to the browser. */
[[nodiscard]] std::unique_ptr<Browser> createBrowser(
    const QString& title, const QUrl& origin, BrowserCallbacks callbacks,
    PresentationMode mode, bool reduceMotion, bool initiallyVisible,
    BrowserRole role = BrowserRole::MiniApp);

}
