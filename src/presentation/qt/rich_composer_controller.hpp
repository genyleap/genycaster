// Textual compatibility boundary: Qt MOC requires this QObject declaration in a .hpp file.
#pragma once

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include <memory>

class QSettings;

/**
 * @brief QML adapter over the native RichCastDocument publishing engine.
 *
 * The controller owns editor/session state only. Parsing, safe Farcaster
 * serialization, language detection and syntax tokenization live in native
 * application/domain modules and remain independent from QML.
 */
class RichComposerController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString markdownSource READ markdownSource WRITE setMarkdownSource NOTIFY documentChanged)
    Q_PROPERTY(QString safeText READ safeText NOTIFY documentChanged)
    Q_PROPERTY(int editorCharacters READ editorCharacters NOTIFY documentChanged)
    Q_PROPERTY(int publishedBytes READ publishedBytes NOTIFY documentChanged)
    Q_PROPERTY(int maximumPublishedBytes READ maximumPublishedBytes CONSTANT)
    Q_PROPERTY(bool withinProtocolLimit READ withinProtocolLimit NOTIFY documentChanged)
    Q_PROPERTY(bool longCast READ longCast NOTIFY documentChanged)
    Q_PROPERTY(int simplifiedElementCount READ simplifiedElementCount NOTIFY documentChanged)
    Q_PROPERTY(QStringList warnings READ warnings NOTIFY documentChanged)
    Q_PROPERTY(QVariantList blocks READ blocks NOTIFY documentChanged)
    Q_PROPERTY(QVariantList languages READ languages CONSTANT)
    Q_PROPERTY(QVariantList mentionFids READ mentionFids NOTIFY documentChanged)
    Q_PROPERTY(QVariantList mentionPositions READ mentionPositions NOTIFY documentChanged)
    Q_PROPERTY(QVariantList protocolEmbeds READ protocolEmbeds NOTIFY documentChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY historyChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY historyChanged)
    Q_PROPERTY(bool dirty READ dirty NOTIFY saveStateChanged)
    Q_PROPERTY(QString saveStatus READ saveStatus NOTIFY saveStateChanged)
    Q_PROPERTY(qulonglong revision READ revision NOTIFY documentChanged)

public:
    explicit RichComposerController(QSettings& settings, QObject* parent = nullptr);
    ~RichComposerController() override;

    RichComposerController(const RichComposerController&) = delete;
    auto operator=(const RichComposerController&) -> RichComposerController& = delete;
    RichComposerController(RichComposerController&&) = delete;
    auto operator=(RichComposerController&&) -> RichComposerController& = delete;

    [[nodiscard]] QString markdownSource() const;
    void setMarkdownSource(const QString& source);
    [[nodiscard]] QString safeText() const;
    [[nodiscard]] int editorCharacters() const;
    [[nodiscard]] int publishedBytes() const noexcept;
    [[nodiscard]] int maximumPublishedBytes() const noexcept { return 1024; }
    [[nodiscard]] bool withinProtocolLimit() const noexcept;
    [[nodiscard]] bool longCast() const noexcept;
    [[nodiscard]] int simplifiedElementCount() const noexcept;
    [[nodiscard]] QStringList warnings() const;
    [[nodiscard]] QVariantList blocks() const;
    [[nodiscard]] QVariantList languages() const;
    [[nodiscard]] QVariantList mentionFids() const;
    [[nodiscard]] QVariantList mentionPositions() const;
    [[nodiscard]] QVariantList protocolEmbeds() const;
    [[nodiscard]] bool canUndo() const noexcept;
    [[nodiscard]] bool canRedo() const noexcept;
    [[nodiscard]] bool dirty() const noexcept;
    [[nodiscard]] QString saveStatus() const;
    [[nodiscard]] qulonglong revision() const noexcept;

    /** @brief Starts a fresh rich draft; an existing autosaved draft may be restored separately. */
    Q_INVOKABLE void newDocument();
    /** @brief Opens an isolated, non-persistent reply/quote/Mini App document. */
    Q_INVOKABLE void beginContextDocument();
    /** @brief Restores the latest versioned rich draft, if one exists. */
    Q_INVOKABLE bool restoreDraft();
    /** @brief Selects the local draft owner. A dirty ordinary draft is saved to
     * its previous owner before the editor is reset and the new owner's draft
     * is restored. A zero FID selects the unassigned legacy draft. */
    void setViewerFid(qulonglong fid);
    /** @brief Marks the composer as in use so automatic account refreshes
     * never replace visible text, even after autosave clears dirty state. */
    Q_INVOKABLE void setComposerActive(bool active);
    /** @brief Applies verified account state only when the editor has no
     * uncommitted changes; remote content is validated before replacement.
     * Return false to retry later without advancing the remote revision. */
    [[nodiscard]] bool applyAccountDraft(qulonglong fid, const QByteArray& serialized,
                                          bool deleted);
    /** @brief Persists the complete AST plus editor source without publishing anything. */
    Q_INVOKABLE void saveDraft();
    /** @brief Deletes the current local draft. */
    Q_INVOKABLE void clearDraft();
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();

    /** @brief Replaces one editable text/list/code block while keeping AST as source of truth. */
    Q_INVOKABLE bool setBlockText(int index, const QString& text);
    /** @brief Reconstructs a bounded semantic inline AST from Qt RichText editor output. */
    Q_INVOKABLE bool setBlockFormattedText(int index, const QString& formattedText);
    Q_INVOKABLE bool setCodeLanguage(int index, const QString& language);
    Q_INVOKABLE bool setCodeWrap(int index, bool wrap);
    Q_INVOKABLE bool setCodeLineNumbers(int index, bool enabled);
    Q_INVOKABLE bool insertBlock(const QString& kind, int afterIndex = -1);
    Q_INVOKABLE bool addCodeBlock(const QString& language = QStringLiteral("auto"));
    Q_INVOKABLE bool removeBlock(int index);
    /** @brief Abandons unpublished gateway attachments, retaining ordinary draft text. */
    Q_INVOKABLE int discardUnpublishedManagedMedia();
    Q_INVOKABLE bool duplicateBlock(int index);
    Q_INVOKABLE bool moveBlock(int from, int to);
    /** @brief Applies one semantic inline style using QML UTF-16 selection positions. */
    Q_INVOKABLE bool applyInlineStyle(int blockIndex, int selectionStart, int selectionLength, const QString& style);
    /** @brief Converts a selected text range into a sanitized semantic link node. */
    Q_INVOKABLE bool applyLink(
        int blockIndex, int selectionStart, int selectionLength, const QString& target);
    /** @brief Inserts optional enhancement blocks without making them required for plain-text rendering. */
    Q_INVOKABLE bool insertEmbed(const QString& url, const QString& title = {}, int afterIndex = -1);
    /** @brief Inserts a protocol-native quoted Cast, not an external URL. */
    Q_INVOKABLE bool insertCastReference(const QString& castId, qulonglong authorFid);
    Q_INVOKABLE bool insertMedia(
        const QString& url, const QString& mediaType = {}, const QString& altText = {}, int afterIndex = -1);
    /** @brief Inserts uploaded media while retaining its opaque local lifecycle token. */
    Q_INVOKABLE bool insertManagedMedia(
        const QString& url,
        const QString& mediaType,
        const QString& managementToken,
        const QString& altText = {},
        int afterIndex = -1);
    /** @brief Converts a textual @mention selection into a resolved semantic mention node. */
    Q_INVOKABLE bool resolveMention(
        int blockIndex, int selectionStart, int selectionLength, qulonglong fid,
        const QString& username, const QString& displayName = {});

    /** @brief Returns native semantic syntax tokens; QML supplies only presentation colors. */
    Q_INVOKABLE QVariantList highlightTokens(const QString& source, const QString& language) const;
    Q_INVOKABLE QString detectLanguage(const QString& source) const;
    /** @brief Attaches or refreshes incremental native syntax highlighting on a QML text document. */
    Q_INVOKABLE void attachSyntaxDocument(
        QObject* quickTextDocument, const QString& language, const QVariantMap& semanticPalette);
    /** @brief Returns a lightweight read-only block projection stored for one published cast. */
    Q_INVOKABLE QVariantList richBlocksForCast(const QString& castId) const;
    /** @brief Associates the current versioned AST sidecar with an already-published cast hash. */
    Q_INVOKABLE void commitPublishedCast(const QString& castId);

    /** @brief Releases local media ownership attached to a confirmed deleted cast. */
    [[nodiscard]] QStringList releaseManagedAssetsForCast(const QString& castId);
    /** @brief Returns durable managed-media tokens waiting for authenticated gateway cleanup. */
    [[nodiscard]] QStringList pendingManagedAssetCleanup() const;
    /** @brief Marks one durable cleanup token complete after the gateway confirms deletion. */
    void markManagedAssetCleanupCompleted(const QString& managementToken);

signals:
    void documentChanged();
    void historyChanged();
    void saveStateChanged();
    /** @brief Requests immediate processing of newly queued managed-media cleanup. */
    void managedAssetCleanupRequested();
    /** @brief Emitted after a user-owned, FID-scoped draft changes on disk. */
    void draftLocallyChanged(qulonglong fid, bool deleted);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};
