#pragma once
#include <QDialog>
#include <QString>

class QEvent;
class QFrame;
class QHBoxLayout;
class QPaintEvent;
class QPushButton;
class QResizeEvent;
class QScrollArea;
class QShowEvent;
class QVariantAnimation;
class QVBoxLayout;
class QWidget;

// Base class of every dialog and message box. Frameless, covers the parent's window with the overlay
// and shows a centred card (header, body, footer). Fields go into contentLayout(), buttons into addButton().
// The card and the overlay are painted from theme tokens; the child widgets are styled in modal.qss.
class ModalDialog : public QDialog {
    Q_OBJECT
public:
    enum class Role { Primary, Secondary, Danger };

    ModalDialog(const QString& title, QWidget* parent);

    QVBoxLayout* contentLayout();
    // Primary and Danger accept, Secondary rejects. Object name "modal<Role>Button".
    // Buttons appear left to right in the order they are added (add Secondary first).
    QPushButton* addButton(const QString& text, Role role);

    // true when the Primary (or Danger) button was pressed. Cancel, Esc and the close button give false.
    static bool confirm(QWidget* parent, const QString& title, const QString& text,
                        const QString& okText = QStringLiteral("OK"), Role okRole = Role::Primary);
    static void info(QWidget* parent, const QString& title, const QString& text);

    void setVisible(bool visible) override;

protected:
    void paintEvent(QPaintEvent*) override;
    void showEvent(QShowEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void layoutCard();
    void focusFirstControl();

    QFrame* card_ = nullptr;
    QScrollArea* scroll_ = nullptr;
    QWidget* body_ = nullptr;
    QFrame* footer_ = nullptr;
    QHBoxLayout* footerLayout_ = nullptr;
    QVariantAnimation* pop_ = nullptr;
    qreal progress_ = 1.0;   // 0 = start of the pop animation, 1 = finished
};
