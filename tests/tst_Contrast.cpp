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
            addPair(Token::Accent, Token::AccentSoft);        }
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
