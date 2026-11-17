#include "stdafx.h"
#include "Combo/ComboVariable.h"

int main() {
	QLocale const locale(QLocale::English, QLocale::UnitedStates);
	QDateTime const now(QDate(2026, 9, 1), QTime(23, 30, 15), QTimeZone::utc());
	QList<QPair<QString, QString>> const examples = {
		{"dateTime:yyyy-MM-dd ddd dddd", "2026-09-01 Tue Tuesday"},
		{"dateTime:+1d:yyyy-MM-dd", "2026-09-02"},
		{"dateTime:+1w-2d:yyyy-MM-dd", "2026-09-06"},
		{"dateTime:+1M-30m:MM-dd HH:mm", "10-01 23:00"},
		{"dateTime:+1h:dd HH:mm", "02 00:30"},
		{"dateTime:+90s:HH:mm:ss", "23:31:45"},
		{"dateTime:+500z:ss.zzz", "15.500"},
		{"dateTime:+1y:yyyy", "2027"},
		{"dateTime:MMM d, yyyy 'at' h:mm AP", "Sep 1, 2026 at 11:30 PM"},
		{"dateTime:w ww", "36 36"},
		{R"(dateTime:yyyy\}MM)", "2026}09"},
		{"dateTime:", locale.toString(now)},
		{"dateTime:+1d:", locale.toString(now.addDays(1))}
	};
	bool passed = true;
	for (auto const &[input, expected] : examples) {
		QString const actual = evaluateDateTimeVariable(input, now, locale);
		if (actual != expected) {
			qCritical() << input << "expected" << expected << "got" << actual;
			passed = false;
		}
	}

	QMap<QString, QString> prompts;
	for (QString const input : {QString("dateTime:+1q:yyyy"), QString("date:yyyy"),
	                            QString("dateTime:+99999999999999999999d:yyyy")}) {
		bool cancelled = false;
		if (evaluateVariable(input, {}, prompts, cancelled) != "#{" + input + "}" || cancelled) {
			qCritical() << "Invalid format or offset should stay literal:" << input;
			passed = false;
		}
	}
	return passed ? 0 : 1;
}
