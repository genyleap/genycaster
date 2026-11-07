#include "stroke.hpp"

#include <QQuickWindow>
#include <QSGGeometry>
#include <QSGGeometryNode>
#include <QSGVertexColorMaterial>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <utility>
#include <vector>

namespace {

constexpr qreal Epsilon = 0.0001;
constexpr int MinimumCircleSegments = 12;
constexpr int MaximumArcSegments = 512;
constexpr int MaximumRoundedRectSegmentsPerCorner = 64;

struct Vertex final {
    float x{};
    float y{};
    std::uint8_t r{};
    std::uint8_t g{};
    std::uint8_t b{};
    std::uint8_t a{};
};

using VertexList = std::vector<Vertex>;
using PointList = std::vector<QPointF>;
using StrokeRuns = std::vector<PointList>;

[[nodiscard]] std::uint8_t channel(const int value) noexcept
{
    return static_cast<std::uint8_t>(std::clamp(value, 0, 255));
}

[[nodiscard]] Vertex makeVertex(const QPointF& point, const QColor& color, const qreal alphaScale = 1.0)
{
    // QSGVertexColorMaterial requires premultiplied RGBA vertex colors.
    const qreal alpha = color.alphaF() * std::clamp(alphaScale, 0.0, 1.0);
    return Vertex{
        .x = static_cast<float>(point.x()),
        .y = static_cast<float>(point.y()),
        .r = channel(static_cast<int>(std::lround(color.red() * alpha))),
        .g = channel(static_cast<int>(std::lround(color.green() * alpha))),
        .b = channel(static_cast<int>(std::lround(color.blue() * alpha))),
        .a = channel(static_cast<int>(std::lround(255.0 * alpha))),
    };
}

void appendTriangle(VertexList& vertices, const Vertex& a, const Vertex& b, const Vertex& c)
{
    vertices.push_back(a);
    vertices.push_back(b);
    vertices.push_back(c);
}

void appendQuad(
    VertexList& vertices,
    const QPointF& p0,
    const QPointF& p1,
    const QPointF& p2,
    const QPointF& p3,
    const QColor& color,
    const qreal alpha0 = 1.0,
    const qreal alpha1 = 1.0,
    const qreal alpha2 = 1.0,
    const qreal alpha3 = 1.0)
{
    const auto v0 = makeVertex(p0, color, alpha0);
    const auto v1 = makeVertex(p1, color, alpha1);
    const auto v2 = makeVertex(p2, color, alpha2);
    const auto v3 = makeVertex(p3, color, alpha3);
    appendTriangle(vertices, v0, v1, v2);
    appendTriangle(vertices, v0, v2, v3);
}

[[nodiscard]] QPointF normalized(const QPointF& value)
{
    const qreal length = std::hypot(value.x(), value.y());
    if (length <= Epsilon) {
        return {};
    }
    return {value.x() / length, value.y() / length};
}

[[nodiscard]] QPointF perpendicular(const QPointF& value) noexcept
{
    return {-value.y(), value.x()};
}

void appendRoundDisc(
    VertexList& vertices,
    const QPointF& center,
    const qreal radius,
    const qreal feather,
    const QColor& color,
    const qreal devicePixelRatio,
    const qreal qualityScale)
{
    if (radius <= Epsilon || color.alpha() == 0) {
        return;
    }

    const qreal physicalCircumference = 2.0 * std::numbers::pi_v<qreal> * radius * devicePixelRatio;
    const int segments = std::clamp(
        static_cast<int>(std::ceil(physicalCircumference / (3.0 / std::clamp(qualityScale, 0.5, 2.0)))), MinimumCircleSegments, 144);

    const auto centerVertex = makeVertex(center, color);
    for (int index = 0; index < segments; ++index) {
        const qreal angle0 = 2.0 * std::numbers::pi_v<qreal> * static_cast<qreal>(index) / segments;
        const qreal angle1 = 2.0 * std::numbers::pi_v<qreal> * static_cast<qreal>(index + 1) / segments;
        const QPointF inner0{center.x() + std::cos(angle0) * radius, center.y() + std::sin(angle0) * radius};
        const QPointF inner1{center.x() + std::cos(angle1) * radius, center.y() + std::sin(angle1) * radius};
        const QPointF outer0{center.x() + std::cos(angle0) * (radius + feather), center.y() + std::sin(angle0) * (radius + feather)};
        const QPointF outer1{center.x() + std::cos(angle1) * (radius + feather), center.y() + std::sin(angle1) * (radius + feather)};

        appendTriangle(vertices, centerVertex, makeVertex(inner0, color), makeVertex(inner1, color));
        appendQuad(vertices, inner0, inner1, outer1, outer0, color, 1.0, 1.0, 0.0, 0.0);
    }
}

void appendSegment(
    VertexList& vertices,
    const QPointF& begin,
    const QPointF& end,
    const qreal halfWidth,
    const qreal feather,
    const QColor& color)
{
    const QPointF direction = normalized(end - begin);
    if (direction.isNull()) {
        return;
    }
    const QPointF normal = perpendicular(direction);
    const QPointF inner = normal * halfWidth;
    const QPointF outer = normal * (halfWidth + feather);

    const QPointF beginLeft = begin + inner;
    const QPointF endLeft = end + inner;
    const QPointF endRight = end - inner;
    const QPointF beginRight = begin - inner;
    appendQuad(vertices, beginLeft, endLeft, endRight, beginRight, color);

    appendQuad(vertices, begin + outer, end + outer, endLeft, beginLeft, color, 0.0, 0.0, 1.0, 1.0);
    appendQuad(vertices, beginRight, endRight, end - outer, begin - outer, color, 1.0, 1.0, 0.0, 0.0);
}

void appendStrokeRun(
    VertexList& vertices,
    const PointList& points,
    const qreal strokeWidth,
    const qreal feather,
    const QColor& color,
    const qreal devicePixelRatio,
    const qreal qualityScale)
{
    if (points.size() < 2 || strokeWidth <= Epsilon || color.alpha() == 0) {
        return;
    }

    const qreal halfWidth = strokeWidth / 2.0;
    for (std::size_t index = 1; index < points.size(); ++index) {
        appendSegment(vertices, points[index - 1], points[index], halfWidth, feather, color);
    }

    // Opaque discs bridge joins and provide true round caps. Their feathered outer
    // rings deliver stable sub-pixel antialiasing without relying on backend line AA.
    for (const QPointF& point : points) {
        appendRoundDisc(vertices, point, halfWidth, feather, color, devicePixelRatio, qualityScale);
    }
}

[[nodiscard]] PointList sparklinePoints(
    const QVariantList& samples,
    const QSizeF& size,
    const qreal padding)
{
    PointList points{};
    if (samples.size() < 2 || size.width() <= Epsilon || size.height() <= Epsilon) {
        return points;
    }

    const qreal clampedPadding = std::clamp(padding, 0.0, std::min(size.width(), size.height()) / 2.0);
    const qreal usableWidth = std::max<qreal>(0.0, size.width() - clampedPadding * 2.0);
    const qreal usableHeight = std::max<qreal>(0.0, size.height() - clampedPadding * 2.0);
    points.reserve(static_cast<std::size_t>(samples.size()));

    std::vector<qreal> values{};
    values.reserve(static_cast<std::size_t>(samples.size()));
    qreal minimum = std::numeric_limits<qreal>::max();
    qreal maximum = std::numeric_limits<qreal>::lowest();
    for (const QVariant& sample : samples) {
        bool ok = false;
        const qreal value = sample.toDouble(&ok);
        if (!ok || !std::isfinite(value)) continue;
        values.push_back(value);
        minimum = std::min(minimum, value);
        maximum = std::max(maximum, value);
    }
    if (values.size() < 2U) return {};

    const qreal span = maximum - minimum;
    const qreal baseline = std::abs(maximum) > Epsilon ? std::abs(maximum) : 1.0;
    const qreal minimumVisualSpan = baseline * 0.0025;
    const qreal effectiveSpan = std::max(span, minimumVisualSpan);
    const qreal center = (minimum + maximum) / 2.0;
    const qreal visualMinimum = center - effectiveSpan / 2.0;

    for (std::size_t index = 0; index < values.size(); ++index) {
        const qreal normalizedValue = std::clamp((values[index] - visualMinimum) / effectiveSpan, 0.0, 1.0);
        const qreal fraction = static_cast<qreal>(index) / static_cast<qreal>(values.size() - 1U);
        points.emplace_back(
            clampedPadding + usableWidth * fraction,
            clampedPadding + usableHeight * (1.0 - normalizedValue));
    }
    return points;
}

[[nodiscard]] PointList arcPoints(
    const QSizeF& size,
    const qreal startAngleDegrees,
    const qreal sweepAngleDegrees,
    const qreal requestedRadius,
    const qreal inset,
    const qreal strokeWidth,
    const qreal devicePixelRatio,
    const qreal qualityScale)
{
    PointList points{};
    if (size.width() <= Epsilon || size.height() <= Epsilon || std::abs(sweepAngleDegrees) <= Epsilon) {
        return points;
    }

    const qreal maximumRadius = std::max<qreal>(0.0, std::min(size.width(), size.height()) / 2.0 - inset - strokeWidth / 2.0);
    const qreal radius = requestedRadius > 0.0 ? std::min(requestedRadius, maximumRadius) : maximumRadius;
    if (radius <= Epsilon) {
        return points;
    }

    const qreal sweepRadians = sweepAngleDegrees * std::numbers::pi_v<qreal> / 180.0;
    const qreal physicalLength = std::abs(sweepRadians) * radius * devicePixelRatio;
    const qreal quality = std::clamp(qualityScale, 0.5, 2.0);
    const int segments = std::clamp(static_cast<int>(std::ceil(physicalLength / (1.65 / quality))), 10, MaximumArcSegments);
    points.reserve(static_cast<std::size_t>(segments + 1));
    const QPointF center{size.width() / 2.0, size.height() / 2.0};

    for (int index = 0; index <= segments; ++index) {
        const qreal fraction = static_cast<qreal>(index) / static_cast<qreal>(segments);
        const qreal angle = (startAngleDegrees + sweepAngleDegrees * fraction)
            * std::numbers::pi_v<qreal> / 180.0;
        points.emplace_back(center.x() + std::cos(angle) * radius, center.y() + std::sin(angle) * radius);
    }
    return points;
}

void appendQuarterArc(
    PointList& points,
    const QPointF& center,
    const qreal radius,
    const qreal beginRadians,
    const qreal endRadians,
    const int segments)
{
    for (int index = 1; index <= segments; ++index) {
        const qreal fraction = static_cast<qreal>(index) / static_cast<qreal>(segments);
        const qreal angle = beginRadians + (endRadians - beginRadians) * fraction;
        points.emplace_back(center.x() + std::cos(angle) * radius, center.y() + std::sin(angle) * radius);
    }
}

[[nodiscard]] PointList roundedRectanglePoints(
    const QSizeF& size,
    const qreal inset,
    const qreal requestedRadius,
    const qreal strokeWidth,
    const qreal devicePixelRatio,
    const qreal qualityScale)
{
    PointList points{};
    const qreal centerInset = std::max<qreal>(0.0, inset + strokeWidth / 2.0);
    const qreal left = centerInset;
    const qreal top = centerInset;
    const qreal right = size.width() - centerInset;
    const qreal bottom = size.height() - centerInset;
    if (right - left <= Epsilon || bottom - top <= Epsilon) {
        return points;
    }

    const qreal radius = std::clamp(requestedRadius, 0.0, std::min(right - left, bottom - top) / 2.0);
    if (radius <= Epsilon) {
        return {{left, top}, {right, top}, {right, bottom}, {left, bottom}, {left, top}};
    }

    const qreal physicalQuarterLength = (std::numbers::pi_v<qreal> / 2.0) * radius * devicePixelRatio;
    const int segments = std::clamp(
        static_cast<int>(std::ceil(physicalQuarterLength / (2.1 / std::clamp(qualityScale, 0.5, 2.0)))), 4, MaximumRoundedRectSegmentsPerCorner);

    points.reserve(static_cast<std::size_t>(segments * 4 + 5));
    points.emplace_back(left + radius, top);
    points.emplace_back(right - radius, top);
    appendQuarterArc(points, {right - radius, top + radius}, radius, -std::numbers::pi_v<qreal> / 2.0, 0.0, segments);
    points.emplace_back(right, bottom - radius);
    appendQuarterArc(points, {right - radius, bottom - radius}, radius, 0.0, std::numbers::pi_v<qreal> / 2.0, segments);
    points.emplace_back(left + radius, bottom);
    appendQuarterArc(points, {left + radius, bottom - radius}, radius, std::numbers::pi_v<qreal> / 2.0, std::numbers::pi_v<qreal>, segments);
    points.emplace_back(left, top + radius);
    appendQuarterArc(points, {left + radius, top + radius}, radius, std::numbers::pi_v<qreal>, std::numbers::pi_v<qreal> * 1.5, segments);
    points.push_back(points.front());
    return points;
}

[[nodiscard]] qreal distance(const QPointF& lhs, const QPointF& rhs) noexcept
{
    return std::hypot(rhs.x() - lhs.x(), rhs.y() - lhs.y());
}

[[nodiscard]] QPointF interpolate(const QPointF& from, const QPointF& to, const qreal fraction)
{
    return from + (to - from) * fraction;
}

[[nodiscard]] StrokeRuns dashPath(const PointList& path, const qreal dashLength, const qreal gapLength)
{
    if (path.size() < 2) {
        return {};
    }
    if (dashLength <= Epsilon || gapLength <= Epsilon) {
        return {path};
    }

    StrokeRuns runs{};
    PointList current{};
    bool drawing = true;
    qreal patternRemaining = dashLength;

    for (std::size_t segmentIndex = 1; segmentIndex < path.size(); ++segmentIndex) {
        QPointF cursor = path[segmentIndex - 1];
        const QPointF segmentEnd = path[segmentIndex];
        qreal segmentRemaining = distance(cursor, segmentEnd);
        if (segmentRemaining <= Epsilon) {
            continue;
        }

        while (segmentRemaining > Epsilon) {
            const qreal step = std::min(segmentRemaining, patternRemaining);
            const qreal fraction = step / segmentRemaining;
            const QPointF next = interpolate(cursor, segmentEnd, fraction);

            if (drawing) {
                if (current.empty()) {
                    current.push_back(cursor);
                }
                current.push_back(next);
            }

            cursor = next;
            segmentRemaining -= step;
            patternRemaining -= step;

            if (patternRemaining <= Epsilon) {
                if (drawing && current.size() >= 2) {
                    runs.push_back(std::move(current));
                    current = {};
                }
                drawing = !drawing;
                patternRemaining = drawing ? dashLength : gapLength;
            }
        }
    }

    if (drawing && current.size() >= 2) {
        runs.push_back(std::move(current));
    }
    return runs;
}

[[nodiscard]] qreal safeDevicePixelRatio(const QQuickWindow* window) noexcept
{
    if (window == nullptr) {
        return 1.0;
    }
    return std::max<qreal>(1.0, window->devicePixelRatio());
}

[[nodiscard]] bool usesMultisampling(const QQuickWindow* window) noexcept
{
    return window != nullptr && window->format().samples() > 1;
}

} // namespace

SceneGraphStrokeItem::SceneGraphStrokeItem(QQuickItem* parent)
    : QQuickItem{parent}
{
    setFlag(ItemHasContents, true);
    setAntialiasing(true);
}

SceneGraphStrokeItem::PathKind SceneGraphStrokeItem::pathKind() const noexcept { return m_pathKind; }
QColor SceneGraphStrokeItem::strokeColor() const { return m_strokeColor; }
qreal SceneGraphStrokeItem::strokeWidth() const noexcept { return m_strokeWidth; }
QVariantList SceneGraphStrokeItem::samples() const { return m_samples; }
qreal SceneGraphStrokeItem::startAngle() const noexcept { return m_startAngle; }
qreal SceneGraphStrokeItem::sweepAngle() const noexcept { return m_sweepAngle; }
qreal SceneGraphStrokeItem::radius() const noexcept { return m_radius; }
qreal SceneGraphStrokeItem::inset() const noexcept { return m_inset; }
qreal SceneGraphStrokeItem::cornerRadius() const noexcept { return m_cornerRadius; }
qreal SceneGraphStrokeItem::dashLength() const noexcept { return m_dashLength; }
qreal SceneGraphStrokeItem::gapLength() const noexcept { return m_gapLength; }
qreal SceneGraphStrokeItem::contentPadding() const noexcept { return m_contentPadding; }
qreal SceneGraphStrokeItem::qualityScale() const noexcept { return m_qualityScale; }

void SceneGraphStrokeItem::setPathKind(const PathKind value)
{
    if (m_pathKind == value) return;
    m_pathKind = value;
    emit pathKindChanged();
    scheduleUpdate();
}

void SceneGraphStrokeItem::setStrokeColor(const QColor& value)
{
    if (m_strokeColor == value) return;
    m_strokeColor = value;
    emit strokeColorChanged();
    scheduleUpdate();
}

void SceneGraphStrokeItem::setStrokeWidth(const qreal value)
{
    const qreal canonical = std::max<qreal>(0.0, value);
    if (qFuzzyCompare(m_strokeWidth, canonical)) return;
    m_strokeWidth = canonical;
    emit strokeWidthChanged();
    scheduleUpdate();
}

void SceneGraphStrokeItem::setSamples(const QVariantList& value)
{
    if (m_samples == value) return;
    m_samples = value;
    emit samplesChanged();
    scheduleUpdate();
}

void SceneGraphStrokeItem::setStartAngle(const qreal value)
{
    if (qFuzzyCompare(m_startAngle, value)) return;
    m_startAngle = value;
    emit startAngleChanged();
    scheduleUpdate();
}

void SceneGraphStrokeItem::setSweepAngle(const qreal value)
{
    if (qFuzzyCompare(m_sweepAngle, value)) return;
    m_sweepAngle = value;
    emit sweepAngleChanged();
    scheduleUpdate();
}

void SceneGraphStrokeItem::setRadius(const qreal value)
{
    if (qFuzzyCompare(m_radius, value)) return;
    m_radius = value;
    emit radiusChanged();
    scheduleUpdate();
}

void SceneGraphStrokeItem::setInset(const qreal value)
{
    const qreal canonical = std::max<qreal>(0.0, value);
    if (qFuzzyCompare(m_inset, canonical)) return;
    m_inset = canonical;
    emit insetChanged();
    scheduleUpdate();
}

void SceneGraphStrokeItem::setCornerRadius(const qreal value)
{
    const qreal canonical = std::max<qreal>(0.0, value);
    if (qFuzzyCompare(m_cornerRadius, canonical)) return;
    m_cornerRadius = canonical;
    emit cornerRadiusChanged();
    scheduleUpdate();
}

void SceneGraphStrokeItem::setDashLength(const qreal value)
{
    const qreal canonical = std::max<qreal>(0.0, value);
    if (qFuzzyCompare(m_dashLength, canonical)) return;
    m_dashLength = canonical;
    emit dashLengthChanged();
    scheduleUpdate();
}

void SceneGraphStrokeItem::setGapLength(const qreal value)
{
    const qreal canonical = std::max<qreal>(0.0, value);
    if (qFuzzyCompare(m_gapLength, canonical)) return;
    m_gapLength = canonical;
    emit gapLengthChanged();
    scheduleUpdate();
}

void SceneGraphStrokeItem::setContentPadding(const qreal value)
{
    const qreal canonical = std::max<qreal>(0.0, value);
    if (qFuzzyCompare(m_contentPadding, canonical)) return;
    m_contentPadding = canonical;
    emit contentPaddingChanged();
    scheduleUpdate();
}

void SceneGraphStrokeItem::setQualityScale(const qreal value)
{
    const qreal canonical = std::clamp(value, 0.5, 2.0);
    if (qFuzzyCompare(m_qualityScale, canonical)) return;
    m_qualityScale = canonical;
    emit qualityScaleChanged();
    scheduleUpdate();
}

QSGNode* SceneGraphStrokeItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*)
{
    auto* node = static_cast<QSGGeometryNode*>(oldNode);
    QSGGeometry* geometry = nullptr;
    if (node == nullptr) {
        node = new QSGGeometryNode{};
        geometry = new QSGGeometry{QSGGeometry::defaultAttributes_ColoredPoint2D(), 0};
        geometry->setDrawingMode(QSGGeometry::DrawTriangles);
        geometry->setVertexDataPattern(QSGGeometry::DynamicPattern);
        node->setGeometry(geometry);
        node->setFlag(QSGNode::OwnsGeometry);

        auto* material = new QSGVertexColorMaterial{};
        material->setFlag(QSGMaterial::Blending, true);
        node->setMaterial(material);
        node->setFlag(QSGNode::OwnsMaterial);
    } else {
        geometry = node->geometry();
    }

    const qreal devicePixelRatio = safeDevicePixelRatio(window());
    PointList path{};
    switch (m_pathKind) {
    case PathKind::Sparkline:
        path = sparklinePoints(m_samples, size(), m_contentPadding + m_strokeWidth / 2.0);
        break;
    case PathKind::Arc:
        path = arcPoints(size(), m_startAngle, m_sweepAngle, m_radius, m_inset, m_strokeWidth, devicePixelRatio, m_qualityScale);
        break;
    case PathKind::RoundedRectangle:
        path = roundedRectanglePoints(size(), m_inset, m_cornerRadius, m_strokeWidth, devicePixelRatio, m_qualityScale);
        break;
    }

    VertexList vertices{};
    if (m_strokeWidth > Epsilon && m_strokeColor.alpha() > 0 && path.size() >= 2) {
        const qreal physicalPixel = 1.0 / devicePixelRatio;
        const qreal quality = std::clamp(m_qualityScale, 0.5, 2.0);
        const qreal feather = usesMultisampling(window())
            ? physicalPixel * (0.40 / quality)
            : physicalPixel * std::clamp(1.35 / quality, 0.70, 1.70);
        const StrokeRuns runs = dashPath(path, m_dashLength, m_gapLength);
        std::size_t pointCount{};
        for (const auto& run : runs) pointCount += run.size();
        vertices.reserve(pointCount * 72U);
        for (const auto& run : runs) {
            appendStrokeRun(vertices, run, m_strokeWidth, feather, m_strokeColor, devicePixelRatio, quality);
        }
    }

    geometry->allocate(static_cast<int>(vertices.size()));
    auto* target = geometry->vertexDataAsColoredPoint2D();
    for (std::size_t index = 0; index < vertices.size(); ++index) {
        const Vertex& vertex = vertices[index];
        target[index].set(vertex.x, vertex.y, vertex.r, vertex.g, vertex.b, vertex.a);
    }
    node->markDirty(QSGNode::DirtyGeometry | QSGNode::DirtyMaterial);
    return node;
}


void SceneGraphStrokeItem::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size()) scheduleUpdate();
}

void SceneGraphStrokeItem::scheduleUpdate()
{
    update();
}
