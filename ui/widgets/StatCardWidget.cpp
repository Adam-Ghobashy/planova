#include "StatCardWidget.h"

#include "theme/ThemeManager.h"

#include <QEasingCurve>
#include <QFont>
#include <QLabel>
#include <QSizePolicy>
#include <QStyle>
#include <QVBoxLayout>
#include <QVariantAnimation>

#include <cstdlib>

StatCardWidget::StatCardWidget(
    const QString& label,
    QWidget* parent
    )
    : QFrame(parent),
    m_targetText(QStringLiteral("0"))
{
    setObjectName("statCardWidget");

    setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Preferred
        );

    // No fixed minimum height: the layout's own minimum includes the caption, so a short window never
    // squeezes the number.

    titleLabel = new QLabel(label, this);
    valueLabel = new QLabel(m_targetText, this);

    titleLabel->setObjectName("statCardTitle");
    valueLabel->setObjectName("statCardValue");

    // Display family at the size home.qss gives it. Segoe UI digits all have the same width,
    // so the count-up does not jitter.
    QFont valueFont = ThemeManager::instance().font(FontRole::Display, 26);
    valueFont.setWeight(QFont::DemiBold);
    valueLabel->setFont(valueFont);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 16, 20, 16);
    layout->setSpacing(6);
    layout->addWidget(titleLabel);
    layout->addWidget(valueLabel);
    layout->addStretch(1);

    m_animation = new QVariantAnimation(this);
    m_animation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_animation, &QVariantAnimation::valueChanged, this,
            [this](const QVariant& value) { showDisplayed(value.toInt()); });

    setValueRole(ValueRole::Default);
}

void StatCardWidget::showDisplayed(int value)
{
    // Only touch the label when the visible number actually changes.
    if (value == m_displayed && valueLabel->text() == QString::number(value))
        return;

    m_displayed = value;
    valueLabel->setText(QString::number(value));
}

void StatCardWidget::setValue(const QString& text)
{
    // Unchanged value: nothing to do (no repaint, no animation restart).
    if (text == m_targetText)
        return;

    m_targetText = text;

    bool ok = false;
    const int target = text.toInt(&ok);

    if (!ok)
    {
        // Non-integer text (e.g. "85%") is shown as is.
        m_animation->stop();
        m_numeric = false;
        valueLabel->setText(text);
        return;
    }

    const int from = m_displayed;
    const bool animate = m_numeric && isVisible() && from != target;

    m_numeric = true;
    m_animation->stop();

    if (!animate)
    {
        m_displayed = target;
        valueLabel->setText(QString::number(target));
        return;
    }

    // Short ramps stay quick, large jumps take a little longer (max 600 ms).
    const int duration = qBound(250, 180 + std::abs(target - from) * 25, 600);

    m_animation->setDuration(duration);
    m_animation->setStartValue(from);
    m_animation->setEndValue(target);
    m_animation->start();
}

void StatCardWidget::setCaption(const QString& text)
{
    if (!captionLabel)
    {
        // Created on first use, so cards that never get a caption are unchanged.
        // Appended after the layout's stretch, so it sits at the bottom of the card.
        captionLabel = new QLabel(this);
        captionLabel->setObjectName("statCardCaption"); // styled in tracking.qss and home.qss
        layout()->addWidget(captionLabel);
    }

    captionLabel->setText(text);
    captionLabel->setVisible(!text.isEmpty());
}

void StatCardWidget::setValueRole(ValueRole role)
{
    QString roleName;

    switch (role)
    {
    case ValueRole::Accent:
        roleName = "accent";
        break;

    case ValueRole::Success:
        roleName = "success";
        break;

    case ValueRole::Warning:
        roleName = "warning";
        break;

    case ValueRole::Default:
    default:
        roleName = "default";
        break;
    }

    valueLabel->setProperty("valueRole", roleName);

    // Re-evaluate the [valueRole="..."] selector.
    valueLabel->style()->unpolish(valueLabel);
    valueLabel->style()->polish(valueLabel);
    valueLabel->update();
}
