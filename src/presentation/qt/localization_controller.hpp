// Textual compatibility boundary: Qt MOC requires this QObject declaration in a .hpp file.
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

#include <memory>

class QQmlEngine;
class QSettings;

/**
 * @brief Owns runtime UI translation and locale-sensitive presentation policy.
 *
 * LocalizationController is a presentation-only, device-local adapter. UI language, regional
 * formatting, calendar, timestamp presentation, time zone and monetary display preferences are
 * intentionally independent. It formats values that already have explicit semantics; it never
 * fetches market data, invents FX rates or mutates canonical timestamps.
 *
 * The referenced QQmlEngine and QSettings must outlive this controller.
 */
class LocalizationController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString languageTag READ languageTag NOTIFY localeChanged)
    Q_PROPERTY(QString regionalLocaleTag READ regionalLocaleTag NOTIFY localeChanged)
    Q_PROPERTY(QString effectiveLocaleName READ effectiveLocaleName NOTIFY localeChanged)
    Q_PROPERTY(QString calendarId READ calendarId NOTIFY localeChanged)
    Q_PROPERTY(QString timestampDisplayMode READ timestampDisplayMode NOTIFY localeChanged)
    Q_PROPERTY(QString timeZoneId READ timeZoneId NOTIFY localeChanged)
    Q_PROPERTY(QString currencyCode READ currencyCode NOTIFY localeChanged)
    Q_PROPERTY(QString effectiveCurrencyCode READ effectiveCurrencyCode NOTIFY localeChanged)
    Q_PROPERTY(QStringList additionalCurrencyCodes READ additionalCurrencyCodes NOTIFY localeChanged)
    Q_PROPERTY(QStringList effectiveDisplayCurrencyCodes READ effectiveDisplayCurrencyCodes NOTIFY localeChanged)
    Q_PROPERTY(QString languageLabel READ languageLabel NOTIFY localeChanged)
    Q_PROPERTY(QString regionalLocaleLabel READ regionalLocaleLabel NOTIFY localeChanged)
    Q_PROPERTY(QString calendarLabel READ calendarLabel NOTIFY localeChanged)
    Q_PROPERTY(QString timestampDisplayModeLabel READ timestampDisplayModeLabel NOTIFY localeChanged)
    Q_PROPERTY(QString timeZoneLabel READ timeZoneLabel NOTIFY localeChanged)
    Q_PROPERTY(QString currencyLabel READ currencyLabel NOTIFY localeChanged)
    Q_PROPERTY(bool rightToLeft READ rightToLeft NOTIFY localeChanged)
    Q_PROPERTY(qulonglong revision READ revision NOTIFY localeChanged)
    Q_PROPERTY(QVariantList availableLanguages READ availableLanguages NOTIFY localeChanged)
    Q_PROPERTY(QVariantList availableRegionalLocales READ availableRegionalLocales NOTIFY localeChanged)
    Q_PROPERTY(QVariantList availableCalendars READ availableCalendars NOTIFY localeChanged)
    Q_PROPERTY(QVariantList availableTimestampDisplayModes READ availableTimestampDisplayModes NOTIFY localeChanged)
    Q_PROPERTY(QVariantList availableTimeZones READ availableTimeZones NOTIFY localeChanged)
    Q_PROPERTY(QVariantList availableCurrencies READ availableCurrencies NOTIFY localeChanged)

public:
    /** @brief Startup overrides used by automation/development without changing source defaults. */
    struct StartupOverrides final {
        QString languageTag;
        QString regionalLocaleTag;
        QString calendarId;
        QString timestampDisplayMode;
        QString timeZoneId;
        QString currencyCode;
        QStringList additionalCurrencyCodes;
    };

    /**
     * @brief Creates the localization runtime and immediately applies the selected preferences.
     * @param engine QML engine to retranslate after runtime language changes.
     * @param settings Device-local settings store. It is borrowed and must outlive this object.
     * @param overrides Optional command-line overrides. Empty values use persisted/system policy.
     * @param parent Optional Qt owner.
     */
    explicit LocalizationController(
        QQmlEngine& engine,
        QSettings& settings,
        StartupOverrides overrides = {},
        QObject* parent = nullptr);
    ~LocalizationController() override;

    LocalizationController(const LocalizationController&) = delete;
    auto operator=(const LocalizationController&) -> LocalizationController& = delete;
    LocalizationController(LocalizationController&&) = delete;
    auto operator=(LocalizationController&&) -> LocalizationController& = delete;

    [[nodiscard]] QString languageTag() const;
    [[nodiscard]] QString regionalLocaleTag() const;
    [[nodiscard]] QString effectiveLocaleName() const;
    [[nodiscard]] QString calendarId() const;
    [[nodiscard]] QString timestampDisplayMode() const;
    [[nodiscard]] QString timeZoneId() const;
    [[nodiscard]] QString currencyCode() const;
    [[nodiscard]] QString effectiveCurrencyCode() const;
    [[nodiscard]] QStringList additionalCurrencyCodes() const;
    [[nodiscard]] QStringList effectiveDisplayCurrencyCodes() const;
    [[nodiscard]] QString languageLabel() const;
    [[nodiscard]] QString regionalLocaleLabel() const;
    [[nodiscard]] QString calendarLabel() const;
    [[nodiscard]] QString timestampDisplayModeLabel() const;
    [[nodiscard]] QString timeZoneLabel() const;
    [[nodiscard]] QString currencyLabel() const;
    [[nodiscard]] bool rightToLeft() const noexcept;
    [[nodiscard]] qulonglong revision() const noexcept;
    [[nodiscard]] QVariantList availableLanguages() const;
    [[nodiscard]] QVariantList availableRegionalLocales() const;
    [[nodiscard]] QVariantList availableCalendars() const;
    [[nodiscard]] QVariantList availableTimestampDisplayModes() const;
    [[nodiscard]] QVariantList availableTimeZones() const;
    [[nodiscard]] QVariantList availableCurrencies() const;

    /** @brief Selects an embedded UI language pack or the system language. Returns false if invalid. */
    Q_INVOKABLE bool setLanguageTag(const QString& languageTag);
    /** @brief Selects system or a valid regional locale used for number/date/currency presentation. */
    Q_INVOKABLE bool setRegionalLocaleTag(const QString& regionalLocaleTag);
    /** @brief Selects Gregorian/Jalali/Shahanshahi/other supported calendar policy. */
    Q_INVOKABLE bool setCalendarId(const QString& calendarId);
    /** @brief Selects smart, relative or calendar-first timestamp presentation. */
    Q_INVOKABLE bool setTimestampDisplayMode(const QString& mode);
    /** @brief Selects system/UTC or a valid IANA time-zone identifier. */
    Q_INVOKABLE bool setTimeZoneId(const QString& timeZoneId);
    /** @brief Selects the primary automatic/ISO-4217 monetary display unit. */
    Q_INVOKABLE bool setCurrencyCode(const QString& currencyCode);
    /** @brief Replaces the optional secondary display currencies/denominations. */
    Q_INVOKABLE bool setAdditionalCurrencyCodes(const QStringList& currencyCodes);
    /** @brief Adds/removes one optional secondary display currency/denomination. */
    Q_INVOKABLE bool toggleAdditionalCurrencyCode(const QString& currencyCode);
    /** @brief Returns whether a secondary display unit is currently enabled. */
    Q_INVOKABLE bool additionalCurrencyEnabled(const QString& currencyCode) const;
    /** @brief Restores all localization preferences to system-aware defaults. */
    Q_INVOKABLE void resetToSystemDefaults();

    /** @brief Formats a numeric value using the active regional locale and requested precision. */
    Q_INVOKABLE QString formatNumber(double value, int precision = -1) const;
    /** @brief Formats an integer using the active regional numbering system and grouping rules. */
    Q_INVOKABLE QString formatInteger(qint64 value) const;
    /** @brief Formats a large count using translated compact units and regional digits. */
    Q_INVOKABLE QString formatCompactNumber(double value) const;
    /** @brief Formats an epoch-millisecond timestamp according to the selected display policy. */
    Q_INVOKABLE QString formatTimestamp(
        qint64 epochMilliseconds, const QString& style = QStringLiteral("short")) const;
    /** @brief Formats an epoch-millisecond timestamp as a localized relative interval. */
    Q_INVOKABLE QString formatRelativeTime(qint64 epochMilliseconds) const;
    /** @brief Formats an epoch-millisecond timestamp using active locale/calendar/time zone. */
    Q_INVOKABLE QString formatDateTime(
        qint64 epochMilliseconds, const QString& style = QStringLiteral("medium")) const;
    /**
     * @brief Formats an already-valued ISO fiat amount; it never performs FX/token conversion.
     * @param value Amount in the supplied ISO currency.
     * @param isoCurrency Optional ISO-4217 code; empty uses the selected automatic/local currency.
     * @param precision Decimal precision, or -1 for Qt's currency default.
     */
    Q_INVOKABLE QString formatCurrency(
        double value, const QString& isoCurrency = {}, int precision = -1) const;
    /**
     * @brief Formats an already-valued display unit, including the non-ISO IRT (toman) denomination.
     * @note IRT is a presentation denomination only. Market/ledger identity remains explicit IRR.
     */
    Q_INVOKABLE QString formatMonetaryUnit(
        double value, const QString& displayUnit, int precision = -1) const;
    /** @brief Formats one already-valued asset quote such as "1 GENY = $0.00060". */
    Q_INVOKABLE QString formatAssetQuote(
        const QString& assetSymbol,
        double value,
        const QString& displayUnit,
        int precision = -1) const;
    /**
     * @brief Converts only exact display denominations, not FX. Currently supports IRR <-> IRT.
     * @return NaN for unsupported cross-currency conversion.
     */
    Q_INVOKABLE double convertDisplayDenomination(
        double value, const QString& fromUnit, const QString& toUnit) const;

signals:
    void localeChanged();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};
