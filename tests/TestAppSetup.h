#pragma once
// Shared by the UI tests whose layout is asserted. The app runs with the Body text style (Segoe UI Variable, 14 px)
// (see ui/app/main.cpp); without this the tests would measure the platform default font instead.
#include "theme/ThemeManager.h"

#include <QApplication>
#include <QFont>

inline void installAppFonts()
{
    static bool done = false;
    if (done) return;
    done = true;
    Q_INIT_RESOURCE(planova);   // the fonts live in the Qt resource file
    ThemeManager::instance().loadFonts();
    QApplication::setFont(ThemeManager::instance().textFont(TextStyle::Body));
}
