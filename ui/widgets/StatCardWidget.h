#ifndef STATCARDWIDGET_H
#define STATCARDWIDGET_H

#include <QFrame>
#include <QString>

class QLabel;
class QVariantAnimation;

// Label + big number card (Home and Tracking).
// Integer values count up/down smoothly when the card is visible; unchanged
// values and the very first value never animate.
class StatCardWidget : public QFrame
{
    Q_OBJECT

public:
    enum class ValueRole
    {
        Default,
        Accent,
        Success,
        Warning
    };

    explicit StatCardWidget(
        const QString& label,
        QWidget* parent = nullptr
        );

    void setValue(const QString& text);
    void setValueRole(ValueRole role);

    // Optional small line under the number (Fair Tracking). Empty text hides it.
    void setCaption(const QString& text);

private:
    void showDisplayed(int value);

    QLabel* titleLabel;
    QLabel* valueLabel;
    QLabel* captionLabel = nullptr;
    QVariantAnimation* m_animation;

    QString m_targetText;   // last text requested through setValue()
    int m_displayed = 0;    // integer currently shown (while numeric)
    bool m_numeric = true;  // is the shown text an integer?
};

#endif
