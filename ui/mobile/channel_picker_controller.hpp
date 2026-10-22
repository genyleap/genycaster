#pragma once

#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QSettings>
#include <QSet>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

#include <functional>

/**
 * Compose-channel read model owned by the Qt composition root.
 *
 * Channel discovery, membership, member counts, avatar URLs and the
 * canonical parent_url come exclusively from the configured HyperSnap API.
 * QML receives projections, never makes HTTP requests or invents URLs.
 * Per-viewer selection frequency is cached locally in QSettings, not the
 * network-sourced membership or channel directory.
 */
class ChannelPickerController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList rows READ rows NOTIFY rowsChanged FINAL)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged FINAL)
    Q_PROPERTY(bool membershipReady READ membershipReady NOTIFY membershipStateChanged FINAL)
    Q_PROPERTY(QString errorText READ errorText NOTIFY errorTextChanged FINAL)
    Q_PROPERTY(QString query READ query NOTIFY queryChanged FINAL)

public:
    ChannelPickerController(QSettings& settings, QString readNodeBaseUrl,
                            QString directoryBaseUrl, bool directProxy,
                            bool networkEnabled, QObject* parent = nullptr);

    [[nodiscard]] QVariantList rows() const;
    [[nodiscard]] bool busy() const noexcept;
    [[nodiscard]] bool membershipReady() const noexcept;
    [[nodiscard]] QString errorText() const;
    [[nodiscard]] QString query() const;

    Q_INVOKABLE void openForViewer(qulonglong fid);
    Q_INVOKABLE void search(const QString& query);
    // Home is the explicit, always-selectable no-parent target.
    // A regular channel is returned only with verified viewer membership.
    Q_INVOKABLE QVariantMap chooseChannel(const QString& id);
    Q_INVOKABLE void refreshMembership();

signals:
    void rowsChanged();
    void busyChanged();
    void membershipStateChanged();
    void errorTextChanged();
    void queryChanged();

private:
    struct CachedSearch {
        QVariantList results{};
        qint64 expiresAtMs{0};
    };
    using JsonCallback = std::function<void(const QJsonObject&, const QString&)>;

    void request(const QString& path, const QVariantMap& query, JsonCallback done);
    void requestDirectory(const QString& path, const QVariantMap& query, JsonCallback done);
    void hydrateMemberImages();
    void retrieveDirectoryResults(QString query, quint64 nodeGeneration, quint64 searchGeneration);
    void mergeMetadata(QVariantMap& channel) const;
    [[nodiscard]] bool isSocialChannel(const QString& id) const;
    void requestMembershipPage(const QString& cursor, int page, quint64 generation);
    void requestTrending();
    void requestSearchNow();
    void setBusy(bool value);
    void setError(QString value);
    void rebuildRows();
    void loadUsage();
    void saveUsage();
    [[nodiscard]] QVariantMap channelProjection(const QJsonObject& channel) const;
    [[nodiscard]] static QVariantList channelsFromResponse(
        const QJsonObject& response, const ChannelPickerController* self);
    [[nodiscard]] static bool validParentUrl(const QString& url);
    [[nodiscard]] int scoreForQuery(const QVariantMap& channel) const;

    QSettings& m_settings;
    QNetworkAccessManager m_network{};
    QTimer m_debounce{};
    QString m_baseUrl;
    QString m_directoryBaseUrl;
    QHash<QString, QVariantMap> m_directoryMetadata{};
    QSet<QString> m_pendingImageLookups{};
    QSet<QString> m_triedImageLookups{};
    bool m_networkEnabled{false};
    qulonglong m_viewerFid{0};
    quint64 m_generation{0};
    quint64 m_membershipGeneration{0};
    quint64 m_searchGeneration{0};
    bool m_membershipReady{false};
    bool m_membershipBusy{false};
    bool m_searchBusy{false};
    bool m_trendingBusy{false};
    qint64 m_membershipValidUntil{0};
    QString m_query{};
    QString m_error{};
    QVariantList m_rows{};
    QVariantList m_searchResults{};
    QVariantList m_directoryResults{};
    QVariantList m_trendingResults{};
    QHash<QString, QVariantMap> m_members{};
    QHash<QString, int> m_usage{};
    QHash<QString, CachedSearch> m_searchCache{};
};
