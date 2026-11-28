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

#include "frame_observation_queue.hpp"

#include <QObject>
#include <QPointer>
#include <QString>

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>

class QQuickWindow;
class QScreen;
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
  Q_PROPERTY(bool mediaDiagnosticsEnabled READ mediaDiagnosticsEnabled CONSTANT)
  Q_PROPERTY(bool stallDiagnosticsEnabled READ stallDiagnosticsEnabled CONSTANT)
  Q_PROPERTY(bool shuttingDown READ shuttingDown NOTIFY shutdownStarted)

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
  [[nodiscard]] bool mediaDiagnosticsEnabled() const noexcept;
  [[nodiscard]] bool stallDiagnosticsEnabled() const noexcept;

  /** Monotonic diagnostic timestamps, shared by GUI and render thread traces. */
  [[nodiscard]] static double traceClockMs() noexcept;
  Q_INVOKABLE double diagnosticClockMs() const noexcept;
  /** Logs slow, fixed-name UI work only. No media or account payloads. */
  Q_INVOKABLE void recordUiOperation(const QString &operation, double startMs, int rows) const;
  Q_INVOKABLE void recordUiMarker(const QString &operation) const;

  /** Reports raster geometry, not physical GPU memory or upload duration. */
  Q_INVOKABLE void recordRasterGeometry(const QString &category,
      int requestedWidth, int requestedHeight, double sourceWidth, double sourceHeight,
      double viewWidth, double viewHeight) const;

  /** @brief Selects efficiency, adaptive, or fidelity. Returns false for an unknown value. */
  Q_INVOKABLE bool setProfile(const QString &profile);

  /**
   * @brief Queues one decoded media surface for admission after a rendered frame.
   * @param requester Borrowed QML media object; destroyed objects are skipped.
   * A QML surface receives its ticket through acceptMediaPresentation(int).
   * @param generation Caller-owned identity used to reject stale source requests.
   */
  Q_INVOKABLE void requestMediaPresentation(QObject *requester, quint64 generation);
  /** @brief Withdraws a queued request when its source is replaced or evicted. */
  Q_INVOKABLE void cancelMediaPresentation(QObject *requester);

  /** Stop presentation admission before native QML teardown; irreversible. */
  void beginShutdown();
  [[nodiscard]] bool shuttingDown() const noexcept { return m_shuttingDown; }

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
  void shutdownStarted();

  /** @brief Observes a granted ticket for native instrumentation and tests. */
  void mediaPresentationGranted(QObject *requester, quint64 generation, quint64 presentationSequence);

private:
  void observeScreen(QScreen *screen);
  void updateStaticTelemetry();
  using FrameTime = std::chrono::steady_clock::time_point;
  void recordFrame(FrameTime timestamp);
  void resetFramePacing();
  void tryGrantMediaPresentation();
  void acknowledgeMediaPresentationFrame(quint64 synchronizedGeneration);
  void updateAdaptiveQuality();

  struct MediaPresentationRequest final {
    QPointer<QObject> requester{};
    quint64 generation{};
  };

  QSettings &m_settings;
  QPointer<QQuickWindow> m_window{};
  QPointer<QScreen> m_screen{};
  QString m_profile{QStringLiteral("adaptive")};
  QString m_effectiveQuality{QStringLiteral("balanced")};
  QString m_graphicsApi{QStringLiteral("unknown")};
  std::optional<FrameTime> m_lastFrame{};
  // A bounded diagnostic window; never a continuously running sampling timer.
  std::array<double, 120> m_frameSamples{};
  std::size_t m_frameSampleCount{0};
  std::size_t m_frameSampleCursor{0};
  std::optional<FrameTime> m_lastTelemetryPublication{};
  double m_smoothedFrameIntervalMs{0.0};
  double m_frameIntervalMs{0.0};
  double m_refreshRate{0.0};
  double m_devicePixelRatio{1.0};
  int m_activeSampleCount{0};
  int m_slowFrameStreak{0};
  int m_stableFrameStreak{0};
  std::deque<MediaPresentationRequest> m_mediaPresentationQueue{};
  // Each attachment owns its render-thread state. An old window's queued
  // callbacks can neither release a new window's ticket nor alter telemetry.
  struct RenderHandshake final {
    std::atomic<quint64> m_publishedMediaPresentationGeneration{0};
    std::atomic<quint64> m_synchronizedMediaPresentationGeneration{0};
    std::atomic<quint64> lastQueuedAcknowledgment{0};
    std::atomic<quint64> observationEpoch{0};
    FrameObservationQueue frameObservations;
  };
  std::shared_ptr<RenderHandshake> m_renderHandshake{};
  quint64 m_currentMediaPresentationGeneration{0};
  bool m_mediaPresentationGrantOutstanding{false};
  bool m_shuttingDown{false};
};
