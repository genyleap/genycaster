#include "localization_controller.hpp"

#include <QCalendar>
#include <QCoreApplication>
#include <QDateTime>
#include <QGuiApplication>
#include <QHash>
#include <QLocale>
#include <QPointer>
#include <QQmlEngine>
#include <QSettings>
#include <QSet>
#include <QTimeZone>
#include <QTranslator>
#include <QtTranslation>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>

namespace {

constexpr auto kLanguageSetting = "localization/language";
constexpr auto kRegionalLocaleSetting = "localization/regionalLocale";
constexpr auto kCalendarSetting = "localization/calendar";
constexpr auto kTimestampDisplaySetting = "localization/timestampDisplayMode";
constexpr auto kTimeZoneSetting = "localization/timeZone";
constexpr auto kCurrencySetting = "localization/currency";
constexpr auto kAdditionalCurrenciesSetting = "localization/additionalCurrencies";
constexpr int kShahanshahiYearOffset = 1180;
constexpr auto kTranslationProbeId = "genycaster.app.genycaster";
constexpr auto kTranslationResourcePrefix = ":/i18n/qml_";

[[nodiscard]] QString languageTagFor(const QLocale::Language language)
{
    switch (language) {
    case QLocale::English: return QStringLiteral("en");
    case QLocale::Persian: return QStringLiteral("fa");
    case QLocale::German: return QStringLiteral("de");
    case QLocale::Japanese: return QStringLiteral("ja");
    case QLocale::Korean: return QStringLiteral("ko");
    case QLocale::Chinese: return QStringLiteral("zh");
    case QLocale::Hindi: return QStringLiteral("hi");
    case QLocale::Arabic: return QStringLiteral("ar");
    case QLocale::Spanish: return QStringLiteral("es");
    case QLocale::French: return QStringLiteral("fr");
    case QLocale::Portuguese: return QStringLiteral("pt");
    case QLocale::Russian: return QStringLiteral("ru");
    default: return {};
    }
}

[[nodiscard]] QString canonicalLanguageTag(QString value)
{
    value = value.trimmed();
    if (value.isEmpty() || value.compare(QStringLiteral("system"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("system");
    }

    value.replace(QLatin1Char('-'), QLatin1Char('_'));
    const QLocale locale{value};
    return languageTagFor(locale.language());
}

[[nodiscard]] QLocale uiLocaleFor(const QString& languageTag)
{
    QString effectiveTag = languageTag;
    if (effectiveTag == QStringLiteral("system")) {
        effectiveTag = languageTagFor(QLocale::system().language());
        if (effectiveTag.isEmpty()) {
            effectiveTag = QStringLiteral("en");
        }
    }

    if (effectiveTag == QStringLiteral("fa")) return QLocale{QStringLiteral("fa_IR")};
    if (effectiveTag == QStringLiteral("de")) return QLocale{QStringLiteral("de_DE")};
    if (effectiveTag == QStringLiteral("ja")) return QLocale{QStringLiteral("ja_JP")};
    if (effectiveTag == QStringLiteral("ko")) return QLocale{QStringLiteral("ko_KR")};
    if (effectiveTag == QStringLiteral("zh")) return QLocale{QStringLiteral("zh_CN")};
    if (effectiveTag == QStringLiteral("hi")) return QLocale{QStringLiteral("hi_IN")};
    if (effectiveTag == QStringLiteral("ar")) return QLocale{QStringLiteral("ar_SA")};
    if (effectiveTag == QStringLiteral("es")) return QLocale{QStringLiteral("es_ES")};
    if (effectiveTag == QStringLiteral("fr")) return QLocale{QStringLiteral("fr_FR")};
    if (effectiveTag == QStringLiteral("pt")) return QLocale{QStringLiteral("pt_BR")};
    if (effectiveTag == QStringLiteral("ru")) return QLocale{QStringLiteral("ru_RU")};
    return QLocale{QStringLiteral("en_US")};
}

[[nodiscard]] bool firstStrongCharacterIsRtl(const QString& text) noexcept
{
    for (const QChar character : text) {
        switch (character.direction()) {
        case QChar::DirR:
        case QChar::DirAL:
        case QChar::DirRLE:
        case QChar::DirRLO:
        case QChar::DirRLI:
            return true;
        case QChar::DirL:
        case QChar::DirLRE:
        case QChar::DirLRO:
        case QChar::DirLRI:
            return false;
        default:
            break;
        }
    }
    return false;
}

[[nodiscard]] QString normalizeNumberGlyphs(QString text, const bool usePersianDigits)
{
    for (qsizetype index = 0; index < text.size(); ++index) {
        const QChar character = text.at(index);
        const ushort value = character.unicode();
        int digit = -1;
        if (value >= u'0' && value <= u'9') {
            digit = static_cast<int>(value - u'0');
        } else if (value >= 0x06F0 && value <= 0x06F9) {
            digit = static_cast<int>(value - 0x06F0);
        } else if (value >= 0x0660 && value <= 0x0669) {
            digit = static_cast<int>(value - 0x0660);
        }
        if (digit >= 0) {
            text[index] = QChar{static_cast<ushort>((usePersianDigits ? 0x06F0 : u'0') + digit)};
            continue;
        }
        if (!usePersianDigits) {
            if (value == 0x066B) text[index] = QLatin1Char('.');
            else if (value == 0x066C) text[index] = QLatin1Char(',');
        }
    }
    return text;
}

[[nodiscard]] QString catalogLocaleNameFor(const QString& languageTag)
{
    return uiLocaleFor(languageTag).name();
}

[[nodiscard]] QString canonicalRegionalLocaleTag(QString value)
{
    value = value.trimmed();
    if (value.isEmpty() || value.compare(QStringLiteral("system"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("system");
    }

    value.replace(QLatin1Char('-'), QLatin1Char('_'));
    const QLocale locale{value};
    if (locale.language() == QLocale::C || locale.name() == QStringLiteral("C")) {
        return {};
    }
    return locale.name();
}

[[nodiscard]] QLocale regionalLocaleFor(const QString& regionalLocaleTag)
{
    return regionalLocaleTag == QStringLiteral("system")
        ? QLocale::system()
        : QLocale{regionalLocaleTag};
}

[[nodiscard]] QString localeDisplayLabel(const QLocale& locale)
{
    QString language = locale.nativeLanguageName().trimmed();
    QString territory = locale.nativeTerritoryName().trimmed();
    if (language.isEmpty()) {
        language = QLocale::languageToString(locale.language());
    }
    if (territory.isEmpty()) {
        territory = QLocale::territoryToString(locale.territory());
    }
    if (territory.isEmpty()) {
        return language;
    }
    return QStringLiteral("%1 — %2").arg(language, territory);
}

[[nodiscard]] QString defaultCalendarFor(const QLocale& locale)
{
    return locale.language() == QLocale::Persian
        ? QStringLiteral("jalali")
        : QStringLiteral("gregorian");
}

[[nodiscard]] QString canonicalCalendarId(QString value)
{
    value = value.trimmed().toLower();
    if (value.isEmpty() || value == QStringLiteral("auto")) {
        return QStringLiteral("auto");
    }
    if (value == QStringLiteral("gregorian") || value == QStringLiteral("jalali") ||
        value == QStringLiteral("shahanshahi") || value == QStringLiteral("islamic-civil") ||
        value == QStringLiteral("julian") || value == QStringLiteral("milankovic")) {
        return value;
    }
    return {};
}


[[nodiscard]] QString canonicalTimestampDisplayMode(QString value)
{
    value = value.trimmed().toLower();
    if (value.isEmpty() || value == QStringLiteral("smart")) {
        return QStringLiteral("smart");
    }
    if (value == QStringLiteral("relative") || value == QStringLiteral("calendar")) {
        return value;
    }
    return {};
}

[[nodiscard]] QCalendar calendarFor(QString calendarId, const QLocale& locale)
{
    if (calendarId == QStringLiteral("auto")) {
        calendarId = defaultCalendarFor(locale);
    }
    if (calendarId == QStringLiteral("jalali") || calendarId == QStringLiteral("shahanshahi")) {
        return QCalendar{QCalendar::System::Jalali};
    }
    if (calendarId == QStringLiteral("islamic-civil")) {
        return QCalendar{QCalendar::System::IslamicCivil};
    }
    if (calendarId == QStringLiteral("julian")) {
        return QCalendar{QCalendar::System::Julian};
    }
    if (calendarId == QStringLiteral("milankovic")) {
        return QCalendar{QCalendar::System::Milankovic};
    }
    return QCalendar{QCalendar::System::Gregorian};
}

[[nodiscard]] QString canonicalTimeZoneId(QString value)
{
    value = value.trimmed();
    if (value.isEmpty() || value.compare(QStringLiteral("system"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("system");
    }
    if (value.compare(QStringLiteral("UTC"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("UTC");
    }
    const QTimeZone zone{value.toUtf8()};
    return zone.isValid() ? QString::fromUtf8(zone.id()) : QString{};
}

[[nodiscard]] QTimeZone timeZoneFor(const QString& id)
{
    return id == QStringLiteral("system") ? QTimeZone::systemTimeZone() : QTimeZone{id.toUtf8()};
}

[[nodiscard]] QString canonicalCurrency(QString value)
{
    value = value.trimmed().toUpper();
    if (value.isEmpty() || value == QStringLiteral("AUTO")) {
        return QStringLiteral("auto");
    }
    if (value.size() != 3) {
        return {};
    }
    for (const QChar character : value) {
        if (!character.isLetter() || character.unicode() > 0x7f) {
            return {};
        }
    }
    return value;
}


[[nodiscard]] QString canonicalDisplayCurrency(QString value)
{
    value = value.trimmed().toUpper();
    if (value == QStringLiteral("IRT")) {
        return value;
    }
    return canonicalCurrency(value);
}

[[nodiscard]] QStringList canonicalAdditionalCurrencies(const QStringList& values)
{
    QStringList result;
    QSet<QString> seen;
    for (const QString& value : values) {
        const QString canonical = canonicalDisplayCurrency(value);
        if (canonical.isEmpty() || canonical == QStringLiteral("auto") || seen.contains(canonical)) {
            continue;
        }
        seen.insert(canonical);
        result.push_back(canonical);
    }
    return result;
}

[[nodiscard]] QString selectedCurrencyCode(const QString& configured, const QLocale& locale)
{
    if (configured != QStringLiteral("auto")) {
        return configured;
    }
    const QString code = locale.currencySymbol(QLocale::CurrencyIsoCode).trimmed().toUpper();
    return code.isEmpty() ? QStringLiteral("USD") : code;
}

[[nodiscard]] QString currencySymbolFor(const QString& isoCurrency)
{
    static const QHash<QString, QString> symbols = [] {
        QHash<QString, QString> result;
        const auto locales = QLocale::matchingLocales(
            QLocale::AnyLanguage, QLocale::AnyScript, QLocale::AnyTerritory);
        for (const QLocale& candidate : locales) {
            const QString code = candidate.currencySymbol(QLocale::CurrencyIsoCode).trimmed().toUpper();
            if (code.isEmpty() || result.contains(code)) {
                continue;
            }
            const QString symbol = candidate.currencySymbol(QLocale::CurrencySymbol);
            result.insert(code, symbol.isEmpty() ? code : symbol);
        }
        return result;
    }();
    return symbols.value(isoCurrency.toUpper(), isoCurrency);
}

[[nodiscard]] QLocale::FormatType formatTypeFor(const QString& style)
{
    return style.compare(QStringLiteral("long"), Qt::CaseInsensitive) == 0
        ? QLocale::LongFormat
        : QLocale::ShortFormat;
}

[[nodiscard]] QString formatShahanshahi(
    const QDateTime& dateTime, const QLocale& locale, const QString& style)
{
    const QCalendar jalali{QCalendar::System::Jalali};
    const auto parts = jalali.partsFromDate(dateTime.date());
    if (!parts.isValid()) {
        return {};
    }

    const int imperialYear = parts.year + kShahanshahiYearOffset;
    const QString year = locale.toString(imperialYear);
    const QString month = locale.toString(parts.month);
    const QString day = locale.toString(parts.day);
    const QString time = locale.toString(dateTime.time(), QLocale::ShortFormat);

    if (style.compare(QStringLiteral("short"), Qt::CaseInsensitive) == 0) {
        return qtTrId("genycaster.locale.shahanshahi_short").arg(year, month, day, time);
    }

    const QString monthName = jalali.monthName(locale, parts.month, parts.year, QLocale::LongFormat);
    const QString weekday = jalali.weekDayName(
        locale, dateTime.date().dayOfWeek(), QLocale::LongFormat);
    if (style.compare(QStringLiteral("long"), Qt::CaseInsensitive) == 0) {
        return qtTrId("genycaster.locale.shahanshahi_long")
            .arg(weekday, day, monthName, year, time);
    }
    return qtTrId("genycaster.locale.shahanshahi_medium")
        .arg(day, monthName, year, time);
}

} // namespace

class LocalizationController::Impl final {
public:
    Impl(QQmlEngine& engineValue, QSettings& settingsValue)
        : engine{&engineValue}, settings{settingsValue}
    {
    }

    QPointer<QQmlEngine> engine;
    QSettings& settings;
    std::unique_ptr<QTranslator> translator{};
    QString languageTag{QStringLiteral("system")};
    QString regionalLocaleTag{QStringLiteral("system")};
    QString calendarId{QStringLiteral("auto")};
    QString timestampDisplayMode{QStringLiteral("smart")};
    QString timeZoneId{QStringLiteral("system")};
    QString currencyCode{QStringLiteral("auto")};
    QStringList additionalCurrencyCodes{};
    QLocale uiLocale{QLocale{QStringLiteral("en_US")}};
    QLocale regionalLocale{QLocale::system()};
    qulonglong revision{0};
};

LocalizationController::LocalizationController(
    QQmlEngine& engine,
    QSettings& settings,
    StartupOverrides overrides,
    QObject* const parent)
    : QObject{parent}, m_impl{std::make_unique<Impl>(engine, settings)}
{
    const QString storedLanguage = settings.value(
        QString::fromLatin1(kLanguageSetting), QStringLiteral("system")).toString();
    const QString storedRegionalLocale = settings.value(
        QString::fromLatin1(kRegionalLocaleSetting), QStringLiteral("system")).toString();
    const QString storedCalendar = settings.value(
        QString::fromLatin1(kCalendarSetting), QStringLiteral("auto")).toString();
    const QString storedTimestampDisplayMode = settings.value(
        QString::fromLatin1(kTimestampDisplaySetting), QStringLiteral("smart")).toString();
    const QString storedTimeZone = settings.value(
        QString::fromLatin1(kTimeZoneSetting), QStringLiteral("system")).toString();
    const QString storedCurrency = settings.value(
        QString::fromLatin1(kCurrencySetting), QStringLiteral("auto")).toString();
    const QStringList storedAdditionalCurrencies = settings.value(
        QString::fromLatin1(kAdditionalCurrenciesSetting)).toStringList();

    m_impl->regionalLocaleTag = canonicalRegionalLocaleTag(
        overrides.regionalLocaleTag.isEmpty() ? storedRegionalLocale : overrides.regionalLocaleTag);
    if (m_impl->regionalLocaleTag.isEmpty()) {
        m_impl->regionalLocaleTag = QStringLiteral("system");
    }
    m_impl->regionalLocale = regionalLocaleFor(m_impl->regionalLocaleTag);

    m_impl->calendarId = canonicalCalendarId(
        overrides.calendarId.isEmpty() ? storedCalendar : overrides.calendarId);
    if (m_impl->calendarId.isEmpty()) {
        m_impl->calendarId = QStringLiteral("auto");
    }
    m_impl->timestampDisplayMode = canonicalTimestampDisplayMode(
        overrides.timestampDisplayMode.isEmpty() ? storedTimestampDisplayMode : overrides.timestampDisplayMode);
    if (m_impl->timestampDisplayMode.isEmpty()) {
        m_impl->timestampDisplayMode = QStringLiteral("smart");
    }
    m_impl->timeZoneId = canonicalTimeZoneId(
        overrides.timeZoneId.isEmpty() ? storedTimeZone : overrides.timeZoneId);
    if (m_impl->timeZoneId.isEmpty()) {
        m_impl->timeZoneId = QStringLiteral("system");
    }
    m_impl->currencyCode = canonicalDisplayCurrency(
        overrides.currencyCode.isEmpty() ? storedCurrency : overrides.currencyCode);
    if (m_impl->currencyCode.isEmpty()) {
        m_impl->currencyCode = QStringLiteral("auto");
    }
    m_impl->additionalCurrencyCodes = canonicalAdditionalCurrencies(
        overrides.additionalCurrencyCodes.isEmpty()
            ? storedAdditionalCurrencies
            : overrides.additionalCurrencyCodes);

    const QString selectedLanguage = canonicalLanguageTag(
        overrides.languageTag.isEmpty() ? storedLanguage : overrides.languageTag);
    if (!setLanguageTag(selectedLanguage.isEmpty() ? QStringLiteral("system") : selectedLanguage)) {
        throw std::runtime_error{"Genycaster could not load its embedded UI translation catalog"};
    }
}

LocalizationController::~LocalizationController()
{
    if (m_impl->translator) {
        QCoreApplication::removeTranslator(m_impl->translator.get());
    }
}

QString LocalizationController::languageTag() const { return m_impl->languageTag; }
QString LocalizationController::effectiveLanguageTag() const { return languageTagFor(m_impl->uiLocale.language()); }
QString LocalizationController::regionalLocaleTag() const { return m_impl->regionalLocaleTag; }
QString LocalizationController::effectiveLocaleName() const { return m_impl->regionalLocale.name(); }
QString LocalizationController::calendarId() const { return m_impl->calendarId; }
QString LocalizationController::timestampDisplayMode() const { return m_impl->timestampDisplayMode; }
QString LocalizationController::timeZoneId() const { return m_impl->timeZoneId; }
QString LocalizationController::currencyCode() const { return m_impl->currencyCode; }
QString LocalizationController::effectiveCurrencyCode() const
{
    return selectedCurrencyCode(m_impl->currencyCode, m_impl->regionalLocale);
}
QStringList LocalizationController::additionalCurrencyCodes() const
{
    return m_impl->additionalCurrencyCodes;
}
QStringList LocalizationController::effectiveDisplayCurrencyCodes() const
{
    QStringList result{effectiveCurrencyCode()};
    for (const QString& code : m_impl->additionalCurrencyCodes) {
        if (!result.contains(code)) {
            result.push_back(code);
        }
    }
    return result;
}
bool LocalizationController::rightToLeft() const noexcept
{
    return m_impl->uiLocale.textDirection() == Qt::RightToLeft;
}
qulonglong LocalizationController::revision() const noexcept { return m_impl->revision; }

QString LocalizationController::languageLabel() const
{
    if (m_impl->languageTag == QStringLiteral("system")) {
        return qtTrId("genycaster.locale.language_system");
    }
    const QLocale locale = uiLocaleFor(m_impl->languageTag);
    QString label = locale.nativeLanguageName().trimmed();
    return label.isEmpty() ? QLocale::languageToString(locale.language()) : label;
}

QString LocalizationController::regionalLocaleLabel() const
{
    return m_impl->regionalLocaleTag == QStringLiteral("system")
        ? qtTrId("genycaster.locale.region_system")
        : localeDisplayLabel(m_impl->regionalLocale);
}

QString LocalizationController::calendarLabel() const
{
    const QString id = m_impl->calendarId == QStringLiteral("auto")
        ? defaultCalendarFor(m_impl->regionalLocale)
        : m_impl->calendarId;
    if (id == QStringLiteral("jalali")) return qtTrId("genycaster.locale.calendar_jalali");
    if (id == QStringLiteral("shahanshahi")) return qtTrId("genycaster.locale.calendar_shahanshahi");
    if (id == QStringLiteral("islamic-civil")) return qtTrId("genycaster.locale.calendar_islamic_civil");
    if (id == QStringLiteral("julian")) return qtTrId("genycaster.locale.calendar_julian");
    if (id == QStringLiteral("milankovic")) return qtTrId("genycaster.locale.calendar_milankovic");
    return qtTrId("genycaster.locale.calendar_gregorian");
}

QString LocalizationController::timestampDisplayModeLabel() const
{
    if (m_impl->timestampDisplayMode == QStringLiteral("relative")) {
        return qtTrId("genycaster.locale.timestamp_relative");
    }
    if (m_impl->timestampDisplayMode == QStringLiteral("calendar")) {
        return qtTrId("genycaster.locale.timestamp_calendar");
    }
    return qtTrId("genycaster.locale.timestamp_smart");
}

QString LocalizationController::timeZoneLabel() const
{
    if (m_impl->timeZoneId == QStringLiteral("system")) {
        return qtTrId("genycaster.locale.time_zone_system");
    }
    return m_impl->timeZoneId;
}

QString LocalizationController::currencyLabel() const
{
    if (m_impl->currencyCode == QStringLiteral("auto")) {
        return qtTrId("genycaster.locale.currency_auto_value").arg(effectiveCurrencyCode());
    }
    if (m_impl->currencyCode == QStringLiteral("IRT")) {
        return qtTrId("genycaster.locale.currency_irt_toman");
    }
    return m_impl->currencyCode;
}

QVariantList LocalizationController::availableLanguages() const
{
    QVariantList result{
        QVariantMap{{QStringLiteral("id"), QStringLiteral("system")},
                    {QStringLiteral("label"), qtTrId("genycaster.locale.language_system")}},
    };
    static constexpr std::array<const char*, 12> kLanguages{
        "en", "fa", "de", "ja", "ko", "zh", "hi", "ar", "es", "fr", "pt", "ru"};
    result.reserve(static_cast<qsizetype>(kLanguages.size() + 1));
    for (const char* tag : kLanguages) {
        const QString languageTag = QString::fromLatin1(tag);
        const QLocale locale = uiLocaleFor(languageTag);
        QString label = locale.nativeLanguageName().trimmed();
        if (label.isEmpty()) {
            label = QLocale::languageToString(locale.language());
        }
        result.push_back(QVariantMap{
            {QStringLiteral("id"), languageTag},
            {QStringLiteral("label"), label},
        });
    }
    return result;
}

QVariantList LocalizationController::availableRegionalLocales() const
{
    static constexpr std::array<const char*, 13> kSuggestedLocales{
        "en_US", "fa_IR", "en_GB", "ar_AE", "tr_TR", "zh_CN", "ja_JP",
        "ko_KR", "hi_IN", "en_CA", "en_AU", "de_CH", "fr_FR"};

    QVariantList result{
        QVariantMap{{QStringLiteral("id"), QStringLiteral("system")},
                    {QStringLiteral("label"), qtTrId("genycaster.locale.region_system")}}
    };
    result.reserve(static_cast<qsizetype>(kSuggestedLocales.size() + 1));
    for (const char* localeName : kSuggestedLocales) {
        const QLocale locale{QString::fromLatin1(localeName)};
        result.push_back(QVariantMap{
            {QStringLiteral("id"), locale.name()},
            {QStringLiteral("label"), localeDisplayLabel(locale)},
        });
    }
    return result;
}

QVariantList LocalizationController::availableCalendars() const
{
    return {
        QVariantMap{{QStringLiteral("id"), QStringLiteral("auto")},
                    {QStringLiteral("label"), qtTrId("genycaster.locale.calendar_auto")}},
        QVariantMap{{QStringLiteral("id"), QStringLiteral("gregorian")},
                    {QStringLiteral("label"), qtTrId("genycaster.locale.calendar_gregorian")}},
        QVariantMap{{QStringLiteral("id"), QStringLiteral("jalali")},
                    {QStringLiteral("label"), qtTrId("genycaster.locale.calendar_jalali")}},
        QVariantMap{{QStringLiteral("id"), QStringLiteral("shahanshahi")},
                    {QStringLiteral("label"), qtTrId("genycaster.locale.calendar_shahanshahi")}},
        QVariantMap{{QStringLiteral("id"), QStringLiteral("islamic-civil")},
                    {QStringLiteral("label"), qtTrId("genycaster.locale.calendar_islamic_civil")}},
        QVariantMap{{QStringLiteral("id"), QStringLiteral("julian")},
                    {QStringLiteral("label"), qtTrId("genycaster.locale.calendar_julian")}},
        QVariantMap{{QStringLiteral("id"), QStringLiteral("milankovic")},
                    {QStringLiteral("label"), qtTrId("genycaster.locale.calendar_milankovic")}},
    };
}

QVariantList LocalizationController::availableTimestampDisplayModes() const
{
    return {
        QVariantMap{{QStringLiteral("id"), QStringLiteral("smart")},
                    {QStringLiteral("label"), qtTrId("genycaster.locale.timestamp_smart")}},
        QVariantMap{{QStringLiteral("id"), QStringLiteral("relative")},
                    {QStringLiteral("label"), qtTrId("genycaster.locale.timestamp_relative")}},
        QVariantMap{{QStringLiteral("id"), QStringLiteral("calendar")},
                    {QStringLiteral("label"), qtTrId("genycaster.locale.timestamp_calendar")}},
    };
}

QVariantList LocalizationController::availableTimeZones() const
{
    QVariantList result{
        QVariantMap{{QStringLiteral("id"), QStringLiteral("system")},
                    {QStringLiteral("label"), qtTrId("genycaster.locale.time_zone_system")}}
    };
    const auto ids = QTimeZone::availableTimeZoneIds();
    result.reserve(ids.size() + 1);
    for (const QByteArray& id : ids) {
        const QString value = QString::fromUtf8(id);
        result.push_back(QVariantMap{
            {QStringLiteral("id"), value},
            {QStringLiteral("label"), value},
        });
    }
    return result;
}

QVariantList LocalizationController::availableCurrencies() const
{
    QSet<QString> uniqueCodes;
    const auto locales = QLocale::matchingLocales(
        QLocale::AnyLanguage, QLocale::AnyScript, QLocale::AnyTerritory);
    for (const QLocale& locale : locales) {
        const QString code = locale.currencySymbol(QLocale::CurrencyIsoCode).trimmed().toUpper();
        if (code.size() == 3) {
            uniqueCodes.insert(code);
        }
    }

    QStringList isoCurrencies = uniqueCodes.values();
    std::ranges::sort(isoCurrencies);

    QVariantList result{
        QVariantMap{{QStringLiteral("id"), QStringLiteral("auto")},
                    {QStringLiteral("label"),
                     qtTrId("genycaster.locale.currency_auto_value").arg(effectiveCurrencyCode())},
                    {QStringLiteral("additional"), false}}
    };
    result.reserve(isoCurrencies.size() + 2);
    for (const QString& code : isoCurrencies) {
        result.push_back(QVariantMap{
            {QStringLiteral("id"), code},
            {QStringLiteral("label"), code},
            {QStringLiteral("additional"), true},
        });
    }
    result.push_back(QVariantMap{
        {QStringLiteral("id"), QStringLiteral("IRT")},
        {QStringLiteral("label"), qtTrId("genycaster.locale.currency_irt_toman")},
        {QStringLiteral("additional"), true},
    });
    return result;
}

bool LocalizationController::setLanguageTag(const QString& languageTag)
{
    const QString canonical = canonicalLanguageTag(languageTag);
    if (canonical.isEmpty()) {
        return false;
    }

    const QString catalogLocale = catalogLocaleNameFor(canonical);
    const QString resource = QString::fromLatin1(kTranslationResourcePrefix)
        + catalogLocale + QStringLiteral(".qm");

    auto candidate = std::make_unique<QTranslator>();
    if (!candidate->load(resource)) {
        qCritical().noquote()
            << "Unable to load embedded Genycaster translation catalog:" << resource;
        return false;
    }

    std::unique_ptr<QTranslator> previous = std::move(m_impl->translator);
    if (previous) {
        QCoreApplication::removeTranslator(previous.get());
    }

    const auto rollback = [&] {
        if (previous) {
            if (!QCoreApplication::installTranslator(previous.get())) {
                qFatal("Unable to restore the previous Genycaster translation catalog.");
            }
        }
        m_impl->translator = std::move(previous);
    };

    if (!QCoreApplication::installTranslator(candidate.get())) {
        qCritical().noquote()
            << "Unable to install embedded Genycaster translation catalog:" << resource;
        rollback();
        return false;
    }

    // qtTrId() deliberately returns the id itself when no id-based catalog entry
    // exists. Treat that as a packaging/build invariant failure instead of
    // allowing hundreds of QString::arg warnings to leak into the UI runtime.
    if (qtTrId(kTranslationProbeId) == QString::fromLatin1(kTranslationProbeId)) {
        QCoreApplication::removeTranslator(candidate.get());
        qCritical().noquote()
            << "Embedded Genycaster catalog is not a valid ID-based translation catalog:"
            << resource;
        rollback();
        return false;
    }

    m_impl->translator = std::move(candidate);
    m_impl->languageTag = canonical;
    m_impl->uiLocale = uiLocaleFor(canonical);

    QLocale::setDefault(m_impl->regionalLocale);
    QGuiApplication::setLayoutDirection(m_impl->uiLocale.textDirection());
    m_impl->settings.setValue(QString::fromLatin1(kLanguageSetting), m_impl->languageTag);
    if (m_impl->engine) {
        m_impl->engine->retranslate();
    }
    ++m_impl->revision;
    emit localeChanged();
    return true;
}

bool LocalizationController::setRegionalLocaleTag(const QString& regionalLocaleTag)
{
    const QString canonical = canonicalRegionalLocaleTag(regionalLocaleTag);
    if (canonical.isEmpty()) {
        return false;
    }
    m_impl->regionalLocaleTag = canonical;
    m_impl->regionalLocale = regionalLocaleFor(canonical);
    QLocale::setDefault(m_impl->regionalLocale);
    m_impl->settings.setValue(QString::fromLatin1(kRegionalLocaleSetting), canonical);
    ++m_impl->revision;
    emit localeChanged();
    return true;
}

bool LocalizationController::setCalendarId(const QString& calendarId)
{
    const QString canonical = canonicalCalendarId(calendarId);
    if (canonical.isEmpty()) return false;
    m_impl->calendarId = canonical;
    m_impl->settings.setValue(QString::fromLatin1(kCalendarSetting), canonical);
    ++m_impl->revision;
    emit localeChanged();
    return true;
}

bool LocalizationController::setTimestampDisplayMode(const QString& mode)
{
    const QString canonical = canonicalTimestampDisplayMode(mode);
    if (canonical.isEmpty()) {
        return false;
    }
    m_impl->timestampDisplayMode = canonical;
    m_impl->settings.setValue(QString::fromLatin1(kTimestampDisplaySetting), canonical);
    ++m_impl->revision;
    emit localeChanged();
    return true;
}

bool LocalizationController::setTimeZoneId(const QString& timeZoneId)
{
    const QString canonical = canonicalTimeZoneId(timeZoneId);
    if (canonical.isEmpty()) return false;
    m_impl->timeZoneId = canonical;
    m_impl->settings.setValue(QString::fromLatin1(kTimeZoneSetting), canonical);
    ++m_impl->revision;
    emit localeChanged();
    return true;
}

bool LocalizationController::setCurrencyCode(const QString& currencyCode)
{
    const QString canonical = canonicalDisplayCurrency(currencyCode);
    if (canonical.isEmpty()) return false;
    m_impl->currencyCode = canonical;
    m_impl->settings.setValue(QString::fromLatin1(kCurrencySetting), canonical);
    ++m_impl->revision;
    emit localeChanged();
    return true;
}

bool LocalizationController::setAdditionalCurrencyCodes(const QStringList& currencyCodes)
{
    const QStringList canonical = canonicalAdditionalCurrencies(currencyCodes);
    m_impl->additionalCurrencyCodes = canonical;
    m_impl->settings.setValue(QString::fromLatin1(kAdditionalCurrenciesSetting), canonical);
    ++m_impl->revision;
    emit localeChanged();
    return true;
}

bool LocalizationController::toggleAdditionalCurrencyCode(const QString& currencyCode)
{
    const QString canonical = canonicalDisplayCurrency(currencyCode);
    if (canonical.isEmpty() || canonical == QStringLiteral("auto")) {
        return false;
    }

    QStringList updated = m_impl->additionalCurrencyCodes;
    if (updated.contains(canonical)) {
        updated.removeAll(canonical);
    } else {
        updated.push_back(canonical);
    }
    return setAdditionalCurrencyCodes(updated);
}

bool LocalizationController::additionalCurrencyEnabled(const QString& currencyCode) const
{
    const QString canonical = canonicalDisplayCurrency(currencyCode);
    return !canonical.isEmpty() && m_impl->additionalCurrencyCodes.contains(canonical);
}

void LocalizationController::resetToSystemDefaults()
{
    m_impl->regionalLocaleTag = QStringLiteral("system");
    m_impl->regionalLocale = QLocale::system();
    m_impl->calendarId = QStringLiteral("auto");
    m_impl->timestampDisplayMode = QStringLiteral("smart");
    m_impl->timeZoneId = QStringLiteral("system");
    m_impl->currencyCode = QStringLiteral("auto");
    m_impl->additionalCurrencyCodes.clear();
    m_impl->settings.remove(QString::fromLatin1(kRegionalLocaleSetting));
    m_impl->settings.remove(QString::fromLatin1(kCalendarSetting));
    m_impl->settings.remove(QString::fromLatin1(kTimestampDisplaySetting));
    m_impl->settings.remove(QString::fromLatin1(kTimeZoneSetting));
    m_impl->settings.remove(QString::fromLatin1(kCurrencySetting));
    m_impl->settings.remove(QString::fromLatin1(kAdditionalCurrenciesSetting));
    m_impl->settings.remove(QString::fromLatin1(kLanguageSetting));
    setLanguageTag(QStringLiteral("system"));
}

bool LocalizationController::isRightToLeftText(const QString& text) const noexcept
{
    if (text.trimmed().isEmpty()) {
        return rightToLeft();
    }
    return firstStrongCharacterIsRtl(text);
}

QString LocalizationController::formatIdentifier(const qulonglong value) const
{
    return QString::number(value);
}

QString LocalizationController::formatNumber(const double value, const int precision) const
{
    const QString formatted = precision < 0
        ? m_impl->regionalLocale.toString(value, 'g', QLocale::FloatingPointShortest)
        : m_impl->regionalLocale.toString(value, 'f', std::clamp(precision, 0, 12));
    return normalizeNumberGlyphs(formatted, rightToLeft());
}

QString LocalizationController::formatInteger(const qint64 value) const
{
    return normalizeNumberGlyphs(m_impl->regionalLocale.toString(value), rightToLeft());
}

QString LocalizationController::formatCompactNumber(const double value) const
{
    const double absolute = std::abs(value);
    if (absolute >= 1'000'000'000.0) {
        return qtTrId("genycaster.locale.compact_billion")
            .arg(formatNumber(value / 1'000'000'000.0, absolute >= 10'000'000'000.0 ? 0 : 1));
    }
    if (absolute >= 1'000'000.0) {
        return qtTrId("genycaster.locale.compact_million")
            .arg(formatNumber(value / 1'000'000.0, absolute >= 10'000'000.0 ? 0 : 1));
    }
    if (absolute >= 1'000.0) {
        return qtTrId("genycaster.locale.compact_thousand")
            .arg(formatNumber(value / 1'000.0, absolute >= 10'000.0 ? 0 : 1));
    }
    return formatInteger(static_cast<qint64>(std::llround(value)));
}

QString LocalizationController::formatTimestamp(
    const qint64 epochMilliseconds, const QString& style) const
{
    if (m_impl->timestampDisplayMode == QStringLiteral("calendar")) {
        return formatDateTime(epochMilliseconds, style);
    }
    if (m_impl->timestampDisplayMode == QStringLiteral("relative")) {
        return formatRelativeTime(epochMilliseconds);
    }

    const qint64 nowMs = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
    const qint64 elapsedMs = std::max<qint64>(0, nowMs - epochMilliseconds);
    constexpr qint64 kSmartRelativeWindowMs = 7LL * 24LL * 60LL * 60LL * 1000LL;
    return elapsedMs < kSmartRelativeWindowMs
        ? formatRelativeTime(epochMilliseconds)
        : formatDateTime(epochMilliseconds, style);
}

QString LocalizationController::formatRelativeTime(const qint64 epochMilliseconds) const
{
    const qint64 nowMs = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
    const qint64 elapsedSeconds = std::max<qint64>(0, (nowMs - epochMilliseconds) / 1000);
    if (elapsedSeconds < 60) return qtTrId("genycaster.locale.relative_now");
    if (elapsedSeconds < 3600) {
        const qint64 value = elapsedSeconds / 60;
        return value == 1 ? qtTrId("genycaster.locale.relative_minute")
                          : qtTrId("genycaster.locale.relative_minutes").arg(formatInteger(value));
    }
    if (elapsedSeconds < 86'400) {
        const qint64 value = elapsedSeconds / 3600;
        return value == 1 ? qtTrId("genycaster.locale.relative_hour")
                          : qtTrId("genycaster.locale.relative_hours").arg(formatInteger(value));
    }
    if (elapsedSeconds < 604'800) {
        const qint64 value = elapsedSeconds / 86'400;
        return value == 1 ? qtTrId("genycaster.locale.relative_day")
                          : qtTrId("genycaster.locale.relative_days").arg(formatInteger(value));
    }
    if (elapsedSeconds < 2'629'746) {
        const qint64 value = elapsedSeconds / 604'800;
        return value == 1 ? qtTrId("genycaster.locale.relative_week")
                          : qtTrId("genycaster.locale.relative_weeks").arg(formatInteger(value));
    }
    if (elapsedSeconds < 31'556'952) {
        const qint64 value = std::max<qint64>(1, elapsedSeconds / 2'629'746);
        return value == 1 ? qtTrId("genycaster.locale.relative_month")
                          : qtTrId("genycaster.locale.relative_months").arg(formatInteger(value));
    }
    const qint64 value = std::max<qint64>(1, elapsedSeconds / 31'556'952);
    return value == 1 ? qtTrId("genycaster.locale.relative_year")
                      : qtTrId("genycaster.locale.relative_years").arg(formatInteger(value));
}

QString LocalizationController::formatDateTime(
    const qint64 epochMilliseconds, const QString& style) const
{
    const QTimeZone zone = timeZoneFor(m_impl->timeZoneId);
    const QDateTime dateTime = QDateTime::fromMSecsSinceEpoch(epochMilliseconds, zone);
    if (!dateTime.isValid()) return {};

    const QString effectiveCalendar = m_impl->calendarId == QStringLiteral("auto")
        ? defaultCalendarFor(m_impl->regionalLocale)
        : m_impl->calendarId;
    if (effectiveCalendar == QStringLiteral("shahanshahi")) {
        return formatShahanshahi(dateTime, m_impl->regionalLocale, style);
    }

    const QCalendar calendar = calendarFor(effectiveCalendar, m_impl->regionalLocale);
    const QLocale::FormatType type = formatTypeFor(style);
    return normalizeNumberGlyphs(calendar.dateTimeToString(
        m_impl->regionalLocale.dateTimeFormat(type), dateTime, QDate{}, QTime{},
        m_impl->regionalLocale), rightToLeft());
}

QString LocalizationController::formatCurrency(
    const double value, const QString& isoCurrency, const int precision) const
{
    QString code = isoCurrency.trimmed().toUpper();
    if (code.isEmpty()) code = effectiveCurrencyCode();
    if (code.size() != 3 || code == QStringLiteral("IRT")) return {};
    const QString symbol = currencySymbolFor(code);
    return normalizeNumberGlyphs(
        m_impl->regionalLocale.toCurrencyString(value, symbol, precision), rightToLeft());
}

QString LocalizationController::formatMonetaryUnit(
    const double value, const QString& displayUnit, const int precision) const
{
    QString unit = canonicalDisplayCurrency(displayUnit);
    if (unit.isEmpty()) {
        return {};
    }
    if (unit == QStringLiteral("auto")) {
        unit = effectiveCurrencyCode();
    }
    if (unit == QStringLiteral("IRT")) {
        const QString amount = precision < 0
            ? formatNumber(value)
            : formatNumber(value, precision);
        return qtTrId("genycaster.locale.toman_amount").arg(amount);
    }
    return formatCurrency(value, unit, precision);
}

QString LocalizationController::formatAssetQuote(
    const QString& assetSymbol,
    const double value,
    const QString& displayUnit,
    const int precision) const
{
    const QString symbol = assetSymbol.trimmed().toUpper();
    if (symbol.isEmpty()) {
        return {};
    }
    const QString formattedValue = formatMonetaryUnit(value, displayUnit, precision);
    if (formattedValue.isEmpty()) {
        return {};
    }
    const QString baseAmount = QStringLiteral("%1 %2").arg(formatNumber(1.0, 0), symbol);
    return qtTrId("genycaster.locale.asset_quote").arg(baseAmount, formattedValue);
}

double LocalizationController::convertDisplayDenomination(
    const double value, const QString& fromUnit, const QString& toUnit) const
{
    const QString from = canonicalDisplayCurrency(fromUnit);
    const QString to = canonicalDisplayCurrency(toUnit);
    if (from.isEmpty() || to.isEmpty()) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    if (from == to) {
        return value;
    }
    if (from == QStringLiteral("IRR") && to == QStringLiteral("IRT")) {
        return value / 10.0;
    }
    if (from == QStringLiteral("IRT") && to == QStringLiteral("IRR")) {
        return value * 10.0;
    }
    return std::numeric_limits<double>::quiet_NaN();
}

