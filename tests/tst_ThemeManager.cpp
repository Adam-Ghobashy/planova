#include <QApplication>
#include <QFile>
#include <QtTest>

#include "theme/ThemeManager.h"

class TstThemeManager : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        Q_INIT_RESOURCE(planova);
        QCoreApplication::setOrganizationName("PlanovaTest"); // keep QSettings away from real settings
    }

    void appliesEveryStylesheet() {
        ThemeManager::instance().apply(ThemeMode::Light);
        const QString css = qApp->styleSheet();
        for (const char* sel : {"QWidget#ChaosSlicerPage", "QWidget#homePage", "QWidget#PinsPage",
                                "#TrackingPage", "QLabel#PageTitle", "QWidget#TeamSkillsPage",
                                "QWidget#GitHubIntegrationPage"})
            QVERIFY2(css.contains(sel), sel);
    }

    void comboPopupRulesAreScoped() {
        ThemeManager::instance().apply(ThemeMode::Light);
        // A bare rule would restyle every combo popup in the app.
        QVERIFY(!qApp->styleSheet().contains("\nQComboBox QAbstractItemView"));
    }

    void chevronIconIsBundled() { QVERIFY(QFile::exists(":/icons/chevron-updown.svg")); }

    void darkTableComplete() {
        QCOMPARE(std::size(kDarkHex), std::size(kLightHex));
        QCOMPARE(std::size(kTokenNames), std::size(kLightHex));
    }
    void darkColorsDiffer() {
        auto& t = ThemeManager::instance();
        t.apply(ThemeMode::Dark);
        QCOMPARE(t.color(Token::Background), QColor(kDarkHex[int(Token::Background)]));
        t.apply(ThemeMode::Light);
    }
    void longestTokenWins() {
        QCOMPARE(ThemeManager::resolveTokens("a{c:@SurfaceAlt;b:@Surface}", ThemeMode::Light),
                 QString("a{c:#F5F2EA;b:#FFFFFF}"));
    }
    void unknownTokenUntouched() {
        QCOMPARE(ThemeManager::resolveTokens("x:@NoSuchToken;", ThemeMode::Light), QString("x:@NoSuchToken;"));
    }
    void toggleEmitsSignal() {
        QSignalSpy s(&ThemeManager::instance(), &ThemeManager::themeChanged);
        ThemeManager::instance().apply(ThemeMode::Light);
        ThemeManager::instance().toggle();
        QCOMPARE(s.count(), 2);
        QCOMPARE(ThemeManager::instance().mode(), ThemeMode::Dark);
        ThemeManager::instance().apply(ThemeMode::Light);
    }
    void paletteFollowsTokens() {
        ThemeManager::instance().apply(ThemeMode::Dark);
        QCOMPARE(qApp->palette().color(QPalette::Window), QColor("#0A0A0B"));
        ThemeManager::instance().apply(ThemeMode::Light);
    }
};

QTEST_MAIN(TstThemeManager)
#include "tst_ThemeManager.moc"
