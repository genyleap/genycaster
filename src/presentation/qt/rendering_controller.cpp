#include "rendering_controller.hpp"

#include <algorithm>
#include <cmath>

#include <QQuickWindow>
#include <QScreen>
#include <QSettings>
#include <QSGRendererInterface>
#include <QSurfaceFormat>

namespace {

constexpr auto kProfileSetting = "rendering/profile";

[[nodiscard]] QString apiName(const QSGRendererInterface::GraphicsApi api) {
  using GraphicsApi = QSGRendererInterface::GraphicsApi;
  if (api == GraphicsApi::Software) {
    return QStringLiteral("Software");
  }
  if (api == GraphicsApi::OpenGL) {
    return QStringLiteral("OpenGL");
  }
  if (api == GraphicsApi::Direct3D11) {
    return QStringLiteral("D3D11");
  }
  if (api == GraphicsApi::Vulkan) {
    return QStringLiteral("Vulkan");
  }
  if (api == GraphicsApi::Metal) {
    return QStringLiteral("Metal");
  }
  if (api == GraphicsApi::Null) {
    return QStringLiteral("Null");
  }
  return QStringLiteral("Unknown");
}

} // namespace

RenderingController::RenderingController(QSettings &settings, QObject *parent)
    : QObject{parent}, m_settings{settings} {
  m_profile = canonicalProfile(
      m_settings.value(QString::fromLatin1(kProfileSetting), QStringLiteral("adaptive")).toString());
  updateAdaptiveQuality();
}

QString RenderingController::profile() const { return m_profile; }
QString RenderingController::effectiveQuality() const { return m_effectiveQuality; }
QString RenderingController::graphicsApi() const { return m_graphicsApi; }
double RenderingController::frameIntervalMs() const noexcept { return m_frameIntervalMs; }
double RenderingController::refreshRate() const noexcept { return m_refreshRate; }
double RenderingController::devicePixelRatio() const noexcept { return m_devicePixelRatio; }
int RenderingController::activeSampleCount() const noexcept { return m_activeSampleCount; }

QString RenderingController::energyMode() const {
  if (m_profile == QStringLiteral("efficiency")) {
    return QStringLiteral("low");
  }
  if (m_profile == QStringLiteral("fidelity")) {
    return QStringLiteral("maximum");
  }
  return m_effectiveQuality == QStringLiteral("efficiency")
      ? QStringLiteral("low")
      : m_effectiveQuality == QStringLiteral("fidelity") ? QStringLiteral("high")
                                                           : QStringLiteral("balanced");
}

int RenderingController::preferredSampleCount() const noexcept {
  return sampleCountForProfile(m_profile);
}

double RenderingController::effectScale() const noexcept {
  if (m_profile == QStringLiteral("efficiency")) {
    return 0.62;
  }
  if (m_profile == QStringLiteral("fidelity")) {
    return 1.0;
  }
  if (m_effectiveQuality == QStringLiteral("efficiency")) {
    return 0.70;
  }
  if (m_effectiveQuality == QStringLiteral("fidelity")) {
    return 1.0;
  }
  return 0.88;
}

double RenderingController::mediaDecodeScale() const noexcept {
  if (m_profile == QStringLiteral("efficiency")) {
    return 0.72;
  }
  // Keep image decode size stable while Adaptive changes effect/geometry quality.
  // Qt reloads an Image whenever sourceSize changes; coupling decode size to the
  // frame-time state machine caused visible flicker and bursts of duplicate image
  // requests. Adaptive and Fidelity therefore decode to the DPR-backed display
  // requirement, while only the explicit Efficiency profile opts into reduction.
  return 1.0;
}

double RenderingController::geometryQualityScale() const noexcept {
  if (m_profile == QStringLiteral("efficiency")) {
    return 0.80;
  }
  if (m_profile == QStringLiteral("fidelity")) {
    return 1.16;
  }
  if (m_effectiveQuality == QStringLiteral("efficiency")) {
    return 0.90;
  }
  if (m_effectiveQuality == QStringLiteral("fidelity")) {
    return 1.10;
  }
  return 1.0;
}

double RenderingController::vectorRasterScale() const noexcept {
  if (m_profile == QStringLiteral("efficiency")) {
    return 0.85;
  }
  if (m_profile == QStringLiteral("fidelity")) {
    return 1.0;
  }
  if (m_effectiveQuality == QStringLiteral("efficiency")) {
    return 0.90;
  }
  if (m_effectiveQuality == QStringLiteral("fidelity")) {
    return 1.0;
  }
  return 1.0;
}

int RenderingController::layerSampleCount() const noexcept {
  // Offscreen QML layers already resolve to textures. Multisampling every
  // masked/avatar/effect layer adds costly resolve passes and can introduce a
  // second filtering stage. Window-level MSAA and analytic geometry AA own edge
  // quality; keep texture layers single-sampled in every profile.
  return 0;
}

bool RenderingController::restartRequired() const noexcept {
  return m_window != nullptr && m_activeSampleCount != preferredSampleCount();
}

bool RenderingController::setProfile(const QString &profile) {
  const QString canonical = canonicalProfile(profile);
  if (canonical.isEmpty()) {
    return false;
  }
  if (canonical == m_profile) {
    return true;
  }
  m_profile = canonical;
  m_settings.setValue(QString::fromLatin1(kProfileSetting), m_profile);
  m_slowFrameStreak = 0;
  m_stableFrameStreak = 0;
  updateAdaptiveQuality();
  emit renderingChanged();
  return true;
}

void RenderingController::attachWindow(QQuickWindow *window) {
  if (m_window == window) {
    return;
  }
  if (m_window != nullptr) {
    disconnect(m_window, nullptr, this, nullptr);
  }
  m_window = window;
  m_frameClock.invalidate();
  m_telemetryPublishClock.invalidate();
  m_smoothedFrameIntervalMs = 0.0;
  m_frameIntervalMs = 0.0;
  if (m_window == nullptr) {
    return;
  }
  connect(m_window, &QQuickWindow::frameSwapped, this, &RenderingController::recordFrame,
          Qt::QueuedConnection);
  connect(m_window, &QQuickWindow::screenChanged, this, [this] { updateStaticTelemetry(); });
  updateStaticTelemetry();
}

int RenderingController::sampleCountForProfile(const QString &profile) noexcept {
  const QString canonical = canonicalProfile(profile);
  if (canonical == QStringLiteral("fidelity")) {
    return 4;
  }
  // Qt Quick's default scene-graph path uses vertex antialiasing for ordinary
  // primitives, while Genycaster's curved design primitives request
  // Shape.CurveRenderer which provides native high-quality curve AA without
  // multisampling. Do not force window-wide MSAA in the recommended Adaptive
  // profile; reserve its GPU/memory cost for the explicit Fidelity profile.
  return 0;
}

QString RenderingController::canonicalProfile(QString profile) {
  profile = profile.trimmed().toLower();
  if (profile.isEmpty()) {
    return QStringLiteral("adaptive");
  }
  if (profile == QStringLiteral("efficiency") || profile == QStringLiteral("adaptive") ||
      profile == QStringLiteral("fidelity")) {
    return profile;
  }
  return {};
}

void RenderingController::updateStaticTelemetry() {
  if (m_window == nullptr) {
    return;
  }
  const QString nextApi = m_window->rendererInterface() != nullptr
      ? apiName(m_window->rendererInterface()->graphicsApi())
      : QStringLiteral("Unknown");
  const double nextDpr = std::max(1.0, m_window->devicePixelRatio());
  const double nextRefresh = m_window->screen() != nullptr ? m_window->screen()->refreshRate() : 0.0;
  const int nextSamples = std::max(0, m_window->format().samples());
  const bool changed = nextApi != m_graphicsApi || nextDpr != m_devicePixelRatio ||
      nextRefresh != m_refreshRate || nextSamples != m_activeSampleCount;
  m_graphicsApi = nextApi;
  m_devicePixelRatio = nextDpr;
  m_refreshRate = nextRefresh;
  m_activeSampleCount = nextSamples;
  if (changed) {
    emit telemetryChanged();
    emit renderingChanged();
  }
}

void RenderingController::recordFrame() {
  if (!m_frameClock.isValid()) {
    m_frameClock.start();
    m_telemetryPublishClock.start();
    updateStaticTelemetry();
    return;
  }
  const double sampleMs = static_cast<double>(m_frameClock.restart());
  if (sampleMs <= 0.0 || sampleMs > 250.0) {
    return;
  }

  m_smoothedFrameIntervalMs = m_smoothedFrameIntervalMs <= 0.0
      ? sampleMs
      : m_smoothedFrameIntervalMs * 0.88 + sampleMs * 0.12;
  updateAdaptiveQuality();

  // Never publish telemetry every frame. A QML binding to frameIntervalMs can
  // schedule another frame, which would otherwise turn frameSwapped into an
  // idle-time render loop. Publish at human-readable cadence and only when the
  // displayed value changed meaningfully.
  constexpr qint64 telemetryIntervalMs = 750;
  constexpr double meaningfulDeltaMs = 0.15;
  if (!m_telemetryPublishClock.isValid()) {
    m_telemetryPublishClock.start();
    return;
  }
  if (m_telemetryPublishClock.elapsed() < telemetryIntervalMs) {
    return;
  }
  m_telemetryPublishClock.restart();
  if (m_frameIntervalMs <= 0.0 ||
      std::abs(m_smoothedFrameIntervalMs - m_frameIntervalMs) >= meaningfulDeltaMs) {
    m_frameIntervalMs = m_smoothedFrameIntervalMs;
    emit telemetryChanged();
  }
}

void RenderingController::updateAdaptiveQuality() {
  QString next = QStringLiteral("balanced");
  if (m_profile == QStringLiteral("efficiency")) {
    next = QStringLiteral("efficiency");
  } else if (m_profile == QStringLiteral("fidelity")) {
    next = QStringLiteral("fidelity");
  } else if (m_smoothedFrameIntervalMs > 0.0) {
    const double targetMs = m_refreshRate > 1.0 ? 1000.0 / m_refreshRate : 16.667;
    const bool isSlow = m_smoothedFrameIntervalMs > targetMs * 1.28;
    const bool isStable = m_smoothedFrameIntervalMs <= targetMs * 1.08;
    m_slowFrameStreak = isSlow ? std::min(m_slowFrameStreak + 1, 120) : 0;
    m_stableFrameStreak = isStable ? std::min(m_stableFrameStreak + 1, 240) : 0;

    if (m_effectiveQuality == QStringLiteral("efficiency")) {
      next = m_stableFrameStreak >= 90 ? QStringLiteral("balanced") : QStringLiteral("efficiency");
    } else if (m_effectiveQuality == QStringLiteral("fidelity")) {
      next = m_slowFrameStreak >= 8 ? QStringLiteral("balanced") : QStringLiteral("fidelity");
    } else if (m_slowFrameStreak >= 10) {
      next = QStringLiteral("efficiency");
    } else if (m_stableFrameStreak >= 120 && m_refreshRate >= 90.0) {
      next = QStringLiteral("fidelity");
    }
  }
  if (next != m_effectiveQuality) {
    m_effectiveQuality = next;
    emit renderingChanged();
  }
}
