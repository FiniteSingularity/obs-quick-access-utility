#include "quick-access-dock.hpp"
#include "quick-access.hpp"
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QMainWindow>
#include <QVBoxLayout>

QuickAccessDock *dock = nullptr;

QuickAccessDock::QuickAccessDock(QWidget *parent) : QWidget(parent)
{
	_widget = new QuickAccess(this, "quick_access_widget");
	auto l = new QVBoxLayout;
	l->setContentsMargins(0, 0, 0, 0);
	l->addWidget(_widget);
	setLayout(l);
}

void QuickAccessDock::Load(obs_data_t *data)
{
	auto docks = obs_data_get_array(data, "quick_access_docks");
	if (!docks) {
		docks = obs_data_array_create();
		auto dockObj = obs_data_create();
		auto dockItems = obs_data_array_create();
		obs_data_set_string(dockObj, "dock_name", "default");
		obs_data_set_string(dockObj, "dock_type", "manual");
		obs_data_set_array(dockObj, "dock_sources", dockItems);
		obs_data_array_push_back(docks, dockObj);
		obs_data_release(dockObj);
		obs_data_array_release(dockItems);
	}
	auto dockObj = obs_data_array_item(docks, 0);
	_widget->Load(dockObj);
	obs_data_array_release(docks);
}

void QuickAccessDock::Save(obs_data_t *data)
{
	auto docks = obs_data_array_create();
	auto dockData = obs_data_create();
	_widget->Save(dockData);
	obs_data_array_push_back(docks, dockData);
	obs_data_release(dockData);
	obs_data_set_array(data, "quick_access_docks", docks);
	obs_data_array_release(docks);
}

QIcon QuickAccessDock::GetIconFromType(const char* type) const
{
	const auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());

	auto icon_type = obs_source_get_icon_type(type);

	switch (icon_type) {
	case OBS_ICON_TYPE_IMAGE:
		return main_window->property("imageIcon").value<QIcon>();
	case OBS_ICON_TYPE_COLOR:
		return main_window->property("colorIcon").value<QIcon>();
	case OBS_ICON_TYPE_SLIDESHOW:
		return main_window->property("slideshowIcon").value<QIcon>();
	case OBS_ICON_TYPE_AUDIO_INPUT:
		return main_window->property("audioInputIcon").value<QIcon>();
	case OBS_ICON_TYPE_AUDIO_OUTPUT:
		return main_window->property("audioOutputIcon").value<QIcon>();
	case OBS_ICON_TYPE_DESKTOP_CAPTURE:
		return main_window->property("desktopCapIcon").value<QIcon>();
	case OBS_ICON_TYPE_WINDOW_CAPTURE:
		return main_window->property("windowCapIcon").value<QIcon>();
	case OBS_ICON_TYPE_GAME_CAPTURE:
		return main_window->property("gameCapIcon").value<QIcon>();
	case OBS_ICON_TYPE_CAMERA:
		return main_window->property("cameraIcon").value<QIcon>();
	case OBS_ICON_TYPE_TEXT:
		return main_window->property("textIcon").value<QIcon>();
	case OBS_ICON_TYPE_MEDIA:
		return main_window->property("mediaIcon").value<QIcon>();
	case OBS_ICON_TYPE_BROWSER:
		return main_window->property("browserIcon").value<QIcon>();
	case OBS_ICON_TYPE_CUSTOM:
		//TODO: Add ability for sources to define custom icons
		return main_window->property("defaultIcon").value<QIcon>();
	case OBS_ICON_TYPE_PROCESS_AUDIO_OUTPUT:
		return main_window->property("audioProcessOutputIcon").value<QIcon>();
	default:
		return main_window->property("defaultIcon").value<QIcon>();
	}
}

QIcon QuickAccessDock::GetSceneIcon() const
{
	const auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	return main_window->property("sceneIcon").value<QIcon>();
}

QIcon QuickAccessDock::GetGroupIcon() const
{
	const auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	return main_window->property("groupIcon").value<QIcon>();
}

extern "C" EXPORT void InitializeQAD(obs_module_t *module,
					 translateFunc translate)
{
	const auto mainWindow = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	dock = new QuickAccessDock(mainWindow);
#if LIBOBS_API_VER >= MAKE_SEMANTIC_VERSION(30, 0, 0)
	obs_frontend_add_dock_by_id("quick-access-dock", "Quick Access Dock", dock);
#else
	const auto d = new QDockWidget(static_cast<QMainWindow *>(obs_frontend_get_main_window()));
	d->setObjectName("quick-access-dock");
	d->setWindowTitle(obs_module_text("QuickAccessDock.dockTitle"));
	d->setWidget(dock);
	d->setFeatures(DockWidgetClosable | DockWidgetMovable |
		       DockWidgetFloatable);
	d->setFloating(true);
	d->hide();
	obs_frontend_add_dock(d);
#endif
	obs_frontend_add_save_callback(frontendSaveLoad, dock);
}

void frontendSaveLoad(obs_data_t* save_data, bool saving, void* data) {
	auto quickAccessDock = static_cast<QuickAccessDock *>(data);
	if (saving) {
		quickAccessDock->Save(save_data);
	} else {
		quickAccessDock->Load(save_data);
	}
}
