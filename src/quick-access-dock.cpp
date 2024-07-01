#include "quick-access-dock.hpp"
#include "quick-access.hpp"
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QDockWidget>
#include "version.h"

QuickAccessDock::QuickAccessDock(QWidget *parent, obs_data_t *obsData)
	: QWidget(parent)
{
	const auto mainWindow =
		static_cast<QMainWindow *>(obs_frontend_get_main_window());

	_dockName = obs_data_get_string(obsData, "dock_name");
	_dockType = obs_data_get_string(obsData, "dock_type");
	_dockId = obs_data_get_string(obsData, "dock_id");
	_showProperties = obs_data_get_bool(obsData, "show_properties");
	_showFilters = obs_data_get_bool(obsData, "show_filters");
	_showScenes = obs_data_get_bool(obsData, "show_scenes");
	_clickableScenes = obs_data_get_bool(obsData, "clickable_scenes");

	_widget = new QuickAccess(this, this, "quick_access_widget");
	setMinimumWidth(200);
	auto l = new QVBoxLayout;
	l->setContentsMargins(0, 0, 0, 0);
	l->addWidget(_widget);
	setLayout(l);

	_widget->Load(obsData);
	if (!_dockWidget) {
		_InitializeDockWidget();
	}
	const auto d = static_cast<QDockWidget *>(parentWidget());
	if (obs_data_get_bool(obsData, "dock_hidden")) {
		d->hide();
	} else {
		d->show();
	}

	const auto floating = obs_data_get_bool(obsData, "dock_floating");
	if (d->isFloating() != floating) {
		d->setFloating(floating);
	}

	const auto area = static_cast<Qt::DockWidgetArea>(
		obs_data_get_int(obsData, "dock_area"));
	if (area != mainWindow->dockWidgetArea(d)) {
		mainWindow->addDockWidget(area, d);
	}

	const char *geometry = obs_data_get_string(obsData, "dock_geometry");
	if (geometry && strlen(geometry)) {
		d->restoreGeometry(
			QByteArray::fromBase64(QByteArray(geometry)));
	}
}

QuickAccessDock::~QuickAccessDock()
{
	if (_dockWidget) {
		delete _dockWidget;
	}
}

void QuickAccessDock::CleanupSourceHandlers()
{
	if (_widget) {
		_widget->CleanupSourceHandlers();
	}
}

void QuickAccessDock::SetItemsButtonVisibility()
{
	if (_widget) {
		_widget->SetItemsButtonVisibility();
	}
}

void QuickAccessDock::Load(obs_data_t *obsData, bool created)
{
	UNUSED_PARAMETER(created);
	const auto mainWindow =
		static_cast<QMainWindow *>(obs_frontend_get_main_window());

	_dockName = obs_data_get_string(obsData, "dock_name");
	_dockType = obs_data_get_string(obsData, "dock_type");
	_dockId = obs_data_get_string(obsData, "dock_id");
	_showProperties = obs_data_get_bool(obsData, "show_properties");
	_showFilters = obs_data_get_bool(obsData, "show_filters");
	_showScenes = obs_data_get_bool(obsData, "show_scenes");
	_clickableScenes = obs_data_get_bool(obsData, "clickable_scenes");

	_widget->Load(obsData);
	if (!_dockWidget) {
		_InitializeDockWidget();
	}
	const auto d = static_cast<QDockWidget *>(parentWidget());
	if (obs_data_get_bool(obsData, "dock_hidden")) {
		d->hide();
	} else {
		d->show();
	}

	const auto area = static_cast<Qt::DockWidgetArea>(
		obs_data_get_int(obsData, "dock_area"));
	if (area != mainWindow->dockWidgetArea(d)) {
		mainWindow->addDockWidget(area, d);
	}

	const auto floating = obs_data_get_bool(obsData, "dock_floating");
	if (d->isFloating() != floating) {
		d->setFloating(floating);
	}

	const char *geometry = obs_data_get_string(obsData, "dock_geometry");
	if (geometry && strlen(geometry)) {
		d->restoreGeometry(
			QByteArray::fromBase64(QByteArray(geometry)));
	}
}

void QuickAccessDock::_InitializeDockWidget()
{
#if LIBOBS_API_VER >= MAKE_SEMANTIC_VERSION(30, 0, 0)
	obs_frontend_add_dock_by_id(
		("quick-access-dock_" + this->_dockId).c_str(),
		this->_dockName.c_str(), this);
#else
	_dockWidget = new QDockWidget(
		static_cast<QMainWindow *>(obs_frontend_get_main_window()));
	_dockWidget->setObjectName(
		("quick-access-dock_" + this->_dockId).c_str());
	_dockWidget->setWindowTitle(this->_dockName.c_str());
	_dockWidget->setWidget(this);
	_dockWidget->setFeatures(QDockWidget::DockWidgetClosable |
				 QDockWidget::DockWidgetMovable |
				 QDockWidget::DockWidgetFloatable);
	_dockWidget->setFloating(true);
	_dockWidget->hide();
	obs_frontend_add_dock(_dockWidget);
#endif
	_dockInjected = true;
}

void QuickAccessDock::Save(obs_data_t *obsData)
{
	const auto mainWindow =
		static_cast<QMainWindow *>(obs_frontend_get_main_window());
	auto docks = obs_data_get_array(obsData, "docks");
	auto dockData = obs_data_create();
	obs_data_set_string(dockData, "dock_name", _dockName.c_str());
	obs_data_set_string(dockData, "dock_type", _dockType.c_str());
	obs_data_set_string(dockData, "dock_id", _dockId.c_str());
	obs_data_set_bool(dockData, "show_properties", _showProperties);
	obs_data_set_bool(dockData, "show_filters", _showFilters);
	obs_data_set_bool(dockData, "show_scenes", _showScenes);
	obs_data_set_bool(dockData, "clickable_scenes", _clickableScenes);
	obs_data_set_bool(dockData, "dock_hidden", parentWidget()->isHidden());
	obs_data_set_string(dockData, "dock_geometry",
			    saveGeometry().toBase64().constData());
	auto *p = dynamic_cast<QMainWindow *>(parent()->parent());
	if (!p || p == mainWindow) {
		obs_data_set_string(dockData, "window", "");
	} else {
		QString wt = p->windowTitle();
		auto t = wt.toUtf8();
		obs_data_set_string(dockData, "window", t.constData());
	}
	if (p) {
		obs_data_set_int(
			dockData, "dock_area",
			p->dockWidgetArea((QDockWidget *)parentWidget()));
	}
	obs_data_set_bool(dockData, "dock_floating",
			  ((QDockWidget *)parentWidget())->isFloating());

	_widget->Save(dockData);
	obs_data_array_push_back(docks, dockData);
	obs_data_release(dockData);
	obs_data_array_release(docks);
}

void QuickAccessDock::SourceCreated(obs_source_t *source)
{
	UNUSED_PARAMETER(source);
	if (_dockType == "Source Search") {
	}
}

void QuickAccessDock::SourceDestroyed()
{
	// Dynamic sources are handled from scene item add/delete events
	if (_dockType != "Dynamic" && !_switchingSC) {
		if (_widget) {
			_widget->RemoveNullSources();
		}
	}
}
