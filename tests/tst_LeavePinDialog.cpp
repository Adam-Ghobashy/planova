#include <QtTest>

#include <QApplication>
#include <QComboBox>
#include <QListView>

#include "pins/LeavePinDialog.h"
#include "pins/StyledComboBox.h"

using planova::s5::LeavePinDialog;
using planova::s5::StyledComboBox;

class TstLeavePinDialog : public QObject {
    Q_OBJECT
private slots:
    void parseTagsNormalises()
    {
        const std::vector<std::string> expected{"a", "b"};
        QCOMPARE(LeavePinDialog::parseTags(QStringLiteral("#A, b  A")), expected);
    }

    void parseTagsDropsEmptyAndHashOnly()
    {
        QVERIFY(LeavePinDialog::parseTags(QStringLiteral("  , # ## ")).empty());
    }

    void parseTagsKeepsFirstOrder()
    {
        const std::vector<std::string> expected{"storage", "conflict"};
        QCOMPARE(LeavePinDialog::parseTags(QStringLiteral("##Storage conflict,STORAGE")), expected);
    }

    void emptyDialogHasNoTextOrTask()
    {
        const std::vector<planova::TaskDto> tasks;
        const std::vector<planova::MemberDto> members;
        LeavePinDialog dlg(tasks, members, planova::Id{});
        QVERIFY(dlg.text().isEmpty());
        QCOMPARE(dlg.taskId(), std::optional<planova::Id>{});
    }

    void defaultAuthorIsPreselected()
    {
        planova::MemberDto a;
        a.id = "m1";
        a.name = "Ana";
        planova::MemberDto b;
        b.id = "m2";
        b.name = "Ben";
        const std::vector<planova::TaskDto> tasks;
        LeavePinDialog dlg(tasks, {a, b}, planova::Id("m2"));
        QCOMPARE(dlg.authorId(), planova::Id("m2"));
    }

    void unknownDefaultAuthorFallsBackToFirst()
    {
        planova::MemberDto a;
        a.id = "m1";
        a.name = "Ana";
        const std::vector<planova::TaskDto> tasks;
        LeavePinDialog dlg(tasks, {a}, planova::Id("nobody"));
        QCOMPARE(dlg.authorId(), planova::Id("m1"));
    }

    void dialogUsesStyledCombosOnly()
    {
        const std::vector<planova::TaskDto> tasks;
        const std::vector<planova::MemberDto> members;
        LeavePinDialog dlg(tasks, members, planova::Id{});
        QCOMPARE(dlg.findChildren<QComboBox*>().size(), 3);
        QCOMPARE(dlg.findChildren<StyledComboBox*>().size(), 3);
    }
};

class TstStyledComboBox : public QObject {
    Q_OBJECT
private slots:
    void popupIsFramelessAndOpaque()
    {
        StyledComboBox c;
        QWidget* popup = c.view()->window();
        QVERIFY(popup->windowFlags() & Qt::FramelessWindowHint);
        QVERIFY(!popup->testAttribute(Qt::WA_TranslucentBackground));
    }

    void arrowColorsAreProperties()
    {
        StyledComboBox c;
        QVERIFY(c.setProperty("arrowColor", QColor(Qt::red)));
        QVERIFY(c.setProperty("arrowActiveColor", QColor(Qt::blue)));
        QCOMPARE(c.arrowColor(), QColor(Qt::red));
        QCOMPARE(c.arrowActiveColor(), QColor(Qt::blue));
    }

    void objectNameMatchesQss() { QCOMPARE(StyledComboBox().objectName(), QStringLiteral("PinCombo")); }
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    int rc = 0;
    TstLeavePinDialog t1;
    rc |= QTest::qExec(&t1, argc, argv);
    TstStyledComboBox t2;
    rc |= QTest::qExec(&t2, argc, argv);
    return rc;
}

#include "tst_LeavePinDialog.moc"
