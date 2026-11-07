/*!
 * @file        ui/app/modules/qt_fwd.hpp
 * @brief       Forward declarations for Qt-facing named-module interfaces.
 *
 * @details
 * Keeps concrete Qt and QObject includes private to module implementations,
 * avoids exporting Qt headers in module ABI and preserves the Qt MOC boundary.
 *
 * @author      Kambiz Asadzadeh
 * @since       07 Nov 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

// Forward declarations for the Qt-facing module interfaces.
// Keep the full QObject and Qt headers private to implementation units.
class QString;
class QStringView;
template <class T> class QList;
using QStringList = QList<QString>;
class QPoint;
class QPointF;
class QQuickWindow;
class QSettings;
class QGuiApplication;
class QQmlApplicationEngine;
class QObject;
class PresentationBridge;
class RichComposerController;
