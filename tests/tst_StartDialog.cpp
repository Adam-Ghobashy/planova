#include "TestAppSetup.h"
#include "welcome/StartDialog.h"

#include <QPushButton>
#include <QTest>

class StartDialogTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { installAppFonts(); }   // also loads the Qt resources (logo, styles)

    void startsWithNoChoice()
    {
        StartDialog dialog(nullptr);
        QCOMPARE(dialog.choice(), StartDialog::Choice::None);
    }

    void startNewButtonChoosesNewProject()
    {
        StartDialog dialog(nullptr);
        dialog.show();
        auto* button = dialog.findChild<QPushButton*>("modalPrimaryButton");
        QVERIFY(button);
        QCOMPARE(button->text(), QString("Start new project"));
        button->click();
        QCOMPARE(dialog.choice(), StartDialog::Choice::NewProject);
        QCOMPARE(dialog.result(), int(QDialog::Accepted));
    }

    void continueButtonChoosesContinue()
    {
        StartDialog dialog(nullptr);
        dialog.show();
        const auto buttons = dialog.findChildren<QPushButton*>("modalSecondaryButton");
        QPushButton* cont = nullptr;
        for (auto* b : buttons)
            if (b->text() == "Continue an old project")
                cont = b;
        QVERIFY(cont);
        cont->click();
        QCOMPARE(dialog.choice(), StartDialog::Choice::ContinueProject);
    }

    void notNowClosesWithoutChoice()
    {
        StartDialog dialog(nullptr);
        dialog.show();
        const auto buttons = dialog.findChildren<QPushButton*>("modalSecondaryButton");
        QPushButton* notNow = nullptr;
        for (auto* b : buttons)
            if (b->text() == "Not now")
                notNow = b;
        QVERIFY(notNow);
        notNow->click();
        QCOMPARE(dialog.choice(), StartDialog::Choice::None);
        QCOMPARE(dialog.result(), int(QDialog::Rejected));
    }
};

QTEST_MAIN(StartDialogTest)
#include "tst_StartDialog.moc"
