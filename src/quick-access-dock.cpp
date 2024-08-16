#include "quick-access-dock.hpp"
#include "quick-access-utility.hpp"
#include "quick-access.hpp"
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QDockWidget>
#include <QApplication>
#include <QThread>
#include <QMetaObject>
#include <algorithm>
#include "version.h"

extern QuickAccessUtility *qau;

const std::vector<SearchType> SearchTypes{SearchType::Source, SearchType::Type,
					  SearchType::File, SearchType::Url,
					  SearchType::Filters};
const std::map<SearchType, std::string> SearchTypeNames{
	{SearchType::Source, "Source"},
	{SearchType::Type, "Source Type"},
	{SearchType::File, "File Path"},
	{SearchType::Url, "URL"},
	{SearchType::Filters, "Filters"}};

bool operator==(const SourceVisibility &lhs, const QuickAccessSource *rhs)
{
	return lhs.source == rhs;
}

QuickAccessDock::QuickAccessDock(QWidget *parent, obs_data_t *obsData,
				 bool modal)
	: QFrame(parent),
	  _modal(modal)
{
	_dockName = obs_data_get_string(obsData, "dock_name");
	_dockType = obs_data_get_string(obsData, "dock_type");
	_dockId = obs_data_get_string(obsData, "dock_id");
	_dockId = obs_data_get_string(obsData, "dock_id");
	_showProperties = obs_data_get_bool(obsData, "show_properties");
	_showFilters = obs_data_get_bool(obsData, "show_filters");
	_showScenes = obs_data_get_bool(obsData, "show_scenes");
	_clickableScenes = obs_data_get_bool(obsData, "clickable_scenes");

	InitializeSearch();

	if (_dockType == "Manual") {
		obs_data_array_t *items =
			obs_data_get_array(obsData, "dock_sources");
		auto numItems = obs_data_array_count(items);
		for (size_t i = 0; i < numItems; i++) {
			auto item = obs_data_array_item(items, i);
			auto sourceName =
				obs_data_get_string(item, "source_name");
			auto source = obs_get_source_by_name(sourceName);
			if (!source) {
				continue;
			}
			std::string id = obs_source_get_uuid(source);
			_sources.push_back(qau->GetSource(id));
			obs_source_release(source);
			obs_data_release(item);
		}
		std::vector<SourceVisibility> sourceStates;
		for (auto &source : _sources) {
			sourceStates.push_back({source, nullptr, true});
		}
		_displayGroups.push_back({"Manual", SearchType::None, nullptr,
					  false, sourceStates, _sources});
		obs_data_array_release(items);
		for (auto &source : _sources) {
			source->addDock(this);
		}
	} else if (_dockType == "Dynamic") {
		_currentScene = qau->GetCurrentScene();
		UpdateDynamicDock(false);
	}

	if (!_modal) {
		DrawDock(obsData);
	}

	_widget = new QuickAccess(this, this, "quick_access_widget");

	setMinimumWidth(200);
	auto l = new QVBoxLayout;
	l->setContentsMargins(0, 0, 0, 0);
	l->addWidget(_widget);
	setLayout(l);

	_ready = true;
}

QuickAccessDock::~QuickAccessDock()
{
	blog(LOG_INFO, "QuickAccessDock::~QuickAccessDock()");
	_ClearSources();
	if (_dockWidget) {
		delete _dockWidget;
	}
}

void QuickAccessDock::InitializeSearch()
{
	if (_dockType != "Source Search") {
		return;
	}
	_sources = qau->GetAllSources();
	std::sort(_sources.begin(), _sources.end(),
		  [](QuickAccessSource *a, QuickAccessSource *b) {
			  return a->getName() < b->getName();
		  });
	std::vector<SourceVisibility> sources;
	for (auto &source : _sources) {
		source->addDock(this);
		sources.push_back({source, nullptr, false});
	}
	_displayGroups.clear();
	_indexer.clear();
	size_t i = 0;
	for (auto &st : SearchTypes) {
		_displayGroups.push_back({SearchTypeNames.at(st), st, nullptr,
					  false, sources, _sources});
		_indexer[st] = i;
		i++;
	}
}

void QuickAccessDock::SetCurrentScene(QuickAccessSource *currentScene)
{
	_currentScene = currentScene;
	if (!_ready) {
		return;
	}
	blog(LOG_INFO, "QuickAccessDock::SetCurrentScene");
	if (_dockType == "Dynamic") {
		UpdateDynamicDock(true);
	}
	// Set up dynamic dock here.
}

void QuickAccessDock::SearchFocus()
{
	if (_widget) {
		_widget->SearchFocus();
	}
}

void QuickAccessDock::SetName(std::string name)
{
	_dockName = name;
	auto d = parentWidget();
	if (d) {
		d->setWindowTitle(name.c_str());
	}
}

void QuickAccessDock::CleanupSourceHandlers() {}

void QuickAccessDock::SetItemsButtonVisibility()
{
	if (_widget) {
		_widget->SetItemsButtonVisibility();
	}
}

void QuickAccessDock::DismissModal()
{
	if (_modal) {
		auto modal =
			dynamic_cast<QuickAccessSearchModal *>(parentWidget());
		modal->close();
	}
}

void QuickAccessDock::_ClearSources()
{
	for (auto &source : _sources) {
		source->removeDock(this);
	}
	_sources.clear();
}

void QuickAccessDock::Load(obs_data_t *obsData, bool created)
{
	UNUSED_PARAMETER(created);
	DrawDock(obsData);
}

void QuickAccessDock::Search(std::string searchTerm)
{
	for (auto &source : _sources) {
		source->BuildSearchTerms();
	}
	for (auto &dg : _displayGroups) {
		dg.headerVisible = false;
		for (auto &source : dg.sources) {
			source.visible = false;
		}
	}
	size_t i = 0;
	for (auto &source : _sources) {
		for (auto &st : source->search(searchTerm)) {
			_displayGroups[_indexer[st]].sources[i].visible = true;
			_displayGroups[_indexer[st]].headerVisible = true;
		}
		i++;
	}
	if (_widget) {
		QMetaObject::invokeMethod(
			QCoreApplication::instance()->thread(),
			[this]() { _widget->UpdateVisibility(); });
	}
}

void QuickAccessDock::DrawDock(obs_data_t *obsData)
{
	const auto mainWindow =
		static_cast<QMainWindow *>(obs_frontend_get_main_window());

	obs_frontend_add_dock_by_id(
		("quick-access-dock_" + this->_dockId).c_str(),
		this->_dockName.c_str(), this);
	_dockInjected = true;

	const auto d = static_cast<QDockWidget *>(parentWidget());

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

	if (obs_data_get_bool(obsData, "dock_hidden")) {
		d->hide();
	} else {
		d->show();
	}
}

void QuickAccessDock::Save(obs_data_t *obsData)
{
	std::unique_lock lock(_m);
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

	if (_dockType == "Manual" && _displayGroups.size() != 0) {
		auto itemsArr = obs_data_array_create();
		for (auto &source : _displayGroups[0].sources2) {
			auto itemObj = obs_data_create();
			source->save(itemObj);
			obs_data_array_push_back(itemsArr, itemObj);
			obs_data_release(itemObj);
		}
		obs_data_set_array(dockData, "dock_sources", itemsArr);
		obs_data_array_release(itemsArr);
	}

	obs_data_array_push_back(docks, dockData);
	obs_data_release(dockData);
	obs_data_array_release(docks);
}

void QuickAccessDock::SourceCreated(QuickAccessSource *source)
{
	if (_dockType == "Source Search") {
		AddSource(source);
	}
}

void QuickAccessDock::SourceDestroyed(QuickAccessSource *source)
{
	if (_dockType == "Search Source" || _dockType == "Manual") {
		RemoveSource(source);
	}
	// TODO: Handle Dynamic Dock.
}

void QuickAccessDock::SourceUpdate() {}

void QuickAccessDock::SourceRename(QuickAccessSource *source)
{
	UNUSED_PARAMETER(source);
	// Dynamic sources are handled from scene item add/delete events
	if (_switchingSC || !_widget) {
		return;
	}
	_widget->Redraw();
}

void QuickAccessDock::RemoveSource(QuickAccessSource *source, bool removeDock)
{
	std::unique_lock lock(_m);
	for (auto &group : _displayGroups) {
		auto it = std::find(group.sources.begin(), group.sources.end(),
				    source);
		if (it != group.sources.end()) {
			_widget->RemoveSource(source, group.name);
			group.sources.erase(it);
		}
	}
	if (auto it = std::find(_sources.begin(), _sources.end(), source);
	    it != _sources.end()) {
		_sources.erase(it);
	}
	if (removeDock) {
		source->removeDock(this);
	}
}

void QuickAccessDock::AddSource(QuickAccessSource *source, int index)
{
	if (_dockType == "Manual") {
		if (std::find(_sources.begin(), _sources.end(), source) !=
		    _sources.end()) {
			return;
		}
		if (index == -1) {
			_sources.push_back(source);
			_displayGroups[0].sources.push_back(
				{source, nullptr, true});
		} else {
			_sources.insert(_sources.begin() + index, source);
			_displayGroups[0].sources.insert(
				_displayGroups[0].sources.begin() + index,
				{source, nullptr, true});
		}
		_widget->AddSource(source, _displayGroups[0].name);
		source->addDock(this);
	} else if (_dockType == "Source Search") {
		auto sIt = _sources.begin();
		while (sIt != _sources.end()) {
			if ((*sIt)->getName() > source->getName()) {
				break;
			}
			sIt++;
		}

		// Add the source to the dock's raw source list.
		_sources.insert(sIt, source);

		// Then add to each of the search display groups.
		for (auto &dg : _displayGroups) {
			auto it = dg.sources.begin();

			// Find location to insert
			while (it != dg.sources.end()) {
				if (it->source->getName() > source->getName()) {
					break;
				}
				it++;
			}
			//int index = it - dg.sources.begin();
			dg.sources.insert(it, {source, nullptr, false});
			_widget->AddSource(source, dg.name);
		}
		source->addDock(this);
	}
}

void QuickAccessDock::UpdateDynamicDock(bool updateWidget)
{
	_ClearSources();
	if (_currentScene) {
		// _currentScene is not set up with links to children.
		// so grab the fully populated version from qau->GetSource
		/*QuickAccessSource *cur =
			qau->GetSource(_currentScene->getUUID());*/
		//_AddToDynDock(_currentScene);
		_sources = qau->GetCurrentSceneSources();
		for (auto &source : _sources) {
			source->addDock(this);
		}
	}
	_displayGroups.clear();
	//_displayGroups.push_back({ "DSK" });
	std::vector<SourceVisibility> sourceStates;
	for (auto &source : _sources) {
		sourceStates.push_back({source, nullptr, true});
	}
	_displayGroups.push_back({"Scene", SearchType::None, nullptr, true,
				  sourceStates, _sources});
	if (updateWidget) {
		QMetaObject::invokeMethod(
			QCoreApplication::instance()->thread(),
			[this]() { _widget->Load(); });
	}
}

void QuickAccessDock::_AddToDynDock(QuickAccessSource *scene)
{
	for (auto &source : scene->children()) {
		if (std::find(_sources.begin(), _sources.end(), source) ==
		    _sources.end()) {
			_sources.push_back(source);
			source->addDock(this);
			if (source->sourceType() != SourceClass::Source) {
				_AddToDynDock(source);
			}
		}
	}
}
