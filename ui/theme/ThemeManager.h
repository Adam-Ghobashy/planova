#pragma once
#include "Tokens.h"
#include <QColor>
#include <QString>

enum class ThemeMode { Light };

class ThemeManager {
public:
    static ThemeManager& instance();
    void apply(ThemeMode mode);
    QColor color(Token token) const;
};