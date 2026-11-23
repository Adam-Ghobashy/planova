#include "TestAppSetup.h"
#include "FocusRingSupport.h"

#include "planova/PlanovaCore.h"
#include "slicer/WeightSlidersPanel.h"
#include "theme/ThemeManager.h"
#include "welcome/SetupPage.h"
#include "welcome/StepperWidget.h"

#include <QApplication>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QSlider>
#include <QTemporaryDir>
#include <QTest>

namespace {

struct Fixture {
    QTemporaryDir dir;
    PlanovaCore core{dir.path().toStdString()};
    SetupPage page{core};

    Fixture()
    {
        page.resize(1100, 760);
        page.show();
        page.reset();
    }

    QPushButton* button(const char* name) { return page.findChild<QPushButton*>(name); }
    QLineEdit* edit(const char* name) { return page.findChild<QLineEdit*>(name); }
    QLabel* label(const char* name) { return page.findChild<QLabel*>(name); }

    void clickNext(int times = 1)
    {
        for (int i = 0; i < times; ++i)
            button("nextButton")->click();
    }
    void addMember(const QString& name)
    {
        button("addMemberButton")->click();
        // the newest row has the highest id; ids start at 0 after reset()
        const auto edits = page.findChildren<QLineEdit*>(QRegularExpression("^memberNameEdit_"));
        edits.last()->setText(name);
    }
};

} // namespace

class tst_SetupPage : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { Q_INIT_RESOURCE(planova);
        installAppFonts(); }
    void init() { ThemeManager::instance().apply(ThemeMode::Light); }
    void cleanupTestCase() { ThemeManager::instance().apply(ThemeMode::Light); }

    void showsHeaderAndSixSteps()
    {
        Fixture f;
        QCOMPARE(f.label("setupTitle")->text(), QString("Guided Project Setup"));
        QCOMPARE(f.label("setupSubtitle")->text(),
                 QString("Configure lifecycle, skill matrix, slicing policy, and repository integration."));
        QCOMPARE(f.button("skipButton")->text(), QString("Skip to Dashboard"));

        const QStringList titles = {"Project Foundations", "Process Lifecycle Model", "Team & Skill Matrix",
                                    "Chaos Slicer Balancing Policy", "Git & Peer Event Synchronization",
                                    "Launch & Final Review"};
        f.edit("projectNameEdit")->setText("Portal");
        for (int step = 0; step < titles.size(); ++step) {
            QCOMPARE(f.label("stepTitle")->text(), titles[step]);
            QCOMPARE(f.page.findChild<StepperWidget*>()->current(), step);
            QCOMPARE(f.button("backButton")->isEnabled(), step > 0);
            QCOMPARE(f.button("nextButton")->isVisible(), step < titles.size() - 1);
            QCOMPARE(f.button("createButton")->isVisible(), step == titles.size() - 1);
            if (step < titles.size() - 1)
                f.clickNext();
        }
    }

    void nextDisabledUntilName()
    {
        Fixture f;
        QVERIFY(!f.button("nextButton")->isEnabled());
        // The reason is written next to the field, not only shown by the disabled button.
        QVERIFY(f.label("nameHint")->isVisible());
        QVERIFY(!f.label("nameHint")->text().isEmpty());

        f.edit("projectNameEdit")->setText("Portal");
        QVERIFY(f.button("nextButton")->isEnabled());
        QVERIFY(!f.label("nameHint")->isVisible());

        f.edit("projectNameEdit")->setText("   ");
        QVERIFY(!f.button("nextButton")->isEnabled());
    }

    void skipWritesNothing()
    {
        Fixture f;
        QSignalSpy skipped(&f.page, &SetupPage::skipped);
        QSignalSpy created(&f.page, &SetupPage::projectCreated);
        f.edit("projectNameEdit")->setText("Portal");
        f.button("skipButton")->click();

        QCOMPARE(skipped.count(), 1);
        QCOMPARE(created.count(), 0);
        QVERIFY(f.core.projects().list().empty());
        QVERIFY(!f.core.currentProject().has_value());
    }

    // The "Team lead" toggle gets wider with its check mark; it must not change the width of the other fields.
    void fieldsKeepTheirWidthWhenTeamLeadIsToggled()
    {
        Fixture f;
        f.edit("projectNameEdit")->setText("Portal");
        f.clickNext(2);
        f.addMember("Ana");
        f.addMember("Ben");
        auto* lead = f.button("memberLead_0");
        auto* other = f.button("memberLead_1");
        QVERIFY(lead && other);
        lead->click();
        QVERIFY(lead->isChecked());
        QVERIFY(!other->isChecked());
        QCoreApplication::processEvents();
        QCOMPARE(lead->width(), other->width());
        QCOMPARE(f.edit("memberNameEdit_0")->width(), f.edit("memberNameEdit_1")->width());
        // The checked text fits the button.
        QVERIFY(lead->fontMetrics().horizontalAdvance(lead->text()) < lead->width());
    }

    void navigatingWritesNothing()
    {
        Fixture f;
        f.edit("projectNameEdit")->setText("Portal");
        f.clickNext(2);
        f.addMember("Ana");
        f.clickNext(3);
        for (int i = 0; i < 5; ++i)
            f.button("backButton")->click();
        QVERIFY(f.core.projects().list().empty());
        QVERIFY(!f.core.currentProject().has_value());
    }

    void createEmitsAndOpens()
    {
        Fixture f;
        QSignalSpy created(&f.page, &SetupPage::projectCreated);
        f.edit("projectNameEdit")->setText("Portal");
        f.edit("teamNameEdit")->setText("Team Six");
        f.page.findChild<QPlainTextEdit*>("ideaEdit")->setPlainText("Book loans");
        f.clickNext(2); // to the team step
        f.addMember("Ana");
        f.page.findChild<QSlider*>("skillSlider_0_0")->setValue(5);
        f.clickNext(3); // slicer, git, review

        QVERIFY(f.button("createButton")->isEnabled());
        f.button("createButton")->click();

        QCOMPARE(created.count(), 1);
        QVERIFY(f.core.currentProject().has_value());
        QCOMPARE(f.core.projects().list().size(), size_t(1));
        QCOMPARE(f.core.projects().list().front().name, std::string("Portal"));
        QCOMPARE(f.core.projects().list().front().team, std::string("Team Six"));
        QCOMPARE(f.core.projects().list().front().seModel, std::string("Agile"));
        QCOMPARE(f.core.memberQuery().all().size(), size_t(1));
        QCOMPARE(f.core.memberQuery().all().front().ratings.at(SkillCategory::Backend), 5);
        QVERIFY(!f.core.gitProjection().isConnected());
    }

    void chosenModelWeightsAndRepoAreSaved()
    {
        Fixture f;
        f.edit("projectNameEdit")->setText("Portal");
        f.clickNext();
        f.button("modelCard_Spiral")->click();
        f.clickNext();
        f.addMember("Ana");
        f.clickNext();
        f.page.findChild<planova::slicer::WeightSlidersPanel*>()->setWeights({60, 20, 20});
        f.clickNext();
        f.edit("repoUrlEdit")->setText("https://github.com/team/portal.git");
        f.clickNext();
        f.button("createButton")->click();

        QCOMPARE(f.core.projects().list().front().seModel, std::string("Spiral"));
        QVERIFY(f.core.slicer().savedWeights() == (planova::slicer::ScoringWeights{60, 20, 20}));
        QVERIFY(f.core.gitProjection().isConnected());
        QCOMPARE(f.core.gitProjection().branch(), std::string("main"));
    }

    void createBlockedWithoutMember()
    {
        Fixture f;
        QSignalSpy created(&f.page, &SetupPage::projectCreated);
        f.edit("projectNameEdit")->setText("Portal");
        f.clickNext(5); // straight to the review

        QVERIFY(f.button("createButton")->isVisible());
        QVERIFY(!f.button("createButton")->isEnabled());
        // The reason is written on the page and a button leads back to the team step.
        QVERIFY(f.label("createProblem")->isVisible());
        QVERIFY(f.label("createProblem")->text().contains("team member"));
        f.button("createButton")->click();
        QCOMPARE(created.count(), 0);
        QVERIFY(f.core.projects().list().empty());

        f.button("fixMembersButton")->click();
        QCOMPARE(f.label("stepTitle")->text(), QString("Team & Skill Matrix"));
    }

    void memberWithoutNameBlocksCreate()
    {
        Fixture f;
        f.edit("projectNameEdit")->setText("Portal");
        f.clickNext(2);
        f.button("addMemberButton")->click();   // a row with no name
        f.clickNext(3);
        QVERIFY(!f.button("createButton")->isEnabled());
        QVERIFY(f.label("createProblem")->isVisible());
    }

    void removingTheMemberBlocksCreateAgain()
    {
        Fixture f;
        f.edit("projectNameEdit")->setText("Portal");
        f.clickNext(2);
        f.addMember("Ana");
        f.button("memberRemove_0")->click();
        QVERIFY(f.page.findChildren<QLineEdit*>(QRegularExpression("^memberNameEdit_")).isEmpty());
        f.clickNext(3);
        QVERIFY(!f.button("createButton")->isEnabled());
    }

    void backKeepsEnteredValues()
    {
        Fixture f;
        f.edit("projectNameEdit")->setText("Portal");
        f.edit("teamNameEdit")->setText("Team Six");
        f.clickNext();
        f.button("modelCard_Waterfall")->click();
        f.clickNext();
        f.addMember("Ana");
        f.clickNext();

        // back from the slicer step to the process model step
        for (int i = 0; i < 2; ++i)
            f.button("backButton")->click();
        QCOMPARE(f.label("stepTitle")->text(), QString("Process Lifecycle Model"));
        QVERIFY(f.button("modelCard_Waterfall")->isChecked());
        f.button("backButton")->click();
        QCOMPARE(f.edit("projectNameEdit")->text(), QString("Portal"));
        QCOMPARE(f.edit("teamNameEdit")->text(), QString("Team Six"));

        f.clickNext(2);
        QCOMPARE(f.edit("memberNameEdit_0")->text(), QString("Ana"));
        QVERIFY(f.core.projects().list().empty());
    }

    void modelCardsAreExclusiveAndAgileIsDefault()
    {
        Fixture f;
        QVERIFY(f.button("modelCard_Agile")->isChecked());
        f.button("modelCard_Scrum")->click();
        QVERIFY(f.button("modelCard_Scrum")->isChecked());
        QVERIFY(!f.button("modelCard_Agile")->isChecked());
        // Clicking the chosen card again keeps it chosen.
        f.button("modelCard_Scrum")->click();
        QVERIFY(f.button("modelCard_Scrum")->isChecked());
    }

    void resetClearsEverything()
    {
        Fixture f;
        f.edit("projectNameEdit")->setText("Portal");
        f.clickNext();
        f.button("modelCard_Spiral")->click();
        f.clickNext();
        f.addMember("Ana");
        f.clickNext();
        f.page.findChild<planova::slicer::WeightSlidersPanel*>()->setWeights({10, 10, 80});

        f.page.reset();
        QCOMPARE(f.label("stepTitle")->text(), QString("Project Foundations"));
        QVERIFY(f.edit("projectNameEdit")->text().isEmpty());
        QVERIFY(f.button("modelCard_Agile")->isChecked());
        QVERIFY(f.page.findChildren<QLineEdit*>(QRegularExpression("^memberNameEdit_")).isEmpty());
        QVERIFY(f.page.findChild<planova::slicer::WeightSlidersPanel*>()->weights()
                == planova::slicer::ScoringWeights{});
        QVERIFY(!f.button("nextButton")->isEnabled());
    }

    void enterOnTheLastStepDoesNotCreate()
    {
        Fixture f;
        QSignalSpy created(&f.page, &SetupPage::projectCreated);
        f.edit("projectNameEdit")->setText("Portal");
        f.clickNext();
        f.clickNext();
        f.addMember("Ana");
        f.clickNext(3);
        QTest::keyClick(&f.page, Qt::Key_Return);
        QTest::keyClick(&f.page, Qt::Key_Escape);
        QCOMPARE(created.count(), 0);
        QVERIFY(f.core.projects().list().empty());
    }

    void primaryButtonsKeepTheirFocusRing_data()
    {
        QTest::addColumn<bool>("dark");
        QTest::newRow("light") << false;
        QTest::newRow("dark") << true;
    }

    // The focus ring is a 2 px TextPrimary border that must differ from the button fill (Dark Accent equals TextPrimary,
    // so an Accent-filled button swaps its fill). A later [kind] rule must not recolour it.
    void primaryButtonsKeepTheirFocusRing()
    {
        QFETCH(bool, dark);
        ThemeManager::instance().apply(dark ? ThemeMode::Dark : ThemeMode::Light);
        Fixture f;
        f.edit("projectNameEdit")->setText("Portal");
        f.page.activateWindow();
        QVERIFY(QTest::qWaitForWindowExposed(&f.page));

        auto expectRing = [&](QPushButton* button) {
            QVERIFY2(button && button->isEnabled(), "button missing or disabled");
            const focusring::Result r = focusring::check(button, focusring::Shape::Box, true);
            if (!r.ok && r.why == "could not take focus")
                QSKIP("window not active in this environment, focus state cannot be rendered");
            QVERIFY2(r.ok, qPrintable(QString("%1: %2").arg(button->objectName(), r.why)));
        };

        expectRing(f.button("nextButton"));      // primary
        expectRing(f.button("skipButton"));      // secondary
        f.clickNext();
        expectRing(f.button("backButton"));      // secondary
        f.clickNext();
        expectRing(f.button("addMemberButton")); // secondary
        f.addMember("Ana");
        expectRing(f.button("memberRemove_0"));  // ghost
        f.clickNext(3);
        expectRing(f.button("createButton"));    // primary
    }
};

QTEST_MAIN(tst_SetupPage)
#include "tst_SetupPage.moc"
