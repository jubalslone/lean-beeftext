#include "stdafx.h"
#include "BeeftextConstants.h"
#include "BeeftextUtils.h"
#include "Combo/ComboManager.h"
#include "Picker/PickerSortFilterProxyModel.h"
#include "ProcessListManager.h"

namespace {

int failures = 0;

void expect(bool condition, char const *description) {
	if (!condition) {
		qCritical() << description;
		++failures;
	}
}

void testTargetIdentity() {
	// Opaque test identities only. These handles are never passed to Windows.
	SubstitutionTarget expected{reinterpret_cast<HWND>(1), reinterpret_cast<HWND>(2), 10, 11};
	expect(expected.sameRecipient(expected), "The original receiving control should match.");
	SubstitutionTarget other = expected;
	other.window = reinterpret_cast<HWND>(3);
	expect(!expected.sameRecipient(other), "A different document window in the same process must be rejected.");
	other = expected;
	other.focus = reinterpret_cast<HWND>(4);
	expect(!expected.sameRecipient(other), "A different field in the same window must be rejected.");
	other = expected;
	++other.focusProcessId;
	expect(!expected.sameRecipient(other), "A new owner of the focused control must be rejected.");
	other = expected;
	++other.processId;
	expect(!expected.sameRecipient(other), "A new owner of the window must be rejected.");
	other = expected;
	other.focus = nullptr;
	expect(!other.sameRecipient(other), "A window with indeterminate keyboard focus must be rejected.");
	expect(!SubstitutionTarget{}.sameRecipient({}), "Two missing targets must never authorize input.");
}

void testExcludedProcesses() {
	// Query only this disposable test process. No windows are activated and no input is sent.
	DWORD const self = GetCurrentProcessId();
	SubstitutionTarget target{};
	target.processId = target.focusProcessId = self;
	ProcessListManager exclusions("test");
	expect(target.allowsProcesses(exclusions), "A known process with no matching exclusions should pass the list check.");
	exclusions.addProcess(QFileInfo(QCoreApplication::applicationFilePath()).fileName().toUpper());
	expect(!target.allowsProcesses(exclusions), "Process exclusions must apply case insensitively.");
	exclusions.clear();
	exclusions.addProcess("*.exe");
	expect(!target.allowsProcesses(exclusions), "Wildcard exclusions must still work.");
	exclusions.clear();
	target.focusProcessId = MAXDWORD;
	expect(!target.allowsProcesses(exclusions), "An unresolvable focus owner must fail even if the host is allowed.");
	target.focusProcessId = 0;
	expect(!target.allowsProcesses(exclusions), "An unknown receiving process must be refused.");
	expect(!target.canReceiveInputNow(), "Lean's own process must not receive substitution.");
	expect(!SubstitutionTarget{}.canReceiveInputNow(), "A missing destination must not accept delayed input.");
	// An empty event batch also exercises the final guard without the ability to type into another program.
	expect(!performRestrictedTextInput(0, QString(), 0, false, {}), "Restricted dispatch must reject a stale target.");
}

void testTriggerMatching() {
	QString const shortcode = "|grinning|";
	QStringList calls;
	auto const comboMiss = [&](QString const &text) { calls.append("combo " + text); return false; };
	auto const emoji = [&](QString const &text) { calls.append("emoji " + text); return text == shortcode; };
	expect(consumeTypedTrigger(shortcode + ' ', true, comboMiss, emoji)
		&& calls == QStringList{"combo " + shortcode, "emoji " + shortcode},
		"The emoji matcher must see the complete shortcode after a space-trigger combo miss.");
	calls.clear();
	auto const comboHit = [&](QString const &text) { calls.append("combo " + text); return true; };
	expect(consumeTypedTrigger(shortcode + ' ', true, comboHit, emoji) && calls.size() == 1,
		"A regular combo sharing a shortcode has priority.");
	calls.clear();
	expect(!consumeTypedTrigger(shortcode, true, comboMiss, emoji) && calls.isEmpty(),
		"Space-trigger mode must retain input until the space arrives.");
	expect(!consumeTypedTrigger("partial", false, comboMiss, emoji),
		"Immediate or manual matching must retain the buffer after both matchers miss.");
	expect(consumeTypedTrigger("miss ", true, comboMiss, emoji), "A space ends the word after both matchers miss.");
	expect(consumeTypedTrigger(shortcode, false, comboMiss, emoji), "Immediate and manual modes must still match shortcodes.");
}

void testDisabledPickerResults() {
	ComboList library;
	auto const enabled = Combo::create("QA enabled", "enabled", "one");
	auto const disabled = Combo::create("QA disabled", "disabled", "two");
	disabled->setEnabled(false);
	auto const disabledGroup = Group::create("QA disabled group");
	disabledGroup->setEnabled(false);
	auto const grouped = Combo::create("QA grouped", "grouped", "three");
	grouped->setGroup(disabledGroup);
	expect(library.append(enabled) && library.append(disabled) && library.append(grouped), "Fixture combos must be distinct.");
	PickerSortFilterProxyModel results;
	results.setSourceModel(&library);
	for (QString const &query : QStringList{QString(), QStringLiteral("QA")}) {
		results.setFilterFixedString(query);
		expect(results.rowCount() == 1, "Only the enabled combo may appear for empty and nonempty queries.");
		expect(results.index(0, 0).data(constants::PointerRole).value<SpCombo>() == enabled, "The selected row must be the enabled combo.");
	}
}

} // namespace

int main(int argc, char **argv) {
	QCoreApplication app(argc, argv);
	testTargetIdentity();
	testExcludedProcesses();
	testDisabledPickerResults();
	testTriggerMatching();
	return failures ? 1 : 0;
}
