/// \file
/// \author Xavier Michelon
///
/// \brief Declaration of utility functions for the Beeftext application
///
/// Copyright (c) Xavier Michelon. All rights reserved.  
/// Licensed under the MIT License. See LICENSE file in the project root for full license information.  


#include "stdafx.h"
#include "BeeftextUtils.h"
#include "Combo/ComboManager.h"
#include "Dialogs/ShortcutDialog.h"
#include "ProcessListManager.h"
#include "InputManager.h"
#include "Preferences/PreferencesManager.h"
#include "BeeftextGlobals.h"
#include <Psapi.h>
#include <XMiLib/SystemUtils.h>
#include <XMiLib/Exception.h>
#include <vector>


using namespace xmilib;


namespace {


QString const kPortableModeBeaconFileName = "Portable.bin"; ///< The name of the 'beacon' file used to detect if the application should run in portable mode
QString const kPortableAppsModeBeaconFileName = "PortableApps.bin"; ///< The name of the 'beacon file used to detect if the app is in PortableApps mode
QList<quint16> const modifierKeys = { VK_LCONTROL, VK_RCONTROL, VK_LMENU, VK_RMENU, VK_LSHIFT, VK_RSHIFT, VK_LWIN,
                                      VK_RWIN }; ///< The modifier keys
QChar constexpr kObjectReplacementChar(0xfffc); ///< The unicode object replacement character.
quint64 constexpr kMaxRestrictedInputEventCount = 200000;
qint64 constexpr kModifierReleaseTimeoutMs = 1000;



QString richTextMessage() {
    return QObject::tr(R"(<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.0//EN" "http://www.w3.org/TR/REC-html40/strict.dtd">
<html><head><meta name="qrichtext" content="1" /><style type="text/css">
p, li { white-space: pre-wrap; }
</style></head><body style=" font-family:'MS Shell Dlg 2'; font-size:8pt; font-weight:400; font-style:normal;">
<p style=" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;">Your combo list contains rich text combos. Lean Beeftext does not support rich text.</p>
<p style="-qt-paragraph-type:empty; margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;"><br /></p>
<p style=" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;">- Click on the 'Convert' button to convert your rich text combos to plain text.</p>
<p style=" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;">- Click on the 'Exit' button to quit the application. If you want to keep using rich text combos, you can re-install <a href="https://github.com/xmichelo/Beeftext/releases/tag/v7.2">upstream Beeftext v7.2</a>.</p></body></html>
)");
}


//****************************************************************************************************************************************************
/// \brief Test if the application is running in portable mode
/// 
/// \return true if and only if the application is running in portable mode
//****************************************************************************************************************************************************
bool isInPortableModeInternal() {
    QDir const appDir(QCoreApplication::applicationDirPath());
    return QFileInfo(appDir.absoluteFilePath(kPortableModeBeaconFileName)).exists() ||
           QFileInfo(appDir.absoluteFilePath(kPortableAppsModeBeaconFileName)).exists();
}


bool isModifierPressed() {
    for (quint16 const key: modifierKeys)
        if (GetAsyncKeyState(key) < 0)
            return true;
    return false;
}


void waitForModifierRelease() {
    QElapsedTimer timer;
    timer.start();
    while (isModifierPressed() && (timer.elapsed() < kModifierReleaseTimeoutMs))
        QThread::msleep(5);
    if (isModifierPressed())
        throw Exception("Could not insert restricted text while a modifier key remained pressed.");
}


bool isExtendedKey(WORD virtualKey) {
    switch (virtualKey) {
    case VK_RCONTROL:
    case VK_RMENU:
    case VK_LWIN:
    case VK_RWIN:
    case VK_INSERT:
    case VK_DELETE:
    case VK_HOME:
    case VK_END:
    case VK_PRIOR:
    case VK_NEXT:
    case VK_LEFT:
    case VK_RIGHT:
    case VK_UP:
    case VK_DOWN:
        return true;
    default:
        return false;
    }
}


void appendVirtualKeyEvent(std::vector<INPUT> &events, WORD virtualKey, bool pressed) {
    INPUT event = {};
    event.type = INPUT_KEYBOARD;
    event.ki.wVk = virtualKey;
    event.ki.wScan = static_cast<WORD>(MapVirtualKey(virtualKey, MAPVK_VK_TO_VSC));
    event.ki.dwFlags = pressed ? 0 : KEYEVENTF_KEYUP;
    event.ki.dwExtraInfo = kLeanInjectedInputMarker;
    if (isExtendedKey(virtualKey))
        event.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    events.push_back(event);
}


void appendVirtualKeyPress(std::vector<INPUT> &events, WORD virtualKey) {
    appendVirtualKeyEvent(events, virtualKey, true);
    appendVirtualKeyEvent(events, virtualKey, false);
}


void appendUnicodeCodeUnit(std::vector<INPUT> &events, QChar character) {
    INPUT keyDown = {};
    keyDown.type = INPUT_KEYBOARD;
    keyDown.ki.wScan = character.unicode();
    keyDown.ki.dwFlags = KEYEVENTF_UNICODE;
    keyDown.ki.dwExtraInfo = kLeanInjectedInputMarker;

    INPUT keyUp = keyDown;
    keyUp.ki.dwFlags |= KEYEVENTF_KEYUP;
    events.push_back(keyDown);
    events.push_back(keyUp);
}


}


//****************************************************************************************************************************************************
// 
//****************************************************************************************************************************************************
void openLogFile() {
    QDesktopServices::openUrl(QUrl::fromLocalFile(globals::logFilePath()));
}


//****************************************************************************************************************************************************
/// \return true if and only if the application is in portable mode
//****************************************************************************************************************************************************
bool isInPortableMode() {
    // portable mode state cannot change during the execution of an instance of the application, so we 'cache' the
    // value using a static variable
    static bool result = isInPortableModeInternal();
    return result;
}


//****************************************************************************************************************************************************
/// \return true if the application is running as part of the PortableApps.com distribution
//****************************************************************************************************************************************************
bool usePortableAppsFolderLayout() {
    return QFileInfo(QDir(QCoreApplication::applicationDirPath())
        .absoluteFilePath(kPortableAppsModeBeaconFileName)).exists();
}


//****************************************************************************************************************************************************
/// \return The name of the currently active application, including its extension (e.g. "explorer.exe")
/// \return A null string in case of failure
//****************************************************************************************************************************************************
QString getActiveExecutableFileName() {
    WCHAR buffer[MAX_PATH + 1] = { 0 };
    DWORD processId = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &processId);
    // ReSharper disable once CppLocalVariableMayBeConst
    HANDLE processHandle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
    if (!processHandle)
        return QString();
    bool const ok = GetModuleFileNameEx(processHandle, nullptr, buffer, MAX_PATH);
    CloseHandle(processHandle);
    return ok ? QFileInfo(QDir::fromNativeSeparators(QString::fromWCharArray(buffer))).fileName() : QString();
}


//****************************************************************************************************************************************************
/// \return true if and only if Beeftext is the application currently in the foreground
//****************************************************************************************************************************************************
bool isBeeftextTheForegroundApplication() {
    DWORD processId = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &processId);
    return QCoreApplication::applicationPid() == processId;
}


//****************************************************************************************************************************************************
/// \param[in] snippet The snippet.
/// \return The snippet in plain text format.
//****************************************************************************************************************************************************
QString htmlToPlainText(QString const &snippet) {
    QTextDocumentFragment const fragment = QTextDocumentFragment::fromHtml(snippet);
    QString plainText = fragment.toPlainText();
    plainText.remove(kObjectReplacementChar); // Remove the 'Object replacement character' that replaced images during conversion to plain text
    return plainText;
}


//****************************************************************************************************************************************************
/// Perform deletion, text insertion, and bounded cursor movement as one serial
/// SendInput batch. Restricted text is sanitized again here so every caller,
/// including emoji insertion, receives the same fail-closed behavior. The only
/// non-text event accepted from content is an unmodified Return generated from a
/// normalized line break when the dedicated preference permits it.
//****************************************************************************************************************************************************
void performRestrictedTextInput(qint32 eraseCount, QString const &text, qint32 cursorLeftCount,
    bool allowRealLineBreaks) {
    waitForModifierRelease();

    qint32 const safeEraseCount = qMax<qint32>(eraseCount, 0);
    qint32 const safeCursorLeftCount = qMax<qint32>(cursorLeftCount, 0);
    QString const safeText = restricted::sanitizeText(text, allowRealLineBreaks);
    quint64 const eventCount64 = 2ULL * (static_cast<quint64>(safeEraseCount)
        + static_cast<quint64>(safeText.size()) + static_cast<quint64>(safeCursorLeftCount));
    if (eventCount64 > kMaxRestrictedInputEventCount)
        throw Exception("The restricted snippet is too long to insert safely.");

    std::vector<INPUT> events;
    events.reserve(static_cast<size_t>(eventCount64));
    for (qint32 index = 0; index < safeEraseCount; ++index)
        appendVirtualKeyPress(events, VK_BACK);
    for (QChar const character: safeText) {
        if (allowRealLineBreaks && (character == QChar::LineFeed))
            appendVirtualKeyPress(events, VK_RETURN);
        else
            appendUnicodeCodeUnit(events, character);
    }
    for (qint32 index = 0; index < safeCursorLeftCount; ++index)
        appendVirtualKeyPress(events, VK_LEFT);

    if (events.empty())
        return;

    UINT const eventCount = static_cast<UINT>(events.size());
    SetLastError(ERROR_SUCCESS);
    UINT const sent = SendInput(eventCount, events.data(), sizeof(INPUT));
    if (sent == eventCount)
        return;

    DWORD const error = GetLastError();
    if ((sent > 0) && ((sent % 2) != 0)) {
        // Every event pair is key-down/key-up. If Windows accepted an odd prefix,
        // release the final key so a partial failure cannot leave it logically held.
        INPUT release = events[sent - 1];
        release.ki.dwFlags |= KEYEVENTF_KEYUP;
        SendInput(1, &release, sizeof(INPUT));
    }
    throw Exception(QString("Could not insert restricted snippet: sent %1 of %2 events (Windows error %3; elevated targets may block input).")
                        .arg(sent).arg(eventCount).arg(error));
}


//****************************************************************************************************************************************************
/// \param[in] charCount The number of characters to substitute.
/// \param[in] newText The new text.
/// \param[in] cursorPos The position of the cursor in the new text. The value is -1 if the cursor does not need
/// \param[in] source The source that triggered the combo
/// repositionning.
//****************************************************************************************************************************************************
bool performTextSubstitution(qint32 charCount, QString const &newText, qint32 cursorPos, ETriggerSource source) {
    InputManager &inputManager = InputManager::instance();
    PreferencesManager const &prefs = PreferencesManager::instance();
    bool const wasKeyboardHookEnabled = inputManager.setKeyboardHookEnabled(false);
    // we disable the hook to prevent endless recursive substitution

    try {
        // we erase the combo
        bool const triggeredByPicker = (ETriggerSource::ComboPicker == source);
        bool const triggersOnSpace = prefs.useAutomaticSubstitution() && prefs.comboTriggersOnSpace();
        QString const text = newText + (triggersOnSpace && prefs.keepFinalSpaceCharacter() && (!triggeredByPicker)
                                        ? " " : QString());
        bool const allowRealLineBreaks = prefs.allowRealLineBreaksInSnippets();
        qint32 const eraseCount = triggeredByPicker ? 0
            : qMax<qint32>(charCount + (triggersOnSpace ? 1 : 0), 0);
        qint32 const cursorLeftCount = cursorPos < 0 ? 0
            : qMax<qint32>(0, printableCharacterCount(restricted::sanitizeText(text, allowRealLineBreaks)) - cursorPos);
        performRestrictedTextInput(eraseCount, text, cursorLeftCount, allowRealLineBreaks);
    }
    catch (Exception const &exception) {
        inputManager.setKeyboardHookEnabled(wasKeyboardHookEnabled);
        reportError(nullptr, QString("Text substitution failed: %1").arg(exception.qwhat()));
        return false;
    }
    catch (...) {
        inputManager.setKeyboardHookEnabled(wasKeyboardHookEnabled);
        reportError(nullptr, "Text substitution failed because of an unexpected error.");
        return false;
    }
    inputManager.setKeyboardHookEnabled(wasKeyboardHookEnabled);
    return true;
}


//****************************************************************************************************************************************************
/// \param[in] parent The parent widget of the message box.
/// \param[in] logMessage The message to add to the log.
/// \param[in] userMessage The message to display to the user. if empty, log message is displayed to the user
/// \brief The report contists in writting logMessage to the debug log
//****************************************************************************************************************************************************
void reportError(QWidget *parent, QString const &logMessage, QString const &userMessage) {
    globals::debugLog().addError(logMessage);
    QString const usrMsg = userMessage.isEmpty() ? logMessage : userMessage;
    QMessageBox::critical(parent, QObject::tr("Error"), usrMsg);
}


//****************************************************************************************************************************************************
/// \return true if and only if the application is running on Windows 10 or higher.
//****************************************************************************************************************************************************
bool isAppRunningOnWindows10OrHigher() {
    QRegularExpression const re(R"(^(\d+)\.)");
    QRegularExpressionMatch const match = re.match(QSysInfo::kernelVersion());
    return match.hasMatch() ? match.captured(1).toInt() >= 10 : false;
}


//****************************************************************************************************************************************************
/// The number of printable character can only be estimated because it depends on the way
/// application edit, most notably when it comes to the behaviour of the left arrow key supporting compound emojis
/// (https://eclecticlight.co/2018/03/15/compound-emoji-can-confuse/)
///
/// \param[in] str the string
/// \return The estimated number of characters for the string.
//****************************************************************************************************************************************************
qint32 printableCharacterCount(QString const &str) {
    // The easiest way to count characters is to convert the string to UTF-32 (a.k.a. UCS-4)

    QVector<quint32> const ucs4 = str.toUcs4();
    // Now we assume that if you use the Zero Width Joiner (U+200D), it is resolved properly
    // We also account for compound emojis made with the skin color tones (Fitzpatrick type)
    qint32 result = qint32(ucs4.size());
    for (quint32 const c: ucs4) {
        if (c == 0x200d) // zero width joiner
            result -= 2;
        if ((c >= 0x1f3fb) && (c <= 0x1f3ff)) // fitzpatrick scale range
            result -= 1;
    }
    return qMax<qint32>(0, result);
}


//****************************************************************************************************************************************************
/// \return false if the user opted not to convert combos to plain text and want to exit the application.
//****************************************************************************************************************************************************
bool warnAndConvertHtmlCombos() {
    QMessageBox dlg;
    dlg.setWindowTitle(QObject::tr("Warning"));
    dlg.setIcon(QMessageBox::Warning);
    dlg.setText(richTextMessage());
    QPushButton *convertButton = dlg.addButton(QObject::tr("&Convert"), QMessageBox::AcceptRole);
    dlg.addButton(QObject::tr("E&xit"), QMessageBox::RejectRole);
    dlg.setDefaultButton(convertButton);
    return QMessageBox::AcceptRole == dlg.exec();
}


//****************************************************************************************************************************************************
/// \param[in] color The color.
/// \param[in] includeAlpha Should the alpha channel be included in the hexadecimal string.
/// \return The hex representation of the color.
//****************************************************************************************************************************************************
QString colorToHex(QColor const &color, bool includeAlpha) {
    QChar constexpr zero('0');
    QString result = QString("%1%2%3").arg(color.red(), 2, 16, zero).arg(color.green(), 2, 16, zero)
        .arg(color.blue(), 2, 16, zero);
    if (includeAlpha)
        result += QString("%1").arg(color.alpha(), 2, 16, zero);
    return result;
}


//****************************************************************************************************************************************************
/// \param[in] parent The parent widget of the dialog.
/// \param[in] title The title for the dialog.
/// \param[in] text The text to display.
/// \param[in] yesText The text to display in the 'yes' button.
/// \param[in] noText The text to display in the 'no' button.
/// \return true if and only if the user replied 'yes'.
//****************************************************************************************************************************************************
bool questionDialog(QWidget *parent, QString const &title, QString const &text,
    QString const &yesText, QString const &noText) {
    QMessageBox msgBox(parent);
    msgBox.setWindowTitle(title);
    msgBox.setText(text);
    QPushButton const *yesButton = msgBox.addButton(yesText, QMessageBox::AcceptRole);
    msgBox.addButton(noText, QMessageBox::RejectRole);
    msgBox.exec();
    return msgBox.clickedButton() == yesButton;
}


//****************************************************************************************************************************************************
/// \param[in] parent The parent widget of the dialog.
/// \param[in] icon The icon.
/// \param[in] title The title of the dialog.
/// \param[in] text The description text for the dialog.
/// \param[in] button0Text The text for button 0.
/// \param[in] button1Text The text for button 1.
/// \param[in] button2Text The text for button 2.
/// \param[in] acceptButtonIndex The index of the button with the 'Accept' role.
/// \param[in] rejectButtonIndex The index of the button with the 'Reject' role.
/// \return The index of the buttons selected by the user.
//****************************************************************************************************************************************************
qint32 threeOptionsDialog(QWidget *parent, QMessageBox::Icon const &icon, QString const &title, QString const &text,
    QString const &button0Text, QString const &button1Text, QString const &button2Text, qint32 acceptButtonIndex,
    qint32 rejectButtonIndex) {
    QMessageBox msgBox(parent);
    msgBox.setWindowTitle(title);
    msgBox.setIcon(icon);
    msgBox.setText(text);
    QList<QPushButton *> buttons = {
        msgBox.addButton(button0Text, acceptButtonIndex == 0 ? QMessageBox::AcceptRole :
                                      (rejectButtonIndex == 0 ? QMessageBox::RejectRole : QMessageBox::NoRole)),
        msgBox.addButton(button1Text, acceptButtonIndex == 1 ? QMessageBox::AcceptRole :
                                      (rejectButtonIndex == 1 ? QMessageBox::RejectRole : QMessageBox::NoRole)),
        msgBox.addButton(button2Text, acceptButtonIndex == 2 ? QMessageBox::AcceptRole :
                                      (rejectButtonIndex == 2 ? QMessageBox::RejectRole : QMessageBox::NoRole)),
    };
    msgBox.exec();
    QAbstractButton const *clickedButton = msgBox.clickedButton();
    return buttons[2] == clickedButton ? 2 : (buttons[1] == clickedButton ? 1 : 0);
}
