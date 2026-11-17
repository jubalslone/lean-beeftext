#include "../Migration/LegacyMigrationCore.h"
#include "../Migration/LegacyMigrationDialogChoice.h"
#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QTimer>
#include <filesystem>

namespace {

int failures = 0;

void expect(bool condition, char const *message) {
	if (!condition) {
		qCritical() << "FAILED:" << message;
		++failures;
	}
}


migration::MigrationChoice exerciseDialog(bool accept, bool import, bool showCleanup, bool checkCleanup) {
	QDialog dialog;
	QComboBox sourceChoice(&dialog);
	sourceChoice.addItem("Synthetic source", 3);
	QCheckBox importCheck(&dialog);
	importCheck.setChecked(import);
	QCheckBox portableCheck(&dialog);
	portableCheck.setVisible(showCleanup);
	portableCheck.setChecked(checkCleanup);
	QTimer::singleShot(0, &dialog, [&] {
		expect(portableCheck.isVisible() == showCleanup, "cleanup checkbox has expected visibility during the dialog");
		accept ? dialog.accept() : dialog.reject();
	});
	return migration::execMigrationDialog(dialog, sourceChoice, importCheck, portableCheck);
}


void testConsent() {
	auto const consent = exerciseDialog(true, true, true, true);
	expect(consent.accepted && consent.importContent && consent.cleanupPortable && consent.selectedGroup == 3,
		"accepting the visible checked cleanup option preserves consent after the dialog closes");
	expect(!exerciseDialog(true, true, true, false).cleanupPortable, "an unchecked option declines cleanup");
	expect(!exerciseDialog(true, true, false, true).cleanupPortable, "an unavailable option cannot grant cleanup");
	expect(!exerciseDialog(true, false, true, true).cleanupPortable, "skipping the import also skips cleanup");
	auto const cancelled = exerciseDialog(false, true, true, true);
	expect(!cancelled.accepted && !cancelled.importContent && !cancelled.cleanupPortable, "cancel grants no consent");
}


bool makeDirectoryLink(QString const &link, QString const &target) {
#ifdef Q_OS_WIN
	// Directory junctions do not require Developer Mode or the symbolic-link privilege.
	QProcess process;
	auto env = QProcessEnvironment::systemEnvironment();
	env.insert("LEAN_QA_LINK", QDir::toNativeSeparators(link));
	env.insert("LEAN_QA_TARGET", QDir::toNativeSeparators(target));
	process.setProcessEnvironment(env);
	process.start("powershell.exe", {"-NoProfile", "-NonInteractive", "-Command",
		"$ErrorActionPreference='Stop'; New-Item -ItemType Junction -Path $env:LEAN_QA_LINK -Target $env:LEAN_QA_TARGET | Out-Null"});
	return process.waitForFinished() && process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
#else
	std::error_code error;
	std::filesystem::create_directory_symlink(QFileInfo(target).filesystemFilePath(), QFileInfo(link).filesystemFilePath(), error);
	return !error;
#endif
}


void testReplacementWithSameLibrary() {
	QTemporaryDir fixture;
	QDir root(fixture.path());
	QString const path = root.filePath("Beeftext Portable");
	QByteArray const library = R"({"fileFormatVersion":3,"combos":[],"groups":[]})";
	auto createPortable = [&] {
		QDir const source(path);
		if (!source.mkpath("Data"))
			return false;
		QFile beacon(source.filePath("Portable.bin")), executable(source.filePath("LeanBeeftext.exe")), combos(source.filePath("Data/comboList.json"));
		return beacon.open(QIODevice::WriteOnly) && executable.open(QIODevice::WriteOnly)
			&& combos.open(QIODevice::WriteOnly) && combos.write(library) == library.size();
	};
	if (!fixture.isValid() || !createPortable()) {
		expect(false, "temporary portable identity fixture is created");
		return;
	}
	QByteArray const recorded = migration::portableDirectoryIdentity(path);
	expect(!recorded.isEmpty() && migration::portableDirectoryMatches(path, recorded), "selected directory identity matches");
	expect(!migration::portableDirectoryMatches(path, {}), "old pending record without a directory identity is refused");
	QString const moved = root.filePath("Old copy");
	if (!root.rename(path, moved) || !createPortable()) {
		expect(false, "a distinct replacement occupies the recorded path");
		return;
	}
	expect(migration::portableDirectoryMatches(moved, recorded), "the original directory keeps its identity across a rename");
	expect(migration::isStrongPortableCandidate(path) && !migration::portableDirectoryMatches(path, recorded),
		"a same-product replacement with identical library bytes cannot inherit removal consent");
}


void testCleanupPaths() {
	QTemporaryDir fixture;
	expect(fixture.isValid(), "temporary migration directory is available");
	if (!fixture.isValid())
		return;

	QDir const root(fixture.path());
	QString const documents = root.filePath("Redirected Documents");
	QString const installed = QDir(documents).filePath("Lean Beeftext");
	QString const recovery = QDir(installed).filePath("Migration Backups/fixture");
	QString const portable = QDir(documents).filePath("Lean Beeftext Portable");
	QString const sharedChild = QDir(portable).filePath("Data");
	expect(root.mkpath(recovery) && root.mkpath(sharedChild), "temporary installed, recovery and portable directories are created");

	QStringList const sharedRoots = {documents};
	QStringList const protectedTrees = {installed};
	auto blocked = [&](QString const &candidate) {
		return migration::isBroadCleanupRoot(candidate, sharedRoots, protectedTrees);
	};
	expect(!blocked(portable), "a dedicated portable sibling under redirected Documents is eligible");
	expect(!migration::isBroadCleanupRoot(portable, sharedRoots, {QDir(documents).filePath("Lean Beeftext New/Data")}),
		"a separate, not-yet-created first-run data tree permits cleanup of its sibling");
	expect(migration::isBroadCleanupRoot(portable, {}, {QDir(portable).filePath("Future Data/Recovery")}),
		"a source containing a future installed or recovery tree is protected");
	expect(blocked(root.absolutePath()) && blocked(documents), "a shared root and its ancestors are protected");
	expect(blocked(installed) && blocked(recovery), "a live data root and its recovery descendants are protected");
	expect(!blocked(sharedChild), "a child with a similar directory prefix remains distinct from installed data");
	expect(blocked(QString()) && blocked(root.filePath("absent")) && migration::isBroadCleanupRoot(portable, {}, {QString()}),
		"missing sources and unknown mandatory data roots refuse cleanup");
	expect(migration::canRecyclePortableTree(portable), "a readable portable tree without directory links passes the final check");

	QString const link = QDir(portable).filePath("Linked recovery");
	bool const madeLink = makeDirectoryLink(link, installed);
	expect(madeLink, "temporary directory link to installed data is created");
	if (!madeLink)
		return;
	expect(!migration::canRecyclePortableTree(portable), "a link inside the portable tree refuses recursive cleanup");
	expect(blocked(link) && blocked(QDir(link).filePath("Migration Backups")), "linked cleanup roots and redirected ancestors are refused");
	expect(migration::isBroadCleanupRoot(installed, {}, {QDir(link).filePath("First Run/Data")}),
		"protection resolves a not-yet-created data directory through its existing alias");
	expect(QDir(recovery).exists(), "recovery fixture remains intact");
	std::error_code error;
	std::filesystem::remove(QFileInfo(link).filesystemFilePath(), error);
	expect(!error && QDir(recovery).exists(), "only the disposable directory link is detached");
}

} // namespace


int main(int argc, char **argv) {
	qputenv("QT_QPA_PLATFORM", "offscreen");
	QApplication app(argc, argv);
	testConsent();
	testCleanupPaths();
	testReplacementWithSameLibrary();
	return failures ? 1 : 0;
}
