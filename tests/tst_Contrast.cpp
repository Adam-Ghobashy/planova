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

// Paints `over` on top of `under` (normal "source over" alpha compositing) and returns the visible colour.
QColor blend(const QColor& over, const QColor& under)
{
    const double ao = over.alphaF();
    const double au = under.alphaF();
    const double outAlpha = ao + au * (1.0 - ao);
    if (outAlpha <= 0.0)
        return QColor(0, 0, 0, 0);
    auto channel = [&](double o, double u) { return (o * ao + u * au * (1.0 - ao)) / outAlpha; };
    QColor result;
    result.setRgbF(float(channel(over.redF(), under.redF())), float(channel(over.greenF(), under.greenF())),
                   float(channel(over.blueF(), under.blueF())), float(outAlpha));
    return result;
}

// The brightest point of the backdrop: the centre of the primary glow, over the top of the gradient.
QColor glowPoint(bool dark)
{
    return blend(tokenColor(dark, Token::GlowPrimary), tokenColor(dark, Token::BackdropTop));
}

// A glass panel over that brightest point. Text on glass must stay readable even there.
QColor glassBase(bool dark)
{
    return blend(tokenColor(dark, Token::GlassFill), glowPoint(dark));
}

QColor glassStrongBase(bool dark)
{
    return blend(tokenColor(dark, Token::GlassFillStrong), glowPoint(dark));
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

    void blendKnownValues()
    {
        const QColor halfWhite(255, 255, 255, 128);
        const QColor mixed = blend(halfWhite, Qt::black);
        QVERIFY(qAbs(mixed.red() - 128) <= 1);
        QCOMPARE(mixed.alpha(), 255);
        QCOMPARE(blend(QColor(Qt::red), Qt::blue), QColor(Qt::red));   // an opaque colour hides what is under it
    }

    void textPairsReachAA_data()
    {
        QTest::addColumn<bool>("dark");
        QTest::addColumn<Token>("foreground");
        QTest::addColumn<QColor>("background");

        for (bool dark : {false, true}) {
            const char* mode = dark ? "dark" : "light";
            // Adds the pair on its solid surface. Text that sits on Background, Surface or SurfaceAlt will sit on
            // glass panels too, so it is also checked on a glass panel and a strong glass panel at the glow point.
            auto addPair = [&](Token text, Token surface) {
                const QByteArray name = QByteArray(mode) + " " + kTokenNames[int(text)] + " on "
                                        + kTokenNames[int(surface)];
                QTest::newRow(name.constData()) << dark << text << tokenColor(dark, surface);
                if (surface == Token::Background || surface == Token::Surface || surface == Token::SurfaceAlt) {
                    QTest::newRow((name + " as glass").constData()) << dark << text << glassBase(dark);
                    QTest::newRow((name + " as strong glass").constData()) << dark << text << glassStrongBase(dark);
                }
            };
            for (Token text : {Token::TextPrimary, Token::TextSecondary, Token::TextMuted}) {
                for (Token surface : {Token::Background, Token::Surface, Token::SurfaceAlt})
                    addPair(text, surface);
            }
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
        QFETCH(QColor, background);
        const double ratio = contrast(tokenColor(dark, foreground), background);
        QVERIFY2(ratio >= 4.5, qPrintable(QString("contrast %1").arg(ratio, 0, 'f', 2)));
    }
};

QTEST_MAIN(TstContrast)
#include "tst_Contrast.moc"
