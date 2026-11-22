#pragma once
#include "Page.h"
#include "planova/PlanovaCore.h"
#include <QList>
#include <QWidget>

class QLabel;
class QVBoxLayout;
class QPushButton;

// Left navigation: brand row, project switcher, the page items in workflow order (three sections),
// a one-line git footer and the theme toggle.
class Sidebar : public QWidget {
    Q_OBJECT
public:
    explicit Sidebar(PlanovaCore& core, QWidget* parent = nullptr);

    // The pages of the sidebar items, top to bottom (Ctrl+1..7 follow this order).
    static QList<Page> navOrder();

    // Marks the item of this page as active. Hub marks the project switcher. Setup leaves none active.
    void setActivePage(Page page);

    // The switcher shows this name; an empty name reads "No project open".
    void setProjectName(const QString& name);

    // Without an open project every page item is disabled, with a tooltip that says why.
    void setProjectOpen(bool open);

public slots:
    // Re-reads the project, the badge counts and the git footer from the core.
    void refresh();

signals:
    void pageRequested(Page page);

private:
    enum class Badge { None, OpenPins, Tasks };

    struct Item {
        QString heading;
        Page page;
        QString label;
        QString iconName;
        Badge badge;
        QPushButton* button = nullptr;
        QLabel* icon = nullptr;
        QLabel* badgeLabel = nullptr;
    };

    // The one list of items: labels, order and headings all come from here.
    static QList<Item> navItems();

    void buildBrand(QVBoxLayout* layout);
    QWidget* buildProjectSwitcher();
    void addHeading(QVBoxLayout* layout, const QString& title);
    void addItem(QVBoxLayout* layout, Item item);
    QWidget* buildGitCard();
    QWidget* buildThemeToggle();
    void updateGitCard();
    void updateThemeToggle();
    void updateIcons();
    void updateBadges();
    int badgeCount(Badge badge) const;

    PlanovaCore& core_;
    QList<Item> items_;
    Page activePage_ = Page::Hub;
    QPushButton* switcher_ = nullptr;
    QLabel* switcherIcon_ = nullptr;
    QLabel* switcherName_ = nullptr;
    QLabel* switcherChevron_ = nullptr;
    QLabel* gitDot_ = nullptr;
    QLabel* gitText_ = nullptr;
    QPushButton* gitSyncButton_ = nullptr;
    QPushButton* themeToggle_ = nullptr;
};
