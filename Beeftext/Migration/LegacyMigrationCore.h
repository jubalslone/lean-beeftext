/// \file
/// \brief Testable safety rules for one-time compatible Beeftext migration.

#ifndef LEAN_BEEFTEXT_LEGACY_MIGRATION_CORE_H
#define LEAN_BEEFTEXT_LEGACY_MIGRATION_CORE_H

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>


namespace migration {


enum class ESourceType {
    Installed,
    Portable,
};


enum class EPortableProduct {
    UpstreamBeeftext,
    LeanBeeftext,
};


enum class EState {
    NeverChecked = 0,
    ImportCompleted = 1,
    Complete = 2,
};


struct ValidationResult {
    bool snapshotCreated { false };
    bool parsed { false };
    bool persisted { false };
    bool reloaded { false };
    bool corresponds { false };
};


bool isStrongPortableCandidate(QString const &executableFolder, QString *outComboFilePath = nullptr,
    QString *outDedicatedRootPath = nullptr, EPortableProduct *outProduct = nullptr); ///< Require a beacon, expected data layout, one recognized executable, and readable combo data.
bool isBroadCleanupRoot(QString const &candidatePath, QStringList const &sharedRoots,
	QStringList const &protectedTrees = {}); ///< Allow dedicated children of shared roots, but forbid any overlap with protected trees.
QByteArray portableDirectoryIdentity(QString const &path); ///< Stable filesystem identity of the selected directory.
bool portableDirectoryMatches(QString const &path, QByteArray const &recordedIdentity);
bool canRecyclePortableTree(QString const &rootPath); ///< Refuse unreadable trees or links that redirect filesystem traversal.
bool isRecognizableInstalledCandidate(QString const &displayName, QString const &publisher,
	QString const &installLocation, QString const &executablePath); ///< Identify upstream without granting cleanup permission.
QByteArray sourceContentDigest(QByteArray const &contents); ///< Compute the digest that binds source refresh, snapshot, import, and cleanup.
bool sourceContentMatchesDigest(QByteArray const &contents, QByteArray const &digest); ///< Revalidate source bytes against their recorded digest.
QList<QList<qsizetype>> groupSourcesByContent(QList<QByteArray> const &digests); ///< Group byte-identical source libraries.
bool cleanupAllowed(ValidationResult const &result); ///< Gate all cleanup on the full import validation sequence.
bool shouldRunMigration(bool portableMode, bool leanComboFileExists, EState state); ///< Installed first-run gating.


} // namespace migration


#endif // LEAN_BEEFTEXT_LEGACY_MIGRATION_CORE_H
