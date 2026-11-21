#pragma once
#include "Page.h"
#include "planova/PlanovaCore.h"
#include <QList>
#include <QWidget>

class QLabel;
class QVBoxLayout;
class QPushButton;

// Left navigation: brand row, two sections of page items, git quick-sync card and the theme toggle.
class Sidebar : public QWidget {
    Q_OBJECT
public:
    explicit Sidebar(PlanovaCore& core, QWidget* parent = nullptr);

    // Marks the item of this page as active. A page without an item (Welcome, Setup) leaves none active.
    void setActivePage(Page page);

public slots:
    // Re-reads the badge counts and the git card from the core.
    void refresh();

signals:
    void pageRequested(Page page);

private:
    enum class Badge { None, OpenPins, Tasks };

    struct Item {
        Page page;
        QString label;
        QString iconName;
        Badge badge;
        QPushButton* button = nullptr;
        QLabel* icon = nullptr;
        QLabel* badgeLabel = nullptr;
    };

    void buildBrand(QVBoxLayout* layout);
    void addSection(QVBoxLayout* layout, const QString& title, const QList<Item>& items);
    QWidget* buildGitCard();
    QWidget* buildThemeToggle();
    void updateGitCard();
    void updateThemeToggle();
    void updateIcons();
    void updateBadges();
    int badgeCount(Badge badge) const;

    PlanovaCore& core_;
    QList<Item> items_;
    Page activePage_ = Page::Welcome;
    QLabel* gitDot_ = nullptr;
    QLabel* gitTitle_ = nullptr;
    QLabel* gitSubtitle_ = nullptr;
    QPushButton* gitSyncButton_ = nullptr;
    QPushButton* themeToggle_ = nullptr;
};
