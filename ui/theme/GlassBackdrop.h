#pragma once
#include <QPixmap>
#include <QWidget>

// The painted background behind the whole app: a vertical gradient (BackdropTop to BackdropBottom) with two soft
// glows. Glass panels on top of it are translucent, so this is what shows through them.
// The picture is cached and only rebuilt when the size or the theme changes, so a repaint is a single pixmap copy.
class GlassBackdrop : public QWidget {
    Q_OBJECT
public:
    explicit GlassBackdrop(QWidget* parent = nullptr);

    // The cached picture, sized to the widget size times the device pixel ratio. For tests.
    QPixmap cachedBackdrop() const;

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void rebuildCache();

    QPixmap cache_;
};
