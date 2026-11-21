#include "board/NewTaskDialog.h"
#include "widgets/ModalDialog.h"

#include <QAbstractButton>
#include <QApplication>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTest>
#include <QTimer>
#include <QVBoxLayout>

namespace {
// The dialog is shown by exec(), so the click has to come from a timer that runs inside its event loop.
ModalDialog* visibleModal()
{
    const auto widgets = QApplication::topLevelWidgets();
    for (QWidget* w : widgets) {
        if (auto* dlg = qobject_cast<ModalDialog*>(w); dlg && dlg->isVisible())
            return dlg;
    }
    return nullptr;
}

void clickLater(const char* buttonName)
{
    QTimer::singleShot(0, [buttonName] {
        ModalDialog* dlg = visibleModal();
        QVERIFY2(dlg, "no visible ModalDialog");
        auto* button = dlg->findChild<QAbstractButton*>(buttonName);
        QVERIFY2(button, buttonName);
        QTest::mouseClick(button, Qt::LeftButton);
    });
}

void keyLater(Qt::Key key)
{
    QTimer::singleShot(0, [key] {
        ModalDialog* dlg = visibleModal();
        QVERIFY2(dlg, "no visible ModalDialog");
        QTest::keyClick(dlg, key);
    });
}
}

class TstModalDialog : public QObject {
    Q_OBJECT
private slots:
    void confirmTrueOnOk()
    {
        QWidget parent;
        parent.show();
        clickLater("modalPrimaryButton");
        QVERIFY(ModalDialog::confirm(&parent, "Delete pin", "Sure?", "Delete"));
    }

    void confirmFalseOnCancel()
    {
        QWidget parent;
        parent.show();
        clickLater("modalSecondaryButton");
        QVERIFY(!ModalDialog::confirm(&parent, "Delete pin", "Sure?", "Delete"));
    }

    void escapeRejects()
    {
        QWidget parent;
        parent.show();
        ModalDialog dlg("Title", &parent);
        dlg.addButton("OK", ModalDialog::Role::Primary);
        keyLater(Qt::Key_Escape);
        QCOMPARE(dlg.exec(), static_cast<int>(QDialog::Rejected));
    }

    void enterTriggersPrimaryButton()
    {
        QWidget parent;
        parent.show();
        ModalDialog dlg("Title", &parent);
        dlg.addButton("Cancel", ModalDialog::Role::Secondary);
        dlg.addButton("OK", ModalDialog::Role::Primary);
        keyLater(Qt::Key_Return);
        QCOMPARE(dlg.exec(), static_cast<int>(QDialog::Accepted));
    }

    void closeButtonRejects()
    {
        QWidget parent;
        parent.show();
        ModalDialog dlg("Title", &parent);
        dlg.addButton("OK", ModalDialog::Role::Primary);
        clickLater("modalClose");
        QCOMPARE(dlg.exec(), static_cast<int>(QDialog::Rejected));
    }

    void coversParentWindow()
    {
        QWidget parent;
        parent.resize(900, 600);
        parent.show();
        QVERIFY(QTest::qWaitForWindowExposed(&parent));
        ModalDialog dlg("Title", &parent);
        dlg.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dlg));
        QCOMPARE(dlg.geometry(), parent.geometry());
    }

    void focusStartsOnFirstField()
    {
        QWidget parent;
        parent.show();
        ModalDialog dlg("Title", &parent);
        auto* edit = new QLineEdit;
        dlg.contentLayout()->addWidget(edit);
        dlg.addButton("OK", ModalDialog::Role::Primary);
        dlg.show();
        QVERIFY(QTest::qWaitForWindowActive(&dlg));
        QVERIFY(edit->hasFocus());
    }

    void focusStartsOnPrimaryWithoutFields()
    {
        QWidget parent;
        parent.show();
        ModalDialog dlg("Title", &parent);
        dlg.addButton("Cancel", ModalDialog::Role::Secondary);
        auto* ok = dlg.addButton("OK", ModalDialog::Role::Primary);
        dlg.show();
        QVERIFY(QTest::qWaitForWindowActive(&dlg));
        QVERIFY(ok->hasFocus());
    }

    void dangerConfirmFocusesCancel()
    {
        QWidget parent;
        parent.show();
        ModalDialog dlg("Title", &parent);
        auto* cancel = dlg.addButton("Cancel", ModalDialog::Role::Secondary);
        dlg.addButton("Delete", ModalDialog::Role::Danger);
        dlg.show();
        QVERIFY(QTest::qWaitForWindowActive(&dlg));
        QVERIFY(cancel->hasFocus());
    }

    void buttonObjectNamesFollowRole()
    {
        ModalDialog dlg("Title", nullptr);
        QCOMPARE(dlg.addButton("a", ModalDialog::Role::Primary)->objectName(), QString("modalPrimaryButton"));
        QCOMPARE(dlg.addButton("b", ModalDialog::Role::Secondary)->objectName(), QString("modalSecondaryButton"));
        QCOMPARE(dlg.addButton("c", ModalDialog::Role::Danger)->objectName(), QString("modalDangerButton"));
    }

    void newTaskDialogNeedsATitle()
    {
        QWidget parent;
        parent.show();
        NewTaskDialog dlg({}, {}, &parent);
        dlg.show();
        auto* add = dlg.findChild<QPushButton*>("modalPrimaryButton");
        QVERIFY(add);
        QTest::mouseClick(add, Qt::LeftButton);          // empty title: stays open and shows the error
        QVERIFY(dlg.isVisible());
        QVERIFY(dlg.findChild<QLabel*>("modalError")->isVisibleTo(&dlg));
        dlg.findChild<QLineEdit*>()->setText("Write tests");
        QTest::mouseClick(add, Qt::LeftButton);
        QCOMPARE(dlg.result(), static_cast<int>(QDialog::Accepted));
        QCOMPARE(QString::fromStdString(dlg.taskData().title), QString("Write tests"));
    }

    void infoClosesOnOk()
    {
        QWidget parent;
        parent.show();
        clickLater("modalPrimaryButton");
        ModalDialog::info(&parent, "Cannot move task", "Because.");   // returns after OK
        QVERIFY(visibleModal() == nullptr);
    }
};

QTEST_MAIN(TstModalDialog)
#include "tst_ModalDialog.moc"
