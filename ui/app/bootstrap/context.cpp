/*!
 * @file        ui/app/bootstrap/context.cpp
 * @brief       Root QML context registration implementation.
 *
 * @details
 * Publishes borrowed presentation objects and public font/platform metadata
 * only after validating all QObject bindings. QML never takes ownership of
 * these pointers; the application controls their teardown order.
 *
 * @author      Kambiz Asadzadeh
 * @since       07 Nov 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include <QByteArray>
#include <QDebug>
#include <QObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QString>
#include <QSet>
#include <QVariant>

#include <initializer_list>

module genycaster.ui.bootstrap.context;

namespace genycaster::ui::bootstrap {

bool exposeControllers(
    QQmlApplicationEngine& engine,
    const std::initializer_list<ContextObject> objects)
{
    QSet<QString> names{};
    // Validate the entire batch first: no partially published bindings.
    for (const auto& entry : objects) {
        if (entry.name == nullptr || entry.object == nullptr) {
            qCritical().noquote() << "QML context includes a missing name or controller.";
            return false;
        }
        const QString name = QString::fromUtf8(entry.name);
        if (name.isEmpty() || names.contains(name)) {
            qCritical().noquote() << "QML context includes an empty or duplicate name.";
            return false;
        }
        names.insert(name);
    }
    QQmlContext* const context = engine.rootContext();
    for (const auto& entry : objects) {
        // Setting C++-owned QObject* does not transfer object ownership.
        context->setContextProperty(QString::fromUtf8(entry.name), entry.object);
    }
    return true;
}

void exposeFontFamilies(QQmlApplicationEngine& engine, const FontFamilies& fonts)
{
    QQmlContext* const context = engine.rootContext();
    context->setContextProperty(QStringLiteral("platformTextFontFamily"), fonts.systemText());
    context->setContextProperty(QStringLiteral("platformRtlFontFamily"), fonts.systemRtl());
    context->setContextProperty(QStringLiteral("platformDevanagariFontFamily"), fonts.systemDevanagari());
    context->setContextProperty(QStringLiteral("platformJapaneseFontFamily"), fonts.systemJapanese());
    context->setContextProperty(QStringLiteral("platformKoreanFontFamily"), fonts.systemKorean());
    context->setContextProperty(QStringLiteral("platformChineseFontFamily"), fonts.systemChinese());
    context->setContextProperty(QStringLiteral("bundledPersianFontFamily"), fonts.persian());
    context->setContextProperty(QStringLiteral("bundledDevanagariFontFamily"), fonts.devanagari());
    context->setContextProperty(QStringLiteral("bundledJapaneseFontFamily"), fonts.japanese());
    context->setContextProperty(QStringLiteral("bundledKoreanFontFamily"), fonts.korean());
    context->setContextProperty(QStringLiteral("bundledChineseFontFamily"), fonts.chinese());
    context->setContextProperty(QStringLiteral("bundledIconFontFamily"), fonts.iconSolid());
    context->setContextProperty(QStringLiteral("bundledIconRegularFontFamily"), fonts.iconRegular());
}

void exposePlatformFlags(
    QQmlApplicationEngine& engine,
    const bool webReadOnly,
    const bool nativeAudioAvailable)
{
    QQmlContext* const context = engine.rootContext();
    context->setContextProperty(QStringLiteral("wasmReadOnly"), QVariant{webReadOnly});
    context->setContextProperty(
        QStringLiteral("nativeAudioOutputEnabled"), QVariant{nativeAudioAvailable});
}

} // namespace genycaster::ui::bootstrap
