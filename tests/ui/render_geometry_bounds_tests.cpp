#include "../../ui/render/geometry_safety.hpp"
#include "../../ui/render/stroke.hpp"
#include "../../ui/render/superellipse.hpp"

#include <QGuiApplication>
#include <QSGNode>
#include <QSGGeometryNode>
#include <QSGGeometry>

#include <cmath>
#include <limits>
#include <source_location>

namespace {
void require(bool valid, std::source_location where = std::source_location::current())
{
    if (!valid)
        qFatal("Renderer numeric-safety test failed at %s:%u",
               where.file_name(), where.line());
}

class InspectStroke final : public SceneGraphStrokeItem {
public:
    using SceneGraphStrokeItem::updatePaintNode;
};

class InspectSuperellipse final : public SceneGraphSuperellipseItem {
public:
    using SceneGraphSuperellipseItem::updatePaintNode;
};
}

int main(int argc, char** argv)
{
    QGuiApplication app{argc, argv};
    using genycaster::render::safety::boundedSegmentCount;
    constexpr int minimum = 8;
    constexpr int maximum = 256;
    require(boundedSegmentCount(8.1, minimum, maximum) == 9);
    require(boundedSegmentCount(9.0, minimum, maximum) == 9);
    require(boundedSegmentCount(0.0, minimum, maximum) == minimum);
    require(boundedSegmentCount(255.2, minimum, maximum) == maximum);
    require(boundedSegmentCount(1.0e300, minimum, maximum) == maximum);
    require(boundedSegmentCount(std::numeric_limits<qreal>::max(), minimum, maximum) == maximum);
    require(boundedSegmentCount(std::numeric_limits<qreal>::infinity(), minimum, maximum) == maximum);
    require(boundedSegmentCount(-std::numeric_limits<qreal>::infinity(), minimum, maximum) == minimum);
    require(boundedSegmentCount(std::numeric_limits<qreal>::quiet_NaN(), minimum, maximum) == minimum);

    // Actual native scene-graph generators, not just the shared helper.
    // The 1e12px inputs previously produced floating-to-int overflow before
    // std::clamp could bound tessellation counts. These do not create a window
    // or submit any geometry to a GPU.
    InspectStroke stroke;
    stroke.setWidth(1.0e12);
    stroke.setHeight(1.0e12);
    stroke.setStrokeColor(Qt::white);
    stroke.setStrokeWidth(2.0);
    stroke.setRadius(1.0e11);
    stroke.setSweepAngle(360.0);
    stroke.setPathKind(SceneGraphStrokeItem::PathKind::Arc);
    auto* arc = stroke.updatePaintNode(nullptr, nullptr);
    require(arc != nullptr);
    delete arc;

    stroke.setPathKind(SceneGraphStrokeItem::PathKind::RoundedRectangle);
    stroke.setCornerRadius(1.0e11);
    auto* rounded = stroke.updatePaintNode(nullptr, nullptr);
    require(rounded != nullptr);
    delete rounded;

    InspectSuperellipse surface;
    surface.setWidth(1.0e12);
    surface.setHeight(1.0e12);
    surface.setCurveDepth(1.0e11);
    surface.setFillColor(Qt::white);
    auto* superellipse = surface.updatePaintNode(nullptr, nullptr);
    require(superellipse != nullptr);
    delete superellipse;

    // Existing scene graph nodes must reflect same-count recoloring and
    // geometry changes during resize; DynamicPattern cannot depend on
    // allocating a new buffer to invalidate its vertex data.
    surface.setWidth(320);
    surface.setHeight(180);
    surface.setCurveDepth(24);
    auto* retainedSurface = surface.updatePaintNode(nullptr, nullptr);
    auto* fill = static_cast<QSGGeometryNode*>(retainedSurface->firstChild());
    const auto vertexCount = fill->geometry()->vertexCount();
    require(vertexCount > 0);
    const auto oldColor = fill->geometry()->vertexDataAsColoredPoint2D()[0].r;
    surface.setFillColor(Qt::blue);
    require(surface.updatePaintNode(retainedSurface, nullptr) == retainedSurface);
    require(fill->geometry()->vertexCount() == vertexCount);
    require(fill->geometry()->vertexDataAsColoredPoint2D()[0].r != oldColor);
    surface.setWidth(640);
    require(surface.updatePaintNode(retainedSurface, nullptr) == retainedSurface);
    require(fill->geometry()->vertexCount() > 0);
    delete retainedSurface;

    stroke.setWidth(120);
    stroke.setHeight(120);
    stroke.setCornerRadius(12);
    stroke.setStrokeColor(Qt::white);
    auto* retainedStroke = static_cast<QSGGeometryNode*>(stroke.updatePaintNode(nullptr, nullptr));
    const auto strokeCount = retainedStroke->geometry()->vertexCount();
    require(strokeCount > 0);
    stroke.setStrokeColor(Qt::blue);
    require(stroke.updatePaintNode(retainedStroke, nullptr) == retainedStroke);
    require(retainedStroke->geometry()->vertexCount() == strokeCount);
    bool hasBlue = false;
    for (int i = 0; i < strokeCount; ++i) {
        const auto& vertex = retainedStroke->geometry()->vertexDataAsColoredPoint2D()[i];
        hasBlue |= vertex.b > vertex.r;
    }
    require(hasBlue);
    delete retainedStroke;
    return 0;
}
