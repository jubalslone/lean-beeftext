/// \file
/// \brief Testable safety rules for one-time compatible Beeftext migration.


#include "LegacyMigrationCore.h"
#include <QDir>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <filesystem>
#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <sys/stat.h>
#endif


namespace migration {


namespace {


QString normalizedPath(QString const &path) {
	if (path.trimmed().isEmpty())
		return QString();
    QFileInfo const info(path);
    QString result = info.canonicalFilePath();
    if (result.isEmpty())
        result = info.absoluteFilePath();
    return QDir::cleanPath(QDir::fromNativeSeparators(result)).toCaseFolded();
}


namespace fs = std::filesystem;

bool containsPath(QString const &root, QString const &path) {
	return root == path || path.startsWith(root.endsWith('/') ? root : root + '/');
}


// Resolve the existing parent even when the first-run data or recovery folder does not exist yet.
QString resolvedCleanupPath(QString const &path) {
	std::error_code error;
	auto const resolved = fs::weakly_canonical(QFileInfo(path).filesystemAbsoluteFilePath(), error);
	return error ? QString() : QString::fromStdU16String(resolved.generic_u16string()).toCaseFolded();
}


bool canTraverseForCleanup(fs::path const &path) {
#ifdef Q_OS_WIN
	DWORD const attributes = GetFileAttributesW(path.c_str());
	if (attributes == INVALID_FILE_ATTRIBUTES)
		return false;
	if (!(attributes & FILE_ATTRIBUTE_REPARSE_POINT))
		return true;

	// Cloud placeholders can be reparse points without redirecting to another named file.
	WIN32_FIND_DATAW data {};
	HANDLE const handle = FindFirstFileW(path.c_str(), &data);
	if (handle == INVALID_HANDLE_VALUE)
		return false;
	FindClose(handle);
	return !IsReparseTagNameSurrogate(data.dwReserved0);
#else
	std::error_code error;
	auto const status = fs::symlink_status(path, error);
	return !error && fs::exists(status) && !fs::is_symlink(status);
#endif
}


} // namespace


bool isStrongPortableCandidate(QString const &executableFolder, QString *outComboFilePath,
    QString *outDedicatedRootPath, EPortableProduct *outProduct) {
    QDir const appDir(executableFolder);
    QString const upstreamExecutable = appDir.absoluteFilePath("Beeftext.exe");
    QString const leanExecutable = appDir.absoluteFilePath("LeanBeeftext.exe");
    bool const hasUpstreamExecutable = QFileInfo(upstreamExecutable).isFile();
    bool const hasLeanExecutable = QFileInfo(leanExecutable).isFile();
    if (hasUpstreamExecutable == hasLeanExecutable)
        return false;
    EPortableProduct const product = hasLeanExecutable ? EPortableProduct::LeanBeeftext : EPortableProduct::UpstreamBeeftext;
    QString comboPath;
    QString rootPath;
    if (QFileInfo(appDir.absoluteFilePath("Portable.bin")).isFile()) {
        comboPath = appDir.absoluteFilePath("Data/comboList.json");
        rootPath = appDir.absolutePath();
    } else if (QFileInfo(appDir.absoluteFilePath("PortableApps.bin")).isFile()) {
        comboPath = appDir.absoluteFilePath("../../Data/settings/comboList.json");
        rootPath = appDir.absoluteFilePath("../..");
    } else {
        return false;
    }

    QFile comboFile(comboPath);
    if (!QFileInfo(comboPath).isFile() || !comboFile.open(QIODevice::ReadOnly))
        return false;
    QByteArray const comboData = comboFile.readAll().trimmed();
    if (comboData.isEmpty())
        return false;
    if (product == EPortableProduct::LeanBeeftext) {
        QJsonParseError error;
        QJsonDocument const document = QJsonDocument::fromJson(comboData, &error);
        QJsonObject const root = document.object();
        if (error.error != QJsonParseError::NoError || !document.isObject() ||
            !root.value("fileFormatVersion").isDouble() || !root.value("combos").isArray() ||
            (root.value("fileFormatVersion").toInt() >= 3 && !root.value("groups").isArray()))
            return false;
    }

    if (outComboFilePath)
        *outComboFilePath = QFileInfo(comboPath).absoluteFilePath();
    if (outDedicatedRootPath)
        *outDedicatedRootPath = QDir::cleanPath(QFileInfo(rootPath).absoluteFilePath());
    if (outProduct)
        *outProduct = product;
    return true;
}


bool isBroadCleanupRoot(QString const &candidatePath, QStringList const &sharedRoots, QStringList const &protectedTrees) {
	if (!QDir::isAbsolutePath(candidatePath) || !QFileInfo::exists(candidatePath) || QDir(candidatePath).isRoot())
		return true;

	auto path = QFileInfo(candidatePath).filesystemAbsoluteFilePath().lexically_normal();
	for (;;) {
		if (!canTraverseForCleanup(path))
			return true;
		if (path == path.root_path())
			break;
		path = path.parent_path();
	}

	QString const candidate = resolvedCleanupPath(candidatePath);
	QString const lexical = QDir::cleanPath(QDir::fromNativeSeparators(candidatePath)).toCaseFolded();
	if (candidate.isEmpty())
		return true;
	auto overlaps = [&](QString const &root, bool protectDescendants) {
		QString const original = QDir::cleanPath(QDir::fromNativeSeparators(root)).toCaseFolded();
		QString const resolved = resolvedCleanupPath(root);
		if (containsPath(lexical, original) || (protectDescendants && containsPath(original, lexical)))
			return true;
		if (resolved.isEmpty())
			return protectDescendants;
		return containsPath(candidate, resolved) || (protectDescendants && containsPath(resolved, candidate));
	};
	for (QString const &root: sharedRoots)
		if (!root.isEmpty() && overlaps(root, false))
			return true;
	for (QString const &tree: protectedTrees)
		if (!QDir::isAbsolutePath(tree) || overlaps(tree, true))
			return true;
	return false;
}


QByteArray portableDirectoryIdentity(QString const &path) {
	QFileInfo const info(path);
	if (!info.isAbsolute() || !info.isDir())
		return {};
#ifdef Q_OS_WIN
	auto const native = info.filesystemAbsoluteFilePath();
	HANDLE const handle = CreateFileW(native.c_str(), FILE_READ_ATTRIBUTES,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
		FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
	if (handle == INVALID_HANDLE_VALUE)
		return {};
	FILE_ID_INFO id {};
	bool const available = GetFileInformationByHandleEx(handle, FileIdInfo, &id, sizeof(id)) != FALSE;
	CloseHandle(handle);
	QByteArray const fileId(reinterpret_cast<char const *>(id.FileId.Identifier), sizeof(id.FileId.Identifier));
	if (!available || fileId == QByteArray(fileId.size(), '\0'))
		return {};
	return QByteArray::number(id.VolumeSerialNumber, 16) + ':' + fileId.toHex();
#else
	struct stat entry {};
	if (::lstat(QFile::encodeName(path).constData(), &entry) != 0 || !S_ISDIR(entry.st_mode))
		return {};
	return QByteArray::number(qulonglong(entry.st_dev), 16) + ':' + QByteArray::number(qulonglong(entry.st_ino), 16);
#endif
}


bool portableDirectoryMatches(QString const &path, QByteArray const &recordedIdentity) {
	return !recordedIdentity.isEmpty() && portableDirectoryIdentity(path) == recordedIdentity;
}


bool canRecyclePortableTree(QString const &rootPath) {
	auto const root = QFileInfo(rootPath).filesystemAbsoluteFilePath();
	if (!canTraverseForCleanup(root))
		return false;
	std::error_code error;
	for (fs::recursive_directory_iterator it(root, error), end; !error && it != end; it.increment(error)) {
		// Inspect each entry before descending. Never follow links out of the selected portable tree.
		if (!canTraverseForCleanup(it->path()))
			return false;
	}
	return !error;
}


bool isRecognizableInstalledCandidate(QString const &displayName, QString const &publisher,
	QString const &installLocation, QString const &executablePath) {
    QString const foldedName = displayName.toCaseFolded();
    if (!foldedName.contains("beeftext") || foldedName.contains("lean beeftext"))
        return false;
    if (!publisher.trimmed().isEmpty() && !publisher.contains("Michelon", Qt::CaseInsensitive) &&
        !publisher.contains("Beeftext", Qt::CaseInsensitive))
        return false;

    QString const root = normalizedPath(installLocation);
    QString const executable = normalizedPath(executablePath);
	if (root.isEmpty() || executable.isEmpty())
        return false;
    QString const prefix = root.endsWith('/') ? root : root + '/';
	return executable.startsWith(prefix) &&
		QFileInfo(executablePath).fileName().compare("Beeftext.exe", Qt::CaseInsensitive) == 0;
}


QByteArray sourceContentDigest(QByteArray const &contents) {
	return QCryptographicHash::hash(contents, QCryptographicHash::Sha256);
}


bool sourceContentMatchesDigest(QByteArray const &contents, QByteArray const &digest) {
	return !digest.isEmpty() && sourceContentDigest(contents) == digest;
}


QList<QList<qsizetype>> groupSourcesByContent(QList<QByteArray> const &digests) {
    QList<QList<qsizetype>> result;
    QHash<QByteArray, qsizetype> groupByDigest;
    for (qsizetype i = 0; i < digests.size(); ++i) {
        auto const existing = groupByDigest.constFind(digests[i]);
        if (existing == groupByDigest.constEnd()) {
            groupByDigest.insert(digests[i], result.size());
            result.append(QList<qsizetype>{ i });
        } else {
            result[*existing].append(i);
        }
    }
    return result;
}


bool cleanupAllowed(ValidationResult const &result) {
    return result.snapshotCreated && result.parsed && result.persisted && result.reloaded && result.corresponds;
}


bool shouldRunMigration(bool portableMode, bool leanComboFileExists, EState state) {
    return !portableMode && !leanComboFileExists && state == EState::NeverChecked;
}


} // namespace migration
