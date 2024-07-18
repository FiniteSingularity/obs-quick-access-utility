#pragma once
#include <obs-module.h>
#include <obs-frontend-api.h>

#include <QString>
#include <QWidget>
#include <QLabel>
#include <QCheckBox>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QToolBar>
#include <QHBoxLayout>
#include <QDialog>
#include <QDialogButtonBox>
#include <QComboBox>

#include <vector>
#include <set>

class QuickAccess;
class QuickAccessDock;

class QuickAccessSceneItem : public QWidget {
	Q_OBJECT

public:
	QuickAccessSceneItem(QWidget *parent, obs_sceneitem_t *sceneItem);
	~QuickAccessSceneItem();
	void setHighlight(bool h);
	//void mouseReleaseEvent(QMouseEvent *e) override;

private:
	obs_sceneitem_t *_sceneItem = nullptr;
	QLabel *_iconLabel = nullptr;
	QHBoxLayout *_layout = nullptr;
	QLabel *_label = nullptr;
	QToolBar *_actionsToolbar;
	QCheckBox *_vis = nullptr;

private slots:
	void on_actionTransform_triggered();
};

class QuickAccessList : public QListWidget {
	Q_OBJECT

public:
	QuickAccessList(QWidget *parent, QuickAccessDock *dock);
	void dropEvent(QDropEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;

private:
	QuickAccess *_qa = nullptr;
	QuickAccessDock *_dock;
};

class QuickAccessItem : public QFrame {
	Q_OBJECT

public:
	QuickAccessItem(QWidget *parent, QuickAccessDock *dock,
			obs_source_t *source);
	QuickAccessItem(QWidget *parent, QuickAccessItem *original);
	~QuickAccessItem();
	void Save(obs_data_t *saveObj);
	const char *GetSourceName();
	void RenameSource(std::string name);
	static bool GetSceneItemsFromScene(void *data, obs_source_t *s);
	static bool AddSceneItems(obs_scene_t *s, obs_sceneitem_t *si,
				  void *data);
	void SetButtonVisibility();
	bool IsSource(obs_source_t *s);
	bool IsNullSource();
	bool IsInteractive();
	void SwitchToScene();
	void UpdateLabel();

	// Actions
	void AddToScene(obs_source_t *scene);
	void OpenFilters();
	void OpenProperties();
	void OpenInteract();

	bool Configurable();

	inline obs_source_t *GetSource()
	{
		return obs_weak_source_get_source(_source);
	}

private:
	QuickAccessDock *_dock;
	QLabel *_label = nullptr;
	QLabel *_iconLabel = nullptr;

	QPushButton *_actionProperties = nullptr;
	QPushButton *_actionFilters = nullptr;
	QPushButton *_actionScenes = nullptr;

	QPushButton *_filters = nullptr;
	obs_weak_source_t *_source = nullptr;
	std::vector<obs_sceneitem_t *> _sceneItems;
	bool _configurable;
	void _getSceneItems();
	void _clearSceneItems();
	QMenu *_CreateSceneMenu();
	void _AddScenePopupMenu(const QPoint &pos);

private slots:
	void on_actionProperties_triggered();
	void on_actionFilters_triggered();
	void on_actionScenes_triggered();
};

class QuickAccess : public QWidget {
	Q_OBJECT

public:
	QuickAccess(QWidget *parent, QuickAccessDock *dock, QString name);
	~QuickAccess();
	void AddSource(const char *sourceName, bool hidden = false);
	void AddSourceAtIndex(const char *sourceName, int index);
	void Load(obs_data_t *loadObj);
	void LoadAllSources();
	void Save(obs_data_t *saveObj);
	void AddSourceMenuItem(obs_source_t *source);
	void SetItemsButtonVisibility();
	void updateEnabled();
	void CleanupSourceHandlers();

	static bool AddSourceName(void *data, obs_source_t *source);

	static void SceneChangeCallback(enum obs_frontend_event event,
					void *data);
	static bool DynAddSceneItems(obs_scene_t *scene,
				     obs_sceneitem_t *sceneItem, void *data);

	static void ItemAddedToScene(void *data, calldata_t *params);
	static void ItemRemovedFromScene(void *data, calldata_t *params);

	void RemoveNullSources();
	void SourceRename(obs_source_t *source);

private:
	QuickAccessDock *_dock;
	QuickAccessList *_sourceList;
	QLineEdit *_searchText;
	QToolBar *_actionsToolbar;
	QAction *_actionAddSource = nullptr;
	QAction *_actionRemoveSource = nullptr;
	QAction *_actionSourceUp = nullptr;
	QAction *_actionSourceDown = nullptr;
	QAction *_actionDockProperties = nullptr;
	QDialog *CreateAddSourcePopupMenu();
	obs_weak_source_t *_current = nullptr;
	signal_handler_t *source_signal_handler = nullptr;
	void AddSourcePopupMenu();
	void _ClearMenuSources();
	void _LoadDynamicScenes();
	std::vector<obs_source_t *> _menuSources;
	std::vector<std::string> _manualSourceNames;
	std::vector<std::string> _allSourceNames;
	std::set<std::string> _dynamicScenes;
	bool _active = true;
	bool _switchingSC = false;

	// Context menu actions
	QAction *_actionCtxtAdd = nullptr;
	QAction *_actionCtxtAddCurrent = nullptr;
	QAction *_actionCtxtAddCurrentClone = nullptr;
	QAction *_actionCtxtFilters = nullptr;
	QAction *_actionCtxtProperties = nullptr;
	QAction *_actionCtxtRemoveFromDock = nullptr;
	QAction *_actionCtxtRenameSource = nullptr;
	QAction *_actionCtxtInteract = nullptr;

private slots:
	void on_actionAddSource_triggered();
	void on_actionRemoveSource_triggered();
	void on_actionSourceUp_triggered();
	void on_actionSourceDown_triggered();
	void on_actionDockProperties_triggered();
	void on_sourceList_itemSelectionChanged();
};

class UpdateDockDialog : public QDialog {
	Q_OBJECT
public:
	UpdateDockDialog(QuickAccessDock *dock, QWidget *parent = nullptr);

private:
	QuickAccessDock *_dock = nullptr;
	QLayout *_layout = nullptr;
	QLayout *_layout2 = nullptr;
	//QLabel* _labelName = nullptr;
	QLineEdit *_inputName = nullptr;

	//QLabel* _labelType = nullptr;
	QComboBox *_inputType = nullptr;

	QCheckBox *_showProperties = nullptr;
	QCheckBox *_showFilters = nullptr;
	QCheckBox *_showScenes = nullptr;
	QCheckBox *_clickThroughScenes = nullptr;

	QDialogButtonBox *_buttonBox = nullptr;
private slots:
	void on_update_dock();
	void on_cancel();
};

bool AddSourceToWidget(void *data, obs_source_t *source);
