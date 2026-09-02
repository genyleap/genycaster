// Textual compatibility boundary: Qt MOC requires this QObject declaration in a
// .hpp file.
#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QString>

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
  Q_PROPERTY(QString effectiveQuality READ effectiveQuality NOTIFY renderingChanged)
  Q_PROPERTY(QString graphicsApi READ graphicsApi NOTIFY telemetryChanged)
  Q_PROPERTY(QString energyMode READ energyMode NOTIFY renderingChanged)
  Q_PROPERTY(double frameIntervalMs READ frameIntervalMs NOTIFY telemetryChanged)
  Q_PROPERTY(double refreshRate READ refreshRate NOTIFY telemetryChanged)
  Q_PROPERTY(double devicePixelRatio READ devicePixelRatio NOTIFY telemetryChanged)
  Q_PROPERTY(int activeSampleCount READ activeSampleCount NOTIFY telemetryChanged)
  Q_PROPERTY(int preferredSampleCount READ preferredSampleCount NOTIFY renderingChanged)
  Q_PROPERTY(double effectScale READ effectScale NOTIFY renderingChanged)
  Q_PROPERTY(double mediaDecodeScale READ mediaDecodeScale NOTIFY renderingChanged)
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
  [[nodiscard]] bool restartRequired() const noexcept;

  /** @brief Selects efficiency, adaptive, or fidelity. Returns false for an unknown value. */
  Q_INVOKABLE bool setProfile(const QString &profile);
  /** @brief Attaches the application window used for render telemetry. */
  void attachWindow(QQuickWindow *window);

  /** @brief Returns the startup MSAA request for one canonical profile. */
  [[nodiscard]] static int sampleCountForProfile(const QString &profile) noexcept;
  /** @brief Canonicalizes a persisted/user profile, defaulting empty input to adaptive. */
  [[nodiscard]] static QString canonicalProfile(QString profile);

signals:
  void renderingChanged();
  void telemetryChanged();

private:
  void updateStaticTelemetry();
  void recordFrame();
  void updateAdaptiveQuality();

  QSettings &m_settings;
  QQuickWindow *m_window{nullptr};
  QString m_profile{QStringLiteral("adaptive")};
  QString m_effectiveQuality{QStringLiteral("balanced")};
  QString m_graphicsApi{QStringLiteral("unknown")};
  QElapsedTimer m_frameClock{};
  double m_frameIntervalMs{0.0};
  double m_refreshRate{0.0};
  double m_devicePixelRatio{1.0};
  int m_activeSampleCount{0};
  int m_slowFrameStreak{0};
  int m_stableFrameStreak{0};
};
