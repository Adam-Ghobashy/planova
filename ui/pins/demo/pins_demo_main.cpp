// Standalone preview of the S5 Pins screen with in-memory fakes. Not part of the real app.
#include <QAction>
#include <QApplication>
#include <QFile>
#include <QMainWindow>
#include <QStyleFactory>
#include <QStyleHints>
#include <QToolBar>

#include "../DeclineDialog.h"
#include "../PinsPage.h"
#include "DemoPinBoard.h"

using namespace planova::s5;

namespace {
QString loadQss(const char* path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()) : QString();
}
}  // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    app.setStyle(QStyleFactory::create("Fusion"));
    app.styleHints()->setColorScheme(Qt::ColorScheme::Light);
    app.setStyleSheet(loadQss(":/themes/light/base.qss") + loadQss(":/themes/light/pins.qss"));

    demo::DemoMemberQuery members;
    demo::DemoTaskQuery tasks;
    demo::DemoPinBoard board(tasks);

    QMainWindow win;
    win.setWindowTitle("Planova S5 Pins demo");
    auto* page = new PinsPage(board, board, members, tasks, &win);
    win.setCentralWidget(page);

    auto* bar = win.addToolBar("Demo");
    QAction* preview = bar->addAction("Preview decline dialog");
    QObject::connect(preview, &QAction::triggered, &win, [&] {
        const auto task = tasks.all().front();
        const auto member = members.byId(*task.assignee);
        DeclineDialog dlg(task.id, member->id, QString::fromStdString(task.title),
                          QString::fromStdString(member->name), board, &win);
        QObject::connect(&dlg, &DeclineDialog::openSlicerRequested, &win, [] { /* demo: no slicer */ });
        if (dlg.exec() == QDialog::Accepted) page->refresh();
    });

    win.resize(1280, 800);
    win.show();
    return app.exec();
}
