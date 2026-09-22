/// \file
/// \author Xavier Michelon
///
/// \brief Implementation of Emoji manager
///  
/// Copyright (c) Xavier Michelon. All rights reserved.  
/// Licensed under the MIT License. See LICENSE file in the project root for full license information. 


#include "stdafx.h"
#include "EmojiManager.h"
#include "EmojiRuntimeData.h"
#include "BeeftextGlobals.h"
#include "LastUse/EmojiLastUseFile.h"
#include <XMiLib/Exception.h>


using namespace xmilib;


//****************************************************************************************************************************************************
/// \return The only allowed instance of the emoji manager
//****************************************************************************************************************************************************
EmojiManager &EmojiManager::instance() {
    static EmojiManager instance;
    return instance;
}


//****************************************************************************************************************************************************
//
//****************************************************************************************************************************************************
void EmojiManager::loadEmojis() {
    this->unloadEmojis();
    QString const filePath = emojiFilePath();
    if (filePath.isEmpty()) {
        globals::debugLog().addWarning("Could not find the emoji list file.");
        return;
    }
    this->load(filePath);

}


//****************************************************************************************************************************************************
//
//****************************************************************************************************************************************************
void EmojiManager::unloadEmojis() {
    saveEmojiLastUseDateTimes(emojis_);
    emojis_.clear();
}


//****************************************************************************************************************************************************
/// \param[in] shortcode The shortcode.
/// \return The emoji.
/// \return A Null pointer if the emoji does not exist.
//****************************************************************************************************************************************************
SpEmoji EmojiManager::find(QString const &shortcode) const {
    return emojis_.find(shortcode);
}


//****************************************************************************************************************************************************
//
//****************************************************************************************************************************************************
EmojiManager::~EmojiManager() {
    saveEmojiLastUseDateTimes(emojis_);
}


//****************************************************************************************************************************************************
/// \param[in] appExeName The application executable name.
//****************************************************************************************************************************************************
bool EmojiManager::isExcludedApplication(QString const &appExeName) const {
    return excludedApps_.filter(appExeName);
}


//****************************************************************************************************************************************************
/// \param[in] parent The parent widget of the dialog.
/// \return true if and only if the user validated the dialog and the list was successfully saved to file.
//****************************************************************************************************************************************************
bool EmojiManager::runDialog(QWidget *parent) {
    return excludedApps_.runDialog(parent);
}


//****************************************************************************************************************************************************
/// \return A constant reference to the list of emojis.
//****************************************************************************************************************************************************
EmojiList const &EmojiManager::emojiListRef() const {
    return emojis_;
}


//****************************************************************************************************************************************************
//
//****************************************************************************************************************************************************
EmojiManager::EmojiManager()
    : excludedApps_(QObject::tr("<html><head/><body><p>Use this dialog to list applications "
                                "in which emoji substitution should be disabled.</p><p>List applications using their process name (e.g, "
                                "notepad.exe). Wildcards are accepted.</p></body></html>")) {
    excludedApps_.setFilePath(globals::emojiExcludedAppsFilePath());
    excludedApps_.load();
}


//****************************************************************************************************************************************************
/// \param[in] path The path of the file to load
/// \return true if and only if the file was successfully loaded
//****************************************************************************************************************************************************
bool EmojiManager::load(QString const &path) {
    try {
		QJsonObject const rootObject = readEmojiRuntimeData(path);

        for (QJsonObject::const_iterator it = rootObject.begin(); it != rootObject.end(); ++it) {
            QJsonValue const value = it.value();
            QJsonObject const object = value.toObject();
            QString const chars = object["char"].toString(QString());
            QString const shortcode = it.key();
            emojis_.append(std::make_shared<Emoji>(shortcode, chars, object["category"].toString()));
        }
        loadEmojiLastUseDateTimes(emojis_);
    }
    catch (Exception const &e) {
        QString const &what = e.qwhat();
        globals::debugLog().addWarning(QString("Error parsing emoji list file%1").arg(what.isEmpty() ? "." :
                                                                                      " (" + what + ")."));
        return false;
    }
    return true;
}


