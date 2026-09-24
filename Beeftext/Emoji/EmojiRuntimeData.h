/// Shared runtime asset lookup/parser used by EmojiManager and package acceptance tests.
/// Licensed under the MIT License. See LICENSE in the project root.
#ifndef BEEFTEXT_EMOJI_RUNTIME_DATA_H
#define BEEFTEXT_EMOJI_RUNTIME_DATA_H

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <XMiLib/Exception.h>

inline QString emojiFilePath() {
	QString const packaged = QDir(QCoreApplication::applicationDirPath()).filePath("emojis/emojis.json");
	if (QFileInfo(packaged).isFile())
		return packaged;
#ifndef NDEBUG
	// Retain the existing source-checkout convenience for Debug builds only.
	// Release/package acceptance must never be rescued by a nearby checkout.
	for (int i = 0; i < 10; ++i) {
		QFileInfo const source(QString("../").repeated(i) + "Submodules/emojilib/emojis.json");
		if (source.isFile())
			return source.canonicalFilePath();
	}
#endif
	return {};
}

inline QJsonObject readEmojiRuntimeData(QString const &path) {
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly))
		throw xmilib::Exception("Could not open the emoji list file.");
	QJsonParseError error {};
	QJsonDocument const document = QJsonDocument::fromJson(file.readAll(), &error);
	if (error.error != QJsonParseError::NoError)
		throw xmilib::Exception(QString("Invalid emoji file at index %1: %2")
			.arg(error.offset).arg(error.errorString()));
	if (!document.isObject() || document.object().isEmpty())
		throw xmilib::Exception("The emoji list file is empty or invalid.");
	QJsonObject const root = document.object();
	for (auto it = root.begin(); it != root.end(); ++it) {
		if (it.key().isEmpty() || !it.value().isObject()
			|| it.value().toObject().value("char").toString().isEmpty())
			throw xmilib::Exception("The emoji list contains an invalid entry.");
	}
	return root;
}

#endif
