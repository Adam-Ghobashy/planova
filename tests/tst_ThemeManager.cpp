#include "TestAppSetup.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QFrame>
#include <QRegularExpression>
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

    void comboPopupRuleIsGlobalAndPagesKeepTheirOwn() {
        ThemeManager::instance().apply(ThemeMode::Light);
        const QString css = qApp->styleSheet();
        // base.qss styles every combo popup (Task 6); the page rules below it are more specific and win.
        QVERIFY(css.contains("\nQComboBox QAbstractItemView"));
        QVERIFY(css.contains("QComboBox#BoardMemberFilter QAbstractItemView"));
        QVERIFY(css.contains("QComboBox#PinCombo QAbstractItemView"));
    }

    void darkTableComplete() {
        QCOMPARE(std::size(kDarkHex), std::size(kLightHex));
        QCOMPARE(std::size(kTokenNames), std::size(kLightHex));
    }
    void disabledTextIsDimmed() {
        auto& t = ThemeManager::instance();
        for (ThemeMode mode : {ThemeMode::Dark, ThemeMode::Light}) {
            t.apply(mode);
            for (QPalette::ColorRole role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
                QCOMPARE(qApp->palette().color(QPalette::Disabled, role), t.color(Token::TextMuted));
        }
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
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("NoSuchToken"));
        QCOMPARE(ThemeManager::resolveTokens("x:@NoSuchToken;", ThemeMode::Light), QString("x:@NoSuchToken;"));
    }
    void unknownTokenWarnsOncePerName() {
        // Only one message is expected for two uses of the same name (QtTest fails on an unexpected second warning).
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("NoSuchToken"));
        const QString css = "x:@NoSuchToken; y:@NoSuchToken;";
        QCOMPARE(ThemeManager::resolveTokens(css, ThemeMode::Light), css);
    }
    void translucentTokenIsAcceptedByQss() {
        // Qt reads #AARRGGBB (alpha first) in a stylesheet, so no rgba() conversion is needed.
        QCOMPARE(ThemeManager::resolveTokens("x:@Overlay;", ThemeMode::Light), QString("x:#B3000000;"));
        QWidget window;
        window.setObjectName("root");
        window.setStyleSheet(ThemeManager::resolveTokens(
            "QWidget#root{background-color:#FFFFFF;} QFrame#probe{background-color:@Overlay;}", ThemeMode::Light));
        window.resize(10, 10);
        auto* probe = new QFrame(&window);
        probe->setObjectName("probe");
        probe->setGeometry(0, 0, 10, 10);
        window.show();
        const QColor pixel = window.grab().toImage().pixelColor(5, 5);
        // 70% black over the white window: neither black nor white.
        QVERIFY2(pixel.red() > 20 && pixel.red() < 120, qPrintable(pixel.name()));
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
        QCOMPARE(qApp->palette().color(QPalette::Window), QColor(kDarkHex[int(Token::Background)]));
        ThemeManager::instance().apply(ThemeMode::Light);
    }
    void fontsRegistered() {
        ThemeManager::instance().loadFonts();
        const QStringList families = QFontDatabase::families();
        QVERIFY(!families.contains("Inter"));
        QVERIFY(!families.contains("Space Grotesk"));
        QVERIFY(families.contains("JetBrains Mono"));
        QVERIFY(families.contains("Macintosh Skeletal Filled"));
        QCOMPARE(ThemeManager::instance().font(FontRole::Mono, 14).family(), QString("JetBrains Mono"));
        QCOMPARE(ThemeManager::instance().font(FontRole::Display, 18).pixelSize(), 18);
        QCOMPARE(ThemeManager::instance().font(FontRole::Body, 14).hintingPreference(),
                 QFont::PreferDefaultHinting);
    }
    void fontFallback() {
        QCOMPARE(ThemeManager::familyFor(FontRole::Body, {"Segoe UI Variable Text", "Segoe UI"}),
                 QString("Segoe UI Variable Text"));
        QCOMPARE(ThemeManager::familyFor(FontRole::Body, {"Segoe UI Variable"}), QString("Segoe UI Variable"));
        QCOMPARE(ThemeManager::familyFor(FontRole::Display, {"Segoe UI Variable", "Segoe UI Variable Display"}),
                 QString("Segoe UI Variable Display"));
        QCOMPARE(ThemeManager::familyFor(FontRole::Display, {"Segoe UI"}), QString("Segoe UI"));
        QCOMPARE(ThemeManager::familyFor(FontRole::Body, {}), QString("Segoe UI"));
        QCOMPARE(ThemeManager::familyFor(FontRole::Display, {}), QString("Segoe UI"));
        QCOMPARE(ThemeManager::familyFor(FontRole::Script, {}), QString("Segoe UI"));
        QCOMPARE(ThemeManager::familyFor(FontRole::Mono, {}), QString("Consolas"));
        QCOMPARE(ThemeManager::familyFor(FontRole::Mono, {"Consolas", "JetBrains Mono"}), QString("JetBrains Mono"));
        QCOMPARE(ThemeManager::familyFor(FontRole::Body, {"Inter"}), QString("Segoe UI"));
    }
    void typeScaleValues_data() {
        QTest::addColumn<int>("style");
        QTest::addColumn<int>("pixels");
        QTest::addColumn<int>("weight");
        QTest::newRow("Caption") << int(TextStyle::Caption) << 12 << int(QFont::Medium);
        QTest::newRow("Body") << int(TextStyle::Body) << 14 << int(QFont::Normal);
        QTest::newRow("Label") << int(TextStyle::Label) << 14 << int(QFont::Medium);
        QTest::newRow("LabelActive") << int(TextStyle::LabelActive) << 14 << int(QFont::DemiBold);
        QTest::newRow("Heading") << int(TextStyle::Heading) << 16 << int(QFont::DemiBold);
        QTest::newRow("PageTitle") << int(TextStyle::PageTitle) << 24 << int(QFont::DemiBold);
    }
    void typeScaleValues() {
        QFETCH(int, style);
        QFETCH(int, pixels);
        QFETCH(int, weight);
        const QFont font = ThemeManager::instance().textFont(static_cast<TextStyle>(style));
        QCOMPARE(font.pixelSize(), pixels);
        QCOMPARE(int(font.weight()), weight);
        QCOMPARE(font.hintingPreference(), QFont::PreferDefaultHinting);
    }
    void typeScaleRoles() {
        // Caption, Body and Label read as body text; the rest are titles and use the Display family.
        auto& t = ThemeManager::instance();
        const QString body = t.font(FontRole::Body, 14).family();
        const QString display = t.font(FontRole::Display, 14).family();
        QCOMPARE(t.textFont(TextStyle::Caption).family(), body);
        QCOMPARE(t.textFont(TextStyle::Body).family(), body);
        QCOMPARE(t.textFont(TextStyle::Label).family(), body);
        QCOMPARE(t.textFont(TextStyle::LabelActive).family(), display);
        QCOMPARE(t.textFont(TextStyle::Heading).family(), display);
        QCOMPARE(t.textFont(TextStyle::PageTitle).family(), display);
    }
    void qssHasNoSmallOrOldFonts() {
        static const QRegularExpression size("font-size:\\s*(\\d+)px");
        const QStringList files = QDir(":/themes").entryList({"*.qss"}, QDir::Files);
        QVERIFY(!files.isEmpty());
        for (const QString& name : files) {
            QFile file(":/themes/" + name);
            QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
            const QString css = QString::fromUtf8(file.readAll());
            QVERIFY2(!css.contains("Inter"), qPrintable(name));
            QVERIFY2(!css.contains("Space Grotesk"), qPrintable(name));
            auto it = size.globalMatch(css);
            while (it.hasNext()) {
                const QRegularExpressionMatch m = it.next();
                QVERIFY2(m.captured(1).toInt() >= 12, qPrintable(name + ": " + m.captured(0)));
            }
        }
    }
    void appFontIsBody14() {
        installAppFonts();
        QCOMPARE(QApplication::font().pixelSize(), 14);
        QCOMPARE(QApplication::font().hintingPreference(), QFont::PreferDefaultHinting);
    }
};

QTEST_MAIN(TstThemeManager)
#include "tst_ThemeManager.moc"
