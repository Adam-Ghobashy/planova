#include "slicer/AssigneeDelegate.h"
#include "slicer/SlicerTableDelegate.h"
#include "theme/ThemeManager.h"
#include <QComboBox>
#include <QPainter>
#include <QTimer>

namespace planova::slicer {

AssigneeDelegate::AssigneeDelegate(const std::vector<MemberDto>& members, QObject* parent)
    : QStyledItemDelegate(parent), members_(members) {}

QWidget* AssigneeDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& /*option*/,
                                        const QModelIndex& /*index*/) const {
    auto* combo = new QComboBox(parent);
    combo->setObjectName("overrideCombo");
    for (const auto& m : members_) {
        combo->addItem(QString::fromStdString(m.name), QString::fromStdString(m.id));
    }
    // Picking a member saves it straight away and closes the editor. The signals need a
    // non-const delegate, which is safe here because the view owns it.
    auto* self = const_cast<AssigneeDelegate*>(this);
    connect(combo, QOverload<int>::of(&QComboBox::activated), self, [self, combo] {
        emit self->commitData(combo);
        emit self->closeEditor(combo);
    });
    QTimer::singleShot(0, combo, &QComboBox::showPopup);  // one click opens the list
    return combo;
}

void AssigneeDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const {
    auto* combo = qobject_cast<QComboBox*>(editor);
    if (!combo) return;

    QString currentId = index.data(Qt::EditRole).toString();
    int idx = combo->findData(currentId);
    if (idx >= 0) {
        combo->setCurrentIndex(idx);
    }
}

void AssigneeDelegate::setModelData(QWidget* editor, QAbstractItemModel* model,
                                    const QModelIndex& index) const {
    auto* combo = qobject_cast<QComboBox*>(editor);
    if (!combo || !model) return;

    model->setData(index, combo->currentData(), Qt::EditRole);
}

void AssigneeDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                             const QModelIndex& index) const {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    auto& tm = ThemeManager::instance();
    const QRect rect = option.rect;

    painter->fillRect(rect, slicerRowBackground(option, index));
    painter->setPen(QPen(tm.color(Token::SurfaceAlt), 1));
    painter->drawLine(rect.bottomLeft(), rect.bottomRight());

    // Combobox pill
    int comboW = 120;
    int comboH = 28;
    QRectF comboRect(rect.left() + 8, rect.top() + (rect.height() - comboH) / 2.0, comboW, comboH);

    painter->setPen(QPen(tm.color(Token::Border), 1));
    painter->setBrush(tm.color(Token::Surface));
    painter->drawRoundedRect(comboRect, 6, 6);

    // Text
    QString text = index.data(Qt::DisplayRole).toString();
    QFont font = painter->font();
    font.setPointSize(9);
    painter->setFont(font);
    painter->setPen(tm.color(Token::TextPrimary));
    painter->drawText(comboRect.adjusted(8, 0, -20, 0), Qt::AlignLeft | Qt::AlignVCenter, text);

    // Chevron down arrow
    painter->setPen(QPen(tm.color(Token::TextMuted), 1.5));
    qreal ax = comboRect.right() - 12;
    qreal ay = comboRect.center().y();
    painter->drawLine(QPointF(ax - 3, ay - 1), QPointF(ax, ay + 2));
    painter->drawLine(QPointF(ax, ay + 2), QPointF(ax + 3, ay - 1));

    painter->restore();
}

QSize AssigneeDelegate::sizeHint(const QStyleOptionViewItem& /*option*/,
                                 const QModelIndex& /*index*/) const {
    return QSize(140, 56);
}

} // namespace planova::slicer
