#pragma once

#include "ComboList.h"

// The active profile may replace its library only after successfully opening it.
// A fresh profile's first save must also refuse a file that appeared in the meantime.
class ComboLibraryFile {
public:
	bool open(QString const &path, ComboList &library, bool &olderFormat, QString *error);
	bool isNew() const { return state_ == State::New; }
	bool save(QString const &path, ComboList const &library, bool archive, QString *error);
	bool importCombos(QString const &path, ComboList &library, QList<SpCombo> const &additions,
		QList<SpCombo> const &replacements, GroupList const *importedGroups,
		SpGroup const &destinationGroup, bool archive, QString *error);

private:
	enum class State { Unopened, New, Loaded };
	State state_ = State::Unopened;
};
