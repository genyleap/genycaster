#include "../render/token_sparkline_geometry.hpp"

#include <QPointF>
#include <QVariant>
#include <QVariantList>
#include <QtTest/QTest>

#include <cmath>
#include <limits>

class TokenSparklineGeometryTests final : public QObject {
    Q_OBJECT

private slots:
    void emptyAndInvalidInputProducesNoGeometry();
    void sampleOrderAndShapeArePreserved();
    void largeSeriesIsBounded();
    void constantSeriesRemainsCentered();
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

QTEST_APPLESS_MAIN(TokenSparklineGeometryTests)
#include "token_sparkline_geometry_tests.moc"
