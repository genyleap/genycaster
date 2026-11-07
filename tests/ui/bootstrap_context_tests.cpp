/*!
 * @file        tests/ui/bootstrap_context_tests.cpp
 * @brief       Validated non-owning QML root context registration tests.
 *
 * @details
 * Verifies atomic controller-name validation, exact QObject identity and
 * stable font/platform property exposure to a native Qt Quick engine.
 *
 * @author      Kambiz Asadzadeh
 * @since       07 Nov 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

#include <QGuiApplication>
#include <QObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QString>
#include <QVariant>

import genycaster.ui.bootstrap.context;
import genycaster.ui.bootstrap.fonts;

int main(int argc, char** argv)
{
    QGuiApplication application(argc, argv);
    QQmlApplicationEngine engine{};
    QObject primary{};
    QObject secondary{};
    auto* context = engine.rootContext();
    using genycaster::ui::bootstrap::exposeControllers;

    if (exposeControllers(engine, {
            {"temporary", &primary},
            {"temporary", &secondary},
        })) return 1;
    // No properties may be visible after a rejected batch.
    if (context->contextProperty(QStringLiteral("temporary")).isValid()) return 2;

    if (exposeControllers(engine, {
            {"partial", &primary},
            {"missing", nullptr},
        })) return 3;
    if (context->contextProperty(QStringLiteral("partial")).isValid()) return 4;

    if (exposeControllers(engine, {
            {"", &primary},
        })) return 5;

    if (!exposeControllers(engine, {
            {"presentationBridge", &primary},
            {"miniAppHost", &secondary},
        })) return 6;
    if (qvariant_cast<QObject*>(context->contextProperty(
            QStringLiteral("presentationBridge"))) != &primary) return 7;
    if (qvariant_cast<QObject*>(context->contextProperty(
            QStringLiteral("miniAppHost"))) != &secondary) return 8;

    genycaster::ui::bootstrap::FontFamilies fonts{};
    genycaster::ui::bootstrap::exposeFontFamilies(engine, fonts);
    for (const QString& name : {
             QStringLiteral("platformTextFontFamily"),
             QStringLiteral("platformRtlFontFamily"),
             QStringLiteral("platformDevanagariFontFamily"),
             QStringLiteral("platformJapaneseFontFamily"),
             QStringLiteral("platformKoreanFontFamily"),
             QStringLiteral("platformChineseFontFamily"),
             QStringLiteral("bundledPersianFontFamily"),
             QStringLiteral("bundledDevanagariFontFamily"),
             QStringLiteral("bundledJapaneseFontFamily"),
             QStringLiteral("bundledKoreanFontFamily"),
             QStringLiteral("bundledChineseFontFamily"),
             QStringLiteral("bundledIconFontFamily"),
             QStringLiteral("bundledIconRegularFontFamily"),
         }) {
        if (!context->contextProperty(name).isValid()) return 9;
    }

    genycaster::ui::bootstrap::exposePlatformFlags(engine, true, false);
    if (!context->contextProperty(QStringLiteral("wasmReadOnly")).toBool()) return 10;
    if (context->contextProperty(QStringLiteral("nativeAudioOutputEnabled")).toBool())
        return 11;
    genycaster::ui::bootstrap::exposePlatformFlags(engine, false, true);
    if (context->contextProperty(QStringLiteral("wasmReadOnly")).toBool()) return 12;
    if (!context->contextProperty(QStringLiteral("nativeAudioOutputEnabled")).toBool())
        return 13;

    // The engine never owns these C++ controllers.
    if (primary.parent() != nullptr || secondary.parent() != nullptr) return 14;
    return 0;
}
