#include "TestAppSetup.h"
#include "planova/slicer/WeightedScoringStrategy.h"
#include "slicer/ChaosSlicerPage.h"
#include "board/ComplexityDotsWidget.h"
#include "slicer/PizzaWidget.h"
#include "tests/fakes/FakeClock.h"
#include "tests/fakes/FakeEventStore.h"
#include "tests/fakes/FakeMemberQuery.h"
#include "tests/fakes/FakeTaskQuery.h"
#include "theme/GlassBackdrop.h"
#include "theme/ThemeManager.h"
#include "tracking/LeaderboardModel.h"
#include "widgets/BarChartWidget.h"
#include "widgets/DonutWidget.h"
#include <QApplication>
#include <QHash>
#include <QImage>
#include <QPainter>
#include <QTableView>
#include <QTest>
#include <algorithm>

using namespace planova::slicer;
using planova::s6::LeaderboardTable;

Q_DECLARE_METATYPE(ThemeMode)

namespace {
// Same fixture as tst_ChaosSlicerPage: a slicer with one member and one task.
struct Board {
    FakeTaskQuery tasks;
    FakeMemberQuery members;
    FakeEventStore store;
    EventLog log{store};
    FakeClock clock{1000};
    WeightedScoringStrategy strategy;
    SlicerService service{tasks, members, strategy, log, clock};

    Board() {
        members.addMember(MemberDto{"1", "Alice", "Dev", false, {{SkillCategory::Backend, 5}}, 100, 1.0});
        tasks.addTask(TaskDto{"t1", "t1", SkillCategory::Backend, Priority::Medium, 3, 4, TaskStatus::ToDo,
                              std::nullopt, {}});
    }
};

// Sum of the channel differences between two colours: 0 means equal.
int colourDistance(const QColor& a, const QColor& b) {
    return qAbs(a.red() - b.red()) + qAbs(a.green() - b.green()) + qAbs(a.blue() - b.blue());
}

// The largest difference on any one channel: a per-channel tolerance check.
int maxChannelDifference(const QColor& a, const QColor& b) {
    return std::max({qAbs(a.red() - b.red()), qAbs(a.green() - b.green()), qAbs(a.blue() - b.blue())});
}

// What `over` looks like painted on top of the opaque colour `under`.
QColor paintedOver(const QColor& over, const QColor& under) {
    QImage pixel(1, 1, QImage::Format_ARGB32);
    pixel.fill(under);
    QPainter p(&pixel);
    p.fillRect(0, 0, 1, 1, over);
    p.end();
    return pixel.pixelColor(0, 0);
}
}

// Counts paint events per watched widget.
class PaintCounter : public QObject {
public:
    QHash<QObject*, int> paints;
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event->type() == QEvent::Paint)
            ++paints[watched];
        return false;
    }
};

class tst_ThemeRepaint : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        Q_INIT_RESOURCE(planova);
        installAppFonts();
        QCoreApplication::setOrganizationName("PlanovaTest");
    }
    void cleanupTestCase() { ThemeManager::instance().apply(ThemeMode::Light); }

    void toggleRepaintsOpenPages() {
        auto& tm = ThemeManager::instance();
        tm.apply(ThemeMode::Light);

        Board b;
        ChaosSlicerPage page(b.service, b.tasks, b.members);
        page.resize(1100, 900);
        page.show();
        QVERIFY(QTest::qWaitForWindowExposed(&page));
        auto* pizza = page.findChild<PizzaWidget*>();
        QVERIFY(pizza);

        const QImage lightPage = page.grab().toImage();
        const QImage lightPizza = pizza->grab().toImage();
        auto* table = page.findChild<QTableView*>("assignmentsTable");
        QVERIFY(table);
        const QImage lightTable = table->viewport()->grab().toImage();
        // Pages are transparent, so the glass backdrop painted behind the app shows through.
        QCOMPARE(lightPage.pixelColor(2, 2).alpha(), 0);

        tm.apply(ThemeMode::Dark);
        QApplication::processEvents();

        const QImage darkPage = page.grab().toImage();
        const QImage darkPizza = pizza->grab().toImage();
        QCOMPARE(darkPage.pixelColor(2, 2).alpha(), 0);
        QVERIFY(lightPizza != darkPizza);
        QVERIFY(lightTable != table->viewport()->grab().toImage());
        // The page window colour stays transparent in the new theme (it never keeps an opaque colour from the old one).
        QCOMPARE(page.palette().color(QPalette::Window).alpha(), 0);

        tm.apply(ThemeMode::Light);
    }

    // These widgets get their colours from qproperty-* rules, so a new theme must reach them.
    void chartColoursFollowTheme() {
        auto& tm = ThemeManager::instance();
        tm.apply(ThemeMode::Light);
        DonutWidget donut;
        BarChartWidget bars;
        LeaderboardTable board;
        board.setObjectName("LeaderboardTable");
        donut.show();
        bars.show();
        board.show();
        QVERIFY(QTest::qWaitForWindowExposed(&donut));

        tm.apply(ThemeMode::Dark);
        QApplication::processEvents();
        QCOMPARE(donut.activeColor(), tm.color(Token::Accent));
        QCOMPARE(donut.textColor(), tm.color(Token::TextPrimary));
        QCOMPARE(bars.barColor(), tm.color(Token::Accent));
        QCOMPARE(bars.trackColor(), tm.color(Token::Track));
        QCOMPARE(board.accentColor(), tm.color(Token::Accent));

        tm.apply(ThemeMode::Light);
        QApplication::processEvents();
        QCOMPARE(donut.activeColor(), tm.color(Token::Accent));
        QCOMPARE(bars.trackColor(), tm.color(Token::Track));
    }

    // The rule: a widget that paints with tokens repaints on themeChanged by itself.
    // The signal is emitted alone (no new stylesheet), so a repaint can only come from the widget.
    void paintedWidgetsRepaintOnThemeChanged() {
        auto& tm = ThemeManager::instance();
        tm.apply(ThemeMode::Light);
        Board b;
        ChaosSlicerPage page(b.service, b.tasks, b.members);
        page.resize(1100, 900);
        ComplexityDotsWidget dots(3);
        PaintCounter counter;
        QWidget* painted[] = {page.findChild<PizzaWidget*>(),
                              page.findChild<QTableView*>("assignmentsTable")->viewport(), &dots};
        for (QWidget* w : painted)
            w->installEventFilter(&counter);
        page.show();
        dots.show();
        QVERIFY(QTest::qWaitForWindowExposed(&page));
        QVERIFY(QTest::qWaitForWindowExposed(&dots));
        QApplication::processEvents();

        counter.paints.clear();
        emit tm.themeChanged(ThemeMode::Light);
        QApplication::processEvents();
        for (QWidget* w : painted)
            QVERIFY2(counter.paints.value(w) > 0, w->metaObject()->className());
    }

    // The backdrop behind the app: a cached picture that follows the theme and the widget size.
    void glassBackdropFollowsThemeAndSize() {
        auto& tm = ThemeManager::instance();
        tm.apply(ThemeMode::Light);
        GlassBackdrop backdrop;
        backdrop.resize(400, 300);
        backdrop.show();
        QVERIFY(QTest::qWaitForWindowExposed(&backdrop));
        const QImage light = backdrop.cachedBackdrop().toImage();
        QVERIFY(!light.isNull());

        tm.apply(ThemeMode::Dark);
        QApplication::processEvents();
        const QImage dark = backdrop.cachedBackdrop().toImage();
        QVERIFY(dark != light);

        backdrop.resize(500, 350);
        QApplication::processEvents();
        QCOMPARE(backdrop.cachedBackdrop().size(), backdrop.size() * backdrop.devicePixelRatioF());

        tm.apply(ThemeMode::Light);
    }

    // The primary glow is painted where it should be: its centre shows the glow colour, a far corner does not.
    void glassBackdropPaintsTheGlow_data() {
        QTest::addColumn<ThemeMode>("mode");
        QTest::addColumn<int>("minGlowDifference");
        // The light glow is the accent at about 6% alpha, so it is much fainter than the dark one.
        QTest::newRow("light") << ThemeMode::Light << 15;
        QTest::newRow("dark") << ThemeMode::Dark << 60;
    }

    void glassBackdropPaintsTheGlow() {
        QFETCH(ThemeMode, mode);
        QFETCH(int, minGlowDifference);
        auto& tm = ThemeManager::instance();
        tm.apply(mode);
        GlassBackdrop backdrop;
        backdrop.resize(400, 300);
        backdrop.show();
        QVERIFY(QTest::qWaitForWindowExposed(&backdrop));
        const QImage image = backdrop.cachedBackdrop().toImage();
        QVERIFY(!image.isNull());

        // (a) The glow centre (15%, 10%) is the glow colour over the top of the gradient. The tolerance covers the
        // gradient having moved 10% of the way towards BackdropBottom at that height.
        const QColor glowCentre = image.pixelColor(int(image.width() * 0.15), int(image.height() * 0.10));
        const QColor expectedGlow = paintedOver(tm.color(Token::GlowPrimary), tm.color(Token::BackdropTop));
        QVERIFY2(maxChannelDifference(glowCentre, expectedGlow) <= 6,
                 qPrintable(glowCentre.name() + " vs " + expectedGlow.name()));

        // (b) The top-right corner is outside both glows: plain BackdropTop, clearly unlike the glow centre.
        const QColor farCorner = image.pixelColor(image.width() - 1, 0);
        QVERIFY2(maxChannelDifference(farCorner, tm.color(Token::BackdropTop)) <= 6, qPrintable(farCorner.name()));
        QVERIFY2(colourDistance(farCorner, glowCentre) >= minGlowDifference,
                 qPrintable(QString::number(colourDistance(farCorner, glowCentre))));

        tm.apply(ThemeMode::Light);
    }
};

QTEST_MAIN(tst_ThemeRepaint)
#include "tst_ThemeRepaint.moc"
