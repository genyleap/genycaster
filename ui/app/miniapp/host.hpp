/*!
 * @file        ui/app/miniapp/host.hpp
 * @brief       Mini App host presentation controller.
 *
 * @details
 * Coordinates launch, navigation and host state for one embedded app
 * while leaving browser session authority to runtime ownership.
 *
 * @author      Kambiz Asadzadeh
 * @since       24 Sep 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

// Textual compatibility boundary: Qt MOC requires a QObject declaration.
#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantMap>
#include <QVariantList>

class QSettings;

/** @brief GUI-thread launch intents/status. Owns no browser, network, or account secrets. */
class MiniAppHostController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(bool active READ active NOTIFY stateChanged)
    Q_PROPERTY(int sessionLimit READ sessionLimit NOTIFY sessionLimitChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY stateChanged)
    Q_PROPERTY(QString launchUrl READ launchUrl NOTIFY stateChanged)
    Q_PROPERTY(bool swapRequestActive READ swapRequestActive NOTIFY swapRequestChanged)
    Q_PROPERTY(QString swapRequestId READ swapRequestId NOTIFY swapRequestChanged)
    Q_PROPERTY(QVariantMap swapRequest READ swapRequest NOTIFY swapRequestChanged)
    Q_PROPERTY(bool swapQuoteBusy READ swapQuoteBusy NOTIFY swapRequestChanged)
    Q_PROPERTY(QVariantMap swapQuote READ swapQuote NOTIFY swapRequestChanged)
    Q_PROPERTY(QString swapQuoteError READ swapQuoteError NOTIFY swapRequestChanged)
    Q_PROPERTY(bool walletBrowserPickerActive READ walletBrowserPickerActive NOTIFY walletBrowserPickerChanged)
    Q_PROPERTY(bool walletBrowserWaiting READ walletBrowserWaiting NOTIFY walletBrowserPickerChanged)
    Q_PROPERTY(QString walletBrowserAppName READ walletBrowserAppName NOTIFY walletBrowserPickerChanged)
    Q_PROPERTY(QString walletBrowserOrigin READ walletBrowserOrigin NOTIFY walletBrowserPickerChanged)
    Q_PROPERTY(QString walletBrowserSelectedName READ walletBrowserSelectedName NOTIFY walletBrowserPickerChanged)
    Q_PROPERTY(QVariantList walletBrowserOptions READ walletBrowserOptions NOTIFY walletBrowserPickerChanged)
    Q_PROPERTY(QString walletBrowserError READ walletBrowserError NOTIFY walletBrowserPickerChanged)
    Q_PROPERTY(QString presentationMode READ presentationMode NOTIFY presentationModeChanged)
public:
    /** @brief Borrows GUI-thread settings, which must outlive this controller. */
    explicit MiniAppHostController(bool runtimeAvailable, QSettings& settings, QObject* parent = nullptr);
    [[nodiscard]] bool available() const noexcept;
    [[nodiscard]] bool active() const noexcept;
    [[nodiscard]] int sessionLimit() const noexcept;

    /** Updated only from the server-authenticated Geny Pro entitlement. */
    void setSessionLimit(int limit);
    [[nodiscard]] QString errorText() const;
    [[nodiscard]] QString launchUrl() const;
    [[nodiscard]] bool walletBrowserPickerActive() const noexcept;
    [[nodiscard]] bool walletBrowserWaiting() const noexcept;
    [[nodiscard]] QString walletBrowserAppName() const;
    [[nodiscard]] QString walletBrowserOrigin() const;
    [[nodiscard]] QString walletBrowserSelectedName() const;
    [[nodiscard]] QVariantList walletBrowserOptions() const;
    [[nodiscard]] QString walletBrowserError() const;

    /** Trusted runtime supplies the visible Mini App identity for wallet consent. */
    void setWalletBrowserContext(const QString& appName, const QString& origin);

    /** GUI-thread request from the EVM transport, never from Mini App JS. */
    void requestWalletBrowser(const QString& localSessionUrl);

    /** Clear a stale chooser when runtime focus/document authority changes. */
    void walletBrowserSessionEnded();

    /** A wallet RPC settled; dismiss waiting and remember only a successful choice. */
    void walletBrowserRequestFinished(bool successful);
    Q_INVOKABLE void chooseWalletBrowser(const QString& browserId);
    Q_INVOKABLE void cancelWalletBrowser();

    /** @brief Returns window/bottom/left/right; physical directions are locale-independent. */
    [[nodiscard]] QString presentationMode() const;

    /** @brief Persists a valid mode for the next launch without restarting a running app. */
    Q_INVOKABLE void setPresentationMode(const QString& mode);

    /** @brief Composition supplies the user's motion preference for subsequent launches. */
    void setReduceMotion(bool reduceMotion) noexcept;
    [[nodiscard]] bool reduceMotion() const noexcept;

    /** @brief Opens untrusted launch metadata; validation belongs to the runtime. No external fallback. */
    Q_INVOKABLE void open(const QString& url, const QString& title);

    /** @brief Opens with presentation metadata used for immediate loading/task feedback. */
    Q_INVOKABLE void openWithMetadata(const QString& url, const QString& title, const QString& iconSource);

    /** @brief Requests teardown of the foreground app, if any. */
    Q_INVOKABLE void close();

    /** @brief Requests deterministic teardown of every live Mini App session. */
    Q_INVOKABLE void closeAll();

    /** @brief Completes a Mini App composeCast request after a visible user publish. */
    Q_INVOKABLE void completeComposeCast(const QString& requestId,
                                         const QString& castId,
                                         const QString& text,
                                         const QVariantList& embeds);

    /** @brief Completes a Mini App composeCast request without publishing. */
    Q_INVOKABLE void cancelComposeCast(const QString& requestId);

    /** @brief The runtime owns one untrusted swap intent; QML cannot execute it. */
    [[nodiscard]] bool swapRequestActive() const noexcept;
    [[nodiscard]] QString swapRequestId() const;
    [[nodiscard]] QVariantMap swapRequest() const;
    void presentSwapRequest(const QString& requestId, const QVariantMap& details);
    void clearSwapRequest(const QString& requestId);
    Q_INVOKABLE void cancelSwapToken(const QString& requestId);
    [[nodiscard]] bool swapQuoteBusy() const noexcept;
    [[nodiscard]] QVariantMap swapQuote() const;
    [[nodiscard]] QString swapQuoteError() const;
    Q_INVOKABLE void beginSwapQuote(const QString& requestId,
                                   const QVariantMap& editedIntent);
    void dispatchSwapQuote(const QString& requestId, const QVariantMap& parameters);
    void receiveSwapQuoteResult(const QString& requestId, const QVariantMap& quote,
                                const QString& error);
    void showSwapQuote(const QString& requestId, const QVariantMap& verifiedQuote);
    Q_INVOKABLE void invalidateSwapQuote(const QString& requestId);
    void setSwapQuoteError(const QString& requestId, const QString& error);

    /** @brief Runtime asks the shell to open Genycaster's visible composer. */
    void requestComposeCast(const QString& requestId, const QVariantMap& options);

    /** @brief QML/application surfaces ask the shell to prepare a visible share draft. */
    Q_INVOKABLE void requestShare(const QString& url, const QString& title);

    /** @brief Runtime asks the shell to share from a minimized session that should be restored afterward. */
    void requestSessionShare(const QString& url, const QString& title, const QString& taskId);

    /** @brief Updates public context from trusted composition, closing any app on account change. */
    void setUser(const QVariantMap& user);

    /** @brief Returns an owned copy of public account metadata, never credentials. */
    [[nodiscard]] QVariantMap user() const;

    /** @brief Runtime status sink. Errors are user-visible; no success is fabricated on failure. */
    Q_INVOKABLE void setState(bool active, const QString& error = {});
signals:
    /** An unprivileged system-browser fallback opened successfully. */
    void externalLaunchSucceeded();
    void launchRequested(const QString& url, const QString& title, const QString& iconSource);
    void closeRequested();
    void closeAllRequested();
    void profileRequested(qulonglong fid);
    void castRequested(const QString& hash);
    void shareRequested(const QString& url, const QString& title, const QString& taskId);
    void composeCastRequested(const QString& requestId, const QVariantMap& options);
    void composeCastCompleted(const QString& requestId, const QVariantMap& result);
    void composeCastCancelled(const QString& requestId);

    /** Emitted only for a user-declined intent, never a signing confirmation. */
    void swapTokenCancelled(const QString& requestId);
    void swapQuotePreparationRequested(const QString& requestId,
                                      const QVariantMap& editedIntent);
    void swapQuoteNetworkRequested(const QString& requestId,
                                   const QVariantMap& parameters);
    void swapQuoteResultReceived(const QString& requestId,
                                 const QVariantMap& quote, const QString& error);
    void swapRequestChanged();
    void stateChanged();

    /** @brief Public SDK user metadata changed without an account switch. */
    void userChanged();
    void sessionLimitChanged();
    void walletBrowserPickerChanged();

    /** Cancel the wallet RPC only; do not terminate the Mini App host. */
    void walletBrowserCancelRequested();
    void presentationModeChanged();
private:
    QSettings& m_settings;
    QString m_presentationMode{QStringLiteral("window")};
    bool m_reduceMotion{};
    bool m_available{};
    int m_sessionLimit{1};
    bool m_active{};
    QString m_error{};
    QString m_launchUrl{};
    QString m_walletBrowserSessionUrl{};
    QString m_walletBrowserError{};
    QString m_walletBrowserAppName{};
    QString m_walletBrowserOrigin{};
    QString m_walletBrowserSelectedId{};
    QString m_walletBrowserSelectedName{};
    bool m_walletBrowserWaiting{};
    QVariantList m_walletBrowserOptions{};
    QVariantMap m_user{};
    QVariantMap m_swapRequest{};
    QString m_swapRequestId{};
    QVariantMap m_swapQuote{};
    QString m_swapQuoteError{};
    bool m_swapQuoteBusy{};
#if defined(Q_OS_WASM)
    QTimer m_browserVisibilityTimer{};
#endif
};
