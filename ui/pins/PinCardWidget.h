#pragma once
#include <QFrame>
#include <QPoint>
#include <QPointer>
#include <functional>
#include <map>

#include "planova/pins/PinBoardApi.h"

class QVBoxLayout;
class QLineEdit;
class QToolButton;
class QPropertyAnimation;
class QParallelAnimationGroup;

namespace planova::s5 {

class StyledComboBox;

// One pin. Drag source for moving it between columns. All colours come from pins.qss
// through the dynamic properties `category` ("handoff|blocker|testing|architecture") and `dragging`.
class PinCardWidget : public QFrame {
    Q_OBJECT
    Q_PROPERTY(int slide READ slide WRITE setSlide)
public:
    // Plan signature (section 7): pin only, author shown as "Unknown", no comments or task line.
    explicit PinCardWidget(const PinDto& pin, QWidget* parent = nullptr);
    PinCardWidget(const PinView& view, const std::map<Id, QString>& memberNames, int memberIndex,
                  const QString& taskTitle, qint64 nowMs, QWidget* parent = nullptr);

    const Id& pinId() const { return id_; }

    void setCommentAuthor(const Id& memberId);  // preselect "comment as"; ignored if unknown
    void setCommentsExpanded(bool expanded);  // no animation, used when the page rebuilds cards
    bool commentsExpanded() const;

    void animateIn(int delayMs);                  // fade + small slide
    // Fade out, then done(). done() runs exactly once, also if the card is deleted mid-fade.
    void animateOut(std::function<void()> done);

    int slide() const { return slide_; }
    void setSlide(int px);

signals:
    void resolveRequested(const Id& pinId);
    void commentSubmitted(const Id& pinId, const QString& text, const Id& authorId);
    void editRequested(const Id& pinId);
    void deleteRequested(const Id& pinId);
    void commentsToggled(const Id& pinId, bool expanded);

protected:
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;

private:
    void animatePanel(bool open);
    int panelTargetHeight() const;
    void submitComment();
    void startDrag();

    Id id_;
    QVBoxLayout* root_ = nullptr;
    QFrame* panel_ = nullptr;
    StyledComboBox* author_ = nullptr;
    QLineEdit* input_ = nullptr;
    QToolButton* chip_ = nullptr;
    QPointer<QPropertyAnimation> panelAnim_;
    QPointer<QParallelAnimationGroup> inGroup_;
    int slide_ = 0;
    QPoint pressPos_;
    bool dragArmed_ = false;
};

}  // namespace planova::s5
