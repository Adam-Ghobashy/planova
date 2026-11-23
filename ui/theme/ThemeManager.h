#pragma once
#include "Tokens.h"
#include <QColor>
#include <QFont>
#include <QIcon>
#include <QObject>
#include <QString>
#include <QStringList>

enum class ThemeMode { Light, Dark };

enum class FontRole { Body, Display, Mono, Script };

// The one type scale (pixel size and weight):
// Caption 12/Medium, Body 14/Normal, Label 14/Medium, LabelActive 14/DemiBold, Heading 16/DemiBold,
// PageTitle 24/DemiBold. Caption, Body, Label and LabelActive use the Body family (so the active nav item
// keeps its width), Heading and PageTitle the Display family.
enum class TextStyle { Caption, Body, Label, LabelActive, Heading, PageTitle };

class ThemeManager : public QObject {
    Q_OBJECT
public:
    static ThemeManager& instance();

    // Resolves the qss files, sets the Fusion palette and emits themeChanged.
    void apply(ThemeMode mode);
    ThemeMode mode() const;
    // Switches to the other mode and remembers it for the next start.
    void toggle();

    QColor color(Token token) const;

    // Loads :/icons/<name>.svg, replaces currentColor with the token colour and renders it into a pixmap
    // twice the nominal 24 px size, so it stays sharp on high-density screens.
    QIcon icon(const QString& name, Token tint) const;

    // Registers every bundled font from :/fonts. Call once, before apply().
    void loadFonts();
    // Font for a role at a size in pixels, with the system's default hinting.
    QFont font(FontRole role, int pixelSize) const;
    // Font family role of one step of the type scale (see TextStyle).
    static FontRole roleFor(TextStyle style);
    // Font for one step of the type scale (see TextStyle).
    QFont textFont(TextStyle style) const;
    // Pure helper behind font(): the first candidate family for the role that is in `available`, otherwise
    // the fallback (Segoe UI, Consolas for Mono).
    static QString familyFor(FontRole role, const QStringList& available);

    // Replaces @TokenName with the token's hex value. Unknown names are left as they are and logged with qWarning.
    static QString resolveTokens(const QString& css, ThemeMode mode);

    // Replaces @icon(name,Token) with url("<png path>"). The icon is tinted with the token of `mode` and written
    // to <cache>/themed-icons/<mode>-<name>-<Token>.png (rewritten on every call), because a stylesheet can only
    // point at a file. The cache folder is used, otherwise the temp folder. A placeholder that cannot be resolved (missing
    // icon, unknown token, no writable folder) becomes "none" and is logged with qWarning, so it never reaches Qt as text.
    static QString resolveIcons(const QString& css, ThemeMode mode);

    // Saved choice ("ui/theme" in QSettings), otherwise the operating system colour scheme.
    static ThemeMode savedOrSystemMode();
    void saveMode() const;

signals:
    void themeChanged(ThemeMode mode);

private:
    ThemeManager() = default;

    ThemeMode m_mode = ThemeMode::Light;
};
