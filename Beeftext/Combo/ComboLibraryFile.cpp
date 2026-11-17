#include "stdafx.h"
#include "ComboLibraryFile.h"
#include "Backup/BackupManager.h"
#include <XMiLib/Exception.h>
#include <QTemporaryFile>
#include <filesystem>

namespace {

bool fail(QString const &message, QString *error) {
	if (error)
		*error = message;
	return false;
}

// QSaveFile atomically replaces an existing file. A new library instead needs a
// same-directory rename that fails if another file now occupies the target.
bool createLibraryFile(QString const &path, ComboList const &library, QString *error) {
	QTemporaryFile temporary(path + ".XXXXXX");
	if (!temporary.open())
		return fail(temporary.errorString(), error);
	QByteArray const data = library.toJsonDocument(true).toJson();
	if (temporary.write(data) != data.size() || !temporary.flush())
		return fail(temporary.errorString(), error);
	if (!temporary.rename(path))
		return fail(QObject::tr("The new combo library could not be saved at %1: %2")
			.arg(QDir::toNativeSeparators(path), temporary.errorString()), error);
	temporary.setAutoRemove(false);
	return true;
}

} // namespace

bool ComboLibraryFile::open(QString const &path, ComboList &library, bool &olderFormat, QString *error) {
	state_ = State::Unopened;
	olderFormat = false;
	std::error_code ec;
	QFileInfo const file(path);
	auto const status = std::filesystem::symlink_status(file.filesystemFilePath(), ec);
	if (status.type() == std::filesystem::file_type::not_found
		&& (!ec || ec == std::errc::no_such_file_or_directory) && file.absoluteDir().exists()) {
		ComboList fresh;
		fresh.ensureCorrectGrouping();
		library = std::move(fresh);
		state_ = State::New;
		return true;
	}
	if (!library.load(path, &olderFormat, error))
		return false;
	state_ = State::Loaded;
	return true;
}

bool ComboLibraryFile::save(QString const &path, ComboList const &library, bool archive, QString *error) {
	if (state_ == State::Unopened)
		return fail(QObject::tr("The existing combo library could not be loaded. Restore access to it and restart Lean Beeftext before saving."), error);
	if (state_ == State::New) {
		if (!createLibraryFile(path, library, error))
			return false;
		state_ = State::Loaded;
		return true;
	}
	if (archive) {
		try {
			BackupManager::instance().archive(path);
		} catch (xmilib::Exception const &e) {
			return fail(e.qwhat(), error);
		}
	}
	return library.save(path, true, error);
}

bool ComboLibraryFile::importCombos(QString const &path, ComboList &library, QList<SpCombo> const &additions,
	QList<SpCombo> const &replacements, GroupList const *importedGroups,
	SpGroup const &destinationGroup, bool archive, QString *error) {
	// Existing rows and groups retain their identities. Only imported rows are modified.
	ComboList staged(library);
	GroupList &groups = staged.groupListRef();
	QHash<QUuid, SpGroup> resolvedGroups;
	SpGroup destination;
	if (importedGroups) {
		for (SpGroup const &source: *importedGroups) {
			auto match = groups.findByUuid(source->uuid());
			if (match == groups.end())
				match = std::find_if(groups.begin(), groups.end(), [&](SpGroup const &group) {
					return group->name() == source->name();
				});
			if (match != groups.end())
				resolvedGroups.insert(source->uuid(), *match);
			else {
				if (!groups.append(source))
					return fail(QObject::tr("A group from the Lean combo file could not be imported."), error);
				resolvedGroups.insert(source->uuid(), source);
			}
		}
	} else {
		auto const match = destinationGroup ? groups.findByUuid(destinationGroup->uuid()) : groups.end();
		if (match == groups.end())
			return fail(QObject::tr("Please select a valid group."), error);
		destination = *match;
	}

	auto prepare = [&](SpCombo const &incoming) -> SpCombo {
		SpGroup const group = importedGroups
			? (incoming->group() ? resolvedGroups.value(incoming->group()->uuid()) : SpGroup())
			: destination;
		if (!group)
			return {};
		return incoming->copyForImport(group);
	};
	for (SpCombo const &incoming: additions) {
		SpCombo const combo = prepare(incoming);
		if (!combo)
			return fail(QObject::tr("A combo's group could not be restored from the Lean combo file."), error);
		if (!staged.append(combo))
			return fail(QObject::tr("A combo from the file could not be added to the library."), error);
	}
	for (SpCombo const &incoming: replacements) {
		SpCombo const combo = prepare(incoming);
		if (!combo)
			return fail(QObject::tr("A combo's group could not be restored from the Lean combo file."), error);
		auto const match = staged.findByKeyword(combo->keyword());
		auto const sameId = staged.findByUuid(combo->uuid());
		if (match == staged.end() || (sameId != staged.end() && sameId != match))
			return fail(QObject::tr("A conflicting combo from the file could not be replaced."), error);
		*match = combo;
	}

	if (!save(path, staged, archive, error))
		return false;
	library = std::move(staged);
	return true;
}
