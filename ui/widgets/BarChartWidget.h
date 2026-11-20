#ifndef BARCHARTWIDGET_H
#define BARCHARTWIDGET_H

#include <QColor>
#include <QSize>
#include <QString>
#include <QWidget>

#include <array>
#include <vector>

class QPaintEvent;

// Colours are Q_PROPERTYs set from home.qss / tracking.qss with qproperty-*.
// No colour literals live in C++.
class BarChartWidget : public QWidget
{
    Q_OBJECT

    Q_PROPERTY(QColor barColor READ barColor WRITE setBarColor)
    Q_PROPERTY(QColor trackColor READ trackColor WRITE setTrackColor)
    Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor)
    Q_PROPERTY(QColor mutedColor READ mutedColor WRITE setMutedColor)

    Q_PROPERTY(QColor member1Color READ member1Color WRITE setMember1Color)
    Q_PROPERTY(QColor member2Color READ member2Color WRITE setMember2Color)
    Q_PROPERTY(QColor member3Color READ member3Color WRITE setMember3Color)
    Q_PROPERTY(QColor member4Color READ member4Color WRITE setMember4Color)
    Q_PROPERTY(QColor member5Color READ member5Color WRITE setMember5Color)
    Q_PROPERTY(QColor member6Color READ member6Color WRITE setMember6Color)

public:
    struct Bar
    {
        QString label;
        double value = 0.0;

        // 0..5 selects member1Color..member6Color.
        // -1 (default) uses barColor, e.g. the complexity chart.
        int memberIndex = -1;
    };

    explicit BarChartWidget(QWidget* parent = nullptr);

    void setBars(
        const std::vector<Bar>& newBars,
        const QString& newUnit = QString()
        );

    QColor barColor() const { return m_barColor; }
    QColor trackColor() const { return m_trackColor; }
    QColor textColor() const { return m_textColor; }
    QColor mutedColor() const { return m_mutedColor; }

    QColor member1Color() const { return m_memberColors[0]; }
    QColor member2Color() const { return m_memberColors[1]; }
    QColor member3Color() const { return m_memberColors[2]; }
    QColor member4Color() const { return m_memberColors[3]; }
    QColor member5Color() const { return m_memberColors[4]; }
    QColor member6Color() const { return m_memberColors[5]; }

    void setBarColor(const QColor& color);
    void setTrackColor(const QColor& color);
    void setTextColor(const QColor& color);
    void setMutedColor(const QColor& color);

    void setMember1Color(const QColor& color) { setMemberColor(0, color); }
    void setMember2Color(const QColor& color) { setMemberColor(1, color); }
    void setMember3Color(const QColor& color) { setMemberColor(2, color); }
    void setMember4Color(const QColor& color) { setMemberColor(3, color); }
    void setMember5Color(const QColor& color) { setMemberColor(4, color); }
    void setMember6Color(const QColor& color) { setMemberColor(5, color); }

    // Value that fills a whole bar when every value is smaller (0 = scale to the largest).
    void setMinScale(double value);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void setMemberColor(int index, const QColor& color);
    QColor colorFor(const Bar& bar) const;
    QString valueText(double value) const;

    std::vector<Bar> bars;
    QString unit;
    double m_minScale = 0.0;

    QColor m_barColor;
    QColor m_trackColor;
    QColor m_textColor;
    QColor m_mutedColor;
    std::array<QColor, 6> m_memberColors;
};

#endif