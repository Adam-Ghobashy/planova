#include "TestAppSetup.h"
#include "app/Page.h"
#include "app/Sidebar.h"
#include "planova/PlanovaCore.h"
#include "theme/ThemeManager.h"
#include <QApplication>
#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

Q_DECLARE_METATYPE(Page)

namespace {
// The nav button whose label text is exactly `text`.
QPushButton* itemWithLabel(Sidebar& sidebar, const QString& text)
{
    for (auto* button : sidebar.findChildren<QPushButton*>("navItem")) {
        auto* label = button->findChild<QLabel*>("navLabel");
        if (label && label->text() == text)
            return button;
    }
    return nullptr;
}

QLabel* badgeOf(QPushButton* item)
{
    return item->findChild<QLabel*>("navBadge");
}

NewTask sampleTask(const char* title)
{
    NewTask t;
    t.title = title;
    t.skill = SkillCategory::Backend;
    t.priority = Priority::Medium;
    t.complexity = 2;
    t.estHours = 4;
    return t;
}
}

class tst_Sidebar : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        Q_INIT_RESOURCE(planova);
        installAppFonts();
        QCoreApplication::setOrganizationName("PlanovaTest");
        qRegisterMetaType<Page>("Page");
    }
    void init() { ThemeManager::instance().apply(ThemeMode::Light); }
    void cleanupTestCase() { ThemeManager::instance().apply(ThemeMode::Light); }

    void labelsShowAmpersand()
    {
        QTemporaryDir dir;
        PlanovaCore core(dir.path().toStdString());
        core.openProject(core.createProject("A", "team", "Agile", "idea"));
        Sidebar sidebar(core);

        QVERIFY(itemWithLabel(sidebar, "Pins & Collab"));
        QVERIFY(itemWithLabel(sidebar, "Team & Skills"));
        QVERIFY(itemWithLabel(sidebar, "Fair Tracking & Charts"));
    }

    void badgesFollowCounts()
    {
        QTemporaryDir dir;
        PlanovaCore core(dir.path().toStdString());
        core.openProject(core.createProject("A", "team", "Agile", "idea"));
        Sidebar sidebar(core);
        QPushButton* pins = itemWithLabel(sidebar, "Pins & Collab");
        QPushButton* board = itemWithLabel(sidebar, "Phase Board");
        QVERIFY(pins && board);
        // Zero counts hide the badge.
        QVERIFY(badgeOf(pins)->isHidden());
        QVERIFY(badgeOf(board)->isHidden());

        const Id member = core.members().add("Ana", "Backend Developer", false);
        core.tasks().create(sampleTask("One"));
        core.tasks().create(sampleTask("Two"));
        const Id pin = core.pins().create(PinCategory::HandoffNote, "Note", {}, member, std::nullopt);
        sidebar.refresh();

        QVERIFY(!badgeOf(board)->isHidden());
        QCOMPARE(badgeOf(board)->text(), QString("2"));
        QVERIFY(!badgeOf(pins)->isHidden());
        QCOMPARE(badgeOf(pins)->text(), QString("1"));
        QVERIFY(pins->accessibleName().contains("1 open"));

        core.pins().resolve(pin, member);
        sidebar.refresh();
        QVERIFY(badgeOf(pins)->isHidden());
    }

    void clickEmitsPage()
    {
        QTemporaryDir dir;
        PlanovaCore core(dir.path().toStdString());
        core.openProject(core.createProject("A", "team", "Agile", "idea"));
        Sidebar sidebar(core);
        QSignalSpy spy(&sidebar, &Sidebar::pageRequested);

        QTest::mouseClick(itemWithLabel(sidebar, "Phase Board"), Qt::LeftButton);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).value<Page>(), Page::Board);

        QTest::mouseClick(itemWithLabel(sidebar, "Projects Hub"), Qt::LeftButton);
        QCOMPARE(spy.at(1).at(0).value<Page>(), Page::Hub);
    }

    void activeProperty()
    {
        QTemporaryDir dir;
        PlanovaCore core(dir.path().toStdString());
        core.openProject(core.createProject("A", "team", "Agile", "idea"));
        Sidebar sidebar(core);

        sidebar.setActivePage(Page::Team);
        for (auto* button : sidebar.findChildren<QPushButton*>("navItem")) {
            const bool isTeam = button == itemWithLabel(sidebar, "Team & Skills");
            QCOMPARE(button->property("active").toBool(), isTeam);
        }

        // A page without a sidebar item (Welcome) leaves no item active.
        sidebar.setActivePage(Page::Welcome);
        for (auto* button : sidebar.findChildren<QPushButton*>("navItem"))
            QVERIFY(!button->property("active").toBool());
    }

    void focusRingVisibleOnActiveItem()
    {
        QTemporaryDir dir;
        PlanovaCore core(dir.path().toStdString());
        Sidebar sidebar(core);
        sidebar.setActivePage(Page::Team);
        sidebar.show();
        QVERIFY(QTest::qWaitForWindowExposed(&sidebar));
        QPushButton* team = itemWithLabel(sidebar, "Team & Skills");
        QVERIFY(team);

        for (ThemeMode mode : {ThemeMode::Light, ThemeMode::Dark}) {
            ThemeManager::instance().apply(mode);
            team->setFocus(Qt::TabFocusReason);
            if (!team->hasFocus())
                QSKIP("window not active in this environment, focus state cannot be rendered");
            const QImage image = team->grab().toImage();
            const int y = image.height() / 2;
            // x = 0 is the antialiased edge; x = 1 is the ring, x = 5 the fill between ring and icon.
            QVERIFY2(image.pixelColor(1, y) != image.pixelColor(5, y),
                     "focus ring has the same colour as the active fill");
        }
    }

    void themeButtonToggles()
    {
        QTemporaryDir dir;
        PlanovaCore core(dir.path().toStdString());
        Sidebar sidebar(core);
        auto* toggle = sidebar.findChild<QPushButton*>("themeToggle");
        QVERIFY(toggle);
        QCOMPARE(toggle->toolTip(), QString("Switch to dark mode"));

        QTest::mouseClick(toggle, Qt::LeftButton);
        QCOMPARE(ThemeManager::instance().mode(), ThemeMode::Dark);
        QCOMPARE(toggle->toolTip(), QString("Switch to light mode"));

        QTest::mouseClick(toggle, Qt::LeftButton);
        QCOMPARE(ThemeManager::instance().mode(), ThemeMode::Light);
        QCOMPARE(toggle->toolTip(), QString("Switch to dark mode"));
    }
};

QTEST_MAIN(tst_Sidebar)
#include "tst_Sidebar.moc"
