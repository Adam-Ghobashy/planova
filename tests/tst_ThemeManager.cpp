#include "TestAppSetup.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QFileInfo>
#include <QFrame>
#include <QImage>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QtTest>

#include "theme/ThemeManager.h"

class TstThemeManager : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        Q_INIT_RESOURCE(planova);
        QCoreApplication::setOrganizationName("PlanovaTest"); // keep QSettings away from real settings
        QStandardPaths::setTestModeEnabled(true);             // and the icon cache away from the real user cache
    }

    void cleanup() {   // a failed blocked-folder test must not leave its blocker files behind
        QFile::remove(QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/themed-icons");
        QFile::remove(QDir::tempPath() + "/planova-themed-icons");
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
    void atSignInsideUrlIsNotAToken() {
        // A cache path may hold "@Accent"; only text outside url("...") is resolved.
        const QString css = "a{image:url(\"C:/Users/x@Accent/themed-icons/p.png\");color:@Accent;}";
        QCOMPARE(ThemeManager::resolveTokens(css, ThemeMode::Light),
                 QString("a{image:url(\"C:/Users/x@Accent/themed-icons/p.png\");color:%1;}")
                     .arg(kLightHex[int(Token::Accent)]));
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
    void iconPlaceholderBecomesAPngPath() {
        const QString css = ThemeManager::resolveIcons("a{image:@icon(check,AccentText);}", ThemeMode::Light);
        QVERIFY2(!css.contains("@icon"), qPrintable(css));
        const QRegularExpression url("url\\(\"?([^\")]+)\"?\\)");
        const QRegularExpressionMatch match = url.match(css);
        QVERIFY2(match.hasMatch(), qPrintable(css));
        const QString path = match.captured(1);
        QVERIFY2(QFile::exists(path), qPrintable(path));
        QVERIFY(path.endsWith("light-check-AccentText.png"));
        QVERIFY(!QImage(path).isNull());
        QVERIFY(QFileInfo(path).size() > 0);
        // The dark file has its own name, so the two themes never overwrite each other.
        QVERIFY(ThemeManager::resolveIcons("@icon(check,AccentText)", ThemeMode::Dark)
                    .contains("dark-check-AccentText.png"));
    }
    void iconPlaceholderAllowsSpaces() {
        const QString css = ThemeManager::resolveIcons("a{image:@icon( check , AccentText );}", ThemeMode::Light);
        QVERIFY2(!css.contains("@icon"), qPrintable(css));
        QVERIFY2(css.contains("light-check-AccentText.png"), qPrintable(css));
    }
    void malformedIconPlaceholderBecomesNoneAndWarnsOnce() {
        // Neither placeholder matches the pattern. One warning for the whole call (a second would fail the test).
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("@icon"));
        QCOMPARE(ThemeManager::resolveIcons("a{image:@icon(bad name!,Accent);} b{image:@icon(check);}", ThemeMode::Light),
                 QString("a{image:none;} b{image:none;}"));
    }
    void iconFilesAreWrittenCompletelyAndLeaveNoTemporaryFiles() {
        // Written twice: the second write replaces the first file.
        QString css;
        for (int i = 0; i < 2; ++i)
            css = ThemeManager::resolveIcons("a{image:@icon(check,AccentText);}", ThemeMode::Light);
        const QString path = QRegularExpression("url\\(\"([^\"]+)\"\\)").match(css).captured(1);
        QVERIFY2(!QImage(path).isNull(), qPrintable(path));
        const QStringList files = QFileInfo(path).dir().entryList(QDir::Files);
        for (const QString& name : files)
            QVERIFY2(name.endsWith(".png"), qPrintable(name));
    }
    void sameIconTwiceGivesTheSamePath() {
        const QString css = ThemeManager::resolveIcons("a{image:@icon(check,AccentText);} b{image:@icon(check,AccentText);}",
                                                      ThemeMode::Dark);
        const QRegularExpression url("url\\(\"([^\"]+)\"\\)");
        auto it = url.globalMatch(css);
        QVERIFY(it.hasNext());
        const QString first = it.next().captured(1);
        QVERIFY(it.hasNext());
        QCOMPARE(it.next().captured(1), first);
    }
    // The cache folder is made unusable by a plain file with its name: the icons go to the temp folder instead,
    // and when that is blocked too the placeholder becomes "none", never the raw "@icon" text (it would make
    // Qt drop the whole rule, so a checked box would look unchecked).
    void blockedCacheFolderFallsBackToTemp() {
        const QString cache = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
        QDir().mkpath(cache);
        const QString blockedCache = cache + "/themed-icons";
        QDir(blockedCache).removeRecursively();
        QFile cacheBlocker(blockedCache);
        QVERIFY(cacheBlocker.open(QIODevice::WriteOnly));
        cacheBlocker.close();

        const QString css = ThemeManager::resolveIcons("a{image:@icon(check,AccentText);}", ThemeMode::Light);
        QVERIFY2(!css.contains("@icon"), qPrintable(css));
        QVERIFY2(css.contains(QDir::fromNativeSeparators(QDir::tempPath())), qPrintable(css));
        const QRegularExpressionMatch match = QRegularExpression("url\\(\"([^\"]+)\"\\)").match(css);
        QVERIFY(match.hasMatch());
        QVERIFY(QFile::exists(match.captured(1)));

        // Block the temp folder too.
        QDir(QDir::tempPath() + "/planova-themed-icons").removeRecursively();
        QFile tempBlocker(QDir::tempPath() + "/planova-themed-icons");
        QVERIFY(tempBlocker.open(QIODevice::WriteOnly));
        tempBlocker.close();
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("themed-icons|cache"));
        const QString none = ThemeManager::resolveIcons("a{image:@icon(check,AccentText);}", ThemeMode::Light);
        QCOMPARE(none, QString("a{image:none;}"));

        tempBlocker.remove();
        cacheBlocker.remove();
    }
    void unknownIconBecomesNoneAndWarns() {
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("NoSuchIcon"));
        // "none" is valid qss; the raw placeholder would make Qt drop the rule.
        QCOMPARE(ThemeManager::resolveIcons("x:@icon(NoSuchIcon,AccentText);", ThemeMode::Light), QString("x:none;"));
    }
    void applyResolvesIconsInTheStyleSheet() {
        ThemeManager::instance().apply(ThemeMode::Light);
        QVERIFY(!qApp->styleSheet().contains("@icon"));
        QVERIFY(qApp->styleSheet().contains("light-check-AccentText.png"));
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
        // The app must not bundle Inter or Space Grotesk. The system may have its own "Inter", so the
        // bundled files are checked, not the installed families.
        const QStringList bundled = QDir(":/fonts").entryList(QDir::Files);
        QVERIFY(!bundled.isEmpty());
        for (const QString& name : bundled) {
            QVERIFY2(!name.startsWith("Inter", Qt::CaseInsensitive), qPrintable(name));
            QVERIFY2(!name.startsWith("SpaceGrotesk", Qt::CaseInsensitive), qPrintable(name));
        }
        const QStringList families = QFontDatabase::families();
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
    void roleForTextStyles() {
        // Caption, Body, Label and LabelActive are body text (so the active nav item keeps its width); Heading
        // and PageTitle are titles. Tested on the role itself: both families can resolve to the same font on a
        // machine without Segoe UI Variable.
        QCOMPARE(int(ThemeManager::roleFor(TextStyle::Caption)), int(FontRole::Body));
        QCOMPARE(int(ThemeManager::roleFor(TextStyle::Body)), int(FontRole::Body));
        QCOMPARE(int(ThemeManager::roleFor(TextStyle::Label)), int(FontRole::Body));
        QCOMPARE(int(ThemeManager::roleFor(TextStyle::LabelActive)), int(FontRole::Body));
        QCOMPARE(int(ThemeManager::roleFor(TextStyle::Heading)), int(FontRole::Display));
        QCOMPARE(int(ThemeManager::roleFor(TextStyle::PageTitle)), int(FontRole::Display));
    }
    void textFontUsesTheRoleFamily() {
        auto& t = ThemeManager::instance();
        for (TextStyle style : {TextStyle::Caption, TextStyle::Body, TextStyle::Label, TextStyle::LabelActive,
                                TextStyle::Heading, TextStyle::PageTitle}) {
            QCOMPARE(t.textFont(style).family(), t.font(ThemeManager::roleFor(style), 14).family());
        }
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
