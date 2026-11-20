#pragma once
#include "Tokens.h"
#include <QColor>
#include <QObject>
#include <QString>

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

    // Replaces @TokenName with the token's hex value. Unknown names are left as they are.
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
