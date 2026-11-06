/// \file
///
/// \brief Fail-closed parsing and evaluation helpers for the Lean Beeftext restricted execution model.
///
/// Licensed under the MIT License. See LICENSE file in the project root for full license information.


#include "RestrictedModel.h"

#include <QChar>
#include <QRegularExpression>
#include <QSettings>
#include <QStringList>
#include <limits>


namespace {


QString const kCursorVariable = "#{cursor}";


QString visibleCodeUnit(quint16 value) {
    return QStringLiteral("\\u") + QStringLiteral("%1").arg(value, 4, 16, QChar('0')).toUpper();
}


bool isHighSurrogate(quint16 value) {
    return (value >= 0xd800) && (value <= 0xdbff);
}


bool isLowSurrogate(quint16 value) {
    return (value >= 0xdc00) && (value <= 0xdfff);
}


/// \brief Split a timeshift string (e.g. +1d-4w+11h) into individual shifts (e.g. { +1d, -4w, +11h }).
QStringList splitTimeShiftString(QString const &shiftStr) {
    QStringList result;
    QString acc;
    for (QChar const c: shiftStr) {
        if (('+' == c) || ('-' == c)) {
            if (!acc.isEmpty()) {
                result.append(acc);
                acc = QString();
            }
        }
        acc += c;
    }
    if (!acc.isEmpty())
        result.append(acc);
    return result;
}


bool checkedScale(qint64 value, qint64 factor, qint64 &result) {
    Q_ASSERT(factor > 0);
    if ((value > (std::numeric_limits<qint64>::max() / factor))
        || (value < (std::numeric_limits<qint64>::min() / factor)))
        return false;
    result = value * factor;
    return true;
}


/// \brief Read a boolean preference, accepting only true/false/1/0. Anything else yields the default.
bool readStrictBool(QSettings const &settings, QString const &key, bool defaultValue) {
    QVariant const value = settings.value(key);
    if (!value.isValid())
        return defaultValue;
    QString const normalized = value.toString().trimmed().toLower();
    if ((normalized == QStringLiteral("true")) || (normalized == QStringLiteral("1")))
        return true;
    if ((normalized == QStringLiteral("false")) || (normalized == QStringLiteral("0")))
        return false;
    return defaultValue;
}


} // anonymous namespace


namespace restricted {


ERestrictedVariable classifyVariable(QString const &variable) {
    if (variable == "cursor")
        return ERestrictedVariable::Cursor;
    if (variable == "date")
        return ERestrictedVariable::Date;
    if (variable == "time")
        return ERestrictedVariable::Time;
    if (variable == "dateTime")
        return ERestrictedVariable::DateTime;

    if (variable.startsWith("dateTime:")) {
        QString const payload = variable.mid(QStringLiteral("dateTime:").size());
        if (!payload.startsWith('+') && !payload.startsWith('-'))
            return ERestrictedVariable::CustomDateTime;

        static QRegularExpression const shiftedDateTime(R"(^([+-]\d+[yMwdhmsz])+:(.*)$)");
        return shiftedDateTime.match(payload).hasMatch() ? ERestrictedVariable::CustomDateTime
                                                          : ERestrictedVariable::Blocked;
    }
    if (variable.startsWith("combo:"))
        return ERestrictedVariable::Combo;
    if (variable.startsWith("upper:"))
        return ERestrictedVariable::Upper;
    if (variable.startsWith("lower:"))
        return ERestrictedVariable::Lower;
    if (variable.startsWith("trim:"))
        return ERestrictedVariable::Trim;
    if (variable.startsWith("input:"))
        return ERestrictedVariable::Input;
    return ERestrictedVariable::Blocked;
}


QString resolveEscapingInVariableParameter(QString paramStr) {
    paramStr.replace(R"(\\)", R"(\)");
    paramStr.replace(R"(\})", R"(})");
    return paramStr;
}


QDateTime shiftedDateTime(QDateTime const &now, QString const &shiftStr) {
    QDateTime result = now;
    static QRegularExpression const shiftRegExp(R"(^([+-])(\d+)([yMwdhmsz])$)");
    QStringList const shifts = splitTimeShiftString(shiftStr);
    if (shifts.isEmpty())
        return QDateTime();
    for (QString const &shift: shifts) {
        QRegularExpressionMatch const match = shiftRegExp.match(shift);
        if (!match.hasMatch())
            return QDateTime();
        bool ok = false;
        qint64 value = match.captured(2).toLongLong(&ok);
        if (!ok)
            return QDateTime();
        if (match.captured(1) == "-")
            value = -value;
        qint64 scaledValue = 0;
        switch (match.captured(3)[0].toLatin1()) {
        case 'y':
            if ((value < std::numeric_limits<qint32>::min()) || (value > std::numeric_limits<qint32>::max()))
                return QDateTime();
            result = result.addYears(static_cast<qint32>(value));
            break;
        case 'M':
            if ((value < std::numeric_limits<qint32>::min()) || (value > std::numeric_limits<qint32>::max()))
                return QDateTime();
            result = result.addMonths(static_cast<qint32>(value));
            break;
        case 'w':
            if (!checkedScale(value, 7, scaledValue))
                return QDateTime();
            result = result.addDays(scaledValue);
            break;
        case 'd':
            result = result.addDays(value);
            break;
        case 'h':
            if (!checkedScale(value, 3600, scaledValue))
                return QDateTime();
            result = result.addSecs(scaledValue);
            break;
        case 'm':
            if (!checkedScale(value, 60, scaledValue))
                return QDateTime();
            result = result.addSecs(scaledValue);
            break;
        case 's':
            result = result.addSecs(value);
            break;
        case 'z':
            result = result.addMSecs(value);
            break;
        default:
            return QDateTime();
        }
        if (!result.isValid())
            return QDateTime();
    }
    return result;
}


QString evaluateDateTimeVariable(QString const &variable, QDateTime const &now, QLocale const &locale) {
    static QRegularExpression const regExp(R"(^dateTime(:(([+-]\d+[yMwdhmsz])+))?:(.*)$)");
    QRegularExpressionMatch const match = regExp.match(variable);
    if (!match.hasMatch())
        return QString();
    // A payload that looks like a shift but did not parse as one (e.g. "+1q:yyyy") must not be
    // silently reinterpreted as a format string.
    if (match.captured(1).isEmpty() && (match.captured(4).startsWith('+') || match.captured(4).startsWith('-')))
        return QString();
    QDateTime const dateTime = match.captured(1).isEmpty() ? now : shiftedDateTime(now, match.captured(2));
    if (!dateTime.isValid())
        return QString();
    QString formatStr = resolveEscapingInVariableParameter(match.captured(4));
    if (formatStr.isEmpty())
        return locale.toString(dateTime);

    // 'ww' and 'w' are a Beeftext extension for the ISO week number.
    qint32 const weekNumber = dateTime.date().weekNumber();
    formatStr = formatStr.replace("ww", QString("%1").arg(weekNumber, 2, 10, QChar('0')));
    formatStr = formatStr.replace("w", QString::number(weekNumber));
    return locale.toString(dateTime, formatStr);
}


QString evaluateDateVariable(ERestrictedVariable kind, QString const &variable, QDateTime const &now,
    QLocale const &locale) {
    switch (kind) {
    case ERestrictedVariable::Date:
        return locale.toString(now.date());
    case ERestrictedVariable::Time:
        return locale.toString(now.time());
    case ERestrictedVariable::DateTime:
        return locale.toString(now);
    case ERestrictedVariable::CustomDateTime:
        return evaluateDateTimeVariable(variable, now, locale);
    default:
        return QString();
    }
}


QString sanitizeText(QString const &text, bool allowRealLineBreaks) {
    QString result;
    result.reserve(text.size());

    for (qsizetype index = 0; index < text.size(); ++index) {
        QChar const character = text[index];
        quint16 const value = character.unicode();

        if (value == '\r') {
            if (((index + 1) < text.size()) && (text[index + 1] == QChar::LineFeed))
                ++index;
            result += allowRealLineBreaks ? QStringLiteral("\n") : QStringLiteral("\\n");
            continue;
        }
        if (value == '\n') {
            result += allowRealLineBreaks ? QStringLiteral("\n") : QStringLiteral("\\n");
            continue;
        }
        if (value == '\t') {
            result += QStringLiteral("\\t");
            continue;
        }

        // Unicode line and paragraph separators can also be interpreted as submission
        // controls, depending on the target application.
        if ((value == 0x2028) || (value == 0x2029)) {
            result += visibleCodeUnit(value);
            continue;
        }

        if (isHighSurrogate(value)) {
            if (((index + 1) < text.size()) && isLowSurrogate(text[index + 1].unicode())) {
                result += character;
                result += text[++index];
            } else {
                result += visibleCodeUnit(value);
            }
            continue;
        }
        if (isLowSurrogate(value)) {
            result += visibleCodeUnit(value);
            continue;
        }

        if (character.category() == QChar::Other_Control) {
            result += visibleCodeUnit(value);
            continue;
        }
        result += character;
    }
    return result;
}


bool isSafeCursorSuffix(QString const &suffix, bool allowRealLineBreaks) {
    if (suffix.size() > std::numeric_limits<qint32>::max())
        return false;

    // Cursor movement is implemented with one Left-arrow event per UTF-16 code unit of the
    // suffix. That is exact for every Basic Multilingual Plane character that forms its own
    // grapheme (Latin, accented letters, CJK, Cyrillic, ...). It is not predictable for:
    //  - surrogate pairs (one character, two code units; emoji and other supplementary planes),
    //  - combining marks and variation selectors (merge into the previous character),
    //  - format characters such as the zero-width joiner, which glue sequences together,
    //  - control characters (already made visible by sanitizeText, but checked anyway).
    for (QChar const character: suffix) {
        quint16 const value = character.unicode();
        if (allowRealLineBreaks && (value == '\n'))
            continue;
        if (isHighSurrogate(value) || isLowSurrogate(value))
            return false;
        switch (character.category()) {
        case QChar::Mark_NonSpacing:
        case QChar::Mark_SpacingCombining:
        case QChar::Mark_Enclosing:
        case QChar::Other_Format:
        case QChar::Other_Control:
        case QChar::Other_Surrogate:
        case QChar::Other_NotAssigned:
            return false;
        default:
            break;
        }
    }
    return true;
}


RestrictedSnippet prepareSnippet(QString const &text, bool allowRealLineBreaks) {
    RestrictedSnippet result;
    result.text = sanitizeText(text, allowRealLineBreaks);

    // Upstream v16 evaluates nested combos before cursor handling and uses the
    // last cursor marker as the final caret position. Matching that rule makes
    // repeated child-combo expansion useful while keeping the Left-event count
    // bounded by the suffix that was actually inserted.
    qsizetype const markerIndex = result.text.lastIndexOf(kCursorVariable);
    if (markerIndex < 0)
        return result;

    QString const suffix = result.text.mid(markerIndex + kCursorVariable.size());
    result.text.remove(kCursorVariable, Qt::CaseSensitive);
    if (!isSafeCursorSuffix(suffix, allowRealLineBreaks)) {
        // The marker is never inserted literally into the user's document, but the caret
        // is left at the end of the inserted text because the movement cannot be bounded.
        result.cursorSyntaxRejected = true;
        return result;
    }
    result.cursorLeftCount = static_cast<qint32>(suffix.size());
    return result;
}


bool readAllowRealLineBreaksInSnippets(QSettings const &settings) {
    return readStrictBool(settings, QString::fromLatin1(kAllowRealLineBreaksInSnippetsSettingKey),
                          kDefaultAllowRealLineBreaksInSnippets);
}


void writeAllowRealLineBreaksInSnippets(QSettings &settings, bool value) {
    settings.setValue(QString::fromLatin1(kAllowRealLineBreaksInSnippetsSettingKey), value);
}


bool readPlaySoundOnCombo(QSettings const &settings) {
    return readStrictBool(settings, QString::fromLatin1(kPlaySoundOnComboSettingKey), kDefaultPlaySoundOnCombo);
}


void writePlaySoundOnCombo(QSettings &settings, bool value) {
    settings.setValue(QString::fromLatin1(kPlaySoundOnComboSettingKey), value);
}


} // namespace restricted
