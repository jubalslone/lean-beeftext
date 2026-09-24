/// Tests the same asset lookup and JSON parser as EmojiManager.
/// The package mode runs beside a copied payload, outside the checkout.
#include "Emoji/EmojiRuntimeData.h"
#include <QCryptographicHash>
#include <QTemporaryDir>
#include <QTextStream>

int main(int argc, char **argv) {
	QCoreApplication app(argc, argv);
	QTextStream out(stdout);
	try {
		QStringList const args = app.arguments();
		if (args.contains("--missing-package")) {
			if (!emojiFilePath().isEmpty())
				throw xmilib::Exception("Release lookup escaped the package into a source checkout.");
			out << "PASS: absent packaged data is not rescued by a source-tree decoy.\n";
			return 0;
		}
		if (args.contains("--package")) {
			QString const path = emojiFilePath();
			QString const expected = QDir(app.applicationDirPath()).filePath("emojis/emojis.json");
			if (path != expected || !QFileInfo(path).isFile())
				throw xmilib::Exception("Emoji lookup did not resolve the packaged runtime asset.");
			QJsonObject const data = readEmojiRuntimeData(path);
			if (data.value("grinning").toObject().value("char").toString() != QString::fromUtf8("\xF0\x9F\x98\x80"))
				throw xmilib::Exception("Packaged grinning shortcode did not resolve to U+1F600.");
			QFile file(path);
			if (!file.open(QIODevice::ReadOnly))
				return 1;
			QByteArray const bytes = file.readAll();
			out << "PASS: runtime asset " << path << "; entries=" << data.size()
				<< "; bytes=" << bytes.size() << "; SHA256="
				<< QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex() << "\n";
			return 0;
		}
		QTemporaryDir directory;
		if (!directory.isValid())
			return 1;
		QString const path = directory.filePath("emojis.json");
		for (QByteArray const &invalid: {QByteArray(), QByteArray("{"), QByteArray("[]"),
			QByteArray("{}"), QByteArray(R"({"grinning": {"char": ""}})"),
			QByteArray(R"({"valid": {"char": "x"}, "bad": 3})")}) {
			QFile file(path);
			if (!file.open(QIODevice::WriteOnly) || file.write(invalid) != invalid.size())
				return 1;
			file.close();
			bool rejected = false;
			try { (void)readEmojiRuntimeData(path); }
			catch (xmilib::Exception const &) { rejected = true; }
			if (!rejected)
				throw xmilib::Exception("Malformed/empty emoji data was accepted.");
		}
		QFile file(path);
		QByteArray const valid = R"({"grinning":{"char":"\ud83d\ude00","category":"people"}})";
		if (!file.open(QIODevice::WriteOnly) || file.write(valid) != valid.size())
			return 1;
		file.close();
		if (readEmojiRuntimeData(path).value("grinning").toObject().value("char").toString()
			!= QString::fromUtf8("\xF0\x9F\x98\x80"))
			return 1;
		out << "PASS: malformed, empty, invalid-entry and surrogate JSON cases.\n";
		return 0;
	}
	catch (xmilib::Exception const &error) {
		out << "FAIL: " << error.qwhat() << "\n";
		return 1;
	}
}
