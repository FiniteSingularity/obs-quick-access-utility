#pragma once
#include <obs-module.h>
#include <obs-frontend-api.h>

#include <QDockWidget>
#include <QWidget>
#include <QListWidgetItem>
#include <QFrame>
#include <string>
#include <map>
#include <vector>
#include <mutex>
#include <thread>

//#include "quick-access.hpp"
#include "quick-access-source.hpp"
class QuickAccess;

typedef const char *(*translateFunc)(const char *);
class QuickAccessSource;

struct SourceVisibility {
	QuickAccessSource *source;
	QListWidgetItem *listItem;
	bool visible;
};

bool operator==(const SourceVisibility &lhs, const QuickAccessSource *rhs);

struct QuickAccessItemGroup {
	std::string name;
	SearchType searchType;
	QListWidgetItem *headerItem;
	bool headerVisible;
	std::vector<SourceVisibility> sources;
	std::vector<QuickAccessSource *> sources2;
};

class QuickAccessDock : public QFrame {
	Q_OBJECT

public:
	QuickAccessDock(QWidget *parent, obs_data_t *obsData,
			bool modal = false);
	~QuickAccessDock();
	void Load(obs_data_t *obsData, bool created = false);
	void Save(obs_data_t *obsData);
	void InitializeSearch();
	inline std::string GetName() { return _dockName; }
	inline std::string GetType() { return _dockType; }
	inline std::string GetId() { return _dockId; }
	inline bool ShowProperties() { return _showProperties; }
	inline bool ShowFilters() { return _showFilters; }
	inline bool ShowScenes() { return _showScenes; }
	inline bool ClickableScenes() { return _clickableScenes; }
	inline void SetProperties(bool on) { _showProperties = on; }
	inline void SetFilters(bool on) { _showFilters = on; }
	inline void SetScenes(bool on) { _showScenes = on; }
	inline void SetClickableScenes(bool on) { _clickableScenes = on; }
	void SetCurrentScene(QuickAccessSource *currentScene);
	void SetName(std::string name);
	inline QDockWidget *GetDockWidget() { return _dockWidget; }
	void SwitchingSceneCollections(bool state) { _switchingSC = state; }
	inline std::vector<QuickAccessSource *> Sources() { return _sources; }
	inline auto &DisplayGroups() { return _displayGroups; }
	void SetItemsButtonVisibility();

	void SourceCreated(QuickAccessSource *source);
	void SourceDestroyed();
	void SourceUpdate();
	void SourceRename(QuickAccessSource *source);
	void CleanupSourceHandlers();
	void RemoveSource(QuickAccessSource *source, bool removeDock = true);
	void AddSource(QuickAccessSource *source, int index = -1);
	void UpdateDynamicDock(bool updateWidget = true);
	void Search(std::string searchTerm);
	void SearchFocus();
	void DismissModal();

private:
	void _InitializeDockWidget();
	void _ClearSources();
	void _AddToDynDock(QuickAccessSource *source);

	QDockWidget *_dockWidget = nullptr;
	QuickAccess *_widget = nullptr;
	std::vector<QuickAccessSource *> _sources;
	std::vector<QuickAccessItemGroup> _displayGroups;
	std::mutex _m;
	std::string _dockName;
	std::string _dockType;
	std::string _dockId;
	QuickAccessSource *_currentScene;
	bool _showProperties = false;
	bool _showFilters = false;
	bool _showScenes = false;
	bool _clickableScenes = false;
	bool _dockInjected = false;
	bool _switchingSC = false;
	bool _modal = false;
	bool _ready = false;
	std::map<SearchType, size_t> _indexer;
};
