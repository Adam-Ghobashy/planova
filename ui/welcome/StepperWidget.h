#pragma once
#include <QStringList>
#include <QWidget>

// Numbered circles joined by a line, as in the setup page of the prototype. The current step is filled with
// the accent colour, finished steps show a check mark, later steps show their number on an empty circle.
// Painted from theme tokens; it repaints when the theme changes. It is a progress display, not a control.
class StepperWidget : public QWidget {
    Q_OBJECT
public:
    explicit StepperWidget(QWidget* parent = nullptr);

    void setSteps(const QStringList& labels);
    void setCurrent(int index);
    int current() const { return current_; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent*) override;

private:
    QStringList labels_;
    int current_ = 0;
};
