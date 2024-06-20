#pragma once
#include <obs-module.h>

#include <QString>
#include <QWidget>
#include <QLabel>
#include <QCheckBox>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QToolBar>
#include <QHBoxLayout>
#include <vector>

class QuickAccess;

class QuickAccessSceneItem : public QWidget {
	Q_OBJECT

public:
	QuickAccessSceneItem(QWidget *parent, obs_sceneitem_t *sceneItem);
	~QuickAccessSceneItem();
	void mouseReleaseEvent (QMouseEvent *e) override;
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
	QuickAccessList(QWidget *parent);
	void dropEvent(QDropEvent *event) override;
private:
	QuickAccess *_qa = nullptr;
};

class QuickAccessItem : public QFrame {
	Q_OBJECT

public:
	QuickAccessItem(QWidget *parent, obs_source_t *source);
	QuickAccessItem(QWidget *parent, QuickAccessItem* original);
	~QuickAccessItem();
	void Save(obs_data_t* saveObj);
	const char* GetSourceName();

	static bool GetSceneItemsFromScene(void* data, obs_source_t* s);
	static bool AddSceneItems(obs_scene_t* s, obs_sceneitem_t* si, void* data);

private:
	QLabel *_label = nullptr;
	QLabel *_iconLabel = nullptr;
	QToolBar *_actionsToolbar = nullptr;
	QPushButton *_filters = nullptr;
	obs_weak_source_t *_source = nullptr;
	std::vector<obs_sceneitem_t *> _sceneItems;
	void _getSceneItems();
	void _clearSceneItems();
	QMenu* _CreateSceneMenu();
	void _AddScenePopupMenu(const QPoint& pos);

private slots:
	void on_actionProperties_triggered();
	void on_actionFilters_triggered();
	void on_actionScenes_triggered();
};

class QuickAccess : public QListWidget {
	Q_OBJECT

public:
	QuickAccess(QWidget *parent, QString name);
	void AddSource(const char* sourceName);
	void AddSourceAtIndex(const char* sourceName, int index);
	void Load(obs_data_t *loadObj);
	void Save(obs_data_t* saveObj);
	void AddSourceMenuItem(obs_source_t* source);
	void updateEnabled();

private:
	QuickAccessList *_sourceList;
	QToolBar *_actionsToolbar;
	QAction *_actionAddSource = nullptr;
	QAction *_actionRemoveSource = nullptr;
	QAction *_actionSourceUp = nullptr;
	QAction *_actionSourceDown = nullptr;
	QMenu* CreateAddSourcePopupMenu();
	void AddSourcePopupMenu(const QPoint& pos);
	void _ClearMenuSources();
	std::vector<obs_source_t *> _menuSources;

private slots:
	void on_actionAddSource_triggered();
	void on_actionRemoveSource_triggered();
	void on_actionSourceUp_triggered();
	void on_actionSourceDown_triggered();
	void on_sourceList_itemSelectionChanged();
};

bool AddSourceToWidget(void *data, obs_source_t *source);
