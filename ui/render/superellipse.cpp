#include "superellipse.hpp"
#include "geometry_safety.hpp"

#include <QQuickWindow>
#include <QSGGeometry>
#include <QSGGeometryNode>
#include <QSGNode>
#include <QSGVertexColorMaterial>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <vector>

namespace {

constexpr qreal Epsilon = 0.0001;
constexpr qreal EdgeGuardPhysicalPixels = 0.25;
constexpr int MinimumCornerSegments = 8;
constexpr int MaximumCornerSegments = 256;

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

struct SurfaceNode final : QSGNode {
    SurfaceNode()
    {
        fillNode = new QSGGeometryNode{};
        fillGeometry = new QSGGeometry{QSGGeometry::defaultAttributes_ColoredPoint2D(), 0};
        fillGeometry->setDrawingMode(QSGGeometry::DrawTriangles);
        fillGeometry->setVertexDataPattern(QSGGeometry::DynamicPattern);
        fillNode->setGeometry(fillGeometry);
        fillNode->setFlag(QSGNode::OwnsGeometry);
        auto* fillMaterial = new QSGVertexColorMaterial{};
        fillMaterial->setFlag(QSGMaterial::Blending, true);
        fillNode->setMaterial(fillMaterial);
        fillNode->setFlag(QSGNode::OwnsMaterial);
        appendChildNode(fillNode);

        borderNode = new QSGGeometryNode{};
        borderGeometry = new QSGGeometry{QSGGeometry::defaultAttributes_ColoredPoint2D(), 0};
        borderGeometry->setDrawingMode(QSGGeometry::DrawTriangles);
        borderGeometry->setVertexDataPattern(QSGGeometry::DynamicPattern);
        borderNode->setGeometry(borderGeometry);
        borderNode->setFlag(QSGNode::OwnsGeometry);
        auto* borderMaterial = new QSGVertexColorMaterial{};
        borderMaterial->setFlag(QSGMaterial::Blending, true);
        borderNode->setMaterial(borderMaterial);
        borderNode->setFlag(QSGNode::OwnsMaterial);
        appendChildNode(borderNode);
    }

    QSGGeometryNode* fillNode{};
    QSGGeometry* fillGeometry{};
    QSGGeometryNode* borderNode{};
    QSGGeometry* borderGeometry{};
};

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
    if (length <= Epsilon) return {};
    return {value.x() / length, value.y() / length};
}

[[nodiscard]] QPointF perpendicular(const QPointF& value) noexcept
{
    return {-value.y(), value.x()};
}

[[nodiscard]] qreal safeDevicePixelRatio(const QQuickWindow* window) noexcept
{
    return window == nullptr ? 1.0 : std::max<qreal>(1.0, window->devicePixelRatio());
}

[[nodiscard]] bool usesMultisampling(const QQuickWindow* window) noexcept
{
    return window != nullptr && window->format().samples() > 1;
}

[[nodiscard]] qreal signedPower(const qreal value, const qreal power)
{
    if (std::abs(value) <= Epsilon) return 0.0;
    return std::copysign(std::pow(std::abs(value), power), value);
}

[[nodiscard]] int superellipseCornerSegments(
    const qreal radius,
    const qreal devicePixelRatio,
    const qreal qualityScale)
{
    const qreal physicalQuarterLength = (std::numbers::pi_v<qreal> / 2.0)
        * std::max<qreal>(0.0, radius) * devicePixelRatio;
    const qreal quality = std::clamp(qualityScale, 0.5, 2.0);
    const qreal targetPhysicalStep = 0.86 / quality;
    return genycaster::render::safety::boundedSegmentCount(
        physicalQuarterLength / targetPhysicalStep,
        MinimumCornerSegments,
        MaximumCornerSegments);
}

void appendSuperellipseCorner(
    PointList& points,
    const QPointF& center,
    const qreal radius,
    const qreal startAngle,
    const qreal power,
    const int segments)
{
    for (int index = 1; index <= segments; ++index) {
        const qreal angle = startAngle + static_cast<qreal>(index)
            * std::numbers::pi_v<qreal> / (2.0 * static_cast<qreal>(segments));
        points.emplace_back(
            center.x() + radius * signedPower(std::cos(angle), power),
            center.y() + radius * signedPower(std::sin(angle), power));
    }
}

[[nodiscard]] PointList superellipseBoundary(
    const QSizeF& size,
    const qreal curveDepth,
    const qreal exponent,
    const qreal inset,
    const bool topLeft,
    const bool topRight,
    const bool bottomRight,
    const bool bottomLeft,
    const qreal devicePixelRatio,
    const qreal qualityScale,
    const int cornerSegmentsOverride = 0)
{
    PointList points{};
    const qreal left = inset;
    const qreal top = inset;
    const qreal right = size.width() - inset;
    const qreal bottom = size.height() - inset;
    const qreal innerWidth = std::max<qreal>(0.0, right - left);
    const qreal innerHeight = std::max<qreal>(0.0, bottom - top);
    if (innerWidth <= Epsilon || innerHeight <= Epsilon) return points;

    const qreal requestedRadius = std::max<qreal>(
        0.0,
        std::min({curveDepth - inset, innerWidth / 2.0, innerHeight / 2.0}));
    // A pill/circle can make the straight segment between adjacent corners
    // exactly zero length. The feather normal then has a duplicate vertex at
    // that join and may produce a tiny axial spike on HiDPI backends. Preserve
    // half a physical pixel of total straight-edge length so every boundary
    // segment remains well-defined without visibly changing the shape.
    const qreal edgeGuard = EdgeGuardPhysicalPixels / std::max<qreal>(1.0, devicePixelRatio);
    const bool saturatesWidth = requestedRadius > Epsilon
        && requestedRadius >= innerWidth / 2.0 - Epsilon;
    const bool saturatesHeight = requestedRadius > Epsilon
        && requestedRadius >= innerHeight / 2.0 - Epsilon;
    const qreal radius = std::max<qreal>(
        0.0,
        std::min({
            requestedRadius,
            innerWidth / 2.0 - (saturatesWidth ? edgeGuard : 0.0),
            innerHeight / 2.0 - (saturatesHeight ? edgeGuard : 0.0)}));
    if (radius <= Epsilon && cornerSegmentsOverride <= 0) {
        return {{left, top}, {right, top}, {right, bottom}, {left, bottom}, {left, top}};
    }

    const qreal canonicalExponent = std::max<qreal>(2.01, exponent);
    const qreal power = 2.0 / canonicalExponent;
    const int segments = cornerSegmentsOverride > 0
        ? std::clamp(cornerSegmentsOverride, MinimumCornerSegments, MaximumCornerSegments)
        : superellipseCornerSegments(radius, devicePixelRatio, qualityScale);

    const qreal topLeftRadius = topLeft ? radius : 0.0;
    const qreal topRightRadius = topRight ? radius : 0.0;
    const qreal bottomRightRadius = bottomRight ? radius : 0.0;
    const qreal bottomLeftRadius = bottomLeft ? radius : 0.0;

    points.reserve(static_cast<std::size_t>(segments * 4 + 9));
    points.emplace_back(left + topLeftRadius, top);
    points.emplace_back(right - topRightRadius, top);
    if (topRight) {
        appendSuperellipseCorner(points, {right - radius, top + radius}, radius, -std::numbers::pi_v<qreal> / 2.0, power, segments);
    } else {
        points.emplace_back(right, top);
    }
    points.emplace_back(right, bottom - bottomRightRadius);
    if (bottomRight) {
        appendSuperellipseCorner(points, {right - radius, bottom - radius}, radius, 0.0, power, segments);
    } else {
        points.emplace_back(right, bottom);
    }
    points.emplace_back(left + bottomLeftRadius, bottom);
    if (bottomLeft) {
        appendSuperellipseCorner(points, {left + radius, bottom - radius}, radius, std::numbers::pi_v<qreal> / 2.0, power, segments);
    } else {
        points.emplace_back(left, bottom);
    }
    points.emplace_back(left, top + topLeftRadius);
    if (topLeft) {
        appendSuperellipseCorner(points, {left + radius, top + radius}, radius, std::numbers::pi_v<qreal>, power, segments);
    } else {
        points.emplace_back(left, top);
    }
    // The final corner sample is the start point. Snap it exactly to keep corresponding
    // inset boundaries index-aligned despite floating-point endpoint noise.
    points.back() = points.front();
    return points;
}

[[nodiscard]] qreal signedArea(const PointList& path)
{
    if (path.size() < 4) return 0.0;
    qreal area{};
    for (std::size_t index = 1; index < path.size(); ++index) {
        const QPointF& a = path[index - 1];
        const QPointF& b = path[index];
        area += a.x() * b.y() - b.x() * a.y();
    }
    return area / 2.0;
}

[[nodiscard]] PointList outerFeatherPath(const PointList& path, const qreal feather)
{
    if (path.size() < 4 || feather <= Epsilon) return path;
    const std::size_t uniqueCount = path.size() - 1;
    PointList outer{};
    outer.reserve(path.size());
    const bool positiveArea = signedArea(path) >= 0.0;

    for (std::size_t index = 0; index < uniqueCount; ++index) {
        const QPointF& previous = path[(index + uniqueCount - 1) % uniqueCount];
        const QPointF& current = path[index];
        const QPointF& next = path[(index + 1) % uniqueCount];
        const QPointF previousDirection = normalized(current - previous);
        const QPointF nextDirection = normalized(next - current);
        QPointF previousOutward = perpendicular(previousDirection);
        QPointF nextOutward = perpendicular(nextDirection);
        if (positiveArea) {
            previousOutward = -previousOutward;
            nextOutward = -nextOutward;
        }
        QPointF outward = normalized(previousOutward + nextOutward);
        if (outward.isNull()) outward = nextOutward;
        const qreal projection = std::max<qreal>(0.35, std::abs(QPointF::dotProduct(outward, nextOutward)));
        outer.push_back(current + outward * (feather / projection));
    }
    outer.push_back(outer.front());
    return outer;
}

void buildFillGeometry(
    VertexList& vertices,
    const PointList& boundary,
    const QColor& color,
    const qreal feather,
    const bool featherEdge)
{
    if (boundary.size() < 4 || color.alpha() == 0) return;
    const std::size_t uniqueCount = boundary.size() - 1;
    QPointF center{};
    for (std::size_t index = 0; index < uniqueCount; ++index) center += boundary[index];
    center /= static_cast<qreal>(uniqueCount);
    const auto centerVertex = makeVertex(center, color);

    for (std::size_t index = 0; index < uniqueCount; ++index) {
        const QPointF& current = boundary[index];
        const QPointF& next = boundary[(index + 1) % uniqueCount];
        appendTriangle(vertices, centerVertex, makeVertex(current, color), makeVertex(next, color));
    }

    if (!featherEdge || feather <= Epsilon) return;
    const PointList outer = outerFeatherPath(boundary, feather);
    for (std::size_t index = 0; index < uniqueCount; ++index) {
        appendQuad(
            vertices,
            boundary[index],
            boundary[(index + 1) % uniqueCount],
            outer[(index + 1) % uniqueCount],
            outer[index],
            color,
            1.0,
            1.0,
            0.0,
            0.0);
    }
}

void appendBand(
    VertexList& vertices,
    const PointList& outerPath,
    const PointList& innerPath,
    const QColor& color,
    const qreal outerAlpha,
    const qreal innerAlpha)
{
    if (outerPath.size() < 4 || innerPath.size() < 4 || color.alpha() == 0) return;
    // The two boundaries must use the same parametric sampling. Pairing an arc-length-resampled
    // inner path with the original corner-dense outer path can connect unrelated perimeter points,
    // producing the large diagonal triangles seen across cards and sheets.
    if (outerPath.size() != innerPath.size()) return;
    const std::size_t sampleCount = outerPath.size() - 1;

    for (std::size_t index = 0; index < sampleCount; ++index) {
        appendQuad(
            vertices,
            outerPath[index],
            outerPath[index + 1],
            innerPath[index + 1],
            innerPath[index],
            color,
            outerAlpha,
            outerAlpha,
            innerAlpha,
            innerAlpha);
    }
}

void buildBorderGeometry(
    VertexList& vertices,
    const PointList& outerBoundary,
    const PointList& innerBoundary,
    const PointList& innerFeatherBoundary,
    const qreal feather,
    const QColor& color)
{
    if (outerBoundary.size() < 4 || innerBoundary.size() < 4 || color.alpha() == 0) return;

    appendBand(vertices, outerBoundary, innerBoundary, color, 1.0, 1.0);

    if (feather > Epsilon) {
        const PointList outerFeather = outerFeatherPath(outerBoundary, feather);
        appendBand(vertices, outerFeather, outerBoundary, color, 0.0, 1.0);
        if (innerFeatherBoundary.size() >= 4) {
            appendBand(vertices, innerBoundary, innerFeatherBoundary, color, 1.0, 0.0);
        }
    }
}

void writeGeometry(QSGGeometry* geometry, const VertexList& vertices)
{
    geometry->allocate(static_cast<int>(vertices.size()));
    auto* target = geometry->vertexDataAsColoredPoint2D();
    for (std::size_t index = 0; index < vertices.size(); ++index) {
        const Vertex& vertex = vertices[index];
        target[index].set(vertex.x, vertex.y, vertex.r, vertex.g, vertex.b, vertex.a);
    }
}

[[nodiscard]] bool pointInsideCorner(
    const QPointF& point,
    const QPointF& center,
    const qreal radius,
    const qreal exponent)
{
    if (radius <= Epsilon) return true;
    const qreal x = std::abs((point.x() - center.x()) / radius);
    const qreal y = std::abs((point.y() - center.y()) / radius);
    return std::pow(x, exponent) + std::pow(y, exponent) <= 1.0 + 1e-6;
}

} // namespace

SceneGraphSuperellipseItem::SceneGraphSuperellipseItem(QQuickItem* parent)
    : QQuickItem{parent}
{
    setFlag(ItemHasContents, true);
    setAntialiasing(true);
}

QColor SceneGraphSuperellipseItem::fillColor() const { return m_fillColor; }
QColor SceneGraphSuperellipseItem::borderColor() const { return m_borderColor; }
qreal SceneGraphSuperellipseItem::borderWidth() const noexcept { return m_borderWidth; }
qreal SceneGraphSuperellipseItem::curveDepth() const noexcept { return m_curveDepth; }
qreal SceneGraphSuperellipseItem::exponent() const noexcept { return m_exponent; }
qreal SceneGraphSuperellipseItem::qualityScale() const noexcept { return m_qualityScale; }
bool SceneGraphSuperellipseItem::curveTopLeft() const noexcept { return m_curveTopLeft; }
bool SceneGraphSuperellipseItem::curveTopRight() const noexcept { return m_curveTopRight; }
bool SceneGraphSuperellipseItem::curveBottomRight() const noexcept { return m_curveBottomRight; }
bool SceneGraphSuperellipseItem::curveBottomLeft() const noexcept { return m_curveBottomLeft; }

void SceneGraphSuperellipseItem::setFillColor(const QColor& value)
{
    if (m_fillColor == value) return;
    m_fillColor = value;
    emit fillColorChanged();
    scheduleUpdate();
}

void SceneGraphSuperellipseItem::setBorderColor(const QColor& value)
{
    if (m_borderColor == value) return;
    m_borderColor = value;
    emit borderColorChanged();
    scheduleUpdate();
}

void SceneGraphSuperellipseItem::setBorderWidth(const qreal value)
{
    const qreal canonical = std::max<qreal>(0.0, value);
    if (qFuzzyCompare(m_borderWidth, canonical)) return;
    m_borderWidth = canonical;
    emit borderWidthChanged();
    scheduleUpdate();
}

void SceneGraphSuperellipseItem::setCurveDepth(const qreal value)
{
    const qreal canonical = std::max<qreal>(0.0, value);
    if (qFuzzyCompare(m_curveDepth, canonical)) return;
    m_curveDepth = canonical;
    emit curveDepthChanged();
    scheduleUpdate();
}

void SceneGraphSuperellipseItem::setExponent(const qreal value)
{
    const qreal canonical = std::max<qreal>(2.01, value);
    if (qFuzzyCompare(m_exponent, canonical)) return;
    m_exponent = canonical;
    emit exponentChanged();
    scheduleUpdate();
}

void SceneGraphSuperellipseItem::setQualityScale(const qreal value)
{
    const qreal canonical = std::clamp(value, 0.5, 2.0);
    if (qFuzzyCompare(m_qualityScale, canonical)) return;
    m_qualityScale = canonical;
    emit qualityScaleChanged();
    scheduleUpdate();
}

void SceneGraphSuperellipseItem::setCurveTopLeft(const bool value)
{
    if (m_curveTopLeft == value) return;
    m_curveTopLeft = value;
    emit curveTopLeftChanged();
    scheduleUpdate();
}

void SceneGraphSuperellipseItem::setCurveTopRight(const bool value)
{
    if (m_curveTopRight == value) return;
    m_curveTopRight = value;
    emit curveTopRightChanged();
    scheduleUpdate();
}

void SceneGraphSuperellipseItem::setCurveBottomRight(const bool value)
{
    if (m_curveBottomRight == value) return;
    m_curveBottomRight = value;
    emit curveBottomRightChanged();
    scheduleUpdate();
}

void SceneGraphSuperellipseItem::setCurveBottomLeft(const bool value)
{
    if (m_curveBottomLeft == value) return;
    m_curveBottomLeft = value;
    emit curveBottomLeftChanged();
    scheduleUpdate();
}

bool SceneGraphSuperellipseItem::contains(const QPointF& point) const
{
    if (!boundingRect().contains(point)) return false;
    const qreal widthValue = width();
    const qreal heightValue = height();
    const qreal radius = std::max<qreal>(0.0, std::min({m_curveDepth, widthValue / 2.0, heightValue / 2.0}));
    if (radius <= Epsilon) return true;
    const qreal exponentValue = std::max<qreal>(2.01, m_exponent);

    if (m_curveTopLeft && point.x() < radius && point.y() < radius) {
        return pointInsideCorner(point, {radius, radius}, radius, exponentValue);
    }
    if (m_curveTopRight && point.x() > widthValue - radius && point.y() < radius) {
        return pointInsideCorner(point, {widthValue - radius, radius}, radius, exponentValue);
    }
    if (m_curveBottomRight && point.x() > widthValue - radius && point.y() > heightValue - radius) {
        return pointInsideCorner(point, {widthValue - radius, heightValue - radius}, radius, exponentValue);
    }
    if (m_curveBottomLeft && point.x() < radius && point.y() > heightValue - radius) {
        return pointInsideCorner(point, {radius, heightValue - radius}, radius, exponentValue);
    }
    return true;
}

QSGNode* SceneGraphSuperellipseItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*)
{
    auto* node = static_cast<SurfaceNode*>(oldNode);
    if (node == nullptr) node = new SurfaceNode{};

    const qreal devicePixelRatio = safeDevicePixelRatio(window());
    const qreal physicalPixel = 1.0 / devicePixelRatio;
    const bool multisampled = usesMultisampling(window());
    // With window MSAA, hardware coverage resolves most triangle silhouettes.
    // Without it, a one-physical-pixel analytic fringe supplies stable coverage
    // on Metal/OpenGL backends without making the continuous corner look blurry.
    const qreal quality = std::clamp(m_qualityScale, 0.5, 2.0);
    const qreal feather = multisampled
        ? physicalPixel * std::clamp(0.48 / quality, 0.28, 0.62)
        : physicalPixel * std::clamp(1.05 / quality, 0.78, 1.28);
    // The old fixed half-pixel inset was narrower than the non-MSAA feather.
    // QQuickItem clipping then cut the transparent half of the fringe off and
    // exposed a hard, stair-stepped outer edge. Keep the complete coverage ramp
    // inside the item, with a tiny transparent guard for sampling precision.
    const qreal outerInset = std::max(
        physicalPixel * 0.5,
        feather + physicalPixel * 0.125);
    const qreal borderWidth = m_borderWidth <= Epsilon
        ? 0.0
        : std::max(physicalPixel, std::round(m_borderWidth * devicePixelRatio) / devicePixelRatio);
    const qreal outerRadius = std::max<qreal>(
        0.0,
        std::min({
            m_curveDepth - outerInset,
            std::max<qreal>(0.0, width() - outerInset * 2.0) / 2.0,
            std::max<qreal>(0.0, height() - outerInset * 2.0) / 2.0}));
    const int sharedCornerSegments = superellipseCornerSegments(outerRadius, devicePixelRatio, quality);
    const PointList outerBoundary = superellipseBoundary(
        size(),
        m_curveDepth,
        m_exponent,
        outerInset,
        m_curveTopLeft,
        m_curveTopRight,
        m_curveBottomRight,
        m_curveBottomLeft,
        devicePixelRatio,
        quality,
        sharedCornerSegments);

    VertexList fillVertices{};
    fillVertices.reserve(outerBoundary.size() * 9U);
    buildFillGeometry(fillVertices, outerBoundary, m_fillColor, feather, borderWidth <= Epsilon);
    writeGeometry(node->fillGeometry, fillVertices);
    node->fillNode->markDirty(QSGNode::DirtyGeometry | QSGNode::DirtyMaterial);

    VertexList borderVertices{};
    if (borderWidth > Epsilon && m_borderColor.alpha() > 0) {
        const PointList innerBoundary = superellipseBoundary(
            size(),
            m_curveDepth,
            m_exponent,
            outerInset + borderWidth,
            m_curveTopLeft,
            m_curveTopRight,
            m_curveBottomRight,
            m_curveBottomLeft,
            devicePixelRatio,
            quality,
            sharedCornerSegments);
        const PointList innerFeatherBoundary = superellipseBoundary(
            size(),
            m_curveDepth,
            m_exponent,
            outerInset + borderWidth + feather,
            m_curveTopLeft,
            m_curveTopRight,
            m_curveBottomRight,
            m_curveBottomLeft,
            devicePixelRatio,
            quality,
            sharedCornerSegments);
        borderVertices.reserve((outerBoundary.size() + innerBoundary.size()) * 18U);
        buildBorderGeometry(borderVertices, outerBoundary, innerBoundary, innerFeatherBoundary, feather, m_borderColor);
    }
    writeGeometry(node->borderGeometry, borderVertices);
    node->borderNode->markDirty(QSGNode::DirtyGeometry | QSGNode::DirtyMaterial);
    return node;
}

void SceneGraphSuperellipseItem::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size()) scheduleUpdate();
}

void SceneGraphSuperellipseItem::scheduleUpdate()
{
    update();
}
