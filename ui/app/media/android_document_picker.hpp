/*!
 * @file        ui/app/media/android_document_picker.hpp
 * @brief       Android Storage Access Framework picker for cast attachments.
 *
 * @details
 * Imports user-granted content:// URIs to private, session-scoped files before
 * MediaUploadController reads them. Android-only, without broad storage or
 * READ_MEDIA permissions.
 *
 * @author      Genyleap Engineering
 * @since       21 Nov 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */
#pragma once

#if !defined(Q_OS_ANDROID)
#error AndroidComposerMediaPicker is only available on Android.
#endif

#include "media_upload_controller.hpp"

#include <QtCore/private/qandroidextras_p.h>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJniEnvironment>
#include <QJniObject>
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QSaveFile>
#include <QStandardPaths>
#include <QString>
#include <QTemporaryDir>
#include <QThread>
#include <QUuid>
#include <QUrl>
#include <QVariant>

#include <array>
#include <memory>
#include <utility>

class AndroidComposerMediaPicker final : public QObject, public QAndroidActivityResultReceiver {
public:
    explicit AndroidComposerMediaPicker(MediaUploadController& media, QObject* parent = nullptr)
        : QObject{parent}, m_media{&media}
    {
    }

    ~AndroidComposerMediaPicker() override
    {
        // The picker owns the temporary files for the lifetime of the app.
        // A worker must stop reading the SAF document before they are removed.
        if (m_copyWorker) {
            m_copyWorker->requestInterruption();
            m_copyWorker->wait();
        }
    }

    void open(const int afterIndex)
    {
        if (m_pending || !m_media || !m_media->available() || m_media->busy())
            return;
        if (!QNativeInterface::QAndroidApplication::isActivityContext()) {
            fail(QStringLiteral("Android cannot open a document picker without an active Activity."));
            return;
        }

        if (!m_temporaryFiles) {
            const QString location = QStandardPaths::writableLocation(
                QStandardPaths::CacheLocation);
            if (location.isEmpty() || !QDir{}.mkpath(location)) {
                fail(QStringLiteral("Unable to prepare the private media import directory."));
                return;
            }
            m_temporaryFiles = std::make_unique<QTemporaryDir>(
                QDir{location}.filePath(QStringLiteral("composer-media-XXXXXX")));
            if (!m_temporaryFiles->isValid()) {
                m_temporaryFiles.reset();
                fail(QStringLiteral("Unable to create the private media import directory."));
                return;
            }
        }

        // ACTION_OPEN_DOCUMENT shows the system Documents / Photos selector.
        // A user-granted URI requires no device-wide media/storage permission.
        const QJniObject action =
            QJniObject::fromString(QStringLiteral("android.intent.action.OPEN_DOCUMENT"));
        QJniObject intent{
            "android/content/Intent", "(Ljava/lang/String;)V", action.object<jstring>()};
        if (!intent.isValid()) {
            fail(QStringLiteral("Android document picker could not be initialized."));
            return;
        }
        const QJniObject category =
            QJniObject::fromString(QStringLiteral("android.intent.category.OPENABLE"));
        intent.callObjectMethod("addCategory",
                                "(Ljava/lang/String;)Landroid/content/Intent;",
                                category.object<jstring>());
        const QJniObject anyType = QJniObject::fromString(QStringLiteral("*/*"));
        intent.callObjectMethod("setType",
                                "(Ljava/lang/String;)Landroid/content/Intent;",
                                anyType.object<jstring>());

        // Restrict the system picker to the media types accepted by the
        // existing native upload gateway. GIF uses the same photo action.
        static constexpr std::array<const char*, 9> types{
            "image/jpeg", "image/png", "image/webp", "image/gif",
            "image/avif", "video/mp4", "video/webm", "video/quicktime",
            "image/jpg"
        };
        QJniEnvironment jni;
        // Qt owns the cached global class reference returned by findClass().
        // Never pass it to DeleteLocalRef (ART aborts with CheckJNI enabled).
        jclass stringClass = jni.findClass("java/lang/String");
        const bool classFailed = jni.checkAndClearExceptions();
        if (classFailed || stringClass == nullptr) {
            fail(QStringLiteral("Android document picker is unavailable."));
            return;
        }
        jobjectArray filters = jni->NewObjectArray(
            static_cast<jsize>(types.size()), stringClass, nullptr);
        const bool allocationFailed = jni.checkAndClearExceptions();
        if (allocationFailed || filters == nullptr) {
            if (filters != nullptr) jni->DeleteLocalRef(filters);
            fail(QStringLiteral("Android document picker could not allocate media filters."));
            return;
        }
        for (qsizetype index = 0; index < static_cast<qsizetype>(types.size()); ++index) {
            const QJniObject mime = QJniObject::fromString(
                QString::fromLatin1(types[static_cast<std::size_t>(index)]));
            if (!mime.isValid()) {
                jni->DeleteLocalRef(filters);
                fail(QStringLiteral("Android document picker could not prepare a media filter."));
                return;
            }
            jni->SetObjectArrayElement(filters, static_cast<jsize>(index),
                                       mime.object<jstring>());
            if (jni.checkAndClearExceptions()) {
                jni->DeleteLocalRef(filters);
                fail(QStringLiteral("Android document picker could not set a media filter."));
                return;
            }
        }
        const QJniObject extra = QJniObject::fromString(
            QStringLiteral("android.intent.extra.MIME_TYPES"));
        intent.callObjectMethod("putExtra",
            "(Ljava/lang/String;[Ljava/lang/String;)Landroid/content/Intent;",
            extra.object<jstring>(), filters);
        jni->DeleteLocalRef(filters);
        // Permission is granted solely for the returned URI, never all photos.
        intent.callObjectMethod("addFlags", "(I)Landroid/content/Intent;", jint{1});
        // Unhandled Java exceptions in an intent or MIME filter can terminate
        // the process instead of presenting the system file picker.
        if (jni.checkAndClearExceptions()) {
            fail(QStringLiteral("Android could not prepare the media picker."));
            return;
        }

        m_afterIndex = afterIndex;
        m_pending = true;
        qInfo() << "Genycaster Android: opening system media document picker";
        // The Qt Quick event loop is not Android's main (Activity) thread.
        // Starting a Java activity from the Qt thread can cause an uncaught
        // CalledFromWrongThreadException on some emulator/OS versions.
        const QPointer<AndroidComposerMediaPicker> receiver{this};
        (void)QNativeInterface::QAndroidApplication::runOnAndroidMainThread(
            [intent, receiver]() -> QVariant {
                if (!receiver) return {};
                QtAndroidPrivate::startActivity(intent, RequestCode, receiver.data());
                QJniEnvironment activityJni;
                if (activityJni.checkAndClearExceptions()) {
                    QMetaObject::invokeMethod(receiver.data(), [receiver] {
                        if (!receiver) return;
                        receiver->m_pending = false;
                        receiver->fail(QStringLiteral(
                            "Android could not open the system document picker."));
                    }, Qt::QueuedConnection);
                }
                return {};
            });
    }

    void handleActivityResult(const int receiverRequestCode, const int resultCode,
                              const QJniObject& result) override
    {
        if (receiverRequestCode != RequestCode)
            return;
        QString uriString;
        QString mimeString;
        if (resultCode == -1 && result.isValid()) {
            const QJniObject uri = result.callObjectMethod(
                "getData", "()Landroid/net/Uri;");
            if (uri.isValid()) {
                uriString = uri.callObjectMethod<jstring>("toString").toString();
                const QJniObject context =
                    QNativeInterface::QAndroidApplication::context();
                const QJniObject resolver = context.callObjectMethod(
                    "getContentResolver", "()Landroid/content/ContentResolver;");
                if (resolver.isValid()) {
                    // The explicit JNI descriptor overload accepts jobject here;
                    // the typed return-template overload rejects this
                    // mixed signature under Qt 6.12's constraints.
                    mimeString = resolver.callObjectMethod(
                        "getType", "(Landroid/net/Uri;)Ljava/lang/String;",
                        uri.object<jobject>()).toString();
                }
            }
        }
        // Qt's Android result is delivered asynchronously; always return to
        // the QML/UI thread before touching the MediaUploadController.
        QMetaObject::invokeMethod(this,
            [this, resultCode, uriString = std::move(uriString),
             mimeString = std::move(mimeString)] {
                if (!m_pending) return;
                if (resultCode != -1) {
                    m_pending = false; // User cancelled without an error.
                    return;
                }
                if (!uriString.startsWith(QStringLiteral("content://"))) {
                    m_pending = false;
                    fail(QStringLiteral("The selected Android document has no readable content URI."));
                    return;
                }
                const auto type = mediaForMime(mimeString);
                if (type.extension == nullptr) {
                    m_pending = false;
                    fail(QStringLiteral("This Android media type is not supported for upload."));
                    return;
                }
                const QString output = QDir{m_temporaryFiles->path()}.filePath(
                    QStringLiteral("cast-")
                    + QUuid::createUuid().toString(QUuid::WithoutBraces)
                    + QLatin1Char('.') + QString::fromLatin1(type.extension));
                const int blockIndex = m_afterIndex;

                // Reading content:// providers may involve cloud storage.
                // Never block the Qt scene-graph/UI thread during that copy.
                m_copyWorker = QThread::create(
                    [this, uriString, output, maximum = type.maximum, blockIndex] {
                        QString error;
                        QFile source{uriString};
                        if (!source.open(QIODevice::ReadOnly)) {
                            error = QStringLiteral("Unable to read the selected Android document.");
                        } else {
                            QSaveFile destination{output};
                            if (!destination.open(QIODevice::WriteOnly)) {
                                error = QStringLiteral("Unable to create the private media upload file.");
                            } else {
                                qint64 totalBytes = 0;
                                for (;;) {
                                    if (QThread::currentThread()->isInterruptionRequested()) {
                                        error = QStringLiteral("Media import was cancelled.");
                                        break;
                                    }
                                    const QByteArray chunk = source.read(256 * 1024);
                                    if (chunk.isEmpty()) {
                                        if (!source.atEnd())
                                            error = QStringLiteral("Android media could not be read completely.");
                                        break;
                                    }
                                    totalBytes += chunk.size();
                                    if (totalBytes > maximum) {
                                        error = QStringLiteral("The selected media exceeds the upload size limit.");
                                        break;
                                    }
                                    if (destination.write(chunk) != chunk.size()) {
                                        error = QStringLiteral("Unable to save the selected media.");
                                        break;
                                    }
                                }
                                if (error.isEmpty() && totalBytes == 0)
                                    error = QStringLiteral("The selected media is empty.");
                                if (error.isEmpty() && !destination.commit())
                                    error = QStringLiteral("Unable to finish importing the Android media.");
                                // Uncommitted QSaveFile automatically removes its temporary file.
                            }
                        }
                        QMetaObject::invokeMethod(this,
                            [this, error = std::move(error), output, blockIndex] {
                                m_pending = false;
                                if (!m_media) return;
                                if (!error.isEmpty()) {
                                    fail(error);
                                    return;
                                }
                                qInfo() << "Genycaster Android: imported document for cast upload";
                                if (!m_media->beginUpload(QUrl::fromLocalFile(output), blockIndex))
                                    fail(QStringLiteral("Unable to begin the selected media upload."));
                            }, Qt::QueuedConnection);
                    });
                QObject::connect(m_copyWorker, &QThread::finished,
                    this, [this, worker = m_copyWorker] {
                        if (m_copyWorker == worker) m_copyWorker = nullptr;
                        worker->deleteLater();
                    });
                m_copyWorker->start();
            }, Qt::QueuedConnection);
    }

private:
    struct MediaFormat {
        const char* extension;
        qint64 maximum;
    };

    [[nodiscard]] static MediaFormat mediaForMime(QString mime)
    {
        mime = mime.toLower().trimmed();
        constexpr qint64 imageLimit = 25LL * 1024 * 1024;
        constexpr qint64 videoLimit = 512LL * 1024 * 1024;
        if (mime == QStringLiteral("image/jpeg") || mime == QStringLiteral("image/jpg"))
            return {"jpg", imageLimit};
        if (mime == QStringLiteral("image/png")) return {"png", imageLimit};
        if (mime == QStringLiteral("image/webp")) return {"webp", imageLimit};
        if (mime == QStringLiteral("image/gif")) return {"gif", imageLimit};
        if (mime == QStringLiteral("image/avif")) return {"avif", imageLimit};
        if (mime == QStringLiteral("video/mp4")) return {"mp4", videoLimit};
        if (mime == QStringLiteral("video/webm")) return {"webm", videoLimit};
        if (mime == QStringLiteral("video/quicktime")) return {"mov", videoLimit};
        return {nullptr, 0};
    }

    void fail(const QString& message) const
    {
        qWarning().noquote() << "Genycaster Android media picker:" << message;
        if (m_media) m_media->applyFailed(message);
    }

    static constexpr int RequestCode = 0x4A43;
    QPointer<MediaUploadController> m_media;
    std::unique_ptr<QTemporaryDir> m_temporaryFiles;
    QThread* m_copyWorker{};
    int m_afterIndex{-1};
    bool m_pending{false};
};
