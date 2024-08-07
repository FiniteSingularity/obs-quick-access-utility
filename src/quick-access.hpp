#pragma once
#include <vector>
#include <map>
#include <set>
#include <memory>

#include <obs-module.h>
#include <obs-frontend-api.h>

#include <QString>
#include <QWidget>
#include <QLabel>
#include <QCheckBox>
#include <QListWidget>
#include <QStackedWidget>
#include <QMenu>
#include <QPushButton>
#include <QToolBar>
#include <QHBoxLayout>
#include <QDialog>
#include <QDialogButtonBox>
#include <QComboBox>
#include <QListView>
#include <QSize>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QLineEdit>
#include <QScrollArea>

enum class SearchType;
class QuickAccessSource;
class QuickAccess;
class QuickAccessDock;
class QuickAccessSourceModel;

class QuickAccessSourceList : public QListView {
	Q_OBJECT
public:
	QuickAccessSourceList(QWidget *parent, SearchType searchType);
	QSize sizeHint() const override;
	void search(std::string searchTerm);
	inline int visibleCount() const { return _numActive; }
	QuickAccessSource *currentSource();

private:
	QuickAccess *_qaParent;
	void _setupContextMenu();
	SearchType _searchType;
	bool _activeSearch;
	int _numActive;
	QAction *_actionCtxtAddCurrent;
	QAction *_actionCtxtAddCurrentClone;
	QAction *_actionCtxtProperties;
	QAction *_actionCtxtFilters;
	QAction *_actionCtxtRenameSource;
	QAction *_actionCtxtInteract;
	QAction *_actionCtxtRefresh;
	QAction *_actionCtxtToggleActivation;

	QItemSelection _selected, _prior;

protected:
	void mousePressEvent(QMouseEvent *event) override;

signals:
	void selectedItemChanged(const QModelIndex &index);
};

struct QuickAccessSourceListView {
	std::string header;
	QLabel *headerLabel;
	QuickAccessSourceList *listView;
	QuickAccessSourceModel *model;
};

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
			QuickAccessSource *qaSource);
	QuickAccessItem(QWidget *parent, QuickAccessItem *original);
	~QuickAccessItem();
	bool Save(obs_data_t *saveObj);
	const char *GetSourceName();
	void RenameSource(std::string name);
	static bool GetSceneItemsFromScene(void *data, obs_source_t *s);
	static bool AddSceneItems(obs_scene_t *s, obs_sceneitem_t *si,
				  void *data);
	void SetButtonVisibility();
	bool IsSource(obs_source_t *s);
	bool IsSource(QuickAccessSource *s);
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

	bool SearchMatch(QString term);

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
	QuickAccessSource *_qaSource = nullptr;
	std::vector<obs_sceneitem_t *> _sceneItems;
	std::map<std::string, std::vector<std::string>> _searchGroups;
	std::vector<std::string> _searchTerms;
	bool _configurable;
	void _getSceneItems();
	void _clearSceneItems();
	QMenu *_CreateSceneMenu();
	void _SetSearchTerms();
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
	void AddSource(QuickAccessSource *source, std::string groupName);
	void RemoveSource(QuickAccessSource *source, std::string groupName);
	void Load();
	void UpdateVisibility();
	void Save(obs_data_t *saveObj);
	void AddSourceMenuItem(obs_source_t *source);
	void SetItemsButtonVisibility();
	void updateEnabled();
	void Redraw();
	void ClearSelections(QuickAccessSourceList *skip);
	void SearchFocus();
	void DismissModal();
	static bool AddSourceName(void *data, obs_source_t *source);
	static bool GetSceneItemsFromScene(void *data, obs_source_t *s);
	static bool AddSceneItems(obs_scene_t *s, obs_sceneitem_t *si,
				  void *data);

	void RemoveNullSources();
	void SourceRename(QListWidgetItem *item);
	virtual void paintEvent(QPaintEvent *);

private:
	QuickAccessDock *_dock;
	QuickAccessList *_sourceList;
	QStackedWidget *_contents;
	QScrollArea *_listsContainer;
	QWidget *_emptySearch;
	QWidget *_emptyManual;
	QWidget *_emptyDynamic;
	QWidget *_noSearchResults;
	QLineEdit *_searchText;
	QToolBar *_actionsToolbar;
	QAction *_actionAddSource = nullptr;
	QAction *_actionRemoveSource = nullptr;
	QAction *_actionSourceUp = nullptr;
	QAction *_actionSourceDown = nullptr;
	QAction *_actionDockProperties = nullptr;
	QDialog *CreateAddSourcePopupMenu();
	obs_weak_source_t *_current = nullptr;
	QuickAccessSource *_currentSource = nullptr;
	signal_handler_t *source_signal_handler = nullptr;
	void AddSourcePopupMenu();
	QMenu *_CreateParentSceneMenu();
	void _ClearMenuSources();
	void _getSceneItems();
	void _clearSceneItems();
	void _createListContainer();
	std::vector<obs_source_t *> _menuSources;
	std::vector<std::string> _manualSourceNames;
	std::vector<std::string> _allSourceNames;
	std::set<std::string> _dynamicScenes;
	std::vector<obs_sceneitem_t *> _sceneItems;

	std::vector<QuickAccessSourceListView> _qaLists;
	std::vector<std::unique_ptr<QuickAccessSourceModel>> _sourceModels;

	bool _active = true;
	bool _switchingSC = false;
	bool _noSearch = true;

	// Context menu actions
	QAction *_actionCtxtAdd = nullptr;
	QAction *_actionCtxtAddCurrent = nullptr;
	QAction *_actionCtxtAddCurrentClone = nullptr;
	QAction *_actionCtxtFilters = nullptr;
	QAction *_actionCtxtProperties = nullptr;
	QAction *_actionCtxtRemoveFromDock = nullptr;
	QAction *_actionCtxtRenameSource = nullptr;
	QAction *_actionCtxtInteract = nullptr;

	int _activeIndex = -1;

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
void EnumerateFilters(obs_source_t *parentScene, obs_source_t *filter,
		      void *param);
