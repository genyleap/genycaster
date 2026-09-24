#pragma once

#include <QObject>
#include <QString>

#include <memory>

/**
 * @brief Resolves public Farcaster `geo:` profile locations into readable place labels.
 *
 * Farcaster location values are protocol-public, coarse coordinates. This UI adapter keeps the
 * canonical `geo:` value untouched and derives a presentation label asynchronously through
 * OpenStreetMap Nominatim. QML never performs network I/O. Results are cached in memory, requests
 * are serialized to respect the public service rate limit, and failures fall back to a localized
 * coordinate label instead of exposing the raw URI.
 *
 * The resolver is presentation-only: derived labels are never written into canonical profile
 * state or persisted as protocol data.
 */
class GeoLocationResolver final : public QObject {
    Q_OBJECT
    Q_PROPERTY(qulonglong revision READ revision NOTIFY revisionChanged)

public:
    /**
     * @brief Creates an empty process-local resolver cache.
     * @param useDirectProxy True to bypass the system proxy, false to honor it.
     * @param parent Optional QObject owner.
     */
    explicit GeoLocationResolver(bool useDirectProxy, QObject* parent = nullptr);
    /** @brief Cancels outstanding network work and releases resolver resources. */
    ~GeoLocationResolver() override;

    GeoLocationResolver(const GeoLocationResolver&) = delete;
    auto operator=(const GeoLocationResolver&) -> GeoLocationResolver& = delete;
    GeoLocationResolver(GeoLocationResolver&&) = delete;
    auto operator=(GeoLocationResolver&&) -> GeoLocationResolver& = delete;

    /** @brief Monotonic value changed whenever a newly resolved label becomes available. */
    [[nodiscard]] qulonglong revision() const noexcept;

    /**
     * @brief Returns a readable location label and schedules reverse geocoding when needed.
     * @param location Canonical profile location, usually a Farcaster `geo:lat,lon` URI.
     * @param languageTag Preferred BCP-47/Qt language tag for returned place names.
     * @return Human-readable place name, original non-geo text, or formatted coordinates while
     * reverse geocoding is pending/unavailable.
     */
    Q_INVOKABLE [[nodiscard]] QString displayLabel(
        const QString& location,
        const QString& languageTag = QString{});

signals:
    /** @brief Emitted after a cache entry changes and QML should reevaluate displayLabel(). */
    void revisionChanged();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};
