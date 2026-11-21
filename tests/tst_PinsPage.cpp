#include "TestAppSetup.h"
#include "pins/PinCardWidget.h"
#include "pins/PinsPage.h"
#include "planova/PlanovaCore.h"
#include "theme/ThemeManager.h"

#include <QApplication>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTest>

namespace {
// A page needs a project, two members and one pin each.
struct Fixture {
    QTemporaryDir dir;
    PlanovaCore core{dir.path().toStdString()};
    Id ana;
    Id ben;

    Fixture()
    {
        core.openProject(core.createProject("A", "team", "Agile", "idea"));
        ana = core.members().add("Ana", "Backend Developer", false);
        ben = core.members().add("Ben", "Frontend Developer", false);
        core.pins().create(PinCategory::HandoffNote, "Ana note", {}, ana, std::nullopt);
        core.pins().create(PinCategory::FeedbackBlocker, "Ben blocker", {}, ben, std::nullopt);
    }

    planova::s5::PinsPage* makePage()
    {
        return new planova::s5::PinsPage(core.pinBoard(), core.pins(), core.memberQuery(), core.taskQuery());
    }
};

QPushButton* chipFor(QWidget* page, const Id& memberId)
{
    for (QPushButton* b : page->findChildren<QPushButton*>("PinFilterChip"))
        if (b->property("memberId").toString() == QString::fromStdString(memberId)) return b;
    return nullptr;
}
QPushButton* chipByText(QWidget* page, const QString& prefix)
{
    for (QPushButton* b : page->findChildren<QPushButton*>("PinFilterChip"))
        if (b->text().startsWith(prefix)) return b;
    return nullptr;
}
}  // namespace

class tst_PinsPage : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { installAppFonts(); }
    void init() { ThemeManager::instance().apply(ThemeMode::Light); }

    void filterClearsWhenMemberGone()
    {
        Fixture f;
        std::unique_ptr<planova::s5::PinsPage> page(f.makePage());
        page->resize(1200, 800);
        page->show();
        QVERIFY(QTest::qWaitForWindowExposed(page.get()));

        QPushButton* anaChip = chipByText(page.get(), "Ana");
        QVERIFY(anaChip);
        anaChip->click();
        QTest::qWait(50);   // the filter is applied on the next event loop turn
        QVERIFY(chipByText(page.get(), "Ana")->isChecked());
        QCOMPARE(page->findChildren<planova::s5::PinCardWidget*>().size(), 1);

        f.core.members().remove(f.ana);
        page->refresh();
        QTest::qWait(50);   // old chips and cards are deleted later

        QPushButton* all = chipByText(page.get(), "All");
        QVERIFY(all);
        QVERIFY2(all->isChecked(), "the All chip must be checked when the filtered member is gone");
        // Both pins are visible again, including the one by the removed member.
        QCOMPARE(page->findChildren<planova::s5::PinCardWidget*>().size(), 2);
    }

    void chipKeepsFocusAfterClick()
    {
        Fixture f;
        std::unique_ptr<planova::s5::PinsPage> page(f.makePage());
        page->resize(1200, 800);
        page->show();
        page->activateWindow();
        QVERIFY(QTest::qWaitForWindowExposed(page.get()));
        const bool active = QTest::qWaitForWindowActive(page.get());   // false when the platform cannot activate

        QPushButton* ben = chipFor(page.get(), f.ben);
        QVERIFY(ben);
        ben->setFocus(Qt::TabFocusReason);
        if (!active && !ben->hasFocus()) QSKIP("window cannot be activated in this environment, focus cannot be checked");
        ben->click();
        QTest::qWait(50);

        QPushButton* now = chipFor(page.get(), f.ben);
        QVERIFY(now);
        QVERIFY2(now != ben, "the chips were not rebuilt, the test proves nothing");
        QVERIFY(now->isChecked());
        QWidget* focused = QApplication::focusWidget();
        QVERIFY2(focused, "focus was lost after the click");
        QCOMPARE(focused, static_cast<QWidget*>(now));
        QCOMPARE(focused->property("memberId").toString(), QString::fromStdString(f.ben));
    }
};

QTEST_MAIN(tst_PinsPage)
#include "tst_PinsPage.moc"
