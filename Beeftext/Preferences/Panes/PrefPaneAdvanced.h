/// \file
/// \author
///
/// \brief Declaration of the advanced preferences pane.
///
/// Copyright (c) . All rights reserved.
/// Licensed under the MIT License. See LICENSE file in the project root for full license information.


#ifndef BEEFTEXT_PREFS_PANE_ADVANCED_H
#define BEEFTEXT_PREFS_PANE_ADVANCED_H


#include "PrefPane.h"
#include "ui_PrefPaneAdvanced.h"
#include "Preferences/PreferencesManager.h"


//****************************************************************************************************************************************************
/// \brief The advanced preference pane.
//****************************************************************************************************************************************************
class PrefPaneAdvanced : public PrefPane {
Q_OBJECT
public: // member functions
    explicit PrefPaneAdvanced(QWidget *parent = nullptr); ///< Default constructor.
    PrefPaneAdvanced(PrefPaneAdvanced const &) = delete; ///< Disabled copy-constructor.
    PrefPaneAdvanced(PrefPaneAdvanced &&) = delete; ///< Disabled assignment copy-constructor.
    ~PrefPaneAdvanced() override = default; ///< Destructor.
    PrefPaneAdvanced &operator=(PrefPaneAdvanced const &) = delete; ///< Disabled assignment operator.
    PrefPaneAdvanced &operator=(PrefPaneAdvanced &&) = delete; ///< Disabled move assignment operator.
    void load() const override;

private slots:
    void onOpenComboListFolder() const; ///< Slot for the 'Open' button of the combo list folder.
    void onCheckWriteDebugLogFile(bool checked) const; ///< Slot the for 'Write debug log file' checkbox
    void onEditExcludedApplications(); ///< Slot for the 'Edit excluded applications' action

private: // member functions
    void changeEvent(QEvent *event) override; ///< Change event handler.

private: // data members
    Ui::PrefPaneAdvanced ui_ {}; ///< The GUI of the pane.
    PreferencesManager &prefs_; ///< The preferences manager.
};


#endif // #ifndef BEEFTEXT_PREFS_PANE_ADVANCED_H
