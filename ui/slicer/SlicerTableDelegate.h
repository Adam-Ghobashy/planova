#pragma once
#include <QColor>
#include <QStyledItemDelegate>

namespace planova::slicer {

// Background for a table row: selected/highlighted rows use AccentSoft, hovered rows SurfaceAlt.
// Shared by both delegates so every cell in a row has the same background.
QColor slicerRowBackground(const QStyleOptionViewItem& option, const QModelIndex& index);

class SlicerTableDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit SlicerTableDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;
};

} // namespace planova::slicer
