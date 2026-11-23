#pragma once
#include <QPixmap>
#include <QWidget>

class QVariantAnimation;

// Frameless glass card shown while the app starts: a blurred copy of the screen behind it, the word "Welcome!"
// being written and a one-line subtitle. Purely decorative: a click or any key press skips to the end.
class SplashWindow : public QWidget {
    Q_OBJECT
public:
    explicit SplashWindow(QWidget* parent = nullptr);

    // Grabs and blurs the screen behind the card, shows it and runs the timeline.
    void start();

signals:
    void finished();

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;

private:
    void skipToEnd();
    void onTick(int elapsedMs);

    QPixmap m_backdrop;               // blurred screen behind the card; empty when the grab failed
    int m_elapsedMs = 0;
    QVariantAnimation* m_animation = nullptr;
};
