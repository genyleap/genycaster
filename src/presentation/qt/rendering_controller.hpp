/*!
 * @file        src/presentation/qt/rendering_controller.hpp
 * @brief       Qt renderer diagnostics and preferences.
 *
 * @details
 * Tracks real scene-graph backend capabilities, frame timing and
 * user-facing graphics settings through a MOC-compatible boundary.
 *
 * @author      Kambiz Asadzadeh
 * @since       02 Sep 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

// Textual compatibility boundary: Qt MOC requires this QObject declaration in a
// .hpp file.
#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QString>

#include <atomic>
#include <cstdint>
#include <deque>

class QQuickWindow;
class QSettings;

/**
 * @brief Owns device-local rendering quality policy and lightweight frame telemetry.
 *
 * The controller exposes product-level Efficiency/Adaptive/Fidelity choices to
 * QML while keeping backend-specific rendering details in the Qt presentation
 * boundary. QSettings and the attached QQuickWindow must outlive any use of
 * their borrowed references.
 */
class RenderingController final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString profile READ profile NOTIFY renderingChanged)
  Q_PROPERTY(QString effectiveQuality READ effectiveQuality NOTIFY adaptiveQualityChanged)
  Q_PROPERTY(QString graphicsApi READ graphicsApi NOTIFY telemetryChanged)
  Q_PROPERTY(QString energyMode READ energyMode NOTIFY adaptiveQualityChanged)
  Q_PROPERTY(double frameIntervalMs READ frameIntervalMs NOTIFY telemetryChanged)
  Q_PROPERTY(double refreshRate READ refreshRate NOTIFY telemetryChanged)
  Q_PROPERTY(double devicePixelRatio READ devicePixelRatio NOTIFY telemetryChanged)
  Q_PROPERTY(int activeSampleCount READ activeSampleCount NOTIFY telemetryChanged)
  Q_PROPERTY(int preferredSampleCount READ preferredSampleCount NOTIFY renderingChanged)
  Q_PROPERTY(double effectScale READ effectScale NOTIFY renderingChanged)
  Q_PROPERTY(double mediaDecodeScale READ mediaDecodeScale NOTIFY renderingChanged)
  Q_PROPERTY(double geometryQualityScale READ geometryQualityScale NOTIFY renderingChanged)
  Q_PROPERTY(double vectorRasterScale READ vectorRasterScale NOTIFY renderingChanged)
  Q_PROPERTY(int layerSampleCount READ layerSampleCount NOTIFY renderingChanged)
  Q_PROPERTY(bool restartRequired READ restartRequired NOTIFY renderingChanged)

public:
  /** @brief Creates rendering policy backed by the borrowed settings store. */
  explicit RenderingController(QSettings &settings, QObject *parent = nullptr);

  [[nodiscard]] QString profile() const;
  [[nodiscard]] QString effectiveQuality() const;
  [[nodiscard]] QString graphicsApi() const;
  [[nodiscard]] QString energyMode() const;
  [[nodiscard]] double frameIntervalMs() const noexcept;
  [[nodiscard]] double refreshRate() const noexcept;
  [[nodiscard]] double devicePixelRatio() const noexcept;
  [[nodiscard]] int activeSampleCount() const noexcept;
  [[nodiscard]] int preferredSampleCount() const noexcept;
  [[nodiscard]] double effectScale() const noexcept;
  [[nodiscard]] double mediaDecodeScale() const noexcept;
  [[nodiscard]] double geometryQualityScale() const noexcept;
  [[nodiscard]] double vectorRasterScale() const noexcept;
  [[nodiscard]] int layerSampleCount() const noexcept;
  [[nodiscard]] bool restartRequired() const noexcept;

  /** @brief Selects efficiency, adaptive, or fidelity. Returns false for an unknown value. */
  Q_INVOKABLE bool setProfile(const QString &profile);

  /**
   * @brief Queues one decoded media surface for admission after a rendered frame.
   * @param requester Borrowed QML media object; destroyed objects are skipped.
   * @param generation Caller-owned identity used to reject stale source requests.
   */
  Q_INVOKABLE void requestMediaPresentation(QObject *requester, quint64 generation);

  /** @brief Attaches the application window used for render telemetry. */
  void attachWindow(QQuickWindow *window);

  /** @brief Returns the startup MSAA request for one canonical profile. */
  [[nodiscard]] static int sampleCountForProfile(const QString &profile) noexcept;

  /** @brief Canonicalizes a persisted/user profile, defaulting empty input to adaptive. */
  [[nodiscard]] static QString canonicalProfile(QString profile);

signals:
  void renderingChanged();
  void adaptiveQualityChanged();
  void telemetryChanged();

  /** @brief Grants at most one live decoded media surface per rendered frame. */
  void mediaPresentationGranted(QObject *requester, quint64 generation, quint64 presentationSequence);

private:
  void updateStaticTelemetry();
  void recordFrame();
  void tryGrantMediaPresentation();
  void acknowledgeMediaPresentationFrame(quint64 synchronizedGeneration);
  void updateAdaptiveQuality();

  struct MediaPresentationRequest final {
    QPointer<QObject> requester{};
    quint64 generation{};
  };

  QSettings &m_settings;
  QQuickWindow *m_window{nullptr};
  QString m_profile{QStringLiteral("adaptive")};
  QString m_effectiveQuality{QStringLiteral("balanced")};
  QString m_graphicsApi{QStringLiteral("unknown")};
  QElapsedTimer m_frameClock{};
  QElapsedTimer m_telemetryPublishClock{};
  double m_smoothedFrameIntervalMs{0.0};
  double m_frameIntervalMs{0.0};
  double m_refreshRate{0.0};
  double m_devicePixelRatio{1.0};
  int m_activeSampleCount{0};
  int m_slowFrameStreak{0};
  int m_stableFrameStreak{0};
  std::deque<MediaPresentationRequest> m_mediaPresentationQueue{};
  std::atomic<quint64> m_publishedMediaPresentationGeneration{0};
  std::atomic<quint64> m_synchronizedMediaPresentationGeneration{0};
  quint64 m_currentMediaPresentationGeneration{0};
  bool m_mediaPresentationGrantOutstanding{false};
};
