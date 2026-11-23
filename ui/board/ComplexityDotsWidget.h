#pragma once
#include <QWidget>

class ComplexityDotsWidget : public QWidget {
    Q_OBJECT
public:
    explicit ComplexityDotsWidget(int level, QWidget* parent = nullptr);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent*) override;

private:
    int level_;
};