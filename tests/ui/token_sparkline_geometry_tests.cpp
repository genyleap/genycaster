#include "../../ui/render/sparkline.hpp"

#include <QPointF>
#include <QVariant>
#include <QVariantList>
#include <QtTest/QTest>

#include <algorithm>
#include <cmath>
#include <limits>

class TokenSparklineGeometryTests final : public QObject {
    Q_OBJECT

private slots:
    void emptyAndInvalidInputProducesNoGeometry();
    void sampleOrderAndShapeArePreserved();
    void largeSeriesIsBounded();
    void constantSeriesRemainsCentered();
    void smoothCurvePreservesSampleExtrema();
    void smoothCurveBoundsPointCountAndArea();
};

void TokenSparklineGeometryTests::emptyAndInvalidInputProducesNoGeometry()
{
    TokenSparklineGeometry geometry{};
    QCOMPARE(geometry.compute({}, 160, 60, 2, 1.9).value("linePoints").toList().size(), 0);
    QCOMPARE(geometry.compute(QVariantList{1, 2}, 1, 60, 2, 1.9).value("linePoints").toList().size(), 0);
    QCOMPARE(geometry.compute(QVariantList{1}, 160, 60, 2, 1.9).value("linePoints").toList().size(), 0);
    QVariantList invalid{QVariant::fromValue(std::numeric_limits<double>::quiet_NaN()), 2.0};
    QCOMPARE(geometry.compute(invalid, 160, 60, 2, 1.9).value("linePoints").toList().size(), 0);
}

void TokenSparklineGeometryTests::sampleOrderAndShapeArePreserved()
{
    TokenSparklineGeometry geometry{};
    const QVariantMap path = geometry.compute(QVariantList{1.0, 2.0, 3.0}, 152, 60, 2, 1.9);
    const QVariantList line = path.value("linePoints").toList();
    const QVariantList area = path.value("areaPoints").toList();
    QCOMPARE(line.size(), 3);
    QCOMPARE(area.size(), 5);
    QVERIFY(line.first().toPointF().x() < line.last().toPointF().x());
    QVERIFY(line.first().toPointF().y() > line.last().toPointF().y());
    QCOMPARE(area.first().toPointF().x(), line.first().toPointF().x());
    QCOMPARE(area.last().toPointF().x(), line.last().toPointF().x());
    QCOMPARE(area.first().toPointF().y(), 58.0);
    QCOMPARE(area.last().toPointF().y(), 58.0);
}

void TokenSparklineGeometryTests::largeSeriesIsBounded()
{
    TokenSparklineGeometry geometry{};
    QVariantList samples{};
    for (int i = 0; i < 1000; ++i) samples.append(static_cast<qreal>(i));
    const QVariantList narrow = geometry.compute(samples, 150, 40, 2, 1.9).value("linePoints").toList();
    const QVariantList wide = geometry.compute(samples, 1000, 40, 2, 1.9).value("linePoints").toList();
    QCOMPARE(narrow.size(), 30);
    QCOMPARE(wide.size(), 96);
    QVERIFY(narrow.first().toPointF().y() > narrow.last().toPointF().y());
}

void TokenSparklineGeometryTests::constantSeriesRemainsCentered()
{
    TokenSparklineGeometry geometry{};
    const QVariantList line = geometry.compute(
        QVariantList{42.0, 42.0, 42.0, 42.0, 42.0}, 200, 80, 2, 1.9).value("linePoints").toList();
    QCOMPARE(line.size(), 5);
    for (const QVariant& point : line) {
        QVERIFY(std::abs(point.toPointF().y() - 40.0) < 1e-8);
    }
}


void TokenSparklineGeometryTests::smoothCurvePreservesSampleExtrema()
{
    TokenSparklineGeometry geometry{};
    const QVariantList values{10.0, 18.0, 20.0, 7.0, 12.0};
    const QVariantList source = geometry.compute(values, 360, 100, 3, 2.0).value("linePoints").toList();
    const QVariantList curve = geometry.computeSmooth(values, 360, 100, 3, 2.0).value("linePoints").toList();
    QVERIFY(curve.size() > source.size());
    QCOMPARE(curve.first().toPointF(), source.first().toPointF());
    QCOMPARE(curve.last().toPointF(), source.last().toPointF());

    for (qsizetype index = 0; index + 1 < source.size(); ++index) {
        const QPointF left = source[index].toPointF();
        const QPointF right = source[index + 1].toPointF();
        const qreal low = std::min(left.y(), right.y()) - 0.0001;
        const qreal high = std::max(left.y(), right.y()) + 0.0001;
        for (const QVariant& sample : curve) {
            const QPointF point = sample.toPointF();
            QVERIFY(std::isfinite(point.x()));
            QVERIFY(std::isfinite(point.y()));
            if (point.x() < left.x() || point.x() > right.x()) {
                continue;
            }
            QVERIFY(point.y() >= low);
            QVERIFY(point.y() <= high);
        }
    }
}

void TokenSparklineGeometryTests::smoothCurveBoundsPointCountAndArea()
{
    TokenSparklineGeometry geometry{};
    QVariantList values{};
    for (int index = 0; index < 1000; ++index) {
        values.append(static_cast<double>(index % 17));
    }
    const QVariantMap shape = geometry.computeSmooth(values, 800, 88, 3, 2);
    const QVariantList curve = shape.value("linePoints").toList();
    const QVariantList area = shape.value("areaPoints").toList();
    QVERIFY(curve.size() <= 96 * 8);
    QCOMPARE(area.size(), curve.size() + 2);
    QCOMPARE(area.first().toPointF().x(), curve.first().toPointF().x());
    QCOMPARE(area.last().toPointF().x(), curve.last().toPointF().x());
    QCOMPARE(geometry.computeSmooth({}, 0, 20, 3, 2)
             .value("linePoints").toList().size(), 0);
}

QTEST_APPLESS_MAIN(TokenSparklineGeometryTests)
#include "token_sparkline_geometry_tests.moc"
