#include "rendering_controller.hpp"

#include <algorithm>
#include <cmath>

#include <QQuickWindow>
#include <QLoggingCategory>
#include <QMetaObject>
#include <QScreen>
#include <QSettings>
#include <QSGRendererInterface>
#include <QSurfaceFormat>

namespace {

constexpr auto kProfileSetting = "rendering/profile";
Q_LOGGING_CATEGORY(framePacingLog, "genycaster.render.pacing", QtWarningMsg)

[[nodiscard]] QString apiName(const QSGRendererInterface::GraphicsApi api) {
  using GraphicsApi = QSGRendererInterface::GraphicsApi;
  if (api == GraphicsApi::Software) {
    return QStringLiteral("Software");
  }
  if (api == GraphicsApi::OpenGL) {
#if defined(Q_OS_WASM)
    // Qt's RHI enumerator calls its WebAssembly WebGL/ES renderer OpenGL.
    // Do not label the browser as using native desktop OpenGL; this does not
    // imply a CPU fallback or establish which GPU driver the browser chose.
    return QStringLiteral("WebGL (OpenGL ES)");
#else
    return QStringLiteral("OpenGL");
#endif
  }
  if (api == GraphicsApi::Direct3D11) {
    return QStringLiteral("D3D11");
  }
  if (api == GraphicsApi::Direct3D12) {
    return QStringLiteral("D3D12");
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
  // Frame telemetry may change the Adaptive status label, but it must not
  // resize every effect texture in the window. Explicit profile changes remain
  // the only live source of render-resource sizing changes.
  return 0.88;
}

double RenderingController::mediaDecodeScale() const noexcept {
  if (m_profile == QStringLiteral("efficiency")) {
    return 0.72;
  }
  if (m_profile == QStringLiteral("fidelity")) {
    return 1.0;
  }
  // Keep the Adaptive budget static with respect to frame telemetry: changing an
  // Image.sourceSize as frame quality fluctuates would itself trigger reloads.
  // On HiDPI screens a modest fixed reduction still leaves substantially more
  // than one decoded pixel per logical pixel while reducing decode memory and
  // render-thread texture-upload bandwidth. Ordinary 1x displays remain native.
  return m_devicePixelRatio >= 1.5 ? 0.88 : 1.0;
}

double RenderingController::geometryQualityScale() const noexcept {
  if (m_profile == QStringLiteral("efficiency")) {
    return 0.80;
  }
  if (m_profile == QStringLiteral("fidelity")) {
    return 1.16;
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
  const QString previousEffectiveQuality = m_effectiveQuality;
  updateAdaptiveQuality();
  // energyMode also incorporates the explicit profile. When the effective
  // quality label happens to stay equal across a profile change, its dedicated
  // notifier still has to publish the energy-mode change.
  if (m_effectiveQuality == previousEffectiveQuality) {
    emit adaptiveQualityChanged();
  }
  emit renderingChanged();
  return true;
}

void RenderingController::requestMediaPresentation(QObject *requester, const quint64 generation) {
  if (requester == nullptr) {
    return;
  }
  // A recycled delegate can request a new source before its previous ticket is
  // reached. Keep only its latest generation so stale work cannot build a queue.
  std::erase_if(m_mediaPresentationQueue, [requester](const MediaPresentationRequest &request) {
    return request.requester.isNull() || request.requester == requester;
  });
  m_mediaPresentationQueue.push_back(MediaPresentationRequest{
      .requester = requester,
      .generation = generation,
  });
  tryGrantMediaPresentation();
}

void RenderingController::cancelMediaPresentation(QObject *requester) {
  std::erase_if(m_mediaPresentationQueue, [requester](const MediaPresentationRequest &request) {
    return request.requester.isNull() || request.requester == requester;
  });
  // An already granted ticket must still cross the render barrier. Otherwise
  // cancellation could allow two texture presentations in a single frame.
}

void RenderingController::attachWindow(QQuickWindow *window) {
  if (m_window == window && (window != nullptr || !m_renderHandshake)) {
    return;
  }
  if (m_window != nullptr) {
    disconnect(m_window, nullptr, this, nullptr);
  }
  observeScreen(nullptr);
  m_window = window;
  m_renderHandshake.reset();
  m_mediaPresentationGrantOutstanding = false;
  resetFramePacing();
  if (m_window == nullptr) {
    return;
  }
  const auto state = std::make_shared<RenderHandshake>();
  m_renderHandshake = state;
  // Timestamp at the render-thread signal, not when the GUI eventually
  // handles its queued notification. GUI congestion otherwise distorts
  // observed intervals and introduces false quality transitions.
  connect(window, &QQuickWindow::frameSwapped, this, [this, state] {
    const auto epoch = state->observationEpoch.load(std::memory_order_acquire);
    const auto timestamp = std::chrono::steady_clock::now();
    QMetaObject::invokeMethod(this, [this, state, timestamp, epoch] {
      if (m_renderHandshake == state
          && state->observationEpoch.load(std::memory_order_acquire) == epoch)
        recordFrame(timestamp);
    }, Qt::QueuedConnection);
  }, Qt::DirectConnection);
  connect(window, &QQuickWindow::beforeSynchronizing, this, [state] {
    state->m_synchronizedMediaPresentationGeneration.store(
        state->m_publishedMediaPresentationGeneration.load(std::memory_order_acquire),
        std::memory_order_release);
  }, Qt::DirectConnection);
  connect(window, &QQuickWindow::afterRendering, this, [this, state] {
    const quint64 synchronizedGeneration =
        state->m_synchronizedMediaPresentationGeneration.load(std::memory_order_acquire);
    // No GUI event for an idle/ordinary render with no new ticket. A ticket
    // is acknowledged once, not once per frame until the next image arrives.
    if (synchronizedGeneration == 0 ||
        state->lastQueuedAcknowledgment.exchange(synchronizedGeneration,
            std::memory_order_relaxed) == synchronizedGeneration)
      return;
    QMetaObject::invokeMethod(this, [this, state, synchronizedGeneration] {
      if (m_renderHandshake == state)
        acknowledgeMediaPresentationFrame(synchronizedGeneration);
    }, Qt::QueuedConnection);
  }, Qt::DirectConnection);
  connect(window, &QQuickWindow::screenChanged, this, [this](QScreen *screen) {
    observeScreen(screen);
    updateStaticTelemetry();
  });
  observeScreen(window->screen());
  connect(window, &QQuickWindow::visibilityChanged, this, [this, state] {
    state->observationEpoch.fetch_add(1, std::memory_order_release);
    resetFramePacing();
    if (m_window && m_window->isVisible() && m_window->visibility() != QWindow::Minimized) {
      if (m_mediaPresentationGrantOutstanding)
        m_window->update();
      else
        tryGrantMediaPresentation();
    }
  });
  connect(window, &QObject::destroyed, this, [this] { attachWindow(nullptr); });
  updateStaticTelemetry();
  tryGrantMediaPresentation();
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

void RenderingController::observeScreen(QScreen *screen) {
  if (m_screen == screen)
    return;
  if (m_screen)
    disconnect(m_screen, nullptr, this, nullptr);
  m_screen = screen;
  if (!screen)
    return;
  connect(screen, &QScreen::refreshRateChanged, this, &RenderingController::updateStaticTelemetry);
  connect(screen, &QScreen::physicalDotsPerInchChanged, this, &RenderingController::updateStaticTelemetry);
  connect(screen, &QScreen::logicalDotsPerInchChanged, this, &RenderingController::updateStaticTelemetry);
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
  const bool budgetChanged = nextDpr != m_devicePixelRatio || nextSamples != m_activeSampleCount;
  const bool refreshChanged = nextRefresh != m_refreshRate;
  const bool changed = nextApi != m_graphicsApi || budgetChanged || refreshChanged;
  if (refreshChanged)
    resetFramePacing();
  m_graphicsApi = nextApi;
  m_devicePixelRatio = nextDpr;
  m_refreshRate = nextRefresh;
  m_activeSampleCount = nextSamples;
  if (changed)
    emit telemetryChanged();
  // Backend/refresh telemetry is not a resource-size change. Avoid waking
  // every image, vector and effect binding for diagnostics-only updates.
  if (budgetChanged)
    emit renderingChanged();
}

void RenderingController::resetFramePacing() {
  m_lastFrame.reset();
  m_lastTelemetryPublication.reset();
  m_smoothedFrameIntervalMs = 0.0;
  m_frameSampleCount = 0;
  m_frameSampleCursor = 0;
  m_slowFrameStreak = 0;
  m_stableFrameStreak = 0;
}

void RenderingController::recordFrame(const FrameTime timestamp) {
  if (!m_window || !m_window->isVisible() || m_window->visibility() == QWindow::Minimized)
    return;
  if (!m_lastFrame) {
    updateStaticTelemetry();
    m_lastFrame = timestamp;
    m_lastTelemetryPublication = timestamp;
    return;
  }
  const double sampleMs = std::chrono::duration<double, std::milli>(
      timestamp - *m_lastFrame).count();
  m_lastFrame = timestamp;
  if (sampleMs <= 0.0 || sampleMs > 250.0) {
    // Qt Quick is demand-driven: a long idle gap is not a slow GPU frame.
    // Start a new observation burst and discard stale quality streaks.
    resetFramePacing();
    m_lastFrame = timestamp;
    m_lastTelemetryPublication = timestamp;
    return;
  }

  m_frameSamples[m_frameSampleCursor] = sampleMs;
  m_frameSampleCursor = (m_frameSampleCursor + 1) % m_frameSamples.size();
  m_frameSampleCount = std::min(m_frameSampleCount + 1, m_frameSamples.size());
  m_smoothedFrameIntervalMs = m_smoothedFrameIntervalMs <= 0.0
      ? sampleMs
      : m_smoothedFrameIntervalMs * 0.88 + sampleMs * 0.12;
  if (m_frameSampleCount >= 8)
    updateAdaptiveQuality();

  // Never publish telemetry every frame. A QML binding to frameIntervalMs can
  // schedule another frame, which would otherwise turn frameSwapped into an
  // idle-time render loop. Publish at human-readable cadence and only when the
  // displayed value changed meaningfully.
  constexpr qint64 telemetryIntervalMs = 750;
  constexpr double meaningfulDeltaMs = 0.15;
  if (!m_lastTelemetryPublication) {
    m_lastTelemetryPublication = timestamp;
    return;
  }
  if (timestamp - *m_lastTelemetryPublication < std::chrono::milliseconds(telemetryIntervalMs))
    return;
  m_lastTelemetryPublication = timestamp;
  if (m_frameSampleCount < 8)
    return;
  if (framePacingLog().isDebugEnabled()) {
    auto sorted = m_frameSamples;
    const auto end = sorted.begin() + static_cast<std::ptrdiff_t>(m_frameSampleCount);
    std::sort(sorted.begin(), end);
    const auto p95Index = (m_frameSampleCount * 95 + 99) / 100 - 1;
    const double budget = m_refreshRate > 1.0 ? 1000.0 / m_refreshRate : 1000.0 / 60.0;
    const auto late = std::count_if(sorted.begin(), end,
        [budget](double sample) { return sample > budget * 1.25; });
    qCDebug(framePacingLog) << "backend" << m_graphicsApi << "refreshHz" << m_refreshRate
        << "samples" << m_frameSampleCount << "budgetMs" << budget
        << "observedP95Ms" << sorted[p95Index]
        << "intervalsOver125PercentBudget" << late;
  }
  if (m_frameIntervalMs <= 0.0 ||
      std::abs(m_smoothedFrameIntervalMs - m_frameIntervalMs) >= meaningfulDeltaMs) {
    m_frameIntervalMs = m_smoothedFrameIntervalMs;
    emit telemetryChanged();
  }
}

void RenderingController::tryGrantMediaPresentation() {
  if (m_window == nullptr || !m_renderHandshake || m_mediaPresentationGrantOutstanding
      || !m_window->isVisible() || m_window->visibility() == QWindow::Minimized) {
    return;
  }
  while (!m_mediaPresentationQueue.empty()) {
    MediaPresentationRequest request = std::move(m_mediaPresentationQueue.front());
    m_mediaPresentationQueue.pop_front();
    if (request.requester.isNull()) {
      continue;
    }
    m_mediaPresentationGrantOutstanding = true;
    ++m_currentMediaPresentationGeneration;
    m_renderHandshake->m_publishedMediaPresentationGeneration.store(
        m_currentMediaPresentationGeneration, std::memory_order_release);
    emit mediaPresentationGranted(
        request.requester.data(), request.generation, m_currentMediaPresentationGeneration);
    if (m_window)
      m_window->update();
    return;
  }
}

void RenderingController::acknowledgeMediaPresentationFrame(
    const quint64 synchronizedGeneration) {
  if (!m_mediaPresentationGrantOutstanding ||
      synchronizedGeneration < m_currentMediaPresentationGeneration) {
    return;
  }
  m_mediaPresentationGrantOutstanding = false;
  tryGrantMediaPresentation();
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
    // Keep frame-pacing feedback observable without invalidating the render
    // sizing bindings carried by renderingChanged across every image and icon.
    emit adaptiveQualityChanged();
  }
}
