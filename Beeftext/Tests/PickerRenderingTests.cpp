#include "stdafx.h"
#include "Picker/PickerItemDelegate.h"
#include "Combo/Combo.h"
#include "BeeftextConstants.h"
#include "Theme.h"
#include "ui_PickerWindow.h"

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
	QString const prefix = "picker-evidence/" + mode + "-" + nativeStyle;
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
	return 0;
}
