// Keyboard focus rings, both themes. The Dark theme is the dangerous one: Accent and TextPrimary are the same
// near-white there, so a TextPrimary ring on an Accent fill is invisible. Every check renders the real widget with
// and without keyboard focus (see FocusRingSupport.h) and compares the ring with the fill.
//
// Not covered: hover and pressed together with focus. QTest::mouseMove does not make underMouse() true in this
// test environment, so those combined states are covered by qss order and specificity instead (focus rules come
// after the hover and pressed rules, or list the combined states).
#include "TestAppSetup.h"
#include "FocusRingSupport.h"

#include "app/CoreBridge.h"
#include "app/DemoData.h"
#include "app/MainWindow.h"
#include "app/Page.h"
#include "app/Sidebar.h"
#include "board/NewTaskDialog.h"
#include "pins/LeavePinDialog.h"
#include "planova/PlanovaCore.h"
#include "theme/ThemeManager.h"
#include "welcome/SetupPage.h"
#include "welcome/WelcomePage.h"
#include "widgets/ModalDialog.h"

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QAbstractSlider>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QPushButton>
#include <QStackedWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QVBoxLayout>

namespace {

// Every focusable, visible, enabled control under `root`, at most three of each kind (task cards, pin chips and
// member rows repeat). Edits inside a combo box or spin box are skipped: the combo box or spin box is checked.
QList<QWidget*> focusableControls(QWidget* root)
{
    QList<QWidget*> result;
    QMap<QString, int> perKind;
    const auto all = root->findChildren<QWidget*>();
    for (QWidget* w : all) {
        if (!w->isVisible() || !w->isEnabled() || !(w->focusPolicy() & Qt::TabFocus))
            continue;
        if (w->width() < 8 || w->height() < 8)
            continue;
        if (qobject_cast<QAbstractSpinBox*>(w->parentWidget()) || qobject_cast<QComboBox*>(w->parentWidget()))
            continue;
        const QString kind = QString("%1#%2").arg(w->metaObject()->className(), w->objectName());
        if (++perKind[kind] > 3)
            continue;
        result.append(w);
    }
    return result;
}

focusring::Shape shapeOf(QWidget* w)
{
    if (qobject_cast<QAbstractSlider*>(w) || qobject_cast<QCheckBox*>(w) || qobject_cast<QAbstractItemView*>(w))
        return focusring::Shape::Indicator;
    return focusring::Shape::Box;
}

// The ring of the focused, active sidebar item is TextPrimary: 15:1 against the sidebar it touches on the outside, but
// only 2.5:1 against the violet active fill in Light (no single token reaches 3:1 against both the fill and the
// sidebar there; see the comment in base.qss). Every other control must reach 3:1 against its fill.
double minFillContrast(QWidget* w)
{
    return (w->objectName() == "navItem" && w->property("active").toBool()) ? 2.4 : 3.0;
}

QString describe(QWidget* w)
{
    return QString("%1#%2 \"%3\"")
        .arg(w->metaObject()->className(), w->objectName(),
             qobject_cast<QAbstractButton*>(w) ? qobject_cast<QAbstractButton*>(w)->text() : QString());
}


} // namespace

class tst_FocusRings : public QObject {
    Q_OBJECT

    // Checks every control under `root` and appends one line per failure.
    void sweep(QWidget* root, const QString& where, QStringList& failures, int& checked)
    {
        for (QWidget* w : focusableControls(root)) {
            const focusring::Result r = focusring::check(w, shapeOf(w), false, minFillContrast(w));
            ++checked;
            if (!r.ok)
                failures << QString("%1: %2: %3").arg(where, describe(w), r.why);
        }
    }

    void sweepNamed(QWidget* root, const char* name, const QString& where, bool outside, QStringList& failures,
                    int& checked)
    {
        const auto matches = root->findChildren<QWidget*>(name);
        if (matches.isEmpty()) {
            failures << QString("%1: control %2 not found").arg(where, name);
            return;
        }
        for (QWidget* w : matches) {
            if (!w->isVisible())
                continue;
            const focusring::Result r = focusring::check(w, shapeOf(w), outside, minFillContrast(w));
            ++checked;
            if (!r.ok)
                failures << QString("%1: %2: %3").arg(where, describe(w), r.why);
            return;
        }
        failures << QString("%1: control %2 is not visible").arg(where, name);
    }

private slots:
    void initTestCase()
    {
        Q_INIT_RESOURCE(planova);
        installAppFonts();
        QCoreApplication::setOrganizationName("PlanovaTest");
    }
    void init() { ThemeManager::instance().apply(ThemeMode::Light); }
    // Restores Light even when a test failed half way.
    void cleanup() { ThemeManager::instance().apply(ThemeMode::Light); }

    void everyPageControlShowsARing_data()
    {
        QTest::addColumn<bool>("dark");
        QTest::newRow("light") << false;
        QTest::newRow("dark") << true;
    }

    // Walks every page of the main window (demo data), the Projects Hub and every Setup step.
    void everyPageControlShowsARing()
    {
        QFETCH(bool, dark);
        ThemeManager::instance().apply(dark ? ThemeMode::Dark : ThemeMode::Light);

        QTemporaryDir dir;
        PlanovaCore core(dir.path().toStdString());
        core.openProject(core.createProject("Planova", "Team Six", "Agile", ""));
        core.createProject("Library Portal", "ECU Team", "Scrum", "");
        demo::seedIfEmpty(core);
        CoreBridge bridge(core);
        MainWindow window(core, bridge);
        window.resize(1440, 900);
        window.show();
        window.activateWindow();
        QVERIFY(QTest::qWaitForWindowExposed(&window));

        auto* pages = window.findChild<QStackedWidget*>();
        QVERIFY(pages);
        QStringList failures;
        int checked = 0;

        for (int i = 0; i < pages->count(); ++i) {
            if (i == static_cast<int>(Page::Setup))
                continue; // Setup is walked step by step below
            pages->setCurrentIndex(i);
            QApplication::processEvents();
            QTest::qWait(300);   // let the page lay out and paint before its controls are checked
            sweep(&window, QString("page %1").arg(i), failures, checked);
        }

        auto* setup = window.findChild<SetupPage*>();
        QVERIFY(setup);
        pages->setCurrentWidget(setup);
        setup->reset();
        QApplication::processEvents();
        setup->findChild<QLineEdit*>("projectNameEdit")->setText("Portal");
        for (int step = 0; step < 6; ++step) {
            sweep(setup, QString("setup step %1").arg(step), failures, checked);
            if (step == 2) { // the team step: add a member so the row controls exist
                setup->findChild<QPushButton*>("addMemberButton")->click();
                QApplication::processEvents();
                sweep(setup, "setup team with a member", failures, checked);
            }
            if (step == 2) {
                const auto edits = setup->findChildren<QLineEdit*>(QRegularExpression("^memberNameEdit_"));
                edits.last()->setText("Ana");
            }
            if (auto* next = setup->findChild<QPushButton*>("nextButton"); next && next->isVisible() && next->isEnabled())
                next->click();
            QApplication::processEvents();
        }

        QVERIFY2(checked > 40, qPrintable(QString("only %1 controls found, the sweep is not reaching the pages").arg(checked)));
        QVERIFY2(failures.isEmpty(), qPrintable(QString("%1 of %2 controls have no visible ring:\n%3")
                                                    .arg(failures.size())
                                                    .arg(checked)
                                                    .arg(failures.join("\n"))));
    }

    void namedControls_data() { everyPageControlShowsARing_data(); }

    // The controls named in the review, with the surroundings check as well (the ring must also stand out from
    // what is just outside the control).
    void namedControls()
    {
        QFETCH(bool, dark);
        ThemeManager::instance().apply(dark ? ThemeMode::Dark : ThemeMode::Light);

        QTemporaryDir dir;
        PlanovaCore core(dir.path().toStdString());
        core.openProject(core.createProject("Planova", "Team Six", "Agile", ""));
        core.createProject("Library Portal", "ECU Team", "Scrum", "");
        demo::seedIfEmpty(core);
        CoreBridge bridge(core);
        MainWindow window(core, bridge);
        window.resize(1440, 900);
        window.show();
        window.activateWindow();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto* pages = window.findChild<QStackedWidget*>();
        QVERIFY(pages);

        QStringList failures;
        int checked = 0;
        auto goTo = [&](Page page) {
            pages->setCurrentIndex(static_cast<int>(page));
            window.findChild<Sidebar*>()->setActivePage(page);
            QApplication::processEvents();
        };

        // Projects Hub
        goTo(Page::Hub);
        for (const char* name : {"createProject", "manageGit", "openProject"})
            sweepNamed(&window, name, "Projects Hub", true, failures, checked);

        // Setup
        auto* setup = window.findChild<SetupPage*>();
        pages->setCurrentWidget(setup);
        setup->reset();
        setup->findChild<QLineEdit*>("projectNameEdit")->setText("Portal");
        QApplication::processEvents();
        for (const char* name : {"nextButton", "skipButton"})
            sweepNamed(setup, name, "Setup step 1", true, failures, checked);
        setup->findChild<QPushButton*>("nextButton")->click();
        setup->findChild<QPushButton*>("modelCard_Scrum")->click(); // selected card, then focus on it
        QApplication::processEvents();
        sweepNamed(setup, "modelCard_Scrum", "Setup selected model card", true, failures, checked);
        sweepNamed(setup, "modelCard_Waterfall", "Setup model card", true, failures, checked);
        setup->findChild<QPushButton*>("nextButton")->click();
        setup->findChild<QPushButton*>("addMemberButton")->click();
        QApplication::processEvents();
        sweepNamed(setup, "backButton", "Setup step 3", true, failures, checked);
        sweepNamed(setup, "memberLead_0", "Setup lead chip", true, failures, checked);
        setup->findChild<QPushButton*>("memberLead_0")->setChecked(true);
        sweepNamed(setup, "memberLead_0", "Setup checked lead chip", true, failures, checked);
        sweepNamed(setup, "memberRemove_0", "Setup remove", true, failures, checked);
        setup->findChildren<QLineEdit*>(QRegularExpression("^memberNameEdit_")).last()->setText("Ana");
        for (int i = 0; i < 3; ++i)
            setup->findChild<QPushButton*>("nextButton")->click();
        QApplication::processEvents();
        sweepNamed(setup, "createButton", "Setup review", true, failures, checked);

        // Sidebar
        goTo(Page::Board);
        auto* sidebar = window.findChild<Sidebar*>();
        const auto navItems = sidebar->findChildren<QPushButton*>("navItem");
        for (QPushButton* item : navItems) {
            const focusring::Result r = focusring::check(item, focusring::Shape::Box, true, minFillContrast(item));
            ++checked;
            if (!r.ok) {
                auto* label = item->findChild<QLabel*>("navLabel");
                failures << QString("Sidebar nav item %1 (active=%2): %3")
                                .arg(label ? label->text() : "?", item->property("active").toString(), r.why);
            }
        }
        sweepNamed(sidebar, "themeToggle", "Sidebar", true, failures, checked);

        // Board and Pins
        sweepNamed(&window, "BoardPrimary", "Board", true, failures, checked);
        sweepNamed(&window, "BoardSearch", "Board", true, failures, checked);
        sweepNamed(&window, "BoardMemberFilter", "Board", true, failures, checked);
        goTo(Page::Home);
        for (const char* name : {"homePinsLink", "homePinsMore"})
            sweepNamed(&window, name, "Home", true, failures, checked);
        goTo(Page::Pins);
        for (const char* name : {"LeavePinButton", "PinFilterChip", "PinResolveButton"})
            sweepNamed(&window, name, "Pins", true, failures, checked);

        // Dialogs: primary, secondary and danger buttons of a ModalDialog, and the fields of the real dialogs.
        {
            ModalDialog dialog("Delete pin", &window);
            auto* message = new QLabel("Delete this pin? This cannot be undone.");
            message->setObjectName("modalMessage");
            dialog.contentLayout()->addWidget(message);
            dialog.addButton("Cancel", ModalDialog::Role::Secondary);
            dialog.addButton("Delete", ModalDialog::Role::Danger);
            dialog.addButton("Keep", ModalDialog::Role::Primary);
            dialog.show();
            dialog.activateWindow();
            QVERIFY(QTest::qWaitForWindowExposed(&dialog));
            for (const char* name : {"modalSecondaryButton", "modalDangerButton", "modalPrimaryButton"})
                sweepNamed(&dialog, name, "ModalDialog", false, failures, checked);
        }
        {
            NewTaskDialog dialog(core.taskQuery().all(), core.memberProjection().all(), &window);
            dialog.show();
            dialog.activateWindow();
            QVERIFY(QTest::qWaitForWindowExposed(&dialog));
            sweep(&dialog, "NewTaskDialog", failures, checked);
        }
        {
            const auto members = core.memberProjection().all();
            planova::s5::LeavePinDialog dialog(core.taskQuery().all(), members,
                                               members.empty() ? planova::Id{} : members.front().id, &window);
            dialog.show();
            dialog.activateWindow();
            QVERIFY(QTest::qWaitForWindowExposed(&dialog));
            sweep(&dialog, "LeavePinDialog", failures, checked);
        }

        QVERIFY2(failures.isEmpty(), qPrintable(QString("%1 of %2 controls have no visible ring:\n%3")
                                                    .arg(failures.size())
                                                    .arg(checked)
                                                    .arg(failures.join("\n"))));
    }
};

QTEST_MAIN(tst_FocusRings)
#include "tst_FocusRings.moc"
