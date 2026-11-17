#pragma once

#include <QtCore/QMetaType>
#include <windows.h>

class ProcessListManager;

// The receiving native control as well as its foreground window. Capture it before
// showing Lean UI or queuing a keyboard event. An unknown focus is ineligible.
struct SubstitutionTarget {
	HWND window{};
	HWND focus{};
	DWORD processId{};
	DWORD focusProcessId{};

	static SubstitutionTarget foreground();
	bool sameRecipient(SubstitutionTarget const &other) const;
	bool allowsProcesses(ProcessListManager const &exclusions, bool checkEmojiExclusions = false) const;
	bool canReceiveInputNow(bool checkEmojiExclusions = false) const;
};

Q_DECLARE_METATYPE(SubstitutionTarget)
