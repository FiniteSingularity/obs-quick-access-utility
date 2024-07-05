#pragma once
#include <obs-module.h>
#include <obs-frontend-api.h>

#include <QDockWidget>
#include <QWidget>
#include <QDialog>
#include <QVBoxLayout>
#include <QListWidget>
#include <QComboBox>
#include <QToolBar>
#include <QAction>
#include <QDialogButtonBox>
#include <vector>
#include <memory>
#include "quick-access-dock.hpp"

class QuickAccessUtility;
extern QuickAccessUtility *qad;

class QuickAccessUtilityDialog;

struct CreateDockFormData {
	std::string dockName;
	std::string dockType;
	bool showProperties;
	bool showFilters;
	bool showScenes;
	bool clickableScenes;
};

class QuickAccessUtility {
public:
	QuickAccessUtility(obs_module_t *m);
	inline ~QuickAccessUtility();

	void Load(obs_data_t *data);
	void Save(obs_data_t *data);
	void RemoveDock(int idx, bool cleanup = false);
	void RemoveDocks();
	inline std::vector<QuickAccessDock *> GetDocks() { return _docks; }

	obs_module_t *GetModule();
	bool mainWindowOpen = false;

	QuickAccessUtilityDialog *dialog = nullptr;

	QIcon GetIconFromType(const char *type) const;
	QIcon GetSceneIcon() const;
	QIcon GetGroupIcon() const;

	void CreateDock(CreateDockFormData data);

	static void FrontendCallback(enum obs_frontend_event event, void *data);
	static void SourceCreated(void *data, calldata_t *params);
	static void SourceDestroyed(void *data, calldata_t *params);
	static void SourceRename(void *data, calldata_t *params);

private:
	obs_module_t *_module = nullptr;
	std::vector<QuickAccessDock *> _docks;
	bool _firstRun;
	bool _sceneCollectionChanging = false;
};

class CreateDockDialog : public QDialog {
	Q_OBJECT
public:
	CreateDockDialog(QWidget *parent = nullptr);

private:
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
	void on_create_dock();
	void on_cancel();
};

class QuickAccessUtilityDialog : public QDialog {
	Q_OBJECT
public:
	QuickAccessUtilityDialog(QWidget *parent = nullptr);
	~QuickAccessUtilityDialog();

	void LoadDockList();

	static QuickAccessUtilityDialog *dialog;

private:
	QLayout *_layout = nullptr;
	QListWidget *_dockList = nullptr;
	QToolBar *_toolbar = nullptr;
	QAction *_actionAddDock = nullptr;
	QAction *_actionRemoveDock = nullptr;

private slots:
	void on_actionAddDock_triggered();
	void on_actionRemoveDock_triggered();
	void on_dockList_itemSelectionChanged();
};

class DockListItem : public QFrame {
	Q_OBJECT
public:
	DockListItem(QuickAccessDock *dock, QWidget *parent = nullptr);

private:
	QuickAccessDock *_dock;
	QLayout *_layout = nullptr;
	QLabel *_label = nullptr;
	QCheckBox *_clickableScenes = nullptr;
	QCheckBox *_properties = nullptr;
	QCheckBox *_filters = nullptr;
	QCheckBox *_scenes = nullptr;
};

void OpenQAUDialog();

void frontendSaveLoad(obs_data_t *save_data, bool saving, void *data);
