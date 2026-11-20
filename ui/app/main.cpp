#include "app/CoreBridge.h"
#include "app/MainWindow.h"
#include "planova/PlanovaCore.h"
#include "theme/ThemeManager.h"

#include <QApplication>
#include <QStandardPaths>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    Q_INIT_RESOURCE(planova);
    ThemeManager::instance().apply(ThemeMode::Light);

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
    window.show();

    return app.exec();
}