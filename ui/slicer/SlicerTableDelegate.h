#pragma once
#include <QColor>
#include <QStyledItemDelegate>

namespace planova::slicer {

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

}
