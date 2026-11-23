#include "TestAppSetup.h"
#include "FocusRingSupport.h"

#include "app/CoreBridge.h"
#include "planova/PlanovaCore.h"
#include "theme/ThemeManager.h"
#include "welcome/ProjectCardWidget.h"
#include "welcome/WelcomePage.h"

#include <QApplication>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QMenu>
#include <QSet>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>

namespace {
// A workspace with the given project names (none is opened) and a shown page.
struct Fixture {
    QTemporaryDir dir;
    PlanovaCore core{dir.path().toStdString()};
    CoreBridge bridge{core};
    std::vector<Id> ids;
    std::unique_ptr<WelcomePage> page;

    explicit Fixture(const QStringList& names = {"Alpha", "Beta"})
    {
        for (const QString& name : names)
            ids.push_back(core.createProject(name.toStdString(), (name + " Team").toStdString(), "Agile", ""));
        page = std::make_unique<WelcomePage>(core, bridge);
        page->resize(1000, 700);
        page->show();
    }

    QList<ProjectCardWidget*> visibleCards() const
    {
        QList<ProjectCardWidget*> result;
        for (auto* card : page->findChildren<ProjectCardWidget*>()) {
            if (card->isVisible())
                result.append(card);
        }
        return result;
    }

    QPushButton* button(const char* name) const { return page->findChild<QPushButton*>(name); }
};
} // namespace

class tst_WelcomePage : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { Q_INIT_RESOURCE(planova);
        installAppFonts(); }
    void init() { ThemeManager::instance().apply(ThemeMode::Light); }
    void cleanupTestCase() { ThemeManager::instance().apply(ThemeMode::Light); }

    void showsOneCardPerProject()
    {
        Fixture f;
        QCOMPARE(f.visibleCards().size(), 2);
    }

    void cardMenuEmitsEditAndDelete()
    {
        Fixture f;
        QSignalSpy opened(f.page.get(), &WelcomePage::projectOpened);
        QSignalSpy edit(f.page.get(), &WelcomePage::editProjectRequested);
        QSignalSpy del(f.page.get(), &WelcomePage::deleteProjectRequested);

        const auto cards = f.visibleCards();
        QCOMPARE(cards.size(), 2);
        ProjectCardWidget* alpha = nullptr;
        for (auto* card : cards)
            if (card->projectName() == "Alpha")
                alpha = card;
        QVERIFY(alpha);
        auto* more = alpha->findChild<QToolButton*>("projectMoreButton");
        QVERIFY(more);
        QCOMPARE(more->accessibleName(), QString("More actions for Alpha"));
        QCOMPARE(more->toolTip(), QString("More actions"));
        QVERIFY(more->width() >= 24 && more->height() >= 24);
        QVERIFY(more->focusPolicy() & Qt::TabFocus);

        QVERIFY(more->menu());
        const auto actions = more->menu()->actions();
        QCOMPARE(actions.size(), 2);
        QCOMPARE(actions[0]->text(), QString("Edit details"));
        QCOMPARE(actions[1]->text(), QString("Delete project"));

        actions[0]->trigger();
        QCOMPARE(edit.count(), 0);   // queued: nothing is emitted inside the menu action
        QTRY_COMPARE(edit.count(), 1);
        QCOMPARE(edit.at(0).at(0).toString(), QString::fromStdString(f.ids[0]));
        actions[1]->trigger();
        QTRY_COMPARE(del.count(), 1);
        QCOMPARE(del.at(0).at(0).toString(), QString::fromStdString(f.ids[0]));
        QCOMPARE(opened.count(), 0);
    }

    void cardMenuReachableByTab()
    {
        Fixture f;
        auto* card = f.visibleCards().first();
        auto* open = card->findChild<QPushButton*>("openProject");
        auto* more = card->findChild<QToolButton*>("projectMoreButton");
        QVERIFY(open && more);
        // Walk the focus chain once around from the Open button; both buttons must be in it.
        QSet<QWidget*> chain;
        for (QWidget* w = open->nextInFocusChain(); w && w != open; w = w->nextInFocusChain())
            if (w->focusPolicy() & Qt::TabFocus)
                chain.insert(w);
        QVERIFY(chain.contains(more));
    }

    void startNewProjectEmitsSignal()
    {
        Fixture f;
        QSignalSpy spy(f.page.get(), &WelcomePage::startNewProject);
        f.button("createProject")->click();
        QCOMPARE(spy.count(), 1);
    }

    void searchFilters()
    {
        Fixture f;
        auto* search = f.page->findChild<QLineEdit*>("projectSearch");
        QVERIFY(search);
        QCOMPARE(f.visibleCards().size(), 2);
        QTest::keyClicks(search, "alp");
        QCOMPARE(f.visibleCards().size(), 1);
        QCOMPARE(f.visibleCards().first()->projectName(), QString("Alpha"));
    }

    void searchMatchesTeamCaseInsensitive()
    {
        Fixture f;
        auto* search = f.page->findChild<QLineEdit*>("projectSearch");
        QTest::keyClicks(search, "BETA TEAM");
        QCOMPARE(f.visibleCards().size(), 1);
        QCOMPARE(f.visibleCards().first()->projectName(), QString("Beta"));
    }

    void escapeClearsSearch()
    {
        Fixture f;
        auto* search = f.page->findChild<QLineEdit*>("projectSearch");
        search->setFocus();
        QTest::keyClicks(search, "alp");
        QCOMPARE(f.visibleCards().size(), 1);
        QTest::keyClick(search, Qt::Key_Escape);
        QVERIFY(search->text().isEmpty());
        QCOMPARE(f.visibleCards().size(), 2);
    }

    void noMatchShowsMessageAndClear()
    {
        Fixture f;
        auto* search = f.page->findChild<QLineEdit*>("projectSearch");
        QTest::keyClicks(search, "zzz");
        QVERIFY(f.visibleCards().isEmpty());
        auto* message = f.page->findChild<QLabel*>("workspaceState");
        QVERIFY(message->isVisible());
        QCOMPARE(message->text(), QString("No projects match 'zzz'"));
        QVERIFY(!f.button("emptyStartNew")->isVisible());

        f.button("clearSearch")->click();
        QVERIFY(search->text().isEmpty());
        QCOMPARE(f.visibleCards().size(), 2);
        QVERIFY(!message->isVisible());
    }

    void emptyWorkspaceShowsMessageAndStartButton()
    {
        Fixture f(QStringList{});
        QVERIFY(f.visibleCards().isEmpty());
        QVERIFY(f.page->findChild<QLabel*>("workspaceState")->isVisible());
        QVERIFY(!f.page->findChild<QLabel*>("workspaceState")->text().isEmpty());
        QVERIFY(!f.button("clearSearch")->isVisible());

        QSignalSpy spy(f.page.get(), &WelcomePage::startNewProject);
        f.button("emptyStartNew")->click();
        QCOMPARE(spy.count(), 1);
    }

    void openEmitsAndOpensCore()
    {
        Fixture f;
        QSignalSpy opened(f.page.get(), &WelcomePage::projectOpened);

        ProjectCardWidget* beta = nullptr;
        for (auto* card : f.visibleCards()) {
            if (card->projectName() == "Beta")
                beta = card;
        }
        QVERIFY(beta);
        auto* open = beta->findChild<QPushButton*>("openProject");
        QVERIFY(open);
        open->click();

        QCOMPARE(opened.count(), 1);
        QVERIFY(f.core.currentProject().has_value());
        QCOMPARE(*f.core.currentProject(), f.ids[1]);
    }

    void openingProjectNotifiesBridge()
    {
        // Sidebar badges refresh from CoreBridge::changed, so opening a project must raise it.
        Fixture f;
        QSignalSpy changed(&f.bridge, &CoreBridge::changed);
        f.core.openProject(f.ids[0]);
        QVERIFY(changed.count() >= 1);
    }

    void cardsHaveAccessibleNames()
    {
        Fixture f;
        QStringList names;
        for (auto* card : f.visibleCards())
            names << card->accessibleName();
        QVERIFY(names.contains("Alpha, Alpha Team, Agile"));
    }

    void newProjectAppearsThroughBridge()
    {
        Fixture f;
        f.core.createProject("Gamma", "G", "Waterfall", "");
        QCOMPARE(f.page->findChildren<ProjectCardWidget*>().size(), 3);
    }

    void bannerShowsNoRepository()
    {
        Fixture f;
        QCOMPARE(f.page->findChild<QLabel*>("gitBannerRepo")->text(), QString("No repository connected"));
        f.core.openProject(f.ids[0]);
        QSignalSpy spy(f.page.get(), &WelcomePage::manageGitRequested);
        f.button("manageGit")->click();
        QCOMPARE(spy.count(), 1);
    }

    // The GitHub page belongs to a project, so "Manage Git" waits until one is open and says why.
    void manageGitNeedsAnOpenProject()
    {
        Fixture f;
        QPushButton* manage = f.button("manageGit");
        QVERIFY(manage);
        QVERIFY(!manage->isEnabled());
        QCOMPARE(manage->toolTip(), QString("Open a project first"));
        QSignalSpy spy(f.page.get(), &WelcomePage::manageGitRequested);
        manage->click();
        QCOMPARE(spy.count(), 0);

        f.core.openProject(f.ids[1]);   // the page refreshes on CoreBridge::changed
        QVERIFY(manage->isEnabled());
        QVERIFY(manage->toolTip() != QString("Open a project first"));
        manage->click();
        QCOMPARE(spy.count(), 1);
    }


    void buttonsKeepTheirFocusRing_data()
    {
        QTest::addColumn<bool>("dark");
        QTest::newRow("light") << false;
        QTest::newRow("dark") << true;
    }

    // Keyboard focus is a 2 px TextPrimary ring that must differ from the button fill (Dark Accent equals TextPrimary,
    // so an Accent-filled button swaps its fill). The [kind] rules must not recolour it. Hover and pressed together with
    // focus are covered by the qss order, see tst_FocusRings.
    void buttonsKeepTheirFocusRing()
    {
        QFETCH(bool, dark);
        ThemeManager::instance().apply(dark ? ThemeMode::Dark : ThemeMode::Light);
        Fixture f;
        f.core.openProject(f.ids[0]);   // "Manage Git" is enabled only with an open project
        f.page->activateWindow();
        QVERIFY(QTest::qWaitForWindowExposed(f.page.get()));

        auto expectRing = [&](const char* name) {
            QPushButton* button = f.button(name);
            QVERIFY2(button && button->isVisible(), name);
            const focusring::Result r = focusring::check(button, focusring::Shape::Box, true);
            if (!r.ok && r.why == "could not take focus")
                QSKIP("window not active in this environment, focus state cannot be rendered");
            QVERIFY2(r.ok, qPrintable(QString("%1: %2").arg(name, r.why)));
        };

        expectRing("createProject");     // primary
        expectRing("manageGit");         // ghost
        expectRing("openProject");       // Accent fill, no kind
    }

    void rendersInBothThemes()
    {
        Fixture f;
        for (ThemeMode mode : {ThemeMode::Light, ThemeMode::Dark}) {
            ThemeManager::instance().apply(mode);
            QApplication::processEvents();
            QVERIFY(!f.page->grab().isNull());
        }
    }
};

QTEST_MAIN(tst_WelcomePage)
#include "tst_WelcomePage.moc"
