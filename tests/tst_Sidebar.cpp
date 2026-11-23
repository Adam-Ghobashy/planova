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

        QVERIFY(itemWithLabel(sidebar, "Team & Skills"));
    }

    // The items follow the workflow, under three headings, in the order navOrder() gives.
    void itemsInWorkflowOrder()
    {
        QTemporaryDir dir;
        PlanovaCore core(dir.path().toStdString());
        core.openProject(core.createProject("A", "team", "Agile", "idea"));
        Sidebar sidebar(core);
        sidebar.show();
        QVERIFY(QTest::qWaitForWindowExposed(&sidebar));

        const QStringList expected = {"Overview", "Phase Board", "Chaos Slicer", "Pins", "Team & Skills",
                                      "Tracking", "GitHub"};
        int lastY = -1;
        for (const QString& text : expected) {
            QPushButton* item = itemWithLabel(sidebar, text);
            QVERIFY2(item, qPrintable(text));
            QVERIFY2(item->y() > lastY, qPrintable(text + " is out of order"));
            lastY = item->y();
        }
        QCOMPARE(sidebar.findChildren<QPushButton*>("navItem").size(), expected.size());

        const auto headings = sidebar.findChildren<QLabel*>("sidebarLabel");
        QCOMPARE(headings.size(), 3);
        QCOMPARE(headings.at(0)->text().toUpper(), QString("PROJECT"));
        QCOMPARE(headings.at(1)->text().toUpper(), QString("COLLABORATE"));
        QCOMPARE(headings.at(2)->text().toUpper(), QString("INSIGHTS"));
        QCOMPARE(headings.at(0)->font().capitalization(), QFont::AllUppercase);
        // Each heading sits right above its first item.
        QVERIFY(headings.at(0)->y() < itemWithLabel(sidebar, "Overview")->y());
        QVERIFY(headings.at(1)->y() > itemWithLabel(sidebar, "Chaos Slicer")->y());
        QVERIFY(headings.at(1)->y() < itemWithLabel(sidebar, "Pins")->y());
        QVERIFY(headings.at(2)->y() > itemWithLabel(sidebar, "Team & Skills")->y());
        QVERIFY(headings.at(2)->y() < itemWithLabel(sidebar, "Tracking")->y());

        QCOMPARE(Sidebar::navOrder(), QList<Page>({Page::Home, Page::Board, Page::Slicer, Page::Pins, Page::Team,
                                                   Page::Tracking, Page::GitHub}));
    }

    void switcherShowsProjectAndOpensHub()
    {
        QTemporaryDir dir;
        PlanovaCore core(dir.path().toStdString());
        Sidebar sidebar(core);
        auto* switcher = sidebar.findChild<QPushButton*>("projectSwitcher");
        QVERIFY(switcher);
        auto* name = switcher->findChild<QLabel*>("projectSwitcherName");
        QVERIFY(name);
        QCOMPARE(name->text(), QString("No project open"));
        QCOMPARE(switcher->toolTip(), QString("Open Projects Hub (Ctrl+0)"));

        core.openProject(core.createProject("Atlas", "team", "Agile", "idea"));
        sidebar.refresh();
        QCOMPARE(name->text(), QString("Atlas"));
        QCOMPARE(switcher->accessibleName(), QString("Projects: Atlas"));
        // The name label is mouse-transparent, so the button carries the tooltip with the full name.
        QCOMPARE(switcher->toolTip(), QString("Atlas: open Projects Hub (Ctrl+0)"));
        QCOMPARE(name->textFormat(), Qt::PlainText);   // a name is never read as markup

        // A long name is shortened on screen; the accessible name keeps all of it.
        const QString longName = "A very long project name that cannot fit in the sidebar";
        sidebar.setProjectName(longName);
        QVERIFY(name->text() != longName);
        QVERIFY(name->text().endsWith(QChar(0x2026)));
        QCOMPARE(switcher->accessibleName(), "Projects: " + longName);
        QCOMPARE(switcher->toolTip(), longName + ": open Projects Hub (Ctrl+0)");

        QSignalSpy spy(&sidebar, &Sidebar::pageRequested);
        QTest::mouseClick(switcher, Qt::LeftButton);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).value<Page>(), Page::Hub);

        sidebar.setActivePage(Page::Hub);
        QVERIFY(switcher->property("active").toBool());
        sidebar.setActivePage(Page::Board);
        QVERIFY(!switcher->property("active").toBool());
    }

    void itemsNeedAnOpenProject()
    {
        QTemporaryDir dir;
        PlanovaCore core(dir.path().toStdString());
        Sidebar sidebar(core);
        const auto items = sidebar.findChildren<QPushButton*>("navItem");
        QCOMPARE(items.size(), 7);
        for (auto* item : items) {
            QVERIFY(!item->isEnabled());
            QCOMPARE(item->toolTip(), QString("Open a project first"));
        }
        // The switcher stays usable: it is the way to open a project.
        QVERIFY(sidebar.findChild<QPushButton*>("projectSwitcher")->isEnabled());

        core.openProject(core.createProject("A", "team", "Agile", "idea"));
        sidebar.refresh();
        for (int i = 0; i < items.size(); ++i) {
            QVERIFY(items[i]->isEnabled());
            QCOMPARE(items[i]->toolTip(),
                     QString("%1 (Ctrl+%2)").arg(items[i]->findChild<QLabel*>("navLabel")->text()).arg(i + 1));
        }

        sidebar.setProjectOpen(false);
        for (auto* item : items)
            QVERIFY(!item->isEnabled());
    }

    // Disabled labels are muted; once a project opens, the active item shows the active text colour.
    void itemColoursFollowProjectState()
    {
        QTemporaryDir dir;
        PlanovaCore core(dir.path().toStdString());
        Sidebar sidebar(core);
        sidebar.setActivePage(Page::Home);
        sidebar.show();
        QVERIFY(QTest::qWaitForWindowExposed(&sidebar));
        auto textColour = [&](const QString& text) {
            auto* label = itemWithLabel(sidebar, text)->findChild<QLabel*>("navLabel");
            return label->palette().color(label->foregroundRole());
        };
        QCOMPARE(textColour("Phase Board"), ThemeManager::instance().color(Token::TextMuted));

        core.openProject(core.createProject("A", "team", "Agile", "idea"));
        sidebar.refresh();
        QCOMPARE(textColour("Overview"), ThemeManager::instance().color(Token::NavActiveText));
        QVERIFY(textColour("Phase Board") != ThemeManager::instance().color(Token::TextMuted));
    }

    void gitFooterIsOneLine()
    {
        QTemporaryDir dir;
        PlanovaCore core(dir.path().toStdString());
        core.openProject(core.createProject("A", "team", "Agile", "idea"));
        Sidebar sidebar(core);
        auto* text = sidebar.findChild<QLabel*>("SidebarGitText");
        QVERIFY(text);
        QCOMPARE(text->text(), QString("No repository"));
        QCOMPARE(text->textFormat(), Qt::PlainText);   // the branch name is never read as markup

        core.git().connect("https://github.com/acme/portal.git", "main");
        sidebar.refresh();
        QCOMPARE(text->text(), QString("Synced · main (0)"));
    }

    void badgesFollowCounts()
    {
        QTemporaryDir dir;
        PlanovaCore core(dir.path().toStdString());
        core.openProject(core.createProject("A", "team", "Agile", "idea"));
        Sidebar sidebar(core);
        QPushButton* pins = itemWithLabel(sidebar, "Pins");
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

        QTest::mouseClick(sidebar.findChild<QPushButton*>("projectSwitcher"), Qt::LeftButton);
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

        // A page without a sidebar item (Setup) leaves no item active.
        sidebar.setActivePage(Page::Setup);
        for (auto* button : sidebar.findChildren<QPushButton*>("navItem"))
            QVERIFY(!button->property("active").toBool());
    }

    void focusRingVisibleOnActiveItem()
    {
        QTemporaryDir dir;
        PlanovaCore core(dir.path().toStdString());
        // Items are disabled without a project, and a disabled item takes no focus.
        core.openProject(core.createProject("A", "team", "Agile", "idea"));
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
        QCOMPARE(toggle->toolTip(), QString("Switch to dark mode (Ctrl+Shift+L)"));

        QTest::mouseClick(toggle, Qt::LeftButton);
        QCOMPARE(ThemeManager::instance().mode(), ThemeMode::Dark);
        QCOMPARE(toggle->toolTip(), QString("Switch to light mode (Ctrl+Shift+L)"));

        QTest::mouseClick(toggle, Qt::LeftButton);
        QCOMPARE(ThemeManager::instance().mode(), ThemeMode::Light);
        QCOMPARE(toggle->toolTip(), QString("Switch to dark mode (Ctrl+Shift+L)"));
    }
};

QTEST_MAIN(tst_Sidebar)
#include "tst_Sidebar.moc"
