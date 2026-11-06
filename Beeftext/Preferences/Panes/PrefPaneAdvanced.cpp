/// \file
/// \author
///
/// \brief Implementation of the advanced preferences pane.
///
/// Copyright (c) . All rights reserved.
/// Licensed under the MIT License. See LICENSE file in the project root for full license information.


#include "stdafx.h"
#include "PrefPaneAdvanced.h"
#include "BeeftextGlobals.h"
#include "BeeftextUtils.h"


//****************************************************************************************************************************************************
///\ param[in] parent The parent widget of the pane.
//****************************************************************************************************************************************************
PrefPaneAdvanced::PrefPaneAdvanced(QWidget *parent)
    : PrefPane(parent)
    , prefs_(PreferencesManager::instance()) {
    ui_.setupUi(this);
    connect(ui_.buttonExcludedApplications, &QPushButton::clicked, this, &PrefPaneAdvanced::onEditExcludedApplications);
    connect(ui_.buttonOpenComboListFolder, &QPushButton::clicked, this, &PrefPaneAdvanced::onOpenComboListFolder);
    connect(ui_.checkWriteDebugLogFile, &QCheckBox::toggled, this, &PrefPaneAdvanced::onCheckWriteDebugLogFile);
}


//****************************************************************************************************************************************************
//
//****************************************************************************************************************************************************
void PrefPaneAdvanced::load() const {
    ui_.editComboListFolder->setText(QDir::toNativeSeparators(prefs_.comboListFolderPath()));
    QSignalBlocker const blocker(ui_.checkWriteDebugLogFile);
    ui_.checkWriteDebugLogFile->setChecked(prefs_.writeDebugLogFile());
}


//****************************************************************************************************************************************************
//
//****************************************************************************************************************************************************
void PrefPaneAdvanced::onOpenComboListFolder() const {
    QString const path = QDir::fromNativeSeparators(ui_.editComboListFolder->text());
    if (QDir(path).exists())
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}


//****************************************************************************************************************************************************
/// \param[in] checked Is the check box checked?
//****************************************************************************************************************************************************
void PrefPaneAdvanced::onCheckWriteDebugLogFile(bool checked) const {
    prefs_.setWriteDebugLogFile(checked);
    xmilib::DebugLog &log = globals::debugLog();
    if (checked)
        log.enableLoggingToFile(globals::logFilePath());
    else
        log.disableLoggingToFile();
}


//****************************************************************************************************************************************************
//
//****************************************************************************************************************************************************
void PrefPaneAdvanced::onEditExcludedApplications() {
    globals::excludedApplications().runDialog(this);
}


//****************************************************************************************************************************************************
/// \param[in] event The event.
//****************************************************************************************************************************************************
void PrefPaneAdvanced::changeEvent(QEvent *event) {
    if (QEvent::LanguageChange == event->type())
        ui_.retranslateUi(this);
    PrefPane::changeEvent(event);
}
