/// \file
/// \brief Read the options accepted in the first-run migration dialog.

#pragma once

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QVariant>


namespace migration {


struct MigrationChoice {
	bool accepted { false };
	bool importContent { false };
	bool cleanupPortable { false };
	qsizetype selectedGroup { 0 };
};


inline MigrationChoice execMigrationDialog(QDialog &dialog, QComboBox const &sourceChoice,
	QCheckBox const &importCheck, QCheckBox const &portableCheck) {
	MigrationChoice result;
	if (dialog.exec() != QDialog::Accepted)
		return result;

	result.accepted = true;
	result.importContent = importCheck.isChecked();
	// The parent has closed. isHidden() records whether this option itself was hidden for the chosen source.
	result.cleanupPortable = result.importContent && !portableCheck.isHidden() && portableCheck.isChecked();
	result.selectedGroup = sourceChoice.currentData().toLongLong();
	return result;
}


} // namespace migration
