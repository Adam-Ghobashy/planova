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
    // Font for a role. Falls back to Segoe UI (Consolas for Mono) when the bundled family is missing.
    QFont font(FontRole role, int pointSize) const;
    // Pure helper behind font(): the wanted family if it is in `available`, otherwise the fallback.
    static QString familyFor(FontRole role, const QStringList& available);

    // Replaces @TokenName with the token's hex value. Unknown names are left as they are and logged with qWarning.
    static QString resolveTokens(const QString& css, ThemeMode mode);

    // Saved choice ("ui/theme" in QSettings), otherwise the operating system colour scheme.
    static ThemeMode savedOrSystemMode();
    void saveMode() const;

signals:
    void themeChanged(ThemeMode mode);

private:
    ThemeManager() = default;

    ThemeMode m_mode = ThemeMode::Light;
};
