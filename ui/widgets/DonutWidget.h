#ifndef DONUTWIDGET_H
#define DONUTWIDGET_H

#include <QColor>
#include <QSize>
#include <QWidget>

class QPaintEvent;

// Colours are Q_PROPERTYs set from home.qss with qproperty-*.
// No colour literals live in C++.
class DonutWidget : public QWidget
{
    Q_OBJECT

    Q_PROPERTY(QColor doneColor READ doneColor WRITE setDoneColor)
    Q_PROPERTY(QColor activeColor READ activeColor WRITE setActiveColor)
    Q_PROPERTY(QColor trackColor READ trackColor WRITE setTrackColor)
    Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor)
    Q_PROPERTY(QColor mutedColor READ mutedColor WRITE setMutedColor)
    Q_PROPERTY(QColor secondaryColor READ secondaryColor WRITE setSecondaryColor)
    Q_PROPERTY(QColor outlineColor READ outlineColor WRITE setOutlineColor)

public:
    explicit DonutWidget(QWidget* parent = nullptr);

    // done / active (InProgress + Review) / todo
    void setValues(int done, int active, int todo);

    QColor doneColor() const { return m_doneColor; }
    QColor activeColor() const { return m_activeColor; }
    QColor trackColor() const { return m_trackColor; }
    QColor textColor() const { return m_textColor; }
    QColor mutedColor() const { return m_mutedColor; }
    QColor secondaryColor() const { return m_secondaryColor; }
    QColor outlineColor() const { return m_outlineColor; }

    void setDoneColor(const QColor& color);
    void setActiveColor(const QColor& color);
    void setTrackColor(const QColor& color);
    void setTextColor(const QColor& color);
    void setMutedColor(const QColor& color);
    void setSecondaryColor(const QColor& color);
    void setOutlineColor(const QColor& color);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    int doneValue;
    int activeValue;
    int todoValue;

    QColor m_doneColor;
    QColor m_activeColor;
    QColor m_trackColor;
    QColor m_textColor;
    QColor m_mutedColor;
    QColor m_secondaryColor;
    QColor m_outlineColor;
};

#endif
