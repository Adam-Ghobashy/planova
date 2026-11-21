#include "app/CoreBridge.h"
#include "app/MainWindow.h"
#include "app/SplashWindow.h"
#include "planova/PlanovaCore.h"
#include "theme/ThemeManager.h"

#include <QApplication>
#include <QPropertyAnimation>
#include <QStandardPaths>

namespace {
constexpr int kWindowFadeMs = 200;
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    // QSettings needs these to find the saved theme choice.
    QCoreApplication::setOrganizationName("Planova");
    QCoreApplication::setApplicationName("Planova");
    Q_INIT_RESOURCE(planova);
    ThemeManager::instance().loadFonts();
    ThemeManager::instance().apply(ThemeManager::savedOrSystemMode());
    QApplication::setFont(ThemeManager::instance().font(FontRole::Body, 10));

    const QString documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const std::filesystem::path workspace = std::filesystem::path(documents.toStdString()) / "Planova";
    PlanovaCore core(workspace);

    // Reopen the most recent project so its data shows right away.
    // Only if it has its own event folder; otherwise stay on the root store so legacy data stays visible.
    const auto projects = core.projects().list();
    if (!projects.empty() && core.hasProjectData(projects.back().id))
        core.openProject(projects.back().id);

    CoreBridge bridge(core);
    MainWindow window(core, bridge);

    // The main window stays hidden while the splash plays, then fades in over it.
    auto* splash = new SplashWindow;
    QObject::connect(splash, &SplashWindow::finished, &window, [&window, splash]() {
        window.setWindowOpacity(0.0);
        window.show();
        window.raise();
        window.activateWindow();
        auto* fade = new QPropertyAnimation(&window, "windowOpacity", &window);
        fade->setDuration(kWindowFadeMs);
        fade->setStartValue(0.0);
        fade->setEndValue(1.0);
        fade->start(QAbstractAnimation::DeleteWhenStopped);
        splash->hide();
        splash->deleteLater();
    });
    splash->start();

    return app.exec();
}