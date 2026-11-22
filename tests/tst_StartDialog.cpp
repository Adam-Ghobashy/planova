#include "TestAppSetup.h"
#include "planova/PlanovaCore.h"
#include "welcome/StartDialog.h"

#include <QLabel>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTest>

namespace {
QPushButton* buttonWithText(QWidget& parent, const QString& text)
{
    for (auto* b : parent.findChildren<QPushButton*>())
        if (b->text() == text)
            return b;
    return nullptr;
}
}

class StartDialogTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { installAppFonts(); }   // also loads the Qt resources (logo, styles)

    void startsWithNoChoice()
    {
        QTemporaryDir dir;
        PlanovaCore core(dir.path().toStdString());
        StartDialog dialog(core, nullptr);
        QCOMPARE(dialog.choice(), StartDialog::Choice::None);
    }

    void startNewButtonChoosesNewProject()
    {
        QTemporaryDir dir;
        PlanovaCore core(dir.path().toStdString());
        StartDialog dialog(core, nullptr);
        dialog.show();
        auto* button = buttonWithText(dialog, "Start new project");
        QVERIFY(button);
        button->click();
        QCOMPARE(dialog.choice(), StartDialog::Choice::NewProject);
        QCOMPARE(dialog.result(), int(QDialog::Accepted));
    }

    void listsEveryProjectAndPickingOneOpensIt()
    {
        QTemporaryDir dir;
        PlanovaCore core(dir.path().toStdString());
        core.createProject("Alpha", "Team A", "Agile", "");
        const auto beta = core.createProject("Beta", "Team B", "Scrum", "");
        StartDialog dialog(core, nullptr);
        dialog.show();
        QVERIFY(buttonWithText(dialog, QString("Alpha  ·  Team A")));
        auto* pick = buttonWithText(dialog, QString("Beta  ·  Team B"));
        QVERIFY(pick);
        pick->click();
        QCOMPARE(dialog.choice(), StartDialog::Choice::OpenProject);
        QCOMPARE(dialog.projectId().toStdString(), beta);
    }

    void currentProjectIsMarked()
    {
        QTemporaryDir dir;
        PlanovaCore core(dir.path().toStdString());
        core.openProject(core.createProject("Alpha", "Team A", "Agile", ""));
        core.createProject("Beta", "Team B", "Scrum", "");
        QVERIFY(core.currentProject());
        StartDialog dialog(core, nullptr);
        dialog.show();

        int marked = 0;
        for (auto* label : dialog.findChildren<QLabel*>())
            if (label->text() == "Current" && label->isVisible())
                ++marked;
        QCOMPARE(marked, 1);

        auto* current = buttonWithText(dialog, QString("Alpha  ·  Team A"));
        auto* other = buttonWithText(dialog, QString("Beta  ·  Team B"));
        QVERIFY(current && other);
        QCOMPARE(current->accessibleDescription(), QString("Current"));
        QVERIFY(other->accessibleDescription().isEmpty());
    }

    void noProjectsShowsAMessage()
    {
        QTemporaryDir dir;
        PlanovaCore core(dir.path().toStdString());
        StartDialog dialog(core, nullptr);
        dialog.show();
        bool found = false;
        for (auto* label : dialog.findChildren<QLabel*>())
            found = found || label->text().startsWith("No projects yet");
        QVERIFY(found);
    }

    void notNowClosesWithoutChoice()
    {
        QTemporaryDir dir;
        PlanovaCore core(dir.path().toStdString());
        StartDialog dialog(core, nullptr);
        dialog.show();
        auto* notNow = buttonWithText(dialog, "Not now");
        QVERIFY(notNow);
        notNow->click();
        QCOMPARE(dialog.choice(), StartDialog::Choice::None);
        QCOMPARE(dialog.result(), int(QDialog::Rejected));
    }
};

QTEST_MAIN(StartDialogTest)
#include "tst_StartDialog.moc"
