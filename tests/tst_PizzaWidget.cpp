#include "slicer/PizzaWidget.h"
#include <QSignalSpy>
#include <QTest>
#include <QtMath>

using namespace planova::slicer;

namespace {
Portion portion(const char* id, const char* name, int tasks, double start, double span) {
    Portion p;
    p.memberId = id;
    p.memberName = name;
    p.taskCount = tasks;
    p.totalComplexity = tasks * 3;
    p.share = span / 360.0;
    p.startAngle = start;
    p.spanAngle = span;
    p.thinSlice = span <= 0.0;
    return p;
}

// A point inside the pie at the given clockwise angle (from 12 o'clock), as a fraction of the radius.
QPointF pointAt(const PizzaWidget& w, double angleDeg, double radiusFraction) {
    const double rad = qDegreesToRadians(angleDeg);
    return QPointF(w.pieCenter().x() + w.pieRadius() * radiusFraction * std::sin(rad),
                   w.pieCenter().y() - w.pieRadius() * radiusFraction * std::cos(rad));
}
}

class tst_PizzaWidget : public QObject {
    Q_OBJECT
private slots:
    void sliceAtFindsSliceByAngle() {
        PizzaWidget w;
        w.resize(400, 500);
        w.setPortions({portion("1", "Alice Smith", 2, 0, 180), portion("2", "Bob Jones", 2, 180, 180)});
        QCOMPARE(w.sliceAt(pointAt(w, 90, 0.5)), 0);   // right half
        QCOMPARE(w.sliceAt(pointAt(w, 270, 0.5)), 1);  // left half
    }
    void sliceAtOutsideIsMinusOne() {
        PizzaWidget w;
        w.resize(400, 500);
        w.setPortions({portion("1", "Alice Smith", 2, 0, 360)});
        QCOMPARE(w.sliceAt(pointAt(w, 90, 1.5)), -1);
    }
    void singleFullSliceResolves() {
        PizzaWidget w;
        w.resize(400, 500);
        w.setPortions({portion("1", "Alice Smith", 4, 0, 360), portion("2", "Bob Jones", 0, 360, 0)});
        QCOMPARE(w.sliceAt(pointAt(w, 10, 0.5)), 0);
        QCOMPARE(w.sliceAt(pointAt(w, 200, 0.5)), 0);
    }
    void allThinSlicesHitNothing() {
        PizzaWidget w;
        w.resize(400, 500);
        w.setPortions({portion("1", "Alice Smith", 0, 0, 0), portion("2", "Bob Jones", 0, 0, 0)});
        QCOMPARE(w.sliceAt(pointAt(w, 90, 0.5)), -1);
    }
    void emptyPortionsHitNothing() {
        PizzaWidget w;
        w.resize(400, 500);
        QCOMPARE(w.sliceAt(pointAt(w, 90, 0.5)), -1);
    }
    void startsWholeUntilSliced() {
        PizzaWidget w;
        w.setPortions({portion("1", "Alice Smith", 2, 0, 180), portion("2", "Bob Jones", 2, 180, 180)});
        QVERIFY(!w.isSliced());
        QVERIFY(!w.isAnimating());
    }
    void slicingAnimatesThenEndsSliced() {
        PizzaWidget w;
        w.setPortions({portion("1", "Alice Smith", 2, 0, 180), portion("2", "Bob Jones", 2, 180, 180)});
        w.sliceAnimated();
        QVERIFY(w.isAnimating());
        QVERIFY(!w.isSliced());  // still whole while the cutter works
        QTRY_VERIFY_WITH_TIMEOUT(w.isSliced(), 5000);
        QVERIFY(!w.isAnimating());
    }
    void setSlicedIsInstant() {
        PizzaWidget w;
        w.setPortions({portion("1", "Alice Smith", 2, 0, 180), portion("2", "Bob Jones", 2, 180, 180)});
        w.setSliced(true);
        QVERIFY(w.isSliced());
        w.setSliced(false);
        QVERIFY(!w.isSliced());
    }
    void changedPortionsMakeItWholeAgain() {
        // Cuts belong to the slices they were made for. New slices need a new press of "Slice".
        PizzaWidget w;
        w.setPortions({portion("1", "Alice Smith", 2, 0, 180), portion("2", "Bob Jones", 2, 180, 180)});
        w.setSliced(true);
        w.setPortions({portion("1", "Alice Smith", 3, 0, 270), portion("2", "Bob Jones", 1, 270, 90)});
        QVERIFY(!w.isSliced());
        QVERIFY(!w.isAnimating());
    }
    void samePortionsKeepItSliced() {
        PizzaWidget w;
        const std::vector<Portion> p = {portion("1", "Alice Smith", 2, 0, 180), portion("2", "Bob Jones", 2, 180, 180)};
        w.setPortions(p);
        w.setSliced(true);
        w.setPortions(p);
        QVERIFY(w.isSliced());
    }
    void newPortionsStopTheCutter() {
        PizzaWidget w;
        w.setPortions({portion("1", "Alice Smith", 2, 0, 180), portion("2", "Bob Jones", 2, 180, 180)});
        w.sliceAnimated();
        QVERIFY(w.isAnimating());
        w.setPortions({portion("1", "Alice Smith", 3, 0, 270), portion("2", "Bob Jones", 1, 270, 90)});
        QVERIFY(!w.isAnimating());
        QVERIFY(!w.isSliced());
    }
    void oneSliceOnlyHasNothingToCut() {
        PizzaWidget w;
        w.setPortions({portion("1", "Alice Smith", 4, 0, 360)});
        w.sliceAnimated();
        QVERIFY(w.isSliced());  // no cutter animation for a single slice
        QVERIFY(!w.isAnimating());
    }
    void clickingTheWholePizzaSelectsNothing() {
        PizzaWidget w;
        w.resize(400, 500);
        w.setPortions({portion("1", "Alice Smith", 2, 0, 180), portion("2", "Bob Jones", 2, 180, 180)});
        QSignalSpy spy(&w, &PizzaWidget::memberSelected);
        QTest::mouseClick(&w, Qt::LeftButton, Qt::NoModifier, pointAt(w, 90, 0.5).toPoint());
        QCOMPARE(spy.count(), 0);  // slices do not exist until the pizza is cut
    }
    void clickEmitsMemberSelectedThenClears() {
        PizzaWidget w;
        w.resize(400, 500);
        w.setPortions({portion("1", "Alice Smith", 2, 0, 180), portion("2", "Bob Jones", 2, 180, 180)});
        w.setSliced(true);
        QSignalSpy spy(&w, &PizzaWidget::memberSelected);

        const QPoint pos = pointAt(w, 90, 0.5).toPoint();
        QTest::mouseClick(&w, Qt::LeftButton, Qt::NoModifier, pos);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QString("1"));

        QTest::mouseClick(&w, Qt::LeftButton, Qt::NoModifier, pos);
        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.at(1).at(0).toString(), QString());
    }
};

QTEST_MAIN(tst_PizzaWidget)
#include "tst_PizzaWidget.moc"
