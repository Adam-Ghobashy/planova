#include "TestAppSetup.h"
#include "planova/common/Dtos.h"
#include "welcome/DeleteProjectDialog.h"
#include "welcome/EditProjectDialog.h"

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTest>
#include <stdexcept>

namespace {
ProjectDto sampleProject()
{
    ProjectDto project;
    project.id = "p1";
    project.name = "Orbit";
    project.team = "Platform";
    project.seModel = "Scrum";
    return project;
}

// The model combo is a StyledComboBox, whose object name must stay "PinCombo" for pins.qss.
// The dialog has exactly one combo box, so it is found by type.
QComboBox* modelCombo(QWidget& dialog) { return dialog.findChild<QComboBox*>(); }

void checkTargetsAndFocus(QWidget& dialog)
{
    for (auto* w : dialog.findChildren<QPushButton*>()) {
        if (!w->isVisibleTo(&dialog)) continue;   // the close button is a QToolButton, not checked here
        QVERIFY2(w->height() >= 24, qPrintable(w->text()));
        QVERIFY2(w->focusPolicy() & (Qt::StrongFocus | Qt::TabFocus), qPrintable(w->text()));
    }
    for (auto* w : dialog.findChildren<QLineEdit*>()) {
        if (!w->isVisibleTo(&dialog) || w->parent()->inherits("QAbstractSpinBox")) continue;
        QVERIFY2(w->height() >= 24, qPrintable(w->objectName()));
        QVERIFY2(w->focusPolicy() & (Qt::StrongFocus | Qt::TabFocus), qPrintable(w->objectName()));
    }
}
}

class ProjectDialogsTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { installAppFonts(); }
    void init() { ThemeManager::instance().apply(ThemeMode::Light); }   // real styles, so heights are real

    void editStartsWithProjectValues()
    {
        EditProjectDialog dialog(sampleProject(), [](const QString&, const QString&, const QString&) {}, nullptr);
        QCOMPARE(dialog.findChild<QLineEdit*>("editProjectName")->text(), QString("Orbit"));
        QCOMPARE(dialog.findChild<QLineEdit*>("editProjectTeam")->text(), QString("Platform"));
        QComboBox* combo = modelCombo(dialog);
        QVERIFY(combo);
        QCOMPARE(combo->currentText(), QString("Scrum"));
        QStringList items;
        for (int i = 0; i < combo->count(); ++i) items << combo->itemText(i);
        QCOMPARE(items, (QStringList{"Agile", "Scrum", "Waterfall", "Spiral"}));
    }

    void editSaveDisabledWhileNameBlank()
    {
        EditProjectDialog dialog(sampleProject(), [](const QString&, const QString&, const QString&) {}, nullptr);
        auto* save = dialog.findChild<QPushButton*>("modalPrimaryButton");
        auto* name = dialog.findChild<QLineEdit*>("editProjectName");
        name->setText("   ");
        QVERIFY(!save->isEnabled());
        name->setText("X");
        QVERIFY(save->isEnabled());
    }

    void editSaveCallsSaveFnAndAccepts()
    {
        QString gotName, gotTeam, gotModel;
        EditProjectDialog dialog(sampleProject(),
                                 [&](const QString& n, const QString& t, const QString& m) {
                                     gotName = n;
                                     gotTeam = t;
                                     gotModel = m;
                                 },
                                 nullptr);
        dialog.show();
        dialog.findChild<QLineEdit*>("editProjectName")->setText("Orbit 2");
        dialog.findChild<QLineEdit*>("editProjectTeam")->setText("Core");
        modelCombo(dialog)->setCurrentText("Waterfall");
        dialog.findChild<QPushButton*>("modalPrimaryButton")->click();
        QCOMPARE(gotName, QString("Orbit 2"));
        QCOMPARE(gotTeam, QString("Core"));
        QCOMPARE(gotModel, QString("Waterfall"));
        QCOMPARE(dialog.result(), int(QDialog::Accepted));
    }

    void editShowsErrorInline()
    {
        EditProjectDialog dialog(sampleProject(),
                                 [](const QString&, const QString&, const QString&) {
                                     throw std::invalid_argument("Unknown process model");
                                 },
                                 nullptr);
        dialog.show();
        auto* error = dialog.findChild<QLabel*>("editProjectError");
        QVERIFY(error);
        QVERIFY(!error->isVisibleTo(&dialog));   // hidden while empty
        dialog.findChild<QPushButton*>("modalPrimaryButton")->click();
        QVERIFY(dialog.isVisible());
        QVERIFY(error->isVisibleTo(&dialog));
        QCOMPARE(error->text(), QString("Unknown process model"));
        QVERIFY(dialog.result() != int(QDialog::Accepted));
    }

    void editOtherExceptionStaysInline()
    {
        EditProjectDialog dialog(sampleProject(),
                                 [](const QString&, const QString&, const QString&) { throw std::runtime_error("disk"); },
                                 nullptr);
        dialog.show();
        dialog.findChild<QPushButton*>("modalPrimaryButton")->click();
        QVERIFY(dialog.isVisible());
        QVERIFY(dialog.findChild<QLabel*>("editProjectError")->isVisibleTo(&dialog));
    }

    void editLabelsFieldTeamName()
    {
        EditProjectDialog dialog(sampleProject(), [](const QString&, const QString&, const QString&) {}, nullptr);
        QStringList labels;
        for (auto* label : dialog.findChildren<QLabel*>("modalFieldLabel")) labels << label->text();
        QVERIFY(labels.contains("Team name"));
    }

    void editStoredModelMatchesIgnoringCase()
    {
        ProjectDto project = sampleProject();
        project.seModel = "scrum";   // an older file may hold another spelling
        EditProjectDialog dialog(project, [](const QString&, const QString&, const QString&) {}, nullptr);
        QCOMPARE(modelCombo(dialog)->currentText(), QString("Scrum"));
    }

    void editUnknownModelShowsNoSelectionAndBlocksSave()
    {
        ProjectDto project = sampleProject();
        project.seModel = "Kanban";   // not one of the four models
        EditProjectDialog dialog(project, [](const QString&, const QString&, const QString&) {}, nullptr);
        QComboBox* combo = modelCombo(dialog);
        QCOMPARE(combo->currentIndex(), -1);
        QCOMPARE(combo->placeholderText(), QString("Choose a process model"));
        auto* save = dialog.findChild<QPushButton*>("modalPrimaryButton");
        QVERIFY(!save->isEnabled());
        combo->setCurrentIndex(1);
        QVERIFY(save->isEnabled());
    }

    void editErrorHidesWhenTeamOrModelChanges()
    {
        EditProjectDialog dialog(sampleProject(),
                                 [](const QString&, const QString&, const QString&) {
                                     throw std::invalid_argument("Nope");
                                 },
                                 nullptr);
        dialog.show();
        auto* error = dialog.findChild<QLabel*>("editProjectError");
        auto* save = dialog.findChild<QPushButton*>("modalPrimaryButton");

        save->click();
        QVERIFY(error->isVisibleTo(&dialog));
        dialog.findChild<QLineEdit*>("editProjectTeam")->setText("Core");
        QVERIFY(!error->isVisibleTo(&dialog));

        save->click();
        QVERIFY(error->isVisibleTo(&dialog));
        modelCombo(dialog)->setCurrentText("Waterfall");
        QVERIFY(!error->isVisibleTo(&dialog));
    }

    void editNonStandardExceptionStaysInline()
    {
        EditProjectDialog dialog(sampleProject(),
                                 [](const QString&, const QString&, const QString&) { throw 42; },
                                 nullptr);
        dialog.show();
        dialog.findChild<QPushButton*>("modalPrimaryButton")->click();
        QVERIFY(dialog.isVisible());
        auto* error = dialog.findChild<QLabel*>("editProjectError");
        QVERIFY(error->isVisibleTo(&dialog));
        QCOMPARE(error->text(), QString("Could not save the project."));
    }

    void deletePlaceholderIsNotTheName()
    {
        DeleteProjectDialog dialog("Orbit", nullptr);
        QCOMPARE(dialog.findChild<QLineEdit*>("deleteProjectConfirm")->placeholderText(), QString("Project name"));
    }

    void deleteNeedsExactName()
    {
        DeleteProjectDialog dialog("Orbit", nullptr);
        auto* confirm = dialog.findChild<QLineEdit*>("deleteProjectConfirm");
        auto* del = dialog.findChild<QPushButton*>("modalDangerButton");
        QVERIFY(confirm && del);
        for (const QString& typed : {QString(), QString("orbit"), QString("Orbit ")}) {
            confirm->setText(typed);
            QVERIFY2(!del->isEnabled(), qPrintable(typed));
        }
        confirm->setText("Orbit");
        QVERIFY(del->isEnabled());
    }

    void deleteTextNamesProject()
    {
        DeleteProjectDialog dialog("Orbit", nullptr);
        const QString expected =
            "This permanently deletes Orbit and all its tasks, pins and team data. This cannot be undone. "
            "Type the project name to confirm.";
        bool found = false;
        for (auto* label : dialog.findChildren<QLabel*>())
            if (label->text() == expected) found = true;
        QVERIFY(found);
    }

    void dialogTargetsAndFocus()
    {
        EditProjectDialog edit(sampleProject(), [](const QString&, const QString&, const QString&) {}, nullptr);
        edit.show();
        QVERIFY(QTest::qWaitForWindowActive(&edit));
        checkTargetsAndFocus(edit);
        QVERIFY(edit.findChild<QLineEdit*>("editProjectName")->hasFocus());
        QVERIFY(modelCombo(edit)->height() >= 24);

        DeleteProjectDialog del("Orbit", nullptr);
        del.show();
        QVERIFY(QTest::qWaitForWindowActive(&del));
        checkTargetsAndFocus(del);
        QVERIFY(del.findChild<QLineEdit*>("deleteProjectConfirm")->hasFocus());
    }
};

QTEST_MAIN(ProjectDialogsTest)
#include "tst_ProjectDialogs.moc"
