#pragma once
#include <obs-module.h>

#include <QString>
#include <QWidget>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QToolBar>
#include <vector>

class QuickAccessItem : public QFrame {
	Q_OBJECT

public:
	QuickAccessItem(QWidget *parent, obs_source_t *source);
	QuickAccessItem(QWidget *parent, QuickAccessItem* original);
	~QuickAccessItem();
	void Save(obs_data_t* saveObj);
private:
	QLabel *_label = nullptr;
	QLabel *_iconLabel = nullptr;
	QToolBar *_actionsToolbar = nullptr;
	QPushButton *_filters = nullptr;
	obs_weak_source_t *_source = nullptr;
private slots:
	void on_actionProperties_triggered();
	void on_actionFilters_triggered();
};

class QuickAccess : public QWidget {
	Q_OBJECT

public:
	QuickAccess(QWidget *parent, QString name);
	void AddSource(const char* sourceName);
	void Load(obs_data_t *loadObj);
	void Save(obs_data_t* saveObj);
	void AddSourceMenuItem(obs_source_t* source);

private:
	QListWidget *_sourceList;
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
