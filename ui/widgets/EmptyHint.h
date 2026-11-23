#pragma once
#include <QLabel>

// One-line message for a panel that has nothing to show yet. Styled by base.qss (QLabel#EmptyHint).
// Say what to do next, e.g. "No tasks yet. Use '+ New Task' to add the first one."
inline QLabel* makeEmptyHint(const QString& text, QWidget* parent = nullptr)
{
    auto* label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("EmptyHint"));
    label->setAlignment(Qt::AlignCenter);
    label->setWordWrap(true);
    return label;
}
