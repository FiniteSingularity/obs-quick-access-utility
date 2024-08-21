#include "quick-access-utility.hpp"
#include "quick-access-dock.hpp"
#include "quick-access.hpp"

#include <util/platform.h>
#include <QMainWindow>
#include <QAction>
#include <QLineEdit>
#include <QApplication>
#include <QThread>
#include <QMetaObject>
#include <QPushButton>

#include "version.h"

#define QT_UTF8(str) QString::fromUtf8(str)
#define QT_TO_UTF8(str) str.toUtf8().constData()

QuickAccessUtility *qau = nullptr;
QuickAccessUtilityDialog *qauDialog = nullptr;
QuickAccessSearchModal *qauSearch = nullptr;

QuickAccessUtilityDialog *QuickAccessUtilityDialog::dialog = nullptr;
QuickAccessSearchModal *QuickAccessSearchModal::dialog = nullptr;

QuickAccessUtility::QuickAccessUtility(obs_module_t *m)
	: _module(m),
	  _firstRun(false),
	  _currentScene(nullptr)
{
	obs_frontend_add_event_callback(QuickAccessUtility::FrontendCallback,
					this);

	_quick_search_hotkey_id = obs_hotkey_register_frontend(
		"Quick Search", "Opens Quick Search",
		QuickAccessSearchModal::OpenQuickSearch, this);
}

QuickAccessUtility::~QuickAccessUtility()
{
	blog(LOG_INFO, "QuickAccessUtility::~QuickAccessUtility");
	//delete _currentScene;
	// Dont need to delete dock pointers, as they are managed by OBS.
	obs_frontend_remove_event_callback(QuickAccessUtility::FrontendCallback,
					   this);
	obs_hotkey_unregister(_quick_search_hotkey_id);
}

void QuickAccessUtility::SourceCreated(void *data, calldata_t *params)
{
	if (!qau->loaded) {
		return;
	}
	blog(LOG_INFO, "Source Created!");
	UNUSED_PARAMETER(data);
	obs_source_t *source =
		static_cast<obs_source_t *>(calldata_ptr(params, "source"));
	std::string uuid = obs_source_get_uuid(source);
	QuickAccessUtility::AddSource(nullptr, source);
	if (qau->_allSources.count(uuid) == 0) {
		return;
	}
	QuickAccessSource *qaSource = qau->_allSources[uuid].get();
	QMetaObject::invokeMethod(
		QCoreApplication::instance()->thread(), [qaSource]() {
			for (auto &dock : qau->_docks) {
				if (dock) {
					dock->SourceCreated(qaSource);
				}
			}
			QuickAccessSearchModal::dialog->SourceCreated(qaSource);
		});
}

void QuickAccessUtility::SourceDestroyed(void *data, calldata_t *params)
{
	UNUSED_PARAMETER(data);
	obs_source_t *source =
		static_cast<obs_source_t *>(calldata_ptr(params, "source"));
	std::string uuid = obs_source_get_uuid(source);
	blog(LOG_INFO, "Source Destroyed!");
	//QMetaObject::invokeMethod(QCoreApplication::instance()->thread(), [uuid]() {
	std::unique_lock lock(qau->_m);
	auto it = qau->_allSources.find(uuid);
	if (it != qau->_allSources.end()) {

		QMetaObject::invokeMethod(
			QCoreApplication::instance()->thread(), [it]() {
				auto qaSource = it->second.get();
				for (auto &dock : qau->_docks) {
					if (dock) {
						dock->SourceDestroyed(qaSource);
					}
				}
				QuickAccessSearchModal::dialog->SourceDestroyed(
					qaSource);
				it->second->markForRemoval();
				qau->_allSources.erase(it);
			});
	}
	lock.unlock();
	//});
}

void QuickAccessUtility::SourceUpdate(void *data, calldata_t *params)
{
	UNUSED_PARAMETER(data);
	UNUSED_PARAMETER(params);

	obs_source_t *source =
		static_cast<obs_source_t *>(calldata_ptr(params, "source"));
	//blog(LOG_INFO, "Updated Source: %s", obs_source_get_name(source));
	std::string uuid = obs_source_get_uuid(source);
	if (qau->_allSources.count(uuid) == 0) {
		return;
	}
	qau->_allSources[uuid]->update();
}

void QuickAccessUtility::SourceRename(void *data, calldata_t *params)
{
	UNUSED_PARAMETER(data);
	if (!qau->loaded) {
		return;
	}
	obs_source_t *source =
		static_cast<obs_source_t *>(calldata_ptr(params, "source"));
	std::string uuid = obs_source_get_uuid(source);
	if (qau->_allSources.count(uuid) == 0) {
		return;
	}
	QuickAccessSource *qaSource = qau->_allSources[uuid].get();
	qaSource->update();
	QMetaObject::invokeMethod(
		QCoreApplication::instance()->thread(), [qaSource]() {
			for (auto &dock : qau->_docks) {
				if (dock) {
					dock->SourceRename(qaSource);
				}
			}
		});
}

obs_module_t *QuickAccessUtility::GetModule()
{
	return _module;
}

void QuickAccessUtility::UnloadDocks()
{
	std::unique_lock lock(qau->_m);
	//delete _currentScene;
	qau->_currentScene = nullptr;
	qau->RemoveDocks();
	qau->_allSources.clear();
}

void QuickAccessUtility::RemoveDock(int idx, bool cleanup)
{
	auto dock = _docks.at(idx);
	if (cleanup) {
		dock->CleanupSourceHandlers();
	}
	obs_frontend_remove_dock(
		("quick-access-dock_" + dock->GetId()).c_str());
	_docks.erase(_docks.begin() + idx);
}

void QuickAccessUtility::Load(obs_data_t *data)
{
	std::unique_lock lock(_m);
	blog(LOG_INFO, "QAU::Load called.");
	RemoveDocks();
	_allSources.clear();
	// Add Scenes and sources to _allSources
	obs_enum_all_sources(QuickAccessUtility::AddSource, qau);
	obs_enum_scenes(QuickAccessUtility::AddSource, qau);

	// Link scenes/groups with their children
	obs_enum_scenes(QuickAccessUtility::LinkScenes, qau);

	const auto mainWindow =
		static_cast<QMainWindow *>(obs_frontend_get_main_window());
	auto qauData = obs_data_get_obj(data, "quick_access_utility");
	if (!qauData) {
		qauData = obs_data_create();
		obs_data_set_bool(qauData, "first_run", true);
		auto docks = obs_data_array_create();
		obs_data_set_array(qauData, "docks", docks);
		obs_data_array_release(docks);
	}
	_firstRun = obs_data_get_bool(qauData, "first_run");
	auto docks = obs_data_get_array(qauData, "docks");
	for (size_t i = 0; i < obs_data_array_count(docks); i++) {
		auto dockData = obs_data_array_item(docks, i);
		auto d = new QuickAccessDock(mainWindow, dockData);
		_docks.push_back(d);
		obs_data_release(dockData);
	}
	obs_data_array_release(docks);

	auto quickSearchHotkey =
		obs_data_get_array(qauData, "quick_search_hotkey");
	obs_hotkey_load(_quick_search_hotkey_id, quickSearchHotkey);
	obs_data_array_release(quickSearchHotkey);

	obs_data_release(qauData);

	// Set up the quick search dialog
	// This shouldn't need to be done, but for some reason
	// the first time we pop open the search, it is a tiny
	// window and then disappears.
	if (!QuickAccessSearchModal::dialog) {
		const auto main_window = static_cast<QMainWindow *>(
			obs_frontend_get_main_window());
		const QRect &hostRect = main_window->geometry();

		QuickAccessSearchModal::dialog =
			new QuickAccessSearchModal(static_cast<QMainWindow *>(
				obs_frontend_get_main_window()));
		auto searchDlg = QuickAccessSearchModal::dialog;
		searchDlg->open();
		searchDlg->move(hostRect.center() - searchDlg->rect().center());
		searchDlg->adjustSize();
	}

	loaded = true;
}

void QuickAccessUtility::Save(obs_data_t *data)
{
	blog(LOG_INFO, "QAU::Save called.");
	auto saveData = obs_data_create();
	auto dockArray = obs_data_array_create();
	obs_data_set_bool(saveData, "first_run", _firstRun);
	obs_data_set_array(saveData, "docks", dockArray);

	for (auto &dock : _docks) {
		dock->Save(saveData);
	}

	auto quickSearchHotkey = obs_hotkey_save(_quick_search_hotkey_id);
	obs_data_set_array(saveData, "quick_search_hotkey", quickSearchHotkey);

	obs_data_set_obj(data, "quick_access_utility", saveData);
	obs_data_array_release(dockArray);
	obs_data_array_release(quickSearchHotkey);
	obs_data_release(saveData);

	QMetaObject::invokeMethod(QCoreApplication::instance()->thread(),
				  [this]() {
					  for (auto &dock : _docks) {
						  if (dock) {
							  dock->SourceUpdate();
						  }
					  }
				  });
}

void QuickAccessUtility::RemoveDocks()
{
	for (auto &dock : _docks) {
		obs_frontend_remove_dock(
			("quick-access-dock_" + dock->GetId()).c_str());
	}
	_docks.clear();
}

QuickAccessSource *QuickAccessUtility::GetSource(std::string uuid)
{
	auto search = _allSources.find(uuid);
	return search != _allSources.end() ? search->second.get() : nullptr;
}

std::vector<QuickAccessSource *> QuickAccessUtility::GetAllSources()
{
	std::vector<QuickAccessSource *> sources;
	for (auto const &[key, val] : _allSources) {
		sources.push_back(val.get());
	}
	return sources;
}

void QuickAccessUtility::_SetCurrentSceneSources()
{
	_currentSceneSources.clear();
	if (_currentScene) {
		_AddChildren(_currentScene);
	}
}

void QuickAccessUtility::_AddChildren(QuickAccessSource *scene)
{
	for (auto child : scene->children()) {
		if (std::find(_currentSceneSources.begin(),
			      _currentSceneSources.end(),
			      child) == _currentSceneSources.end()) {
			_currentSceneSources.push_back(child);
			if (child->sourceType() != SourceClass::Source) {
				_AddChildren(child);
			}
		}
	}
}

void QuickAccessUtility::_SetupSignals()
{
	signal_handler_t *signalHandler = obs_get_signal_handler();
	signal_handler_connect_ref(signalHandler, "source_create",
				   QuickAccessUtility::SourceCreated, qau);
	signal_handler_connect_ref(signalHandler, "source_destroy",
				   QuickAccessUtility::SourceDestroyed, qau);
	signal_handler_connect_ref(signalHandler, "source_rename",
				   QuickAccessUtility::SourceRename, qau);
	signal_handler_connect_ref(signalHandler, "source_update",
				   QuickAccessUtility::SourceUpdate, qau);
	signal_handler_connect_ref(signalHandler, "source_filter_add",
				   QuickAccessUtility::SourceUpdate, qau);
	signal_handler_connect_ref(signalHandler, "source_filter_remove",
				   QuickAccessUtility::SourceUpdate, qau);
}

void QuickAccessUtility::_TearDownSignals()
{
	signal_handler_t *signalHandler = obs_get_signal_handler();
	signal_handler_disconnect(signalHandler, "source_create",
				  QuickAccessUtility::SourceCreated, qau);
	signal_handler_disconnect(signalHandler, "source_destroy",
				  QuickAccessUtility::SourceDestroyed, qau);
	signal_handler_disconnect(signalHandler, "source_rename",
				  QuickAccessUtility::SourceRename, qau);
	signal_handler_disconnect(signalHandler, "source_update",
				  QuickAccessUtility::SourceUpdate, qau);
	signal_handler_disconnect(signalHandler, "source_filter_add",
				  QuickAccessUtility::SourceUpdate, qau);
	signal_handler_disconnect(signalHandler, "source_filter_remove",
				  QuickAccessUtility::SourceUpdate, qau);
	if (_currentScene) {
		signal_handler_t *oldSignalHandler =
			_currentScene->getSignalHandler();
		signal_handler_disconnect(
			oldSignalHandler, "item_add",
			QuickAccessUtility::SourceAddedToScene, qau);
		signal_handler_disconnect(
			oldSignalHandler, "item_remove",
			QuickAccessUtility::SourceRemovedFromScene, qau);
		//delete _currentScene;
		//_currentScene = nullptr;
	}
}

void QuickAccessUtility::FrontendCallback(enum obs_frontend_event event,
					  void *data)
{
	UNUSED_PARAMETER(data);
	if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING) {
		blog(LOG_INFO, "============== QAU::Finished Loading");
		qau->_sceneCollectionChanging = false;
		qau->_SetupSignals();
		qau->SceneChanged();
	} else if (event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP) {
		blog(LOG_INFO, "============== QAU::Scene Collection Cleanup");
		QMetaObject::invokeMethod(
			QCoreApplication::instance()->thread(),
			[]() { qau->UnloadDocks(); });
		qau->loaded = false;
	} else if (event == OBS_FRONTEND_EVENT_EXIT) {
		blog(LOG_INFO, "============== QAU::Frontend Exit");
	} else if (event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGING) {
		blog(LOG_INFO, "============== QAU::Scene Collection Changing");
		qau->_TearDownSignals();
		qau->_sceneCollectionChanging = true;
	} else if (event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED) {
		blog(LOG_INFO, "============== QAU::SCENE COLLECTION CHANGED");
		qau->_sceneCollectionChanging = false;
		qau->_SetupSignals();
		qau->InitializeSearch();
		qau->SceneChanged();
	} else if (event == OBS_FRONTEND_EVENT_SCRIPTING_SHUTDOWN) {
		blog(LOG_INFO, "============== QAU::SCRIPTING SHUTDOWN");
		qau->_TearDownSignals();
		QMetaObject::invokeMethod(
			QCoreApplication::instance()->thread(), []() {
				std::unique_lock lock(qau->_m);
				qau->RemoveDocks();
				qau->_allSources.clear();
				if (QuickAccessUtilityDialog::dialog) {
					delete QuickAccessUtilityDialog::dialog;
				}
			});
	} else if (event == OBS_FRONTEND_EVENT_SCENE_CHANGED) {
		blog(LOG_INFO, "============== QAU::SCENE CHANGED");
		if (!qau->_sceneCollectionChanging) {
			qau->SceneChanged();
		}
	}
}

void QuickAccessUtility::InitializeSearch()
{
	QuickAccessSearchModal::dialog->InitializeSearch();
}

void QuickAccessUtility::SceneChanged()
{
	obs_source_t *newScene = obs_frontend_get_current_scene();

	std::string uuid = obs_source_get_uuid(newScene);
	QuickAccessSource *qaNewScene = _allSources[uuid].get();
	if (qaNewScene) {
		if (_currentScene) {
			signal_handler_t *oldSignalHandler =
				_currentScene->getSignalHandler();
			signal_handler_disconnect(
				oldSignalHandler, "item_add",
				QuickAccessUtility::SourceAddedToScene, qau);
			signal_handler_disconnect(
				oldSignalHandler, "item_remove",
				QuickAccessUtility::SourceRemovedFromScene,
				qau);
			//delete _currentScene;
			_currentScene = nullptr;
		}
		signal_handler_t *signalHandler =
			obs_source_get_signal_handler(newScene);
		_currentScene = qaNewScene;

		signal_handler_connect_ref(
			signalHandler, "item_add",
			QuickAccessUtility::SourceAddedToScene, qau);
		signal_handler_connect_ref(
			signalHandler, "item_remove",
			QuickAccessUtility::SourceRemovedFromScene, qau);
		_SetCurrentSceneSources();
		for (auto &dock : _docks) {
			dock->SetCurrentScene(qaNewScene);
		}
	}
	obs_source_release(newScene);
}

void QuickAccessUtility::SourceAddedToScene(void *data, calldata_t *params)
{
	blog(LOG_INFO, "SOURCE ADDED TO THE SCENE!");
	UNUSED_PARAMETER(data);
	obs_scene_t *parentScene =
		static_cast<obs_scene_t *>(calldata_ptr(params, "scene"));
	obs_source_t *parentSource = obs_scene_get_source(parentScene);
	obs_sceneitem_t *sceneItem =
		static_cast<obs_sceneitem_t *>(calldata_ptr(params, "item"));
	obs_source_t *source = obs_sceneitem_get_source(sceneItem);
	std::string sourceId = obs_source_get_uuid(source);
	std::string sceneId = obs_source_get_uuid(parentSource);

	QuickAccessSource *parent = qau->_allSources[sceneId].get();
	QuickAccessSource *child = qau->_allSources[sourceId].get();

	child->addParent(parent);
	parent->addChild(child);

	qau->_SetCurrentSceneSources();
	for (auto &dock : qau->_docks) {
		dock->SetCurrentScene(qau->_currentScene);
	}
}

void QuickAccessUtility::SourceRemovedFromScene(void *data, calldata_t *params)
{
	blog(LOG_INFO, "SOURCE REMOVED FROM THE SCENE!");
	UNUSED_PARAMETER(data);
	obs_scene_t *parentScene =
		static_cast<obs_scene_t *>(calldata_ptr(params, "scene"));
	obs_source_t *parentSource = obs_scene_get_source(parentScene);
	obs_sceneitem_t *sceneItem =
		static_cast<obs_sceneitem_t *>(calldata_ptr(params, "item"));
	obs_source_t *source = obs_sceneitem_get_source(sceneItem);
	std::string sourceId = obs_source_get_uuid(source);
	std::string sceneId = obs_source_get_uuid(parentSource);

	QuickAccessSource *parent = qau->_allSources[sceneId].get();
	QuickAccessSource *child = qau->_allSources[sourceId].get();

	child->removeParent(parent);
	parent->removeChild(child);

	qau->_SetCurrentSceneSources();
	for (auto &dock : qau->_docks) {
		dock->SetCurrentScene(qau->_currentScene);
	}
}

void QuickAccessUtility::CheckModule(void *data, obs_module_t *module)
{
	UNUSED_PARAMETER(data);

	const char *modName = obs_get_module_name(module);
	if (modName && strcmp(modName, "Source Clone") == 0) {
		qau->_sourceCloneInstalled = true;
	}
}

void QuickAccessUtility::CreateDock(CreateDockFormData data)
{
	// Set up data
	auto dockData = obs_data_create();
	obs_data_set_string(dockData, "dock_name", data.dockName.c_str());
	obs_data_set_string(dockData, "dock_type", data.dockType.c_str());

	char *dockId = os_generate_uuid();
	obs_data_set_string(dockData, "dock_id", dockId);
	bfree(dockId);

	obs_data_set_bool(dockData, "show_properties", data.showProperties);
	obs_data_set_bool(dockData, "show_filters", data.showFilters);
	obs_data_set_bool(dockData, "show_scenes", data.showScenes);
	obs_data_set_bool(dockData, "clickable_scenes", data.clickableScenes);
	obs_data_set_bool(dockData, "dock_hidden", false);
	obs_data_set_bool(dockData, "dock_floating", true);
	obs_data_set_string(dockData, "dock_geometry", "");
	auto sourcesArray = obs_data_array_create();
	obs_data_set_array(dockData, "dock_sources", sourcesArray);

	// Create new dock
	const auto mainWindow =
		static_cast<QMainWindow *>(obs_frontend_get_main_window());
	auto dock = new QuickAccessDock(mainWindow, dockData);
	_docks.push_back(dock);
	obs_data_array_release(sourcesArray);
	obs_data_release(dockData);

	if (mainWindowOpen) {
		QuickAccessUtilityDialog::dialog->LoadDockList();
	}
}

QIcon QuickAccessUtility::GetIconFromType(const char *type) const
{
	const auto main_window =
		static_cast<QMainWindow *>(obs_frontend_get_main_window());

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
		return main_window->property("audioProcessOutputIcon")
			.value<QIcon>();
	default:
		return main_window->property("defaultIcon").value<QIcon>();
	}
}

QIcon QuickAccessUtility::GetSceneIcon() const
{
	const auto main_window =
		static_cast<QMainWindow *>(obs_frontend_get_main_window());
	return main_window->property("sceneIcon").value<QIcon>();
}

QIcon QuickAccessUtility::GetGroupIcon() const
{
	const auto main_window =
		static_cast<QMainWindow *>(obs_frontend_get_main_window());
	return main_window->property("groupIcon").value<QIcon>();
}

bool QuickAccessUtility::AddSource(void *data, obs_source_t *source)
{
	UNUSED_PARAMETER(data);
	obs_source_type st = obs_source_get_type(source);
	if (st == OBS_SOURCE_TYPE_FILTER || st == OBS_SOURCE_TYPE_TRANSITION) {
		return true;
	}
	std::string uuid = obs_source_get_uuid(source);
	if (qau->_allSources.find(uuid) == qau->_allSources.end()) {
		qau->_allSources.emplace(
			uuid, std::make_unique<QuickAccessSource>(source));
	}
	return true;
}

bool QuickAccessUtility::LinkScenes(void *data, obs_source_t *source)
{
	obs_scene_t *scene = obs_group_or_scene_from_source(source);
	obs_scene_enum_items(scene, QuickAccessUtility::LinkSceneItem, data);
	return true;
}

bool QuickAccessUtility::LinkSceneItem(obs_scene_t *scene,
				       obs_sceneitem_t *sceneItem, void *data)
{
	QuickAccessUtility &self = *static_cast<QuickAccessUtility *>(data);
	obs_source_t *source = obs_sceneitem_get_source(sceneItem);
	std::string sourceId = obs_source_get_uuid(source);
	obs_source_t *sceneSrc = obs_scene_get_source(scene);
	std::string sceneId = obs_source_get_uuid(sceneSrc);
	QuickAccessSource *qasSource = self._allSources[sourceId].get();
	QuickAccessSource *qasScene = self._allSources[sceneId].get();
	self._allSources[sourceId].get()->addParent(qasScene);
	self._allSources[sceneId].get()->addChild(qasSource);
	return true;
}

QuickAccessUtilityDialog::QuickAccessUtilityDialog(QWidget *parent)
	: QDialog(parent)
{
	qau->mainWindowOpen = true;
	qau->dialog = this;
	setWindowTitle(QString("Quick Access Utility"));
	setMinimumWidth(800);
	setMinimumHeight(600);
	_layout = new QVBoxLayout(this);
	_layout->setSpacing(0);
	_layout->setContentsMargins(0, 0, 0, 0);

	auto title = new QLabel();
	title->setText("Edit Dock Options");
	title->setStyleSheet("QLabel{ font-size: 18pt; padding: 10px; }");
	_layout->addWidget(title);
	auto header = new QWidget(this);
	auto layout = new QHBoxLayout();
	auto headerLabel = new QLabel();
	layout->setSpacing(8);
	layout->setContentsMargins(11, 10, 16, 10);
	QSizePolicy expandHoriz(QSizePolicy::Expanding, QSizePolicy::Preferred);
	headerLabel->setSizePolicy(expandHoriz);
	headerLabel->setText("Dock");
	layout->addWidget(headerLabel);
	header->setLayout(layout);

	QIcon sceneIcon = qau->GetSceneIcon();
	QPixmap scenePixmap = sceneIcon.pixmap(QSize(18, 18));

	auto headerProperties = new QPushButton();
	headerProperties->setProperty("themeID", "propertiesIconSmall");
	layout->addWidget(headerProperties);
	headerProperties->setDisabled(true);
	headerProperties->setAccessibleDescription("Show Properties Button?");
	headerProperties->setAccessibleName("Show Properties Button");
	headerProperties->setToolTip("Show Properties Button?");
	headerProperties->setStyleSheet("padding: 0px; background: none");

	auto headerFilters = new QPushButton();
	headerFilters->setProperty("themeID", "filtersIcon");
	layout->addWidget(headerFilters);
	headerFilters->setDisabled(true);
	headerFilters->setAccessibleDescription("Show Filters Button?");
	headerFilters->setAccessibleName("Show Filters Button");
	headerFilters->setToolTip("Show Filters Button?");
	headerFilters->setStyleSheet("padding: 0px; background: none");

	auto headerScenes = new QLabel();
	headerScenes->setPixmap(scenePixmap);
	headerScenes->setAccessibleDescription("Show Parent Scenes Button?");
	headerScenes->setAccessibleName("Show Parent Scenes Button");
	headerScenes->setToolTip("Show Parent Scenes Button?");
	headerScenes->setStyleSheet("background: none");
	layout->addWidget(headerScenes);

	auto headerClickableScenes = new QPushButton();
	headerClickableScenes->setProperty("themeID", "playIcon");
	layout->addWidget(headerClickableScenes);
	headerClickableScenes->setDisabled(true);
	headerClickableScenes->setAccessibleDescription("Clickable Scenes?");
	headerClickableScenes->setAccessibleName("Clickable Scenes");
	headerClickableScenes->setToolTip("Clickable Scenes?");

	headerClickableScenes->setStyleSheet("padding: 0px; background: none");

	_layout->addWidget(header);

	_dockList = new QListWidget(this);
	_dockList->setObjectName(QStringLiteral("docks"));
	QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
	sizePolicy.setHorizontalStretch(0);
	sizePolicy.setVerticalStretch(0);
	sizePolicy.setHeightForWidth(
		_dockList->sizePolicy().hasHeightForWidth());
	_dockList->setSizePolicy(sizePolicy);
	_dockList->setContextMenuPolicy(Qt::CustomContextMenu);
	_dockList->setFrameShape(QFrame::NoFrame);
	_dockList->setFrameShadow(QFrame::Plain);
	_dockList->setDragEnabled(false);

	connect(_dockList, SIGNAL(itemSelectionChanged()), this,
		SLOT(on_dockList_itemSelectionChanged()));

	// Add docks to the list widget
	LoadDockList();

	_layout->addWidget(_dockList);

	_toolbar = new QToolBar(this);
	_toolbar->setObjectName(QStringLiteral("actionsToolbar"));
	_toolbar->setIconSize(QSize(16, 16));
	_toolbar->setFloatable(false);

	// Spacer to align buttons to right side of toolbar.
	QWidget *spacer = new QWidget(this);
	spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	spacer->setVisible(true);
	_toolbar->addWidget(spacer);

	_actionAddDock = new QAction(this);
	_actionAddDock->setObjectName(QStringLiteral("actionAddDock"));
	_actionAddDock->setProperty("themeID", "addIconSmall");
	_actionAddDock->setText(QT_UTF8(obs_module_text("New Dock")));
	connect(_actionAddDock, SIGNAL(triggered()), this,
		SLOT(on_actionAddDock_triggered()));
	_toolbar->addAction(_actionAddDock);

	_actionRemoveDock = new QAction(this);
	_actionRemoveDock->setObjectName(QStringLiteral("actionRemoveDock"));
	_actionRemoveDock->setShortcutContext(Qt::WidgetWithChildrenShortcut);
	_actionRemoveDock->setProperty("themeID", "removeIconSmall");
	_actionRemoveDock->setText(QT_UTF8(obs_module_text("Remove Dock")));
	_actionRemoveDock->setEnabled(false);
	connect(_actionRemoveDock, SIGNAL(triggered()), this,
		SLOT(on_actionRemoveDock_triggered()));
	_toolbar->addAction(_actionRemoveDock);

	_layout->addWidget(_toolbar);
	setLayout(_layout);
}

void QuickAccessUtilityDialog::on_dockList_itemSelectionChanged()
{
	bool activeItem = _dockList->currentItem() != nullptr;
	_actionRemoveDock->setEnabled(activeItem);
}

void QuickAccessUtilityDialog::LoadDockList()
{
	_dockList->clear();

	for (auto &dock : qau->GetDocks()) {
		auto item = new QListWidgetItem();
		_dockList->addItem(item);
		auto itemWidget = new DockListItem(dock, this);
		_dockList->setItemWidget(item, itemWidget);
	}
}

QuickAccessUtilityDialog::~QuickAccessUtilityDialog()
{
	if (qau) {
		qau->mainWindowOpen = false;
		qau->dialog = nullptr;
	}
	QuickAccessUtilityDialog::dialog = nullptr;
}

void QuickAccessUtilityDialog::on_actionAddDock_triggered()
{
	const auto main_window =
		static_cast<QMainWindow *>(obs_frontend_get_main_window());
	CreateDockDialog *dockDialog = new CreateDockDialog(main_window);
	dockDialog->show();
}

void QuickAccessUtilityDialog::on_actionRemoveDock_triggered()
{
	auto idx = _dockList->currentRow();
	qau->RemoveDock(idx, true);
	LoadDockList();
}

void OpenQAUDialog()
{
	const auto main_window =
		static_cast<QMainWindow *>(obs_frontend_get_main_window());
	const QRect &hostRect = main_window->geometry();
	if (qau->mainWindowOpen) {
		QuickAccessUtilityDialog::dialog->show();
		QuickAccessUtilityDialog::dialog->move(
			hostRect.center() -
			QuickAccessUtilityDialog::dialog->rect().center());
		QuickAccessUtilityDialog::dialog->raise();
		QuickAccessUtilityDialog::dialog->activateWindow();
	} else {
		QuickAccessUtilityDialog::dialog =
			new QuickAccessUtilityDialog(static_cast<QMainWindow *>(
				obs_frontend_get_main_window()));
		QuickAccessUtilityDialog::dialog->setAttribute(
			Qt::WA_DeleteOnClose);
		QuickAccessUtilityDialog::dialog->show();
		QuickAccessUtilityDialog::dialog->move(
			hostRect.center() -
			QuickAccessUtilityDialog::dialog->rect().center());
	}
}

void QuickAccessSearchModal::OpenQuickSearch(void *data, obs_hotkey_id id,
					     obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(data);
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);
	if (!pressed) {
		return;
	}
	const auto main_window =
		static_cast<QMainWindow *>(obs_frontend_get_main_window());
	const QRect &hostRect = main_window->geometry();
	QuickAccessSearchModal *dialog = QuickAccessSearchModal::dialog;
	if (dialog != nullptr) {
		dialog->open();
		dialog->move(hostRect.center() - dialog->rect().center());
		dialog->raise();
		dialog->activateWindow();
	} else if (dialog == nullptr) {
		QuickAccessSearchModal::dialog =
			new QuickAccessSearchModal(static_cast<QMainWindow *>(
				obs_frontend_get_main_window()));
		dialog = QuickAccessSearchModal::dialog;
		dialog->open();
		dialog->move(hostRect.center() - dialog->rect().center());
		dialog->adjustSize();
	}
	dialog->SearchFocus();
}

DockListItem::DockListItem(QuickAccessDock *dock, QWidget *parent)
	: QFrame(parent),
	  _dock(dock)
{
	_layout = new QHBoxLayout(this);
	_layout->setSpacing(4);
	_layout->setContentsMargins(0, 0, 0, 0);
	std::string labelText =
		_dock->GetName() + " [" + _dock->GetType() + "]";
	_label = new QLabel(this);
	_label->setText(labelText.c_str());
	_label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	_label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	_label->setAttribute(Qt::WA_TranslucentBackground);

	_clickableScenes = new QCheckBox();
	_clickableScenes->setSizePolicy(QSizePolicy::Maximum,
					QSizePolicy::Maximum);
	_clickableScenes->setChecked(_dock->ClickableScenes());
	_clickableScenes->setStyleSheet("background: none");
	_clickableScenes->setAccessibleName("Clickable Scenes");
	_clickableScenes->setAccessibleDescription("Clickable Scenes?");

	_properties = new QCheckBox();
	_properties->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
	_properties->setChecked(_dock->ShowProperties());
	_properties->setStyleSheet("background: none");
	_properties->setAccessibleName("Properties Button");
	_properties->setAccessibleDescription("Properties Button?");

	_filters = new QCheckBox();
	_filters->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
	_filters->setChecked(_dock->ShowFilters());
	_filters->setStyleSheet("background: none");
	_filters->setAccessibleName("Filters Button");
	_filters->setAccessibleDescription("Filters Button?");

	_scenes = new QCheckBox();
	_scenes->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
	_scenes->setChecked(_dock->ShowFilters());
	_scenes->setStyleSheet("background: none");
	_scenes->setAccessibleName("Scenes Button");
	_scenes->setAccessibleDescription("Scenes Button?");

	auto setClickableScenes = [this](bool val) {
		_dock->SetClickableScenes(val);
	};
	connect(_clickableScenes, &QAbstractButton::clicked,
		setClickableScenes);

	auto setPropertiesVisible = [this](bool val) {
		_dock->SetProperties(val);
		_dock->SetItemsButtonVisibility();
	};
	connect(_properties, &QAbstractButton::clicked, setPropertiesVisible);

	auto setFiltersVisible = [this](bool val) {
		_dock->SetFilters(val);
		_dock->SetItemsButtonVisibility();
	};
	connect(_filters, &QAbstractButton::clicked, setFiltersVisible);

	auto setScenesVisible = [this](bool val) {
		_dock->SetScenes(val);
		_dock->SetItemsButtonVisibility();
	};
	connect(_scenes, &QAbstractButton::clicked, setScenesVisible);

	_layout->addWidget(_label);
	_layout->addWidget(_properties);
	_layout->addWidget(_filters);
	_layout->addWidget(_scenes);
	_layout->addWidget(_clickableScenes);
}

CreateDockDialog::CreateDockDialog(QWidget *parent) : QDialog(parent)
{
	setWindowModality(Qt::WindowModal);
	setAttribute(Qt::WA_DeleteOnClose, true);
	setWindowTitle(QString("Add Quick Access Dock"));
	setMinimumWidth(400);
	setMinimumHeight(300);
	int labelWidth = 120;
	_layout = new QVBoxLayout();

	// Form layout
	_layout2 = new QVBoxLayout();

	auto layoutName = new QHBoxLayout();

	auto inputLabel = new QLabel(this);
	inputLabel->setText("Dock Name:");
	inputLabel->setFixedWidth(labelWidth);

	_inputName = new QLineEdit(this);
	_inputName->setPlaceholderText("Dock Name");
	_inputName->connect(_inputName, &QLineEdit::textChanged,
			    [this](const QString text) {
				    _buttonBox->button(QDialogButtonBox::Ok)
					    ->setEnabled(text.length() > 0);
			    });

	layoutName->addWidget(inputLabel);
	layoutName->addWidget(_inputName);

	_layout2->addItem(layoutName);

	auto layoutType = new QHBoxLayout();

	auto typeLabel = new QLabel(this);
	typeLabel->setText("Dock Type:");
	typeLabel->setFixedWidth(labelWidth);

	_inputType = new QComboBox(this);
	_inputType->addItem("Manual");
	_inputType->addItem("Dynamic");
	_inputType->addItem("Source Search");
	layoutType->addWidget(typeLabel);
	layoutType->addWidget(_inputType);
	_layout2->addItem(layoutType);

	auto optionsLabel = new QLabel(this);
	optionsLabel->setText("Dock Options:");
	_layout2->addWidget(optionsLabel);

	_showProperties = new QCheckBox(this);
	_showProperties->setText("Show Properties");
	_showProperties->setChecked(true);
	_layout2->addWidget(_showProperties);

	_showFilters = new QCheckBox(this);
	_showFilters->setText("Show Filters");
	_showFilters->setChecked(true);
	_layout2->addWidget(_showFilters);

	_showScenes = new QCheckBox(this);
	_showScenes->setText("Show Parent Scenes");
	_showScenes->setChecked(true);
	_layout2->addWidget(_showScenes);

	_clickThroughScenes = new QCheckBox(this);
	_clickThroughScenes->setText("Clickable Scenes");
	_clickThroughScenes->setChecked(false);
	_layout2->addWidget(_clickThroughScenes);

	_buttonBox = new QDialogButtonBox(this);
	_buttonBox->setStandardButtons(QDialogButtonBox::Cancel |
				       QDialogButtonBox::Ok);
	_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);

	connect(_buttonBox, SIGNAL(accepted()), this, SLOT(on_create_dock()));
	connect(_buttonBox, SIGNAL(rejected()), this, SLOT(on_cancel()));

	_layout->addItem(_layout2);

	// Spacer to push buttons to bottom of widget
	QWidget *spacer = new QWidget(this);
	spacer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
	spacer->setVisible(true);
	_layout->addWidget(spacer);

	_layout->addWidget(_buttonBox);

	setLayout(_layout);
}

void CreateDockDialog::on_create_dock()
{
	blog(LOG_INFO, "Create Dock");
	CreateDockFormData formData;
	formData.dockName = QT_TO_UTF8(_inputName->text());
	formData.dockType = QT_TO_UTF8(_inputType->currentText());
	formData.showProperties = _showProperties->isChecked();
	formData.showFilters = _showFilters->isChecked();
	formData.showScenes = _showScenes->isChecked();
	formData.clickableScenes = _clickThroughScenes->isChecked();
	qau->CreateDock(formData);
	done(DialogCode::Accepted);
}

void CreateDockDialog::on_cancel()
{
	blog(LOG_INFO, "Cancel");
	done(DialogCode::Rejected);
}

QuickAccessSearchModal::QuickAccessSearchModal(QWidget *parent)
{
	UNUSED_PARAMETER(parent);
	setMinimumSize(400, 600);
	resize(400, 600);
	// Set up data
	auto dockData = obs_data_create();
	obs_data_set_string(dockData, "dock_name", "Quick Search");
	obs_data_set_string(dockData, "dock_type", "Source Search");

	char *dockId = os_generate_uuid();
	obs_data_set_string(dockData, "dock_id", dockId);
	bfree(dockId);

	obs_data_set_bool(dockData, "show_properties", true);
	obs_data_set_bool(dockData, "show_filters", true);
	obs_data_set_bool(dockData, "show_scenes", true);
	obs_data_set_bool(dockData, "clickable_scenes", true);
	auto sourcesArray = obs_data_array_create();
	obs_data_set_array(dockData, "dock_sources", sourcesArray);

	_layout = new QVBoxLayout();
	// Create new dock
	_widget = new QuickAccessDock(this, dockData, true);
	obs_data_array_release(sourcesArray);
	obs_data_release(dockData);

	_layout->addWidget(_widget);
	setLayout(_layout);
	setWindowTitle("Quick Search");
	setWindowFlags(Qt::Popup);
	setStyleSheet("border-radius: 20px;");
	//setModal(true);
	qau->searchDialog = this;
	qau->quickSearchOpen = true;
}

QuickAccessSearchModal::~QuickAccessSearchModal()
{
	if (qau) {
		qau->quickSearchOpen = false;
		qau->searchDialog = nullptr;
	}
}

void QuickAccessSearchModal::InitializeSearch()
{
	_widget->InitializeSearch();
}

void QuickAccessSearchModal::SearchFocus()
{
	if (_widget) {
		_widget->SearchFocus();
	}
}

void QuickAccessSearchModal::SourceCreated(QuickAccessSource *source)
{
	if (_widget) {
		_widget->SourceCreated(source);
	}
}

void QuickAccessSearchModal::SourceDestroyed(QuickAccessSource *source)
{
	if (_widget) {
		_widget->SourceDestroyed(source);
	}
}

extern "C" EXPORT void InitializeQAU(obs_module_t *module,
				     translateFunc translate)
{
	UNUSED_PARAMETER(translate);
	qau = new QuickAccessUtility(module);
	QAction *action = (QAction *)obs_frontend_add_tools_menu_qaction(
		"Quick Access Utility");
	action->connect(action, &QAction::triggered, OpenQAUDialog);

	//QAction* action2 = (QAction*)obs_frontend_add_tools_menu_qaction(
	//	"Quick Search"
	//);
	//action2->connect(action2, &QAction::triggered, OpenQuickSearch);

	obs_frontend_add_save_callback(frontendSaveLoad, qau);
}

extern "C" EXPORT void ShutdownQAU()
{
	blog(LOG_INFO, "ShutdownQAU called.");
	obs_frontend_remove_save_callback(frontendSaveLoad, qau);
	delete qau;
}

extern "C" EXPORT void CheckModules()
{
	obs_enum_modules(&QuickAccessUtility::CheckModule, qau);
}

void frontendSaveLoad(obs_data_t *save_data, bool saving, void *data)
{
	auto quickAccessUtility = static_cast<QuickAccessUtility *>(data);
	if (saving) {
		quickAccessUtility->Save(save_data);
	} else {
		quickAccessUtility->Load(save_data);
	}
}
