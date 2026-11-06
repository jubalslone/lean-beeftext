/// \file
/// \author
///
/// \brief Implementation of a delegate for the picker items.
///
/// Copyright (c) . All rights reserved.
/// Licensed under the MIT License. See LICENSE file in the project root for full license information.


#include "stdafx.h"
#include "PickerItemDelegate.h"
#include "Combo/Combo.h"
#include "Emoji/Emoji.h"
#include "BeeftextConstants.h"
#include <algorithm>
#include <cmath>


namespace {
qint32 kItemVMargin = 2; ///< The size of the vertical margin of an item.
qint32 kItemHMargin = 10; ///< The size of the horizontal margin of an item.


//****************************************************************************************************************************************************
/// \brief Relative luminance of a color as defined by WCAG, in the range [0, 1].
//****************************************************************************************************************************************************
qreal relativeLuminance(QColor const &color) {
    auto const channel = [](qreal value) {
        return value <= 0.03928 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * channel(color.redF()) + 0.7152 * channel(color.greenF()) + 0.0722 * channel(color.blueF());
}


//****************************************************************************************************************************************************
/// \brief WCAG contrast ratio between two colors, in the range [1, 21].
//****************************************************************************************************************************************************
qreal contrastRatio(QColor const &a, QColor const &b) {
    qreal const la = relativeLuminance(a);
    qreal const lb = relativeLuminance(b);
    return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
}


//****************************************************************************************************************************************************
/// \brief Return a text color that is guaranteed to be readable on the given background.
///
/// The picker is a frameless window whose palette can come from the application stylesheet machinery rather than
/// from the live Windows light/dark palette. When the two disagree, the foreground and background roles can both
/// resolve to dark colors. The delegate therefore never trusts a palette text role blindly: if the preferred color
/// does not contrast with the background it is painting on, a plain black or white text color is used instead.
///
/// \param[in] preferred The palette color the delegate would like to use.
/// \param[in] background The color the text is painted on.
/// \param[in] minimumRatio The minimum acceptable WCAG contrast ratio.
//****************************************************************************************************************************************************
QColor readableTextColor(QColor const &preferred, QColor const &background, qreal minimumRatio) {
    if (contrastRatio(preferred, background) >= minimumRatio)
        return preferred;
    return relativeLuminance(background) < 0.5 ? QColor(Qt::white) : QColor(Qt::black);
}


}


//****************************************************************************************************************************************************
/// \return The big font used in item rendering.
//****************************************************************************************************************************************************
QFont bigFont() {
    QFont font;
    font.setPointSize(12);
    font.setBold(true);
    font.setItalic(false);
    return font;
}


//****************************************************************************************************************************************************
/// \return The small font used in item rendering.
//****************************************************************************************************************************************************
QFont smallFont() {
    QFont font;
    font.setPointSize(8);
    font.setBold(true);
    font.setItalic(false);
    return font;
}


//****************************************************************************************************************************************************
/// \param[in] painter The painter.
/// \param[in] option The style options.
/// \param[in] index The index of the item to paint.
//****************************************************************************************************************************************************
void PickerItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const {
    // Prefer the live palette of the view we are painting into; option.palette may be stale.
    QPalette const &palette = option.widget ? option.widget->palette() : option.palette;
    bool const selected = option.state & QStyle::State_Selected;
    QColor const bgColor = selected ? palette.color(QPalette::Highlight) : palette.color(QPalette::Base);
    QColor const bigTextColor = readableTextColor(
        selected ? palette.color(QPalette::HighlightedText) : palette.color(QPalette::Text), bgColor, 4.5);
    QColor smallTextColor = selected ? bigTextColor : palette.color(QPalette::PlaceholderText);
    smallTextColor = readableTextColor(smallTextColor, bgColor, 3.0);

    bool const isEmoji = (constants::Emoji == index.data(constants::TypeRole).value<constants::EITemType>());
    SpEmoji emoji = isEmoji ? index.data(constants::PointerRole).value<SpEmoji>() : nullptr;
    SpCombo combo = isEmoji ? nullptr : index.data(constants::PointerRole).value<SpCombo>();
    bool const ok = isEmoji ? !!emoji : !!combo;
    Q_ASSERT(ok);
    if (!ok)
        return;

    // we draw the background. Color depends on wether the item is selected or not.
    painter->save();
    painter->setBrush(bgColor);
    painter->setPen(Qt::NoPen);
    painter->drawRect(option.rect);

    // we exclude the margin from the rendering rect.
    QRect const rect = option.rect.adjusted(kItemHMargin, kItemVMargin, -kItemHMargin, -kItemVMargin);

    // First line use big font.
    QFont const bFont = bigFont();
    QFontMetrics const bMetrics(bFont);
    // second line use small font.
    QFont const sFont = smallFont();

    QString const nameText = bMetrics.elidedText(index.data(Qt::DisplayRole).toString(), Qt::ElideRight, rect.width());
    qint32 const nameYPos = rect.top() + bMetrics.ascent();
    qint32 const nameWidth = bMetrics.boundingRect(nameText).width();
    painter->setFont(bFont);
    painter->setPen(bigTextColor);
    painter->drawText(QPoint(rect.left(), nameYPos), nameText);

    painter->setPen(smallTextColor);
    painter->setFont(sFont);
    QFontMetrics const sMetrics(sFont);

    // draw keyword
    qint32 yPos = rect.bottom() - sMetrics.descent();
    painter->drawText(QPoint(rect.left(), yPos), isEmoji ? emoji->shortcode() : combo->keyword());

    // draw group name
    QString groupName = (isEmoji ? tr("Emojis") : (combo->group() ? combo->group()->name() : QString()));
    if (!groupName.trimmed().isEmpty()) {
        qint32 const groupNameMaxWidth = rect.width() - nameWidth - kItemHMargin;
        groupName = sMetrics.elidedText(groupName, Qt::ElideRight, groupNameMaxWidth);
        if (!groupName.isEmpty())
            painter->drawText(QPoint(rect.right() - sMetrics.boundingRect(groupName).width(), nameYPos), groupName);
    }
    painter->restore();
}


//****************************************************************************************************************************************************
/// \param[in] option The style options.
//****************************************************************************************************************************************************
QSize PickerItemDelegate::sizeHint(const QStyleOptionViewItem &option, QModelIndex const &) const {
    // we compute the size hint for the item. width is trivial, but height needs attention.
    // An item contains one line of text in big font, one line of text with small font, and there are margins.
    return QSize(option.rect.width(), QFontMetrics(bigFont()).height() + QFontMetrics(smallFont()).height()
                                      + 2 * kItemVMargin);
}
