/// \file
/// \author Xavier Michelon
///
/// \brief Implementation of functions related to combo variables
///
/// Lean Beeftext evaluates only the allowlisted variables described in SECURITY_MODEL.md. Anything else
/// (clipboard, environment variables, scripts, synthetic keys, shortcuts, delays, unknown names) is left as
/// literal text. The pure parts of the evaluation live in RestrictedModel.cpp so they can be unit tested.


#include "stdafx.h"
#include "ComboVariable.h"
#include "ComboManager.h"
#include "Dialogs/VariableInputDialog.h"
#include "BeeftextGlobals.h"
#include <XMiLib/RandomNumberGenerator.h>


namespace {


enum class ECaseChange {
    NoChange, ///< Do not change case
    ToUpper, ///< Convert to upper case
    ToLower ///< Conver to lower case
}; ///< Enumeration for case change


QString const kInputVariable = "input:"; ///< The input variable.


//****************************************************************************************************************************************************
/// \brief Evaluate a #{combo:} variable.
///
/// \param[in] variable The variable, without the enclosing #{}.
/// \param[in] caseChange The change of case (uppercase, lowercase) to apply to the evaluated variable.
/// \param[in] forbiddenSubCombos The text of the combos that are not allowed to be substituted using #{combo:}, to
/// avoid endless recursion.
/// \param[in,out] knownInputVariables The list of know input variables.
/// \param[out] outCancelled Was the input variable cancelled by the user.
/// \return The result of evaluating the variable.
//****************************************************************************************************************************************************
QString evaluateComboVariable(QString const &variable, ECaseChange caseChange, QSet<QString> forbiddenSubCombos,
    QMap<QString, QString> &knownInputVariables, bool &outCancelled) {
    QString fallbackResult = QString("#{%1}").arg(variable);
    qint32 const varNameLength = qint32(variable.indexOf(':'));
    if (varNameLength < 0)
        return QString();
    QString const comboName = restricted::resolveEscapingInVariableParameter(
        variable.right(variable.size() - varNameLength - 1));
    if (forbiddenSubCombos.contains(comboName))
        return fallbackResult;

    ComboList results;
    for (SpCombo const &combo: ComboManager::instance().comboListRef())
        if (combo->keyword() == comboName)
            results.push_back(combo);

    qint32 const resultCount = results.size();
    ComboList::const_iterator it;
    switch (resultCount) {
    case 0:
        return fallbackResult;
    case 1:
        it = results.begin();
        break;
    default: {
        xmilib::RandomNumberGenerator rng(0, results.size() - 1);
        it = results.begin() + rng.get();
        break;
    }
    }

    QString str = (*it)->evaluatedSnippet(outCancelled, forbiddenSubCombos << comboName, knownInputVariables); // forbiddenSubcombos is intended at avoiding endless recursion
    switch (caseChange) {
    case ECaseChange::ToUpper:
        return str.toUpper();
    case ECaseChange::ToLower:
        return str.toLower();
    case ECaseChange::NoChange:
    default:
        return str;
    }
}


//****************************************************************************************************************************************************
/// \brief Evaluate an #{input:} variable.
///
/// \param[in] variable The variable, without the enclosing #{}.
/// \param[in,out] knownInputVariables The list of know input variables.
/// \param[out] outCancelled Was the input variable cancelled by the user.
/// \return The result of evaluating the variable.
//****************************************************************************************************************************************************
QString evaluateInputVariable(QString const &variable, QMap<QString, QString> &knownInputVariables, bool &outCancelled) {
    // check if we already add the user input for the given description
    QString const description = variable.right(variable.size() - kInputVariable.size());
    if (knownInputVariables.contains(description))
        return knownInputVariables[description];

    QString result;
    if (!VariableInputDialog::run(restricted::resolveEscapingInVariableParameter(description), result)) {
        outCancelled = true;
        return QString();
    }
    knownInputVariables.insert(description, result); // add the new input to the list of known ones
    return result;
}


}


//****************************************************************************************************************************************************
/// \param[in] variable The variable, without the enclosing #{}.
/// \param[in] forbiddenSubCombos The text of the combos that are not allowed to be substituted using #{combo:}, to
/// avoid endless recursion.
/// \param[in,out] knownInputVariables The list of know input variables.
/// \param[out] outCancelled Was the input variable cancelled by the user.
/// \return The result of evaluating the variable.
//****************************************************************************************************************************************************
QString evaluateVariable(QString const &variable, QSet<QString> const &forbiddenSubCombos,
    QMap<QString, QString> &knownInputVariables, bool &outCancelled) {
    using restricted::ERestrictedVariable;
    outCancelled = false;
    QString const literal = QString("#{%1}").arg(variable);
    ERestrictedVariable const kind = restricted::classifyVariable(variable);
    switch (kind) {
    case ERestrictedVariable::Cursor:
        return literal; // Resolved once, after the complete snippet has been evaluated.
    case ERestrictedVariable::Date:
    case ERestrictedVariable::Time:
    case ERestrictedVariable::DateTime:
    case ERestrictedVariable::CustomDateTime:
    {
        QString const value = restricted::evaluateDateVariable(kind, variable, QDateTime::currentDateTime());
        return value.isNull() ? literal : value;
    }
    case ERestrictedVariable::Combo:
        return evaluateComboVariable(variable, ECaseChange::NoChange, forbiddenSubCombos,
                                     knownInputVariables, outCancelled);
    case ERestrictedVariable::Upper:
        return evaluateComboVariable(variable, ECaseChange::ToUpper, forbiddenSubCombos,
                                     knownInputVariables, outCancelled);
    case ERestrictedVariable::Lower:
        return evaluateComboVariable(variable, ECaseChange::ToLower, forbiddenSubCombos,
                                     knownInputVariables, outCancelled);
    case ERestrictedVariable::Trim:
        return evaluateComboVariable(variable, ECaseChange::NoChange, forbiddenSubCombos,
                                     knownInputVariables, outCancelled).trimmed();
    case ERestrictedVariable::Input:
        return evaluateInputVariable(variable, knownInputVariables, outCancelled);
    case ERestrictedVariable::Blocked:
    default:
        globals::debugLog().addWarning("Blocked a restricted combo variable.");
        return literal;
    }
}
