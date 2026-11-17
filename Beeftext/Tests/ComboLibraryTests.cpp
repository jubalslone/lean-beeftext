#include "stdafx.h"
#include "Combo/ComboLibraryFile.h"
#include <stdexcept>

namespace {

void require(bool condition, QString const &message) {
	if (!condition)
		throw std::runtime_error(message.toStdString());
}

void writeFixture(QString const &path, QByteArray const &data) {
	QFile file(path);
	require(file.open(QIODevice::WriteOnly | QIODevice::Truncate), file.errorString());
	require(file.write(data) == data.size(), file.errorString());
}

QByteArray readFixture(QString const &path) {
	QFile file(path);
	require(file.open(QIODevice::ReadOnly), file.errorString());
	return file.readAll();
}

SpCombo makeCombo(QString const &keyword, SpGroup const &group, QString const &snippet = "example") {
	SpCombo const combo = Combo::create(keyword, keyword, snippet);
	combo->setGroup(group);
	return combo;
}

void testFailedLoad(QString const &path) {
	ComboList library;
	ComboLibraryFile file;
	bool older = false;
	QString error;
	require(file.open(path, library, older, &error), error);
	SpGroup const group = library.groupListRef()[0];
	SpCombo const original = makeCombo("existing", group);
	require(library.append(original), "fixture append");
	require(file.save(path, library, false, &error), error);
	QJsonObject document = library.toJsonDocument(true).object();
	QJsonArray rows = document.value("combos").toArray();
	rows.append(42);
	document.insert("combos", rows);
	QByteArray const damaged = QJsonDocument(document).toJson();
	writeFixture(path, damaged);

	int resets = 0;
	QObject::connect(&library, &ComboList::modelReset, [&] { ++resets; });
	require(!file.open(path, library, older, &error) && !error.isEmpty(), "late invalid row must fail");
	require(library.size() == 1 && library[0] == original && library.groupListRef()[0] == group && !resets,
		"failed reload must preserve existing objects and model indexes");
	require(library.append(makeCombo("attempted-edit", group)), "attempted ordinary edit");
	require(!file.save(path, library, false, &error) && readFixture(path) == damaged,
		"an attempted edit after a failed read must preserve the original file exactly");

	ComboList startup;
	ComboLibraryFile startupFile;
	require(!startupFile.open(path, startup, older, &error) && startup.isEmpty(),
		"startup must never expose a partially parsed library");
	require(!startupFile.save(path, startup, false, &error) && readFixture(path) == damaged,
		"failed startup must block an empty-library overwrite");

	rows.removeLast();
	rows.append(rows.first());
	document.insert("combos", rows);
	writeFixture(path, QJsonDocument(document).toJson());
	require(!file.open(path, library, older, &error) && library.size() == 2,
		"duplicated UUIDs must fail the entire load");
	rows.removeLast();
	document.insert("combos", rows);
	writeFixture(path, QJsonDocument(document).toJson());
	require(file.open(path, library, older, &error) && library.size() == 1, "recover after file is repaired");
	require(file.save(path, library, false, &error), "valid reload permits writes again");
}

void testImport(QString const &path) {
	ComboList library;
	ComboLibraryFile file;
	bool older = false;
	QString error;
	require(file.open(path, library, older, &error), error);
	SpGroup const local = library.groupListRef()[0];
	SpCombo const original = makeCombo("existing", local);
	require(library.append(original), "fixture append");
	require(file.save(path, library, false, &error), error);
	QByteArray const before = readFixture(path);

	GroupList importedGroups;
	SpGroup const sameName = Group::create(local->name());
	SpGroup const additional = Group::create("Imported group");
	SpGroup const missing = Group::create("Undeclared group");
	require(importedGroups.append(sameName) && importedGroups.append(additional), "fixture groups");
	SpCombo const addition = makeCombo("new", additional);
	SpCombo const bad = makeCombo("invalid-group", missing);
	SpCombo const replacement = makeCombo("existing", sameName, "replacement");
	int comboResets = 0;
	int groupResets = 0;
	QObject::connect(&library, &ComboList::modelReset, [&] { ++comboResets; });
	QObject::connect(&library.groupListRef(), &GroupList::modelReset, [&] { ++groupResets; });
	require(!file.importCombos(path, library, {addition, bad}, {replacement}, &importedGroups, {}, false, &error),
		"late bad group must reject the whole import");
	require(!error.isEmpty() && library.size() == 1 && library[0] == original && library.groupListRef().size() == 1
		&& readFixture(path) == before && !comboResets && !groupResets, "rejected import must have no side effects");

#ifdef Q_OS_WIN
	HANDLE const held = CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()), GENERIC_READ, FILE_SHARE_READ,
		nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	require(held != INVALID_HANDLE_VALUE, "hold destination open without delete sharing");
	bool const written = file.importCombos(path, library, {addition}, {replacement}, &importedGroups, {}, false, &error);
	CloseHandle(held);
	require(!written && readFixture(path) == before && !comboResets && !groupResets && library[0] == original,
		"failed atomic replacement must keep the file and visible library intact");
#endif
	require(file.importCombos(path, library, {addition}, {replacement}, &importedGroups, {}, false, &error), error);
	require(library.size() == 2 && library[0]->snippet() == "replacement" && library[0]->group() == local
		&& library[1]->group() == additional && library.groupListRef().size() == 2, "published canonical groups");
	require(replacement->group() == sameName && original->snippet() == "example", "incoming and previous rows remain intact");
	require(comboResets == 1 && groupResets == 1, "successful import publishes both models exactly once");
	ComboList disk;
	require(disk.load(path) && disk.toJsonDocument(true) == library.toJsonDocument(true),
		"visible import and persisted library must agree");

	SpCombo const legacy = makeCombo("csv", {});
	require(file.importCombos(path, library, {legacy}, {}, nullptr, local, false, &error) &&
		library[2]->group() == local && !legacy->group(), "legacy import retains chosen destination");
}

void testAppearingLibrary(QString const &path) {
	ComboList library;
	ComboLibraryFile file;
	bool older = false;
	QString error;
	require(file.open(path, library, older, &error), error);
	QByteArray const external("existing library supplied after startup");
	writeFixture(path, external);
	SpGroup const local = library.groupListRef()[0];
	require(!file.importCombos(path, library, {makeCombo("new", {})}, {}, nullptr, local, false, &error)
		&& !error.isEmpty() && library.isEmpty() && readFixture(path) == external,
		"failed first import must leave both visible library and competing file intact");
	require(!file.save(path, library, false, &error) && readFixture(path) == external,
		"a repeated ordinary save must also refuse the competing file");
}

} // namespace

int main(int argc, char **argv) {
	QCoreApplication app(argc, argv);
	try {
		QTemporaryDir root;
		require(root.isValid(), root.errorString());
		testFailedLoad(root.filePath("library.json"));
		testImport(root.filePath("import.json"));
		testAppearingLibrary(root.filePath("appears.json"));
		qInfo() << "Combo library load, write protection and import transactions passed.";
		return 0;
	} catch (std::exception const &error) {
		qCritical() << error.what();
		return 1;
	}
}
