#pragma once
#include <QWidget>

class QVariantAnimation;

// Writes "Welcome!" on, using the splash painter and timeline. The animation starts the first time the label
// is shown; later shows draw the finished word at once.
class HandwritingLabel : public QWidget {
    Q_OBJECT
public:
    // Length of the whole intro in ms: the hero (1.5 s), then the subtitle and the buttons fading in.
    static constexpr int kPlayMs = 1900;

    explicit HandwritingLabel(QWidget* parent = nullptr);

    bool isPlayed() const { return played_; }
    QSize sizeHint() const override;

signals:
    // Milliseconds into the intro, on every tick. The page uses it to fade the subtitle and buttons in.
    void frameChanged(int elapsedMs);

protected:
    void showEvent(QShowEvent*) override;
    void paintEvent(QPaintEvent*) override;

private:
    QVariantAnimation* animation_;
    int elapsedMs_ = 0;
    bool played_ = false;
};
