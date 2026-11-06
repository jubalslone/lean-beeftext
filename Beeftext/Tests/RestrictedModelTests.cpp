/// \file
///
/// \brief Behavior tests for the Lean Beeftext restricted execution model.
///
/// Every test here exercises code (variable classification and evaluation, text sanitizing, cursor planning,
/// preference persistence, combo import/export, migration safety rules). None of them inspect source files.


#include "../RestrictedModel.h"
#include "../Combo/ComboPortability.h"
#include "../Migration/LegacyMigrationCore.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QSettings>
#include <QSet>
#include <QStringList>
#include <QTemporaryDir>
#include <QTimeZone>
#include <QUuid>

#include <cstdio>


namespace {


int failureCount = 0;


void expect(bool condition, QString const &description) {
    if (condition)
        return;
    QByteArray const utf8Description = description.toUtf8();
    std::fprintf(stderr, "FAILED: %s\n", utf8Description.constData());
    ++failureCount;
}


void expectText(QString const &actual, QString const &expected, QString const &description) {
    if (actual == expected)
        return;
    QByteArray const utf8Description = description.toUtf8();
    QByteArray const utf8Expected = expected.toUtf8();
    QByteArray const utf8Actual = actual.toUtf8();
    std::fprintf(stderr, "FAILED: %s expected \"%s\" but got \"%s\"\n", utf8Description.constData(),
        utf8Expected.constData(), utf8Actual.constData());
    ++failureCount;
}


//****************************************************************************************************************************************************
// Variable allowlist
//****************************************************************************************************************************************************
void testVariableAllowlist() {
    using Variable = restricted::ERestrictedVariable;
    expect(restricted::classifyVariable("cursor") == Variable::Cursor, "cursor is allowed");
    expect(restricted::classifyVariable("date") == Variable::Date, "date is allowed");
    expect(restricted::classifyVariable("time") == Variable::Time, "time is allowed");
    expect(restricted::classifyVariable("dateTime") == Variable::DateTime, "dateTime is allowed");
    expect(restricted::classifyVariable("dateTime:+1d:yyyy-MM-dd") == Variable::CustomDateTime,
           "valid shifted dateTime is allowed");
    expect(restricted::classifyVariable("dateTime:yyyy-MM-dd") == Variable::CustomDateTime,
           "valid unshifted dateTime is allowed");
    expect(restricted::classifyVariable("combo:client") == Variable::Combo, "combo is allowed");
    expect(restricted::classifyVariable("upper:client") == Variable::Upper, "upper is allowed");
    expect(restricted::classifyVariable("lower:client") == Variable::Lower, "lower is allowed");
    expect(restricted::classifyVariable("trim:client") == Variable::Trim, "trim is allowed");
    expect(restricted::classifyVariable("input:Client name") == Variable::Input, "input is allowed");

    QStringList const blocked = {
        "clipboard", "discordemoji", "envVar:USERNAME", "powershell:C:\\test.ps1",
        "key:enter", "shortcut:Win+R", "delay:50", "dateTime:+1q:yyyy", "dateTime:+1d", "unknown",
        "CURSOR", "Date", "DATETIME", "combo", "input"
    };
    for (QString const &variable: blocked)
        expect(restricted::classifyVariable(variable) == Variable::Blocked,
               QString("%1 is blocked").arg(variable));
}


//****************************************************************************************************************************************************
// Date/time evaluation with a fixed clock
//****************************************************************************************************************************************************
void testDateTimeEvaluation() {
    using Variable = restricted::ERestrictedVariable;
    QLocale const locale(QLocale::English, QLocale::UnitedStates);
    QDateTime const now(QDate(2026, 9, 1), QTime(23, 30, 15), QTimeZone::utc());

    expectText(restricted::evaluateDateTimeVariable("dateTime:yyyy-MM-dd", now, locale), "2026-09-01",
               "custom format without shift");
    expectText(restricted::evaluateDateTimeVariable("dateTime:+1d:yyyy-MM-dd", now, locale), "2026-09-02",
               "one-day shift");
    expectText(restricted::evaluateDateTimeVariable("dateTime:+1w-2d:yyyy-MM-dd", now, locale), "2026-09-06",
               "combined week and day shift");
    expectText(restricted::evaluateDateTimeVariable("dateTime:-1M:yyyy-MM-dd", now, locale), "2026-08-01",
               "uppercase M shifts months");
    expectText(restricted::evaluateDateTimeVariable("dateTime:-30m:HH:mm", now, locale), "23:00",
               "lowercase m shifts minutes");
    expectText(restricted::evaluateDateTimeVariable("dateTime:+1y:yyyy", now, locale), "2027", "year shift");
    expectText(restricted::evaluateDateTimeVariable("dateTime:+90s:HH:mm:ss", now, locale), "23:31:45",
               "second shift");
    expectText(restricted::evaluateDateTimeVariable("dateTime:+1h:HH", now, locale), "00", "hour shift crosses midnight");
    expectText(restricted::evaluateDateTimeVariable("dateTime:MMM d, yyyy 'at' h:mm AP", now, locale),
               "Sep 1, 2026 at 11:30 PM", "quoted literal text in format");
    expectText(restricted::evaluateDateTimeVariable("dateTime:ww", now, locale), "36", "two-digit ISO week extension");
    expectText(restricted::evaluateDateTimeVariable("dateTime:w", now, locale), "36", "ISO week extension");
    expectText(restricted::evaluateDateTimeVariable("dateTime:yyyy\\}MM", now, locale), "2026}09",
               "escaped closing brace in format");
    expectText(restricted::evaluateDateTimeVariable("dateTime:", now, locale), locale.toString(now),
               "empty format falls back to the locale long format");
    expectText(restricted::evaluateDateTimeVariable("dateTime:+1d:", now, locale), locale.toString(now.addDays(1)),
               "shift with empty format uses the locale long format");

    expect(restricted::evaluateDateTimeVariable("dateTime:+99999999999999999999d:yyyy", now, locale).isNull(),
           "overflowing shift fails closed");
    expect(restricted::evaluateDateTimeVariable("dateTime:+1q:yyyy", now, locale).isNull(),
           "unknown shift unit fails closed");
    expect(restricted::evaluateDateTimeVariable("date:yyyy", now, locale).isNull(),
           "only dateTime accepts a custom format");
    expect(!restricted::shiftedDateTime(now, "+1d-").isValid(), "dangling shift sign is invalid");
    expect(!restricted::shiftedDateTime(now, "").isValid(), "empty shift is invalid");
    expect(restricted::shiftedDateTime(now, "+2w") == now.addDays(14), "week shift is seven days");
    expect(restricted::shiftedDateTime(now, "+500z") == now.addMSecs(500), "millisecond shift");

    expectText(restricted::evaluateDateVariable(Variable::Date, "date", now, locale), locale.toString(now.date()),
               "date uses the locale date format");
    expectText(restricted::evaluateDateVariable(Variable::Time, "time", now, locale), locale.toString(now.time()),
               "time uses the locale time format");
    expectText(restricted::evaluateDateVariable(Variable::DateTime, "dateTime", now, locale), locale.toString(now),
               "dateTime uses the locale date/time format");
    expectText(restricted::evaluateDateVariable(Variable::CustomDateTime, "dateTime:yyyy", now, locale), "2026",
               "custom dateTime dispatches to the format evaluator");
    expect(restricted::evaluateDateVariable(Variable::Blocked, "clipboard", now, locale).isNull(),
           "blocked variables never evaluate");

    expectText(restricted::resolveEscapingInVariableParameter(R"(a\}b\\c)"), R"(a}b\c)", "parameter escapes resolve");
}


//****************************************************************************************************************************************************
// Sanitizing
//****************************************************************************************************************************************************
void testSanitizer() {
    expectText(restricted::sanitizeText("a\r\nb\nc\rd\te"), "a\\nb\\nc\\nd\\te",
               "line endings and tabs become visible text");

    QString controls;
    controls += QChar(0x0000);
    controls += QChar(0x0008);
    controls += QChar(0x001b);
    controls += QChar(0x007f);
    controls += QChar(0x0085);
    controls += QChar(0x2028);
    controls += QChar(0x2029);
    expectText(restricted::sanitizeText(controls),
               "\\u0000\\u0008\\u001B\\u007F\\u0085\\u2028\\u2029",
               "control and Unicode separator characters become visible text");

    QString emoji;
    emoji += QChar(0xd83d);
    emoji += QChar(0xde00);
    expectText(restricted::sanitizeText(emoji), emoji, "valid UTF-16 surrogate pairs are preserved");

    QString unpaired;
    unpaired += QChar(0xd83d);
    unpaired += 'x';
    unpaired += QChar(0xde00);
    expectText(restricted::sanitizeText(unpaired), "\\uD83Dx\\uDE00", "unpaired surrogates become visible text");
    expectText(restricted::sanitizeText(restricted::sanitizeText(controls)), restricted::sanitizeText(controls),
               "sanitization is idempotent");
    expectText(restricted::sanitizeText(QString::fromUtf8("caf\xc3\xa9 \xe6\x97\xa5\xe6\x9c\xac")),
               QString::fromUtf8("caf\xc3\xa9 \xe6\x97\xa5\xe6\x9c\xac"), "ordinary non-ASCII text is untouched");
}


void testMultilineSanitizer() {
    QString const mixedLineEndings = "a\n\nb\r\nc\rd\n";
    expectText(restricted::sanitizeText(mixedLineEndings, true), "a\n\nb\nc\nd\n",
               "real-line-break mode normalizes LF, CRLF, and CR to one LF");
    expectText(restricted::sanitizeText(mixedLineEndings, false), "a\\n\\nb\\nc\\nd\\n",
               "strict mode renders every logical line break as visible text");
    expectText(restricted::sanitizeText(restricted::sanitizeText(mixedLineEndings, true), true),
               restricted::sanitizeText(mixedLineEndings, true),
               "real-line-break sanitization is idempotent");

    QString emoji;
    emoji += QChar(0xd83d);
    emoji += QChar(0xde00);
    QString const multilineEmoji = QString("before\n") + emoji + "\nafter";
    expectText(restricted::sanitizeText(multilineEmoji, true), multilineEmoji,
               "real line breaks do not change valid surrogate pairs");
    expectText(restricted::sanitizeText(QString(QChar(0x2028)), true), "\\u2028",
               "Unicode line separators remain visible in real-line-break mode");
}


void testBlockedControlsInBothModes() {
    QStringList const blocked = {
        "#{key:enter}", "#{shortcut:Win+R}", "#{delay:500}", "#{clipboard}",
        "#{envVar:USERNAME}", "#{powershell:C:\\test.ps1}"
    };
    for (bool const allowRealLineBreaks: { false, true }) {
        for (QString const &token: blocked) {
            restricted::RestrictedSnippet const snippet = restricted::prepareSnippet(token, allowRealLineBreaks);
            expectText(snippet.text, token,
                QString("%1 remains literal when real line breaks are %2")
                    .arg(token, allowRealLineBreaks ? "allowed" : "visible"));
            expect(snippet.cursorLeftCount == -1, "blocked syntax cannot create cursor movement");
        }
    }

    restricted::RestrictedSnippet const multiline =
        restricted::prepareSnippet("first\n#{key:enter}\nlast", true);
    expectText(multiline.text, "first\n#{key:enter}\nlast",
               "allowed text line breaks do not activate a blocked Enter variable");
}


//****************************************************************************************************************************************************
// Cursor planning
//****************************************************************************************************************************************************
void testCursorPlan() {
    restricted::RestrictedSnippet snippet = restricted::prepareSnippet("plain");
    expectText(snippet.text, "plain", "plain text is unchanged");
    expect(snippet.cursorLeftCount == -1, "plain text has no cursor action");

    snippet = restricted::prepareSnippet("#{cursor}abc");
    expectText(snippet.text, "abc", "cursor marker at start is removed");
    expect(snippet.cursorLeftCount == 3, "cursor marker at start moves within inserted text");

    snippet = restricted::prepareSnippet("ab#{cursor}cd");
    expectText(snippet.text, "abcd", "cursor marker in middle is removed");
    expect(snippet.cursorLeftCount == 2, "cursor marker in middle has a bounded move");

    snippet = restricted::prepareSnippet("abc#{cursor}");
    expectText(snippet.text, "abc", "cursor marker at end is removed");
    expect(snippet.cursorLeftCount == 0, "cursor marker at end needs no move");

    snippet = restricted::prepareSnippet("parent child-#{cursor}end");
    expectText(snippet.text, "parent child-end", "one cursor from a nested-combo expansion is removed");
    expect(snippet.cursorLeftCount == QString("end").size(), "one post-expansion nested cursor has a bounded move");

    snippet = restricted::prepareSnippet("#{cursor}X#{CURSOR}");
    expectText(snippet.text, "X#{CURSOR}", "mixed-case cursor text remains literal");
    expect(snippet.cursorLeftCount == snippet.text.size(), "mixed-case literal is included in the cursor bound");

    snippet = restricted::prepareSnippet("a#{cursor}b#{cursor}c");
    expectText(snippet.text, "abc", "two direct cursor markers are removed");
    expect(snippet.cursorLeftCount == 1, "the last direct cursor marker wins");
    expect(!snippet.cursorSyntaxRejected, "two safe direct cursor markers are deterministic");

    snippet = restricted::prepareSnippet("parent child-#{cursor}end / child-#{cursor}end");
    expectText(snippet.text, "parent child-end / child-end", "cursor markers from two nested-combo expansions are removed");
    expect(snippet.cursorLeftCount == QString("end").size(), "the last cursor from repeated nested-combo expansion wins");

    snippet = restricted::prepareSnippet("UPPER-#{CURSOR}-TEXT");
    expectText(snippet.text, "UPPER-#{CURSOR}-TEXT", "an upper transformation makes a cursor marker literal");
    expect(snippet.cursorLeftCount == -1, "transformed mixed-case cursor text cannot create movement");

    snippet = restricted::prepareSnippet("lower-#{cursor}-text");
    expectText(snippet.text, "lower--text", "a lower or trim transformation that preserves an exact marker remains usable");
    expect(snippet.cursorLeftCount == QString("-text").size(), "transformation output uses the same bounded suffix rule");

    // Precomposed accented letters, CJK, and Cyrillic are single code units and single graphemes: one Left each.
    QString const accented = QString::fromUtf8("Bonjour #{cursor}, \xc3\xa0 bient\xc3\xb4t");
    snippet = restricted::prepareSnippet(accented);
    expectText(snippet.text, QString::fromUtf8("Bonjour , \xc3\xa0 bient\xc3\xb4t"), "accented Latin suffix is accepted");
    expect(!snippet.cursorSyntaxRejected && snippet.cursorLeftCount == QString::fromUtf8(", \xc3\xa0 bient\xc3\xb4t").size(),
           "accented Latin suffix moves one position per character");
    snippet = restricted::prepareSnippet(QString::fromUtf8("#{cursor}\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e"));
    expect(!snippet.cursorSyntaxRejected && snippet.cursorLeftCount == 3, "CJK suffix moves one position per character");
    snippet = restricted::prepareSnippet(QString::fromUtf8("#{cursor}\xd0\x9f\xd1\x80\xd0\xb8\xd0\xb2\xd0\xb5\xd1\x82"));
    expect(!snippet.cursorSyntaxRejected && snippet.cursorLeftCount == 6, "Cyrillic suffix moves one position per character");

    // Supplementary-plane characters, combining marks, and joiners cannot be counted; the marker is still removed.
    QString emoji;
    emoji += QChar(0xd83d);
    emoji += QChar(0xde00);
    snippet = restricted::prepareSnippet("a#{cursor}" + emoji);
    expectText(snippet.text, "a" + emoji, "emoji after the marker: marker is removed, emoji is kept");
    expect(snippet.cursorSyntaxRejected && snippet.cursorLeftCount == -1, "emoji suffix refuses cursor movement");

    QString const combining = QString("e") + QChar(0x0301);
    snippet = restricted::prepareSnippet("a#{cursor}" + combining);
    expectText(snippet.text, "a" + combining, "combining mark after the marker: marker is removed");
    expect(snippet.cursorSyntaxRejected, "combining mark suffix refuses cursor movement");

    snippet = restricted::prepareSnippet("a#{cursor}b" + QString(QChar(0x200d)) + "c");
    expect(snippet.cursorSyntaxRejected, "zero-width joiner suffix refuses cursor movement");
    snippet = restricted::prepareSnippet("a#{cursor}" + QString(QChar(0xfe0f)));
    expect(snippet.cursorSyntaxRejected, "variation selector suffix refuses cursor movement");

    snippet = restricted::prepareSnippet("a#{cursor}b#{cursor}" + emoji);
    expectText(snippet.text, "ab" + emoji, "all markers are removed even when the final suffix is unsafe");
    expect(snippet.cursorLeftCount == -1 && snippet.cursorSyntaxRejected, "unsafe repeated cursor syntax fails closed");

    snippet = restricted::prepareSnippet(emoji + "#{cursor}abc");
    expect(!snippet.cursorSyntaxRejected && snippet.cursorLeftCount == 3,
           "emoji before the marker does not affect the suffix bound");

    snippet = restricted::prepareSnippet("a#{cursor}#{key:enter}#{delay:10}");
    expectText(snippet.text, "a#{key:enter}#{delay:10}", "blocked control syntax remains visible");
    expect(snippet.cursorLeftCount == QString("#{key:enter}#{delay:10}").size(),
           "blocked literal syntax is included in the cursor bound");

    snippet = restricted::prepareSnippet("before\n#{cursor}after\nline", true);
    expectText(snippet.text, "before\nafter\nline", "newlines before and after cursor are preserved when allowed");
    expect(snippet.cursorLeftCount == QString("after\nline").size(),
           "each allowed logical line break counts as one bounded cursor movement");

    snippet = restricted::prepareSnippet("before#{cursor}\r\n", true);
    expectText(snippet.text, "before\n", "CRLF after cursor is normalized to one line break");
    expect(snippet.cursorLeftCount == 1, "normalized CRLF requires one bounded cursor movement");

    snippet = restricted::prepareSnippet("first#{cursor}\nsecond#{cursor}\nthird", true);
    expectText(snippet.text, "first\nsecond\nthird", "multiple markers preserve real line breaks after expansion");
    expect(snippet.cursorLeftCount == QString("\nthird").size(), "real-line-break mode bounds movement from the last marker");

    snippet = restricted::prepareSnippet("first#{cursor}\r\nsecond#{cursor}\r\nthird", false);
    expectText(snippet.text, "first\\nsecond\\nthird", "multiple markers preserve visible line breaks in strict mode");
    expect(snippet.cursorLeftCount == QString("\\nthird").size(), "strict mode bounds movement from the last marker");

    snippet = restricted::prepareSnippet("a#{cursor}b\tc");
    expectText(snippet.text, "ab\\tc", "tabs after the marker are visible text");
    expect(snippet.cursorLeftCount == 4, "visible tab text counts as inserted characters");
}


//****************************************************************************************************************************************************
// Preference persistence
//****************************************************************************************************************************************************
void testMultilinePreferencePersistence() {
    QTemporaryDir temporaryDirectory;
    expect(temporaryDirectory.isValid(), "temporary preference directory is available");
    if (!temporaryDirectory.isValid())
        return;

    QString const dataFolder = QDir(temporaryDirectory.path()).absoluteFilePath("Data");
    expect(QDir().mkpath(dataFolder), "portable Data test folder is created");
    QString const settingsPath = QDir(dataFolder).absoluteFilePath("Settings.ini");

    {
        QSettings settings(settingsPath, QSettings::IniFormat);
        expect(!restricted::readAllowRealLineBreaksInSnippets(settings),
               "missing multiline preference defaults to visible line breaks");
        restricted::writeAllowRealLineBreaksInSnippets(settings, true);
        settings.sync();
        expect(settings.status() == QSettings::NoError, "multiline preference is written without error");
    }

    expect(QFileInfo(settingsPath).exists(), "portable multiline preference is stored under Data");
    {
        QSettings reloaded(settingsPath, QSettings::IniFormat);
        expect(restricted::readAllowRealLineBreaksInSnippets(reloaded), "multiline preference survives a settings reload");

        reloaded.setValue(QString::fromLatin1(restricted::kAllowRealLineBreaksInSnippetsSettingKey), "not-a-boolean");
        expect(!restricted::readAllowRealLineBreaksInSnippets(reloaded), "invalid stored multiline preference remains fail-closed");
    }
}


void testCompletionSoundPreferencePersistence() {
    QTemporaryDir temporaryDirectory;
    expect(temporaryDirectory.isValid(), "temporary completion-sound preference directory is available");
    if (!temporaryDirectory.isValid())
        return;

    QStringList const settingsPaths = {
        QDir(temporaryDirectory.path()).absoluteFilePath("Documents/Lean Beeftext/Settings.ini"),
        QDir(temporaryDirectory.path()).absoluteFilePath("Portable/Data/Settings.ini"),
    };
    for (QString const &settingsPath: settingsPaths) {
        expect(QDir().mkpath(QFileInfo(settingsPath).absolutePath()),
            QString("completion-sound preference directory is created: %1").arg(settingsPath));
        {
            QSettings settings(settingsPath, QSettings::IniFormat);
            expect(!restricted::readPlaySoundOnCombo(settings),
                QString("missing completion-sound preference defaults off: %1").arg(settingsPath));
            restricted::writePlaySoundOnCombo(settings, true);
            settings.sync();
            expect(settings.status() == QSettings::NoError,
                QString("explicit enabled completion-sound preference is saved: %1").arg(settingsPath));
        }
        {
            QSettings settings(settingsPath, QSettings::IniFormat);
            expect(restricted::readPlaySoundOnCombo(settings),
                QString("explicit enabled completion-sound preference survives reload: %1").arg(settingsPath));
            restricted::writePlaySoundOnCombo(settings, false);
            settings.sync();
        }
        {
            QSettings settings(settingsPath, QSettings::IniFormat);
            expect(!restricted::readPlaySoundOnCombo(settings),
                QString("explicit disabled completion-sound preference survives reload: %1").arg(settingsPath));
            settings.setValue(QString::fromLatin1(restricted::kPlaySoundOnComboSettingKey), "garbage");
            settings.sync();
        }
        {
            QSettings settings(settingsPath, QSettings::IniFormat);
            expect(!restricted::readPlaySoundOnCombo(settings),
                QString("unparseable completion-sound preference defaults off: %1").arg(settingsPath));
        }
    }
}


//****************************************************************************************************************************************************
// Combo import/export
//****************************************************************************************************************************************************
QJsonObject testGroup(QString const &uuid, QString const &name) {
    return QJsonObject {
        { "uuid", uuid },
        { "name", name },
        { "description", QString("Test group") },
        { "creationDateTime", QString("2026-09-05T12:00:00.000") },
        { "modificationDateTime", QString("2026-09-05T12:00:00.000") },
        { "enabled", true },
    };
}


QJsonObject testCombo(QString const &uuid, QString const &keyword, QString const &groupUuid) {
    return QJsonObject {
        { "uuid", uuid },
        { "name", keyword },
        { "keyword", keyword },
        { "snippet", QString("Snippet for %1").arg(keyword) },
        { "description", QString("Test combo") },
        { "matchingMode", 0 },
        { "caseSensitivity", 0 },
        { "group", groupUuid },
        { "creationDateTime", QString("2026-09-05T12:00:00.000") },
        { "modificationDateTime", QString("2026-09-05T12:00:00.000") },
        { "enabled", true },
    };
}


bool writeTestFile(QString const &path, QByteArray const &data) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    return file.write(data) == data.size();
}


void testComboExportBundle() {
    QString const groupAUuid = QUuid::createUuid().toString();
    QString const groupBUuid = QUuid::createUuid().toString();
    QJsonArray const groups = {
        testGroup(groupAUuid, "Group A"),
        testGroup(groupBUuid, "Group B"),
    };
    QJsonObject const comboOne = testCombo(QUuid::createUuid().toString(), "one", groupAUuid);
    QJsonObject const comboTwo = testCombo(QUuid::createUuid().toString(), "two", groupAUuid);
    QJsonObject const comboThree = testCombo(QUuid::createUuid().toString(), "three", groupBUuid);

    QJsonArray const oneSelected = { comboOne };
    QJsonDocument const oneBundle = combo_portability::createLeanBundle(oneSelected,
        combo_portability::referencedGroups(oneSelected, groups));
    expect(oneBundle.object().value("combos").toArray().size() == 1, "one selected combo is exported");
    expect(oneBundle.object().value("groups").toArray().size() == 1, "one selected combo carries its group");

    QJsonArray const multipleSelected = { comboOne, comboThree };
    QJsonDocument const multipleBundle = combo_portability::createLeanBundle(multipleSelected,
        combo_portability::referencedGroups(multipleSelected, groups));
    expect(multipleBundle.object().value("combos").toArray().size() == 2, "multiple selected combos are exported");
    expect(multipleBundle.object().value("groups").toArray().size() == 2, "multiple selected combos carry every referenced group");

    QJsonArray const allCombos = { comboOne, comboTwo, comboThree };
    QJsonDocument const allBundle = combo_portability::createLeanBundle(allCombos, groups);
    expect(allBundle.object().value("combos").toArray().size() == 3, "all combos are exported");
    expect(allBundle.object().value("groups").toArray() == groups,
        "an all-combos export preserves the complete group list, including empty groups");
    expectText(allBundle.object().value("format").toString(), "lean-beeftext-combos", "Lean combo bundle is self-identifying");
    expect(allBundle.object().value("version").toInt() == 1, "Lean combo bundle has schema version 1");
    QSet<QString> const keys = { "format", "version", "groups", "combos" };
    QSet<QString> actualKeys;
    for (QString const &key: allBundle.object().keys())
        actualKeys.insert(key);
    expect(actualKeys == keys, "Lean combo bundle contains no preferences or settings data");
}


void testComboPortabilityFiles() {
    QTemporaryDir directory;
    expect(directory.isValid(), "temporary combo-portability directory is available");
    if (!directory.isValid())
        return;

    QString const groupUuid = QUuid::createUuid().toString();
    QJsonArray const groups = { testGroup(groupUuid, "Clients") };
    QJsonArray const combos = { testCombo(QUuid::createUuid().toString(), "client", groupUuid) };
    QString const leanPath = QDir(directory.path()).absoluteFilePath("Lean-Beeftext-Combos.txt");
    QString error;
    expect(combo_portability::saveLeanBundle(leanPath, combos, groups, &error),
        QString("Lean .txt export succeeds: %1").arg(error));
    QFile leanFile(leanPath);
    expect(leanFile.open(QIODevice::ReadOnly), "Lean .txt export can be reopened");
    QJsonParseError parseError;
    QJsonDocument const exportedDocument = QJsonDocument::fromJson(leanFile.readAll(), &parseError);
    expect(parseError.error == QJsonParseError::NoError && exportedDocument.isObject(),
        "Lean .txt export contains valid UTF-8 JSON");

    QJsonDocument importedDocument;
    bool preserveGroups = false;
    error.clear();
    expect(combo_portability::loadJsonForImport(leanPath, 10, importedDocument, preserveGroups, &error),
        QString("Lean .txt import succeeds: %1").arg(error));
    expect(preserveGroups, "Lean .txt import requests group preservation");
    expect(importedDocument.object().value("combos").toArray() == combos, "Lean .txt round trip preserves combo records");
    expect(importedDocument.object().value("groups").toArray() == groups,
        "Lean .txt round trip preserves group records and relationships");

    QJsonObject legacyRoot;
    legacyRoot.insert("fileFormatVersion", 10);
    legacyRoot.insert("groups", QJsonArray());
    legacyRoot.insert("combos", combos);
    QString const legacyJsonPath = QDir(directory.path()).absoluteFilePath("upstream.json");
    expect(writeTestFile(legacyJsonPath, QJsonDocument(legacyRoot).toJson()), "legacy upstream JSON fixture is written");
    preserveGroups = true;
    expect(combo_portability::loadJsonForImport(legacyJsonPath, 10, importedDocument, preserveGroups, &error),
        "legacy upstream JSON remains accepted");
    expect(!preserveGroups, "legacy upstream JSON keeps destination-group import behavior");

    QString const legacyCsvPath = QDir(directory.path()).absoluteFilePath("upstream.csv");
    expect(writeTestFile(legacyCsvPath, "legacy,Legacy snippet,Legacy name\n"), "legacy upstream CSV fixture is written");
    QVector<QStringList> rows;
    expect(combo_portability::loadLegacyCsvRows(legacyCsvPath, rows, &error), "legacy upstream CSV remains accepted");
    expect(rows.size() == 1 && rows[0] == QStringList({ "legacy", "Legacy snippet", "Legacy name" }),
        "legacy upstream CSV fields retain their compatible meaning");

    QJsonDocument const sentinel(QJsonObject { { "sentinel", true } });
    importedDocument = sentinel;
    preserveGroups = true;
    QString const malformedPath = QDir(directory.path()).absoluteFilePath("malformed.txt");
    expect(writeTestFile(malformedPath, "{ definitely-not-json"), "malformed Lean fixture is written");
    expect(!combo_portability::loadJsonForImport(malformedPath, 10, importedDocument, preserveGroups, &error),
        "malformed Lean .txt fails clearly");
    expect(!error.isEmpty(), "malformed Lean .txt provides an error message");
    expect(importedDocument == sentinel && preserveGroups, "malformed Lean .txt does not partially change import output");

    QJsonObject danglingRoot;
    danglingRoot.insert("format", "lean-beeftext-combos");
    danglingRoot.insert("version", 1);
    danglingRoot.insert("groups", QJsonArray());
    danglingRoot.insert("combos", combos);
    QString const danglingPath = QDir(directory.path()).absoluteFilePath("dangling.txt");
    expect(writeTestFile(danglingPath, QJsonDocument(danglingRoot).toJson()), "dangling-group Lean fixture is written");
    expect(!combo_portability::loadJsonForImport(danglingPath, 10, importedDocument, preserveGroups, &error),
        "a Lean bundle whose combo refers to a missing group is rejected");

    QJsonObject extraRoot = danglingRoot;
    extraRoot.insert("groups", groups);
    extraRoot.insert("preferences", QJsonObject());
    QString const extraPath = QDir(directory.path()).absoluteFilePath("extra.txt");
    expect(writeTestFile(extraPath, QJsonDocument(extraRoot).toJson()), "extra-key Lean fixture is written");
    expect(!combo_portability::loadJsonForImport(extraPath, 10, importedDocument, preserveGroups, &error),
        "a Lean bundle with unexpected top-level data is rejected");

    QString const unsupportedPath = QDir(directory.path()).absoluteFilePath("old.btbackup");
    expect(writeTestFile(unsupportedPath, "ignored"), "unsupported fixture is written");
    error.clear();
    expect(!combo_portability::loadJsonForImport(unsupportedPath, 10, importedDocument, preserveGroups, &error),
        "unsupported files are rejected");
    expect(!error.isEmpty() && importedDocument == sentinel && preserveGroups,
        "unsupported files fail without partial import output");
}


//****************************************************************************************************************************************************
// Migration safety rules
//****************************************************************************************************************************************************
void testMigrationSafetyRules() {
    QTemporaryDir temporaryDirectory;
    expect(temporaryDirectory.isValid(), "migration safety test directory is available");
    if (!temporaryDirectory.isValid())
        return;
    QDir root(temporaryDirectory.path());
    QString const portable = root.absoluteFilePath("Beeftext Portable");
    QDir().mkpath(QDir(portable).absoluteFilePath("Data"));
    expect(writeTestFile(QDir(portable).absoluteFilePath("Beeftext.exe"), "fixture"), "portable fixture executable is created");
    expect(writeTestFile(QDir(portable).absoluteFilePath("Portable.bin"), ""), "portable fixture beacon is created");
    expect(writeTestFile(QDir(portable).absoluteFilePath("Data/comboList.json"), "{\"combos\":[]}"),
        "portable fixture combo library is created");
    QString comboPath;
    QString cleanupRoot;
    migration::EPortableProduct upstreamProduct = migration::EPortableProduct::LeanBeeftext;
    expect(migration::isStrongPortableCandidate(portable, &comboPath, &cleanupRoot, &upstreamProduct)
        && upstreamProduct == migration::EPortableProduct::UpstreamBeeftext
        && QFileInfo(comboPath).fileName() == "comboList.json"
        && QFileInfo(cleanupRoot).fileName() == "Beeftext Portable",
        "strong portable detection requires the expected beacon/data layout");
    QFile::remove(QDir(portable).absoluteFilePath("Portable.bin"));
    expect(!migration::isStrongPortableCandidate(portable),
        "an arbitrary Beeftext.exe plus data is rejected without a portable beacon");

    QString const leanPortable = root.absoluteFilePath("release-47");
    QDir().mkpath(QDir(leanPortable).absoluteFilePath("Data"));
    expect(writeTestFile(QDir(leanPortable).absoluteFilePath("LeanBeeftext.exe"), "lean fixture"), "Lean portable fixture executable is created");
    expect(writeTestFile(QDir(leanPortable).absoluteFilePath("Portable.bin"), ""), "Lean portable fixture beacon is created");
    expect(writeTestFile(QDir(leanPortable).absoluteFilePath("Data/comboList.json"),
        "{\"fileFormatVersion\":3,\"groups\":[],\"combos\":[]}"), "Lean portable fixture combo library is created");
    migration::EPortableProduct portableProduct = migration::EPortableProduct::UpstreamBeeftext;
    expect(migration::isStrongPortableCandidate(leanPortable, &comboPath, &cleanupRoot, &portableProduct)
        && portableProduct == migration::EPortableProduct::LeanBeeftext
        && QFileInfo(cleanupRoot).fileName() == "release-47",
        "LeanBeeftext.exe plus Portable.bin and readable combo data identifies Lean portable regardless of folder name");
    expect(writeTestFile(QDir(leanPortable).absoluteFilePath("Data/comboList.json"), "not JSON"),
        "malformed Lean portable combo fixture is written");
    expect(!migration::isStrongPortableCandidate(leanPortable),
        "LeanBeeftext.exe plus Portable.bin is rejected when combo data is malformed");
    QFile::remove(QDir(leanPortable).absoluteFilePath("Data/comboList.json"));
    expect(!migration::isStrongPortableCandidate(leanPortable),
        "LeanBeeftext.exe plus Portable.bin is rejected without readable combo data");

    QString const bothExecutables = root.absoluteFilePath("both");
    QDir().mkpath(QDir(bothExecutables).absoluteFilePath("Data"));
    writeTestFile(QDir(bothExecutables).absoluteFilePath("LeanBeeftext.exe"), "x");
    writeTestFile(QDir(bothExecutables).absoluteFilePath("Beeftext.exe"), "x");
    writeTestFile(QDir(bothExecutables).absoluteFilePath("Portable.bin"), "");
    writeTestFile(QDir(bothExecutables).absoluteFilePath("Data/comboList.json"), "{\"combos\":[]}");
    expect(!migration::isStrongPortableCandidate(bothExecutables),
        "a folder containing both executables is ambiguous and rejected");

    QString const loneLean = root.absoluteFilePath("lone-application");
    QDir().mkpath(loneLean);
    expect(writeTestFile(QDir(loneLean).absoluteFilePath("LeanBeeftext.exe"), "lean fixture"), "lone Lean executable fixture is created");
    expect(!migration::isStrongPortableCandidate(loneLean), "a lone LeanBeeftext.exe is not classified as portable");

    QString const portableAppsRoot = root.absoluteFilePath("portable-apps-fixture");
    QString const portableAppsExecutableFolder = QDir(portableAppsRoot).absoluteFilePath("App/LeanBeeftext");
    QDir().mkpath(portableAppsExecutableFolder);
    QDir().mkpath(QDir(portableAppsRoot).absoluteFilePath("Data/settings"));
    expect(writeTestFile(QDir(portableAppsExecutableFolder).absoluteFilePath("LeanBeeftext.exe"), "lean fixture"),
        "Lean PortableApps executable fixture is created");
    expect(writeTestFile(QDir(portableAppsExecutableFolder).absoluteFilePath("PortableApps.bin"), ""), "Lean PortableApps beacon is created");
    expect(writeTestFile(QDir(portableAppsRoot).absoluteFilePath("Data/settings/comboList.json"),
        "{\"fileFormatVersion\":3,\"groups\":[],\"combos\":[]}"), "Lean PortableApps combo library is created");
    portableProduct = migration::EPortableProduct::UpstreamBeeftext;
    expect(migration::isStrongPortableCandidate(portableAppsExecutableFolder, &comboPath, &cleanupRoot, &portableProduct)
        && portableProduct == migration::EPortableProduct::LeanBeeftext
        && QDir::cleanPath(cleanupRoot) == QDir::cleanPath(portableAppsRoot),
        "the existing PortableApps beacon and Data/settings layout recognizes portable Lean Beeftext");

    expect(migration::isBroadCleanupRoot(QString(), { root.absolutePath() })
        && migration::isBroadCleanupRoot(root.absolutePath(), { root.absolutePath() })
        && migration::isBroadCleanupRoot(QDir::rootPath(), {})
        && !migration::isBroadCleanupRoot(portable, { root.absolutePath() }),
        "broad protected roots are refused while a dedicated child folder is eligible");
    expect(migration::isRecognizableInstalledCandidate("Beeftext", "Xavier Michelon", portable,
        QDir(portable).absoluteFilePath("Beeftext.exe"))
        && !migration::isRecognizableInstalledCandidate("Lean Beeftext", "Jubal Slone", portable,
            QDir(portable).absoluteFilePath("Beeftext.exe"))
        && !migration::isRecognizableInstalledCandidate("Beeftext", "Xavier Michelon", leanPortable,
            QDir(portable).absoluteFilePath("Beeftext.exe"))
        && !migration::isRecognizableInstalledCandidate("Beeftext", "Someone Else", portable,
            QDir(portable).absoluteFilePath("Beeftext.exe")),
        "installed detection accepts exact upstream identity and rejects Lean, a foreign publisher, or an executable outside the recorded root");

    QByteArray const preCloseContents("digest A");
    QByteArray const postCloseContents("digest B");
    QByteArray const changedAgainContents("digest C");
    QByteArray const preCloseDigest = migration::sourceContentDigest(preCloseContents);
    QByteArray const refreshedDigest = migration::sourceContentDigest(postCloseContents);
    expect(preCloseDigest != refreshedDigest
        && migration::sourceContentMatchesDigest(postCloseContents, refreshedDigest)
        && !migration::sourceContentMatchesDigest(postCloseContents, preCloseDigest)
        && !migration::sourceContentMatchesDigest(changedAgainContents, refreshedDigest)
        && !migration::sourceContentMatchesDigest(postCloseContents, QByteArray()),
        "a post-close digest refresh accepts the exact imported bytes and refuses a later source change or an empty digest");

    QList<QList<qsizetype>> const groups = migration::groupSourcesByContent({ "same", "different", "same" });
    expect(groups.size() == 2 && groups[0] == QList<qsizetype>({ 0, 2 }) && groups[1] == QList<qsizetype>({ 1 }),
        "identical libraries group together while differing libraries remain separate choices");
    migration::ValidationResult validation;
    validation.snapshotCreated = validation.parsed = validation.persisted = validation.reloaded = true;
    expect(!migration::cleanupAllowed(validation), "cleanup is refused until correspondence validation succeeds");
    validation.corresponds = true;
    expect(migration::cleanupAllowed(validation), "cleanup is allowed only after the full validation sequence");
    validation.snapshotCreated = false;
    expect(!migration::cleanupAllowed(validation), "cleanup is refused without a recovery snapshot");
    expect(migration::shouldRunMigration(false, false, migration::EState::NeverChecked)
        && !migration::shouldRunMigration(true, false, migration::EState::NeverChecked)
        && !migration::shouldRunMigration(false, true, migration::EState::NeverChecked)
        && !migration::shouldRunMigration(false, false, migration::EState::ImportCompleted)
        && !migration::shouldRunMigration(false, false, migration::EState::Complete),
        "migration is installed-only, first-run, non-overwriting, and idempotent");
}


} // anonymous namespace


int main(int argc, char *argv[]) {
    QCoreApplication application(argc, argv);
    testVariableAllowlist();
    testDateTimeEvaluation();
    testSanitizer();
    testMultilineSanitizer();
    testBlockedControlsInBothModes();
    testCursorPlan();
    testMultilinePreferencePersistence();
    testCompletionSoundPreferencePersistence();
    testComboExportBundle();
    testComboPortabilityFiles();
    testMigrationSafetyRules();
    if (failureCount == 0)
        qInfo() << "All Lean Beeftext restricted-model tests passed.";
    else
        qCritical() << failureCount << "Lean Beeftext restricted-model test(s) failed.";
    return failureCount == 0 ? 0 : 1;
}
