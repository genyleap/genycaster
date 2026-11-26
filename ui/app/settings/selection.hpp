/*!
 * @file        ui/app/settings/selection.hpp
 * @brief       Cross-platform text selection preferences.
 *
 * @details
 * Stores user-facing selection behavior choices for native and browser Qt
 * views without changing canonical content.
 *
 * @author      Kambiz Asadzadeh
 * @since       22 Oct 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

#pragma once
#include <QObject>
#include <QString>

class QSettings;

/** Per-device choice between Genycaster selection affordances and Qt's
 * platform text input / selection behavior. WASM uses a separate selectable
 * browser DOM selection surface for cast text rather than selecting canvas pixels. */
class TextSelectionSettings final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString mode READ mode NOTIFY modeChanged FINAL)
    Q_PROPERTY(bool nativeAvailable READ nativeAvailable CONSTANT FINAL)
    Q_PROPERTY(bool systemSelectionEnabled READ systemSelectionEnabled NOTIFY modeChanged FINAL)
public:
    explicit TextSelectionSettings(QSettings& settings, QObject* parent = nullptr);
    [[nodiscard]] QString mode() const;
    [[nodiscard]] bool nativeAvailable() const noexcept;
    [[nodiscard]] bool systemSelectionEnabled() const noexcept;
    Q_INVOKABLE void setMode(const QString& mode);
signals:
    void modeChanged();
private:
    QSettings& m_settings;
    QString m_mode;
};
