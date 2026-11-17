/// \file
/// \author Xavier Michelon
///
/// \brief Declaration of functions related to combo variables


#ifndef BEEFTEXT_COMBO_VARIABLE_H
#define BEEFTEXT_COMBO_VARIABLE_H


QString evaluateDateTimeVariable(QString const &variable, QDateTime const &now, QLocale const &locale);

QString evaluateVariable(QString const &variable, QSet<QString> const &forbiddenSubCombos,
    QMap<QString, QString> &knownInputVariables, bool &outCancelled); ///< Compute the value of a variable.


#endif // #ifndef BEEFTEXT_COMBO_VARIABLE_H
