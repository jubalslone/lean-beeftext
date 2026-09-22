#include "stdafx.h"
#include "Picker/PickerItemDelegate.h"
#include "Combo/Combo.h"
#include "BeeftextConstants.h"
#include "Theme.h"
#include "ui_PickerWindow.h"
#include <cmath>

namespace {

double luminance(QColor const &color) {
	auto linear = [](double v) { return v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4); };
	return 0.2126 * linear(color.redF()) + 0.7152 * linear(color.greenF()) + 0.0722 * linear(color.blueF());
}

// Inspect actual painted glyph pixels, not merely the nominal palette colors.
// The old Windows 11 light-mode style paints black text on black: zero pixels pass.
bool visibleText(QImage const &image, QRect const &area, QColor const &background, QString const &label) {
	double const base = luminance(background);
	int visiblePixels = 0;
	for (int y = area.top(); y <= area.bottom(); ++y)
		for (int x = area.left(); x <= area.right(); ++x) {
			double const pixel = luminance(image.pixelColor(x, y));
			double const ratio = (qMax(base, pixel) + 0.05) / (qMin(base, pixel) + 0.05);
			if (ratio >= 3.0)
				++visiblePixels;
		}
	QTextStream(stdout) << label << ": " << visiblePixels << " contrasting glyph pixels\n";
	return visiblePixels >= 10;
}

bool visibleRow(QImage const &image, QRect const &row, QString const &label) {
	// Fixture text occupies the left and right ends; the middle is blank background.
	QColor const background = image.pixelColor(row.center().x(), row.bottom() - 3);
	int const halfHeight = row.height() / 2;
	bool const name = visibleText(image, QRect(row.left() + 10, row.top() + 2, 170, halfHeight), background, label + "/name");
	bool const keyword = visibleText(image, QRect(row.left() + 10, row.top() + halfHeight + 2, 150, halfHeight - 4), background, label + "/keyword");
	bool const group = visibleText(image, QRect(row.right() - 100, row.top() + 2, 90, halfHeight), background, label + "/group");
	return name && keyword && group;
}

} // namespace

int main(int argc, char **argv) {
	QApplication app(argc, argv);
	QString const mode = app.arguments().value(1, "light");
	app.styleHints()->setColorScheme(mode == "dark" ? Qt::ColorScheme::Dark : Qt::ColorScheme::Light);
	QCoreApplication::processEvents();
	QString const requestedStyle = app.arguments().value(2);
	if (!requestedStyle.isEmpty()) {
		QStyle *style = QStyleFactory::create(requestedStyle);
		if (!style)
			return 1;
		app.setStyle(style);
	}
	QString const nativeStyle = app.style()->objectName();
	applySystemTheme();
	bool const knownBroken = app.arguments().value(3) == "known-broken";
	if (knownBroken)
		app.setStyleSheet(app.styleSheet() + "\n#listViewResults { background-color: palette(base); }\n");
	QWidget window;
	Ui::PickerWindow ui;
	ui.setupUi(&window);
	window.setWindowFlag(Qt::FramelessWindowHint);
	window.setAttribute(Qt::WA_TranslucentBackground);
	window.setAttribute(Qt::WA_AlwaysStackOnTop);
	QStandardItemModel model;
	for (int i = 0; i < 3; ++i) {
		auto combo = Combo::create(QString("Visible combo %1").arg(i), QString(";keyword%1").arg(i), "example");
		combo->setGroup(Group::create("Visible group"));
		auto item = new QStandardItem(combo->name());
		item->setData(QVariant::fromValue(combo), constants::PointerRole);
		item->setData(constants::Combo, constants::TypeRole);
		model.appendRow(item);
	}
	ui.listViewResults->setModel(&model);
	ui.listViewResults->setItemDelegate(new PickerItemDelegate(ui.listViewResults));
	ui.listViewResults->setCurrentIndex(model.index(0, 0));
	ui.editSearch->setText("Visible search");
	window.show();
	ui.editSearch->setFocus();
	QEventLoop loop;
	QTimer::singleShot(500, &loop, &QEventLoop::quit);
	loop.exec();
	QDir().mkpath("picker-evidence");
	QString const prefix = "picker-evidence/" + mode + "-" + nativeStyle + (knownBroken ? "-known-broken" : "");
	window.grab().save(prefix + "-window.png");
	if (window.screen())
		window.screen()->grabWindow(0).save(prefix + "-desktop.png");
	QTextStream out(stdout);
	out << "Theme=" << mode << " effective=" << int(app.styleHints()->colorScheme())
		<< " style=" << nativeStyle << "\n";
	for (QWidget *widget: {static_cast<QWidget *>(&window), static_cast<QWidget *>(ui.editSearch),
		static_cast<QWidget *>(ui.listViewResults), ui.listViewResults->viewport()}) {
		out << widget->metaObject()->className() << " " << widget->objectName() << "\n";
		for (auto group: {QPalette::Active, QPalette::Inactive, QPalette::Disabled}) {
			out << "group=" << int(group);
			for (auto role: {QPalette::Window, QPalette::Base, QPalette::Text, QPalette::Highlight,
				QPalette::HighlightedText, QPalette::PlaceholderText})
				out << " role" << int(role) << "=" << widget->palette().color(group, role).name(QColor::HexArgb);
			out << "\n";
		}
	}
	QImage const viewport = ui.listViewResults->viewport()->grab().toImage();
	bool const selected = visibleRow(viewport, ui.listViewResults->visualRect(model.index(0, 0)), "view/selected");
	bool const unselected = visibleRow(viewport, ui.listViewResults->visualRect(model.index(1, 0)), "view/unselected");
	QImage const search = ui.editSearch->grab().toImage();
	bool const searchVisible = visibleText(search, QRect(4, 4, 160, search.height() - 8),
		search.pixelColor(search.width() - 10, search.height() / 2), "search");
	if (knownBroken) {
		// Negative control: prove these pixel checks detect the released defect.
		bool const reproduced = nativeStyle == "windows11" && mode == "light" && selected && !unselected
			&& searchVisible && ui.listViewResults->palette().color(QPalette::Base) == QColor(Qt::black);
		out << "Known broken stylesheet reproduced: " << reproduced << "\n";
		return reproduced ? 0 : 1;
	}
	bool ok = selected && unselected && searchVisible;
	for (auto group: {QPalette::Active, QPalette::Inactive}) {
		for (bool isSelected: {false, true}) {
			QStyleOptionViewItem option;
			option.initFrom(ui.listViewResults);
			option.palette.setCurrentColorGroup(group);
			option.state = QStyle::State_Enabled;
			if (group == QPalette::Active)
				option.state |= QStyle::State_Active;
			if (isSelected)
				option.state |= QStyle::State_Selected;
			option.rect = QRect(0, 0, viewport.width(), ui.listViewResults->visualRect(model.index(1, 0)).height());
			QImage row(option.rect.size(), QImage::Format_ARGB32_Premultiplied);
			row.fill(window.palette().color(group, QPalette::Window));
			QPainter painter(&row);
			ui.listViewResults->itemDelegate()->paint(&painter, option, model.index(1, 0));
			painter.end();
			QString const label = QString("delegate/group%1/selected%2").arg(int(group)).arg(isSelected);
			ok = visibleRow(row, option.rect, label) && ok;
		}
	}
	return ok ? 0 : 1;
}
