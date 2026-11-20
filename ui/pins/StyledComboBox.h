#pragma once
#include <QColor>
#include <QComboBox>

class QEnterEvent;
class QPaintEvent;
class QVariantAnimation;

namespace planova::s5 {

// QComboBox with a painted chevron (turns when the list is open), an opaque light popup.
// Object name is "PinCombo"; every colour comes from pins.qss:
//   qproperty-arrowColor / qproperty-arrowActiveColor. No colour literals here.
class StyledComboBox : public QComboBox {
    Q_OBJECT
    Q_PROPERTY(QColor arrowColor READ arrowColor WRITE setArrowColor)
    Q_PROPERTY(QColor arrowActiveColor READ arrowActiveColor WRITE setArrowActiveColor)
public:
    explicit StyledComboBox(QWidget* parent = nullptr);

    QColor arrowColor() const { return arrowColor_; }
    QColor arrowActiveColor() const { return arrowActiveColor_; }
    void setArrowColor(const QColor& c);
    void setArrowActiveColor(const QColor& c);

protected:
    void paintEvent(QPaintEvent* e) override;
    void enterEvent(QEnterEvent* e) override;
    void leaveEvent(QEvent* e) override;
    bool eventFilter(QObject* watched, QEvent* ev) override;

private:
    void animateTurn(qreal target);

    QWidget* popup_ = nullptr;  // the list's top-level window (owned by the combo)
    QVariantAnimation* turn_ = nullptr;
    qreal turnValue_ = 0.0;     // 0 = chevron down, 1 = chevron up
    bool open_ = false;
    QColor arrowColor_;
    QColor arrowActiveColor_;
};

}  // namespace planova::s5
