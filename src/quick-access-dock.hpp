#pragma once
#include <obs-module.h>
#include <obs-frontend-api.h>

#include <QDockWidget>
#include <QWidget>
#include <string>
#include "quick-access.hpp"

typedef const char *(*translateFunc)(const char *);

class QuickAccessDock : public QWidget {
	Q_OBJECT

public:
	QuickAccessDock(QWidget* parent, obs_data_t* obsData);
	~QuickAccessDock();
	void Load(obs_data_t *obsData, bool created = false);
	void Save(obs_data_t *obsData);
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
	inline QDockWidget* GetDockWidget() { return _dockWidget; }
	void SwitchingSceneCollections(bool state) { _switchingSC = state; }
	void SetItemsButtonVisibility();

	void SourceCreated(obs_source_t* source);
	void SourceDestroyed();
private:
	void _InitializeDockWidget();

	QDockWidget* _dockWidget = nullptr;
	QuickAccess* _widget = nullptr;
	std::string _dockName;
	std::string _dockType;
	std::string _dockId;
	bool _showProperties = false;
	bool _showFilters = false;
	bool _showScenes = false;
	bool _clickableScenes = false;
	bool _dockInjected = false;
	bool _switchingSC = false;
};
