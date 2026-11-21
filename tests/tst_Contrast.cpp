#include <QtTest>

#include "theme/Tokens.h"

#include <algorithm>
#include <cmath>

namespace {
// WCAG relative luminance of one 0..255 channel.
double channelLuminance(int value)
{
    const double c = value / 255.0;
    return c <= 0.03928 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
}

double luminance(const QColor& color)
{
    return 0.2126 * channelLuminance(color.red()) + 0.7152 * channelLuminance(color.green())
           + 0.0722 * channelLuminance(color.blue());
}

// WCAG contrast ratio, from 1 (same colour) to 21 (black on white).
double contrast(const QColor& a, const QColor& b)
{
    const double la = luminance(a);
    const double lb = luminance(b);
    return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
}

QColor tokenColor(bool dark, Token token)
{
    return QColor((dark ? kDarkHex : kLightHex)[static_cast<int>(token)]);
}
}

Q_DECLARE_METATYPE(Token)

class TstContrast : public QObject {
    Q_OBJECT
private slots:
    void helperKnownValues()
    {
        QVERIFY(qAbs(contrast(Qt::black, Qt::white) - 21.0) < 0.01);
        QVERIFY(qAbs(contrast(Qt::white, Qt::white) - 1.0) < 0.01);
    }

    void textPairsReachAA_data()
    {
        QTest::addColumn<bool>("dark");
        QTest::addColumn<Token>("foreground");
        QTest::addColumn<Token>("background");

        for (bool dark : {false, true}) {
            const char* mode = dark ? "dark" : "light";
            for (Token text : {Token::TextPrimary, Token::TextSecondary, Token::TextMuted}) {
                for (Token surface : {Token::Background, Token::Surface, Token::SurfaceAlt}) {
                    const QByteArray row = QByteArray(mode) + " " + kTokenNames[int(text)] + " on "
                                           + kTokenNames[int(surface)];
                    QTest::newRow(row.constData()) << dark << text << surface;
                }
            }
            auto addPair = [&](Token text, Token surface) {
                const QByteArray row = QByteArray(mode) + " " + kTokenNames[int(text)] + " on "
                                       + kTokenNames[int(surface)];
                QTest::newRow(row.constData()) << dark << text << surface;
            };
            addPair(Token::AccentText, Token::Accent);
            addPair(Token::DangerText, Token::DangerSoft);
            addPair(Token::WarningText, Token::WarningSoft);
            addPair(Token::ReviewText, Token::ReviewSoft);
            addPair(Token::NavActiveText, Token::NavActiveBg);
            // Welcome page: raised banner, hovered ghost button, current project card and its chip.
            for (Token surface : {Token::SurfaceRaised, Token::SurfaceHover, Token::AccentSoft}) {
                addPair(Token::TextPrimary, surface);
                addPair(Token::TextSecondary, surface);
            }
            addPair(Token::Accent, Token::Surface);
            addPair(Token::Accent, Token::AccentSoft);
            // Setup page: stepper, model cards, member cards, error text and summary.
            addPair(Token::Accent, Token::Background);          // current step label
            addPair(Token::PositiveText, Token::SuccessSoft);   // finished step check mark
            addPair(Token::DangerText, Token::Surface);         // field error text
            addPair(Token::DangerText, Token::Background);   // the other text pairs on Surface are covered above
            // Chaos Slicer table: match badge and Ready pill text.
            addPair(Token::TextPrimary, Token::SuccessSoft);
            // Board and Tracking: column count chips, skill and pin chips, the member efficiency chip.
            addPair(Token::DoneChipText, Token::SuccessSoft);
            addPair(Token::Accent, Token::AccentSoftStrong);
            addPair(Token::WarningText, Token::ReviewSoft);
            // Home: pin count line, quick link subtitles and the empty pins message.
            addPair(Token::TextTertiary, Token::Surface);
            addPair(Token::TextTertiary, Token::Background);
            // Pins: text on the four sticky-note tints, and the muted comment chip that stays TextMuted.
            addPair(Token::StickyGreenText, Token::PinHandoff);
            addPair(Token::StickyPinkText, Token::PinBlocker);
            addPair(Token::StickyYellowText, Token::PinTesting);
            addPair(Token::StickyBlueText, Token::PinArchitecture);
            for (Token tint : {Token::PinHandoff, Token::PinBlocker, Token::PinTesting, Token::PinArchitecture})
                addPair(Token::TextMuted, tint);
            // Track (pressed buttons, the Pins column count chip), SurfaceHover (hovered buttons, nav items) and the
            // status bar. TextMuted is not used on any of them (its pairs would fail in Light), apart from a disabled button.
            for (Token surface : {Token::Track, Token::StatusBar}) {   // TextPrimary and TextSecondary on SurfaceHover are above
                addPair(Token::TextPrimary, surface);
                addPair(Token::TextSecondary, surface);
            }
            // GitHub: the notice bar (TextSecondary on SurfaceAlt) and disabled buttons (TextMuted on SurfaceAlt)
            // are already covered by the general Text-on-Surface pairs above.
        }
    }

    void textPairsReachAA()
    {
        QFETCH(bool, dark);
        QFETCH(Token, foreground);
        QFETCH(Token, background);
        const double ratio = contrast(tokenColor(dark, foreground), tokenColor(dark, background));
        QVERIFY2(ratio >= 4.5, qPrintable(QString("contrast %1").arg(ratio, 0, 'f', 2)));
    }
};

QTEST_MAIN(TstContrast)
#include "tst_Contrast.moc"
