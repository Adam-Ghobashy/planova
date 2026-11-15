#include "slicer/SliceTimeline.h"
#include <QTest>

using namespace planova::slicer;

class tst_SliceTimeline : public QObject {
    Q_OBJECT
private slots:
    void startIsAWholePizzaWithTheCutterAtTheFirstCut() {
        SliceTimeline t({0.0, 120.0, 240.0});
        const SliceFrame f = t.at(0);
        QCOMPARE(int(f.cutProgress.size()), 3);
        for (double p : f.cutProgress) QCOMPARE(p, 0.0);
        QVERIFY(f.wheelVisible);
        QCOMPARE(f.wheelDistance, 1.0);  // at the rim
        QCOMPARE(f.wheelAngleDeg, 0.0);
        QVERIFY(!f.wheelLifted);
        QCOMPARE(f.spread, 0.0);
    }
    void firstCutIsHalfDrawnHalfwayThroughItsPass() {
        SliceTimeline t({0.0, 120.0, 240.0});
        // The cutting pass is the first 55% of a cut's time, so half of it is 27.5% of kCutMs.
        const SliceFrame f = t.at(int(SliceTimeline::kCutMs * 0.55 / 2.0));
        QVERIFY(qAbs(f.cutProgress[0] - 0.5) < 0.02);
        QVERIFY(qAbs(f.wheelDistance - 0.5) < 0.02);
        QVERIFY(!f.wheelLifted);
        QCOMPARE(f.cutProgress[1], 0.0);
    }
    void secondCutStartsWhenTheFirstEnds() {
        SliceTimeline t({0.0, 120.0, 240.0});
        const SliceFrame f = t.at(SliceTimeline::kCutMs);
        QCOMPARE(f.cutProgress[0], 1.0);
        QCOMPARE(f.cutProgress[1], 0.0);
        QCOMPARE(f.wheelAngleDeg, 120.0);
        QCOMPARE(f.wheelDistance, 1.0);
    }
    void wheelIsLiftedWhileItLeavesTheCut() {
        SliceTimeline t({0.0, 120.0});
        const SliceFrame f = t.at(int(SliceTimeline::kCutMs * 0.7));  // between 55% and 80%
        QVERIFY(f.wheelLifted);
        QCOMPARE(f.cutProgress[0], 1.0);
    }
    void wheelTravelsClockwiseAcrossTheZeroMark() {
        SliceTimeline t({350.0, 10.0});
        // 90% of the first cut's time = half way along the 20 degree clockwise arc 350 -> 10.
        const SliceFrame f = t.at(int(SliceTimeline::kCutMs * 0.9));
        QVERIFY(f.wheelAngleDeg < 1.0 || f.wheelAngleDeg > 359.0);
    }
    void slicesStayTogetherWhileCutting() {
        SliceTimeline t({0.0, 120.0});
        QCOMPARE(t.at(2 * SliceTimeline::kCutMs - 1).spread, 0.0);
    }
    void endIsFullyCutAndSpreadWithNoCutter() {
        SliceTimeline t({0.0, 120.0, 240.0});
        const SliceFrame f = t.at(t.totalMs());
        for (double p : f.cutProgress) QCOMPARE(p, 1.0);
        QVERIFY(!f.wheelVisible);
        QCOMPARE(f.spread, 1.0);
        QCOMPARE(t.totalMs(), 3 * SliceTimeline::kCutMs + SliceTimeline::kSpreadMs);
    }
    void manyCutsAreSpedUpToStayUnderThreeSeconds() {
        SliceTimeline twelve(std::vector<double>{0, 30, 60, 90, 120, 150, 180, 210, 240, 270, 300, 330});
        QVERIFY(twelve.totalMs() <= 3000 + SliceTimeline::kSpreadMs);
        QCOMPARE(twelve.at(twelve.totalMs()).spread, 1.0);
        QVERIFY(!twelve.at(twelve.totalMs()).wheelVisible);
    }
    void noCutsMeansOnlyTheSpread() {
        SliceTimeline t({});
        QCOMPARE(t.totalMs(), SliceTimeline::kSpreadMs);
        QVERIFY(!t.at(0).wheelVisible);
        QCOMPARE(t.at(0).spread, 0.0);
        QCOMPARE(t.at(t.totalMs()).spread, 1.0);
    }
};

QTEST_APPLESS_MAIN(tst_SliceTimeline)
#include "tst_SliceTimeline.moc"
