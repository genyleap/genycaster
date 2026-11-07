#include "text_selection_settings.hpp"
#include <QSettings>
#include <QtGlobal>

namespace { constexpr auto key = "accessibility/textSelectionMode"; }
TextSelectionSettings::TextSelectionSettings(QSettings& settings, QObject* parent)
    : QObject(parent), m_settings(settings)
{
    const QString saved = settings.value(QString::fromLatin1(key)).toString();
    m_mode = nativeAvailable() && saved == QStringLiteral("system")
        ? QStringLiteral("system") : QStringLiteral("genycaster");
}
QString TextSelectionSettings::mode() const { return m_mode; }
bool TextSelectionSettings::nativeAvailable() const noexcept
{
#if defined(Q_OS_WASM) || defined(Q_OS_WASM)
    // Browser-native selection operates on DOM text, not a Qt Quick canvas.
    return false;
#else
    return true;
#endif
}
bool TextSelectionSettings::systemSelectionEnabled() const noexcept
{
    return nativeAvailable() && m_mode == QStringLiteral("system");
}
void TextSelectionSettings::setMode(const QString& requested)
{
    if (requested != QStringLiteral("genycaster")
        && requested != QStringLiteral("system")) return;
    if (requested == QStringLiteral("system") && !nativeAvailable()) return;
    if (m_mode == requested) return;
    m_mode = requested;
    m_settings.setValue(QString::fromLatin1(key), m_mode);
    m_settings.sync();
    emit modeChanged();
}
