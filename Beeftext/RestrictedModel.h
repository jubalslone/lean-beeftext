/// \file
///
/// \brief Fail-closed parsing and evaluation helpers for the Lean Beeftext restricted execution model.
///
/// Everything in this file is pure (no UI, no combo list, no system input) so it can be unit tested.
///
/// Licensed under the MIT License. See LICENSE file in the project root for full license information.


#ifndef LEAN_RESTRICTED_MODEL_H
#define LEAN_RESTRICTED_MODEL_H

#include <QDateTime>
#include <QLocale>
#include <QString>

class QSettings;


namespace restricted {


inline constexpr char kAllowRealLineBreaksInSnippetsSettingKey[] = "AllowRealLineBreaksInSnippets";
inline constexpr bool kDefaultAllowRealLineBreaksInSnippets = false;
inline constexpr char kPlaySoundOnComboSettingKey[] = "PlaySoundOnCombo";
inline constexpr bool kDefaultPlaySoundOnCombo = false;


enum class ERestrictedVariable {
    Blocked,
    Cursor,
    Date,
    Time,
    DateTime,
    CustomDateTime,
    Combo,
    Upper,
    Lower,
    Trim,
    Input
};


struct RestrictedSnippet {
    QString text; ///< The sanitized text to insert. Exact cursor markers are always removed.
    qint32 cursorLeftCount { -1 }; ///< Number of Left-arrow events to send, or -1 for no cursor movement.
    bool cursorSyntaxRejected { false }; ///< A marker was present but the suffix after it could not be counted safely.
};


ERestrictedVariable classifyVariable(QString const &variable); ///< Classify a variable (without the enclosing #{}).
QString resolveEscapingInVariableParameter(QString paramStr); ///< Resolve \\} and \\\\ escapes in a variable parameter.
QDateTime shiftedDateTime(QDateTime const &now, QString const &shiftStr); ///< Apply a +1d-2h style shift. Invalid on error.
QString evaluateDateTimeVariable(QString const &variable, QDateTime const &now,
    QLocale const &locale = QLocale::system()); ///< Evaluate dateTime[:shift]:format. Null string on error.
QString evaluateDateVariable(ERestrictedVariable kind, QString const &variable, QDateTime const &now,
    QLocale const &locale = QLocale::system()); ///< Evaluate date, time, dateTime, or custom dateTime. Null string on error.
QString sanitizeText(QString const &text, bool allowRealLineBreaks = false); ///< Make control characters visible.
bool isSafeCursorSuffix(QString const &suffix, bool allowRealLineBreaks = false); ///< Can every code unit be undone with one Left event?
RestrictedSnippet prepareSnippet(QString const &text, bool allowRealLineBreaks = false); ///< Sanitize and resolve cursor markers.
bool readAllowRealLineBreaksInSnippets(QSettings const &settings);
void writeAllowRealLineBreaksInSnippets(QSettings &settings, bool value);
bool readPlaySoundOnCombo(QSettings const &settings);
void writePlaySoundOnCombo(QSettings &settings, bool value);


} // namespace restricted

#endif // LEAN_RESTRICTED_MODEL_H
