#include "quick-access.hpp"
#include "quick-access-dock.hpp"
#include "quick-access-utility.hpp"

#include <obs-module.h>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QWidgetAction>
#include <QLineEdit>
#include <QApplication>
#include <QThread>
#include <QMetaObject>
#include <QScrollArea>
#include <QDialog>

#include <algorithm>
#include "version.h"

#define QT_UTF8(str) QString::fromUtf8(str)
#define QT_TO_UTF8(str) str.toUtf8().constData()

extern QuickAccessUtility *qau;

QuickAccessList::QuickAccessList(QWidget *parent) : QListWidget(parent)
{
	_qa = dynamic_cast<QuickAccess *>(parent);
}

void QuickAccessList::dropEvent(QDropEvent *event)
{
	QListWidget::dropEvent(event);
	_qa->updateEnabled();
}

QuickAccessItem::QuickAccessItem(QWidget *parent, QuickAccessItem *original)
	: QFrame(parent),
	  _dock(original->_dock),
	  _configurable(false)
{
	obs_source_t *source = obs_weak_source_get_source(original->_source);
	_source = obs_source_get_weak_source(source);
	_source = obs_source_get_weak_source(source);
	obs_source_release(source);
}

QuickAccessItem::QuickAccessItem(QWidget *parent, QuickAccessDock *dock,
				 obs_source_t *source)
	: QFrame(parent),
	  _dock(dock)
{
	_source = obs_source_get_weak_source(source);
	_configurable = obs_source_configurable(source);
	const char *id = obs_source_get_id(source);

	setAttribute(Qt::WA_TranslucentBackground);
	setMouseTracking(true);
	setStyleSheet("background: none");

	QIcon icon;

	if (strcmp(id, "scene") == 0)
		icon = qau->GetSceneIcon();
	else if (strcmp(id, "group") == 0)
		icon = qau->GetGroupIcon();
	else
		icon = qau->GetIconFromType(id);
	QPixmap pixmap = icon.pixmap(QSize(16, 16));
	_iconLabel = new QLabel(this);
	_iconLabel->setPixmap(pixmap);
	_iconLabel->setStyleSheet("background: none");

	_label = new QLabel(this);
	_label->setText(obs_source_get_name(source));
	_label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	_label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	_label->setAttribute(Qt::WA_TranslucentBackground);

	auto layout = new QHBoxLayout();
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(_iconLabel);
	layout->addSpacing(2);
	layout->addWidget(_label);

	_actionProperties = new QPushButton();
	_actionProperties->setProperty("themeID", "propertiesIconSmall");
	_actionProperties->setDisabled(false);
	_actionProperties->setAccessibleDescription(
		"Opens the source properties window.");
	_actionProperties->setAccessibleName("Open Source Properties");
	_actionProperties->setToolTip("Open Source Properties");
	_actionProperties->setStyleSheet("padding: 0px; background: none");
	connect(_actionProperties, &QPushButton::released, this,
		&QuickAccessItem::on_actionProperties_triggered);

	_actionFilters = new QPushButton();
	_actionFilters->setProperty("themeID", "filtersIcon");
	_actionFilters->setDisabled(false);
	_actionFilters->setAccessibleDescription(
		"Opens the source filters window.");
	_actionFilters->setAccessibleName("Open Source Filters");
	_actionFilters->setToolTip("Open Source Filters");
	_actionFilters->setStyleSheet("padding: 0px; background: none");
	connect(_actionFilters, &QPushButton::released, this,
		&QuickAccessItem::on_actionFilters_triggered);

	_actionScenes = new QPushButton();
	QIcon sceneIcon;
	sceneIcon = qau->GetSceneIcon();
	_actionScenes->setIcon(sceneIcon);
	_actionScenes->setDisabled(false);
	_actionScenes->setAccessibleDescription(
		"Opens list of all parent scenes");
	_actionScenes->setAccessibleName("Show Parent Scenes");
	_actionScenes->setToolTip("Show Parent Scenes");
	_actionScenes->setStyleSheet("padding: 0px; background: none");
	connect(_actionScenes, &QPushButton::released, this,
		&QuickAccessItem::on_actionScenes_triggered);

	layout->addWidget(_actionProperties);
	layout->addWidget(_actionFilters);
	layout->addWidget(_actionScenes);

	setLayout(layout);
	SetButtonVisibility();
}

void QuickAccessItem::SetButtonVisibility()
{
	_actionProperties->setVisible(_configurable && _dock->ShowProperties());
	_actionFilters->setVisible(_dock->ShowFilters());
	_actionScenes->setVisible(_dock->ShowScenes());
}

const char *QuickAccessItem::GetSourceName()
{
	obs_source_t *source = obs_weak_source_get_source(_source);
	const char *name = obs_source_get_name(source);
	obs_source_release(source);
	return name;
}

QuickAccessItem::~QuickAccessItem()
{
	delete _label;
	delete _iconLabel;
	delete _actionProperties;
	delete _actionFilters;
	delete _actionScenes;
	_clearSceneItems();
	obs_weak_source_release(_source);
}

void QuickAccessItem::Save(obs_data_t *itemObj)
{
	obs_source_t *source = obs_weak_source_get_source(_source);
	if (!source) {
		return;
	}

	const char *sourceName = obs_source_get_name(source);
	obs_data_set_string(itemObj, "source_name", sourceName);

	obs_source_release(source);
}

void QuickAccessItem::on_actionProperties_triggered()
{
	obs_source_t *source = obs_weak_source_get_source(_source);
	if (!source) {
		return;
	}
	obs_frontend_open_source_properties(source);
	obs_source_release(source);
}

void QuickAccessItem::on_actionFilters_triggered()
{
	obs_source_t *source = obs_weak_source_get_source(_source);
	if (!source) {
		return;
	}
	obs_frontend_open_source_filters(source);
	obs_source_release(source);
}

void QuickAccessItem::_getSceneItems()
{
	_clearSceneItems();
	obs_enum_scenes(QuickAccessItem::GetSceneItemsFromScene, this);
}

bool QuickAccessItem::GetSceneItemsFromScene(void *data, obs_source_t *s)
{
	obs_scene_t *scene = obs_scene_from_source(s);
	obs_scene_enum_items(scene, QuickAccessItem::AddSceneItems, data);
	return true;
}

bool QuickAccessItem::AddSceneItems(obs_scene_t *scene,
				    obs_sceneitem_t *sceneItem, void *data)
{
	UNUSED_PARAMETER(scene);

	auto qai = static_cast<QuickAccessItem *>(data);
	auto source = obs_sceneitem_get_source(sceneItem);
	if (obs_source_is_group(source)) {
		obs_scene_t *group = obs_group_from_source(source);
		obs_scene_enum_items(group, QuickAccessItem::AddSceneItems,
				     data);
	}
	if (obs_weak_source_references_source(qai->_source, source)) {
		obs_sceneitem_addref(sceneItem);
		qai->_sceneItems.push_back(sceneItem);
	}
	return true;
}

bool QuickAccessItem::IsSource(obs_source_t *s)
{
	obs_source_t *source = obs_weak_source_get_source(_source);
	bool ret = source == s;
	obs_source_release(source);
	return ret;
}

bool QuickAccessItem::IsNullSource()
{
	obs_source_t *source = obs_weak_source_get_source(_source);
	bool ret = source == nullptr;
	if (source) {
		obs_source_release(source);
	}
	return ret;
}

QMenu *QuickAccessItem::_CreateSceneMenu()
{
	QMenu *popup = new QMenu("SceneMenu", this);

	auto wa = new QWidgetAction(popup);
	auto t = new QLineEdit;
	t->connect(t, &QLineEdit::textChanged, [popup](const QString text) {
		foreach(auto action, popup->actions()) action->setVisible(
			action->text().isEmpty() ||
			action->text().contains(text, Qt::CaseInsensitive));
	});
	wa->setDefaultWidget(t);
	popup->addAction(wa);

	auto getActionAfter = [](QMenu *menu, const QString &name) {
		QList<QAction *> actions = menu->actions();

		for (QAction *menuAction : actions) {
			if (menuAction->text().compare(
				    name, Qt::CaseInsensitive) >= 0)
				return menuAction;
		}

		return (QAction *)nullptr;
	};

	auto addSource = [this, getActionAfter](QMenu *pop,
						obs_sceneitem_t *sceneItem) {
		auto scene = obs_sceneitem_get_scene(sceneItem);
		obs_source_t *sceneSource = obs_scene_get_source(scene);
		const char *name = obs_source_get_name(sceneSource);
		QString qname = name;

		QWidgetAction *popupItem = new QWidgetAction(this);
		QWidget *itemWidget = new QuickAccessSceneItem(this, sceneItem);
		popupItem->setDefaultWidget(itemWidget);

		QAction *after = getActionAfter(pop, qname);
		pop->insertAction(after, popupItem);
		return true;
	};
	for (auto &src : _sceneItems) {
		addSource(popup, src);
	}

	return popup;
}

void QuickAccessItem::_AddScenePopupMenu(const QPoint &pos)
{
	QScopedPointer<QMenu> popup(_CreateSceneMenu());
	if (popup) {
		popup->exec(pos);
	}
}

void QuickAccessItem::_clearSceneItems()
{
	for (auto &sceneItem : _sceneItems) {
		obs_sceneitem_release(sceneItem);
	}
	_sceneItems.clear();
}

void QuickAccessItem::on_actionScenes_triggered()
{
	obs_source_t *source = obs_weak_source_get_source(_source);
	if (!source) {
		return;
	}
	_getSceneItems();
	_AddScenePopupMenu(QCursor::pos());
}

void QuickAccessItem::SwitchToScene()
{
	obs_source_t *source = obs_weak_source_get_source(_source);
	if (obs_source_is_scene(source)) {
		obs_frontend_set_current_scene(source);
	}

	obs_source_release(source);
}

QuickAccess::QuickAccess(QWidget *parent, QuickAccessDock *dock, QString name)
	: QListWidget(parent),
	  _dock(dock)
{
	setObjectName(name);
	auto layout = new QVBoxLayout(this);
	layout->setSpacing(0);
	layout->setContentsMargins(0, 0, 0, 0);

	_sourceList = new QuickAccessList(this);
	_sourceList->setObjectName(QStringLiteral("sources"));
	QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
	sizePolicy.setHorizontalStretch(0);
	sizePolicy.setVerticalStretch(0);
	sizePolicy.setHeightForWidth(
		_sourceList->sizePolicy().hasHeightForWidth());
	_sourceList->setSizePolicy(sizePolicy);
	_sourceList->setContextMenuPolicy(Qt::CustomContextMenu);
	_sourceList->setFrameShape(QFrame::NoFrame);
	_sourceList->setFrameShadow(QFrame::Plain);
	_sourceList->setProperty("showDropIndicator", QVariant(true));
	if (_dock->GetType() == "Manual") {
		_sourceList->setDragEnabled(true);
		_sourceList->setDragDropMode(QAbstractItemView::InternalMove);
		_sourceList->setDefaultDropAction(Qt::TargetMoveAction);
	} else {
		_sourceList->setDragEnabled(false);
		_sourceList->setDragDropMode(QAbstractItemView::NoDragDrop);
		_sourceList->viewport()->setAcceptDrops(false);
	}

	connect(_sourceList, SIGNAL(itemSelectionChanged()), this,
		SLOT(on_sourceList_itemSelectionChanged()));

	if (_dock->GetType() == "Source Search") {
		_searchText = new QLineEdit;
		_searchText->setPlaceholderText("Search...");
		_searchText->connect(
			_searchText, &QLineEdit::textChanged,
			[this](const QString text) {
				blog(LOG_INFO, "=== Search List Size: %i",
				     _sourceList->count());
				for (int i = 0; i < _sourceList->count(); i++) {
					QListWidgetItem *item =
						_sourceList->item(i);
					QuickAccessItem *widget =
						dynamic_cast<QuickAccessItem *>(
							_sourceList->itemWidget(
								item));
					if (widget) {
						QString wName =
							widget->GetSourceName();
						item->setHidden(
							text.isEmpty() ||
							!wName.contains(
								text,
								Qt::CaseInsensitive));
					}
				}
			});
		layout->addWidget(_searchText);
	}

	layout->addWidget(_sourceList);

	_actionsToolbar = new QToolBar(this);
	_actionsToolbar->setObjectName(QStringLiteral("actionsToolbar"));
	_actionsToolbar->setIconSize(QSize(16, 16));
	_actionsToolbar->setFloatable(false);

	if (_dock->GetType() == "Manual") {
		_actionAddSource = new QAction(this);
		_actionAddSource->setObjectName(
			QStringLiteral("actionAddSource"));
		_actionAddSource->setProperty("themeID", "addIconSmall");
		_actionAddSource->setText(QT_UTF8(obs_module_text("Add")));
		connect(_actionAddSource, SIGNAL(triggered()), this,
			SLOT(on_actionAddSource_triggered()));
		_actionsToolbar->addAction(_actionAddSource);

		_actionRemoveSource = new QAction(this);
		_actionRemoveSource->setObjectName(
			QStringLiteral("actionRemoveSource"));
		_actionRemoveSource->setShortcutContext(
			Qt::WidgetWithChildrenShortcut);
		_actionRemoveSource->setProperty("themeID", "removeIconSmall");
		_actionRemoveSource->setText(
			QT_UTF8(obs_module_text("Remove")));
		connect(_actionRemoveSource, SIGNAL(triggered()), this,
			SLOT(on_actionRemoveSource_triggered()));
		_actionsToolbar->addAction(_actionRemoveSource);

		_actionsToolbar->addSeparator();

		_actionSourceUp = new QAction(this);
		_actionSourceUp->setObjectName(
			QStringLiteral("actionSourceUp"));
		_actionSourceUp->setProperty("themeID", "upArrowIconSmall");
		_actionSourceUp->setText(QT_UTF8(obs_module_text("MoveUp")));
		connect(_actionSourceUp, SIGNAL(triggered()), this,
			SLOT(on_actionSourceUp_triggered()));
		_actionsToolbar->addAction(_actionSourceUp);

		_actionSourceDown = new QAction(this);
		_actionSourceDown->setObjectName(
			QStringLiteral("actionSourceDown"));
		_actionSourceDown->setProperty("themeID", "downArrowIconSmall");
		_actionSourceDown->setText(
			QT_UTF8(obs_module_text("MoveDown")));
		connect(_actionSourceDown, SIGNAL(triggered()), this,
			SLOT(on_actionSourceDown_triggered()));
		_actionsToolbar->addAction(_actionSourceDown);

		// Themes need the QAction dynamic properties
		for (QAction *x : _actionsToolbar->actions()) {
			QWidget *temp = _actionsToolbar->widgetForAction(x);

			for (QByteArray &y : x->dynamicPropertyNames()) {
				temp->setProperty(y, x->property(y));
			}
		}

		_actionRemoveSource->setEnabled(false);
		_actionSourceUp->setEnabled(false);
		_actionSourceDown->setEnabled(false);
	}

	layout->addWidget(_actionsToolbar);
	layout->addItem(new QSpacerItem(150, 0, QSizePolicy::Fixed,
					QSizePolicy::Minimum));

	if (_dock->GetType() == "Dynamic") {
		QuickAccess::SceneChangeCallback(
			OBS_FRONTEND_EVENT_SCENE_CHANGED, this);
		obs_frontend_add_event_callback(
			QuickAccess::SceneChangeCallback, this);
	}
}

QuickAccess::~QuickAccess()
{
	blog(LOG_INFO,
	     "QuickAccess::~QuickAccess() called (Dock was destroyed)");
	obs_frontend_remove_event_callback(QuickAccess::SceneChangeCallback,
					   this);
	if (_current) {
		obs_weak_source_release(_current);
	}
	source_signal_handler = nullptr;
}

void QuickAccess::CleanupSourceHandlers()
{
	if (source_signal_handler) {
		blog(LOG_INFO, "QuickAccess::CleanupSourceHandlers()");
		signal_handler_disconnect(source_signal_handler, "item_add",
					  QuickAccess::ItemAddedToScene, this);
		signal_handler_disconnect(source_signal_handler, "item_remove",
					  QuickAccess::ItemRemovedFromScene,
					  this);
		source_signal_handler = nullptr;
	}
}

void QuickAccess::SceneChangeCallback(enum obs_frontend_event event, void *data)
{
	QuickAccess *qa = static_cast<QuickAccess *>(data);
	QMetaObject::invokeMethod(
		QCoreApplication::instance()->thread(), [qa, event]() {
			if (event == OBS_FRONTEND_EVENT_SCENE_CHANGED) {
				blog(LOG_INFO, "SCENE CHANGE!");
				qa->CleanupSourceHandlers();
				obs_source_t *current =
					obs_frontend_get_current_scene();
				if (qa->_current) {
					obs_weak_source_release(qa->_current);
				}
				qa->_current =
					obs_source_get_weak_source(current);
				qa->source_signal_handler =
					obs_source_get_signal_handler(current);
				signal_handler_connect(
					qa->source_signal_handler, "item_add",
					QuickAccess::ItemAddedToScene, qa);
				signal_handler_connect(
					qa->source_signal_handler,
					"item_remove",
					QuickAccess::ItemRemovedFromScene, qa);
				obs_source_release(current);
				qa->_LoadDynamicScenes();
			} else if (event ==
				   OBS_FRONTEND_EVENT_FINISHED_LOADING) {
				blog(LOG_INFO, "FINISHED LOADING...");
				// Load the scene list if is a search bar.
				qa->_active = true;
			} else if (event == OBS_FRONTEND_EVENT_EXIT) {
				blog(LOG_INFO, "EXITING...");
				qa->_active = false;
			} else if (event ==
				   OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGING) {
				blog(LOG_INFO, "SCENE COLLECTION CHANGING");
				qa->_dock->SwitchingSceneCollections(true);
				qa->_active = false;
				qa->CleanupSourceHandlers();
			}
		});
}

void QuickAccess::_LoadDynamicScenes()
{
	if (!_active || !_sourceList) {
		return;
	}
	_dynamicScenes.clear();
	_sourceList->clear();

	obs_source_t *current = obs_weak_source_get_source(_current);
	obs_scene_t *currentScene = obs_scene_from_source(current);
	obs_scene_enum_items(currentScene, QuickAccess::DynAddSceneItems, this);
	for (auto &sceneName : _dynamicScenes) {
		AddSource(sceneName.c_str());
	}
	obs_source_release(current);
}

bool QuickAccess::DynAddSceneItems(obs_scene_t *scene,
				   obs_sceneitem_t *sceneItem, void *data)
{
	UNUSED_PARAMETER(scene);
	QuickAccess *qa = static_cast<QuickAccess *>(data);
	obs_source_t *source = obs_sceneitem_get_source(sceneItem);
	std::string sourceName = obs_source_get_name(source);
	if (qa->_dynamicScenes.count(sourceName) == 0) {
		qa->_dynamicScenes.insert(sourceName);
		if (obs_source_is_scene(source)) {
			obs_scene_t *currentScene =
				obs_scene_from_source(source);
			obs_scene_enum_items(currentScene,
					     QuickAccess::DynAddSceneItems,
					     data);
		} else if (obs_source_is_group(source)) {
			obs_scene_t *currentGroup =
				obs_group_from_source(source);
			obs_scene_enum_items(currentGroup,
					     QuickAccess::DynAddSceneItems,
					     data);
		}
	}
	return true;
}

void QuickAccess::ItemAddedToScene(void *data, calldata_t *params)
{
	blog(LOG_INFO, "Item added to scene");
	UNUSED_PARAMETER(params);
	QuickAccess *qa = static_cast<QuickAccess *>(data);
	QMetaObject::invokeMethod(QCoreApplication::instance()->thread(),
				  [qa]() { qa->_LoadDynamicScenes(); });
}

void QuickAccess::ItemRemovedFromScene(void *data, calldata_t *params)
{
	QuickAccess *qa = static_cast<QuickAccess *>(data);
	blog(LOG_INFO, "Item removed from scene");
	UNUSED_PARAMETER(params);
	QMetaObject::invokeMethod(QCoreApplication::instance()->thread(),
				  [qa]() {
					  if (qa->_active) {
						  qa->_LoadDynamicScenes();
					  }
				  });
}

void QuickAccess::Save(obs_data_t *saveObj)
{
	auto itemsArr = obs_data_array_create();
	blog(LOG_INFO, "SAVING");
	if (_dock->GetType() == "Manual") {
		for (int i = 0; i < _sourceList->count(); ++i) {
			auto itemObj = obs_data_create();
			QListWidgetItem *item = _sourceList->item(i);
			QuickAccessItem *widget =
				dynamic_cast<QuickAccessItem *>(
					_sourceList->itemWidget(item));
			widget->Save(itemObj);
			obs_data_array_push_back(itemsArr, itemObj);
			obs_data_release(itemObj);
		}
	}
	obs_data_set_array(saveObj, "dock_sources", itemsArr);
	obs_data_array_release(itemsArr);
}

void QuickAccess::Load(obs_data_t *loadObj)
{
	if (_dock->GetType() == "Manual") {
		obs_data_array_t *items =
			obs_data_get_array(loadObj, "dock_sources");
		auto numItems = obs_data_array_count(items);
		for (size_t i = 0; i < numItems; i++) {
			auto item = obs_data_array_item(items, i);
			auto sourceName =
				obs_data_get_string(item, "source_name");
			AddSource(sourceName);
			obs_data_release(item);
		}
		obs_data_array_release(items);
	} else if (_dock->GetType() == "Source Search") {
		// Get all sources/scenes/groups
		LoadAllSources();
	}
}

void QuickAccess::LoadAllSources()
{
	_allSourceNames.clear();
	_sourceList->clear();
	obs_enum_sources(QuickAccess::AddSourceName, this);
	obs_enum_scenes(QuickAccess::AddSourceName, this);
	for (auto &name : _allSourceNames) {
		AddSource(name.c_str(), true);
	}
	QString text = _searchText->text();
	for (int i = 0; i < _sourceList->count(); i++) {
		QListWidgetItem *item = _sourceList->item(i);
		QuickAccessItem *widget = dynamic_cast<QuickAccessItem *>(
			_sourceList->itemWidget(item));
		QString name = widget->GetSourceName();
		item->setHidden(text.isEmpty() ||
				!name.contains(text, Qt::CaseInsensitive));
	}
}

bool QuickAccess::AddSourceName(void *data, obs_source_t *source)
{
	auto qa = static_cast<QuickAccess *>(data);
	std::string sourceName = obs_source_get_name(source);
	qa->_allSourceNames.push_back(sourceName);
	return true;
}

void QuickAccess::SetItemsButtonVisibility()
{
	for (int i = 0; i < _sourceList->count(); i++) {
		QListWidgetItem *item = _sourceList->item(i);
		auto widget = dynamic_cast<QuickAccessItem *>(
			_sourceList->itemWidget(item));
		widget->SetButtonVisibility();
	}
}

void QuickAccess::AddSource(const char *sourceName, bool hidden)
{
	obs_source_t *source = obs_get_source_by_name(sourceName);
	auto item = new QListWidgetItem();
	item->setHidden(hidden);
	_sourceList->addItem(item);
	auto row = new QuickAccessItem(this, _dock, source);
	_sourceList->setItemWidget(item, row);
	obs_source_release(source);
}

void QuickAccess::AddSourceAtIndex(const char *sourceName, int index)
{
	obs_source_t *source = obs_get_source_by_name(sourceName);
	auto item = new QListWidgetItem();
	_sourceList->insertItem(index, item);
	auto row = new QuickAccessItem(this, _dock, source);
	_sourceList->setItemWidget(item, row);
	obs_source_release(source);
}

void QuickAccess::AddSourceMenuItem(obs_source_t *source)
{
	_menuSources.push_back(source);
}

void QuickAccess::_ClearMenuSources()
{
	_menuSources.clear();
}

QDialog *QuickAccess::CreateAddSourcePopupMenu()
{
	QDialog *popup = new QDialog(this);
	std::string title = "Add Sources to " + _dock->GetName() + " Dock";
	popup->setWindowTitle(title.c_str());
	QVBoxLayout *layoutV = new QVBoxLayout();
	QHBoxLayout *layoutH = new QHBoxLayout();
	QVBoxLayout *allSourcesLayout = new QVBoxLayout();
	QLabel *allSourcesLabel = new QLabel();
	allSourcesLabel->setText("Available Sources");

	QLineEdit *searchText = new QLineEdit();
	searchText->setPlaceholderText("Search...");

	// MenuSources *should* be empty, but just in case
	_ClearMenuSources();
	obs_enum_sources(AddSourceToWidget, this);
	obs_enum_scenes(AddSourceToWidget, this);
	QListWidget *allSourcesList = new QListWidget(this);

	searchText->connect(
		searchText, &QLineEdit::textChanged,
		[allSourcesList](const QString text) {
			blog(LOG_INFO, "=== Search List Size: %i",
			     allSourcesList->count());
			for (int i = 0; i < allSourcesList->count(); i++) {
				QListWidgetItem *item = allSourcesList->item(i);
				QString wName = item->text();
				item->setHidden(
					text.isEmpty() ||
					!wName.contains(text,
							Qt::CaseInsensitive));
			}
		});

	allSourcesLayout->addWidget(allSourcesLabel);
	allSourcesLayout->addWidget(searchText);
	allSourcesLayout->addWidget(allSourcesList);
	layoutH->addLayout(allSourcesLayout);

	QVBoxLayout *addRemoveButtonsLayout = new QVBoxLayout();
	QPushButton *addButton = new QPushButton();
	addButton->setText("→");
	addButton->setDisabled(true);
	QPushButton *removeButton = new QPushButton();
	removeButton->setText("←");
	removeButton->setDisabled(true);
	addRemoveButtonsLayout->addWidget(addButton);
	addRemoveButtonsLayout->addWidget(removeButton);
	layoutH->addLayout(addRemoveButtonsLayout);

	QLabel *dockSourcesLabel = new QLabel();
	dockSourcesLabel->setText("Sources To Add");
	QVBoxLayout *dockSourcesLayout = new QVBoxLayout();
	QListWidget *dockSourcesList = new QListWidget();
	dockSourcesLayout->addWidget(dockSourcesLabel);
	dockSourcesLayout->addWidget(dockSourcesList);
	layoutH->addLayout(dockSourcesLayout);

	layoutV->addLayout(layoutH);
	QHBoxLayout *buttonBar = new QHBoxLayout();
	QWidget *spacer = new QWidget();
	QPushButton *saveButton = new QPushButton();
	saveButton->setText("Add Sources");
	QPushButton *cancelButton = new QPushButton();
	cancelButton->setText("Cancel");
	buttonBar->addWidget(spacer);
	buttonBar->addWidget(saveButton);
	buttonBar->addWidget(cancelButton);
	layoutV->addLayout(buttonBar);
	popup->setLayout(layoutV);

	auto getItemInsert = [](QListWidget *list, const QString &name) {
		for (int i = 0; i < list->count(); i++) {
			auto item = list->item(i);
			auto cmp =
				item->text().compare(name, Qt::CaseInsensitive);
			if (cmp > 0)
				return i;
			else if (cmp == 0)
				return -1;
		}

		return list->count();
	};

	auto addSource = [getItemInsert](QListWidget *list,
					 obs_source_t *source) {
		const char *name = obs_source_get_name(source);
		const char *type = obs_source_get_unversioned_id(source);
		QString qname = name;
		QListWidgetItem *row = new QListWidgetItem(name);
		QIcon icon;

		if (strcmp(type, "scene") == 0)
			icon = qau->GetSceneIcon();
		else if (strcmp(type, "group") == 0)
			icon = qau->GetGroupIcon();
		else
			icon = qau->GetIconFromType(type);

		row->setIcon(icon);
		auto insertIdx = getItemInsert(list, qname);

		if (insertIdx >= 0)
			list->insertItem(insertIdx, row);
		return true;
	};

	_manualSourceNames.clear();
	for (int i = 0; i < _sourceList->count(); i++) {
		auto item = _sourceList->item(i);
		auto widget = dynamic_cast<QuickAccessItem *>(
			_sourceList->itemWidget(item));
		const char *sourceName = widget->GetSourceName();
		_manualSourceNames.push_back(sourceName);
	}

	for (auto &src : _menuSources) {
		if (std::find(_manualSourceNames.begin(),
			      _manualSourceNames.end(),
			      std::string(obs_source_get_name(src))) ==
		    _manualSourceNames.end()) {
			addSource(allSourcesList, src);
		}
	}
	_ClearMenuSources();

	connect(allSourcesList, &QListWidget::itemSelectionChanged, popup,
		[allSourcesList, addButton, removeButton, dockSourcesList]() {
			auto selectedItem = allSourcesList->currentItem();
			if (selectedItem) {
				addButton->setDisabled(false);
				removeButton->setDisabled(true);
				dockSourcesList->clearSelection();
			} else {
				addButton->setDisabled(true);
			}
		});

	connect(dockSourcesList, &QListWidget::itemSelectionChanged, popup,
		[allSourcesList, addButton, removeButton, dockSourcesList]() {
			auto selectedItem = allSourcesList->currentItem();
			if (selectedItem) {
				addButton->setDisabled(true);
				removeButton->setDisabled(false);
				allSourcesList->clearSelection();
			} else {
				removeButton->setDisabled(true);
			}
		});

	connect(addButton, &QPushButton::released, popup,
		[addButton, allSourcesList, dockSourcesList]() {
			auto selectedItem = allSourcesList->takeItem(
				allSourcesList->currentRow());
			dockSourcesList->addItem(selectedItem);
			allSourcesList->clearSelection();
			addButton->setDisabled(true);
		});

	connect(removeButton, &QPushButton::released, popup,
		[removeButton, allSourcesList, dockSourcesList,
		 getItemInsert]() {
			auto selectedItem = dockSourcesList->takeItem(
				dockSourcesList->currentRow());
			auto insertIdx = getItemInsert(allSourcesList,
						       selectedItem->text());
			allSourcesList->insertItem(insertIdx, selectedItem);
			dockSourcesList->clearSelection();
			removeButton->setDisabled(true);
		});

	connect(cancelButton, &QPushButton::released, popup,
		[popup]() { popup->reject(); });

	connect(saveButton, &QPushButton::released, popup,
		[this, popup, dockSourcesList]() {
			for (int i = 0; i < dockSourcesList->count(); i++) {
				auto item = dockSourcesList->item(i);
				auto sceneName = item->text();
				std::string sceneNameStr =
					sceneName.toStdString();
				AddSourceAtIndex(sceneNameStr.c_str(), i);
			}
			popup->accept();
		});

	return popup;
}

void QuickAccess::AddSourcePopupMenu()
{
	QScopedPointer<QDialog> popup(CreateAddSourcePopupMenu());
	if (popup) {
		popup->setModal(true);
		popup->exec();
	}
}

void QuickAccess::on_actionAddSource_triggered()
{
	AddSourcePopupMenu();
}

void QuickAccess::on_actionRemoveSource_triggered()
{
	auto item = _sourceList->currentItem();
	if (!item)
		return;
	_sourceList->setCurrentItem(nullptr);
	_sourceList->removeItemWidget(item);
	delete item;
}

void QuickAccess::on_actionSourceUp_triggered()
{
	int index = _sourceList->currentRow();
	if (index == -1) {
		return;
	}
	_sourceList->blockSignals(true);

	QListWidgetItem *widgetItem = _sourceList->item(index);
	QuickAccessItem *widget = dynamic_cast<QuickAccessItem *>(
		_sourceList->itemWidget(widgetItem));
	const char *sourceName = widget->GetSourceName();
	QListWidgetItem *toDelete = _sourceList->takeItem(index);
	AddSourceAtIndex(sourceName, index - 1);
	_sourceList->blockSignals(false);
	_sourceList->setCurrentRow(index - 1);
	delete toDelete;
}

void QuickAccess::on_actionSourceDown_triggered()
{
	int index = _sourceList->currentRow();
	if (index == -1) {
		return;
	}
	_sourceList->blockSignals(true);

	QListWidgetItem *widgetItem = _sourceList->item(index);
	QuickAccessItem *widget = dynamic_cast<QuickAccessItem *>(
		_sourceList->itemWidget(widgetItem));
	const char *sourceName = widget->GetSourceName();
	QListWidgetItem *toDelete = _sourceList->takeItem(index);
	AddSourceAtIndex(sourceName, index + 1);
	_sourceList->blockSignals(false);
	_sourceList->setCurrentRow(index + 1);
	delete toDelete;
}

void QuickAccess::updateEnabled()
{
	bool itemActions = _sourceList->currentItem() != nullptr;
	bool firstElement = itemActions && _sourceList->currentRow() == 0;
	bool lastElement = itemActions && _sourceList->currentRow() ==
						  _sourceList->count() - 1;
	_actionRemoveSource->setEnabled(itemActions);
	_actionSourceUp->setEnabled(itemActions && !firstElement);
	_actionSourceDown->setEnabled(itemActions && !lastElement);

	// Refresh Toolbar Styling
	for (auto x : _actionsToolbar->actions()) {
		auto widget = _actionsToolbar->widgetForAction(x);

		if (!widget) {
			continue;
		}
		widget->style()->unpolish(widget);
		widget->style()->polish(widget);
	}
}

void QuickAccess::on_sourceList_itemSelectionChanged()
{
	auto item = _sourceList->currentItem();
	if (item == nullptr) {
		return;
	}
	if (_dock->GetType() == "Manual") {
		updateEnabled();
	}
	if (_dock->ClickableScenes()) {

		QuickAccessItem *widget = dynamic_cast<QuickAccessItem *>(
			_sourceList->itemWidget(item));
		widget->SwitchToScene();
	}
}

void QuickAccess::RemoveNullSources()
{
	if (!_active) {
		return;
	}
	blog(LOG_INFO, "SIZE OF LIST BEFORE: %i", _sourceList->count());
	std::vector<QListWidgetItem *> toDelete;
	for (int i = 0; i < _sourceList->count(); i++) {
		QListWidgetItem *item = _sourceList->item(i);
		auto widget = dynamic_cast<QuickAccessItem *>(
			_sourceList->itemWidget(item));
		if (widget && widget->IsNullSource()) {
			toDelete.push_back(item);
		}
	}
	for (auto &item : toDelete) {
		_sourceList->removeItemWidget(item);
		_sourceList->takeItem(_sourceList->row(item));
		delete item;
	}
	_sourceList->update();
	blog(LOG_INFO, "SIZE OF LIST AFTER: %i", _sourceList->count());
}

QuickAccessSceneItem::QuickAccessSceneItem(QWidget *parent,
					   obs_sceneitem_t *sceneItem)
	: QWidget(parent),
	  _sceneItem(sceneItem)
{
	obs_sceneitem_addref(_sceneItem);
	bool sourceVisible = obs_sceneitem_visible(_sceneItem);
	auto scene = obs_sceneitem_get_scene(sceneItem);
	obs_source_t *sceneSource = obs_scene_get_source(scene);
	const char *name = obs_source_get_name(sceneSource);
	const char *type = obs_source_get_unversioned_id(sceneSource);
	QString qname = name;

	setAttribute(Qt::WA_TranslucentBackground);
	setMouseTracking(true);
	setStyleSheet("background: none");

	QIcon icon;

	if (strcmp(type, "scene") == 0)
		icon = qau->GetSceneIcon();
	else if (strcmp(type, "group") == 0)
		icon = qau->GetGroupIcon();
	else
		icon = qau->GetIconFromType(type);

	QPixmap pixmap = icon.pixmap(QSize(16, 16));
	_iconLabel = new QLabel(this);
	_iconLabel->setPixmap(pixmap);
	_iconLabel->setStyleSheet("background: none");

	_layout = new QHBoxLayout();
	//_layout->setContentsMargins(0, 0, 0, 0);
	_label = new QLabel(this);
	_label->setText(name);
	_label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	_label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	_label->setAttribute(Qt::WA_TranslucentBackground);

	_actionsToolbar = new QToolBar(this);
	_actionsToolbar->setObjectName(QStringLiteral("actionsToolbar"));
	_actionsToolbar->setIconSize(QSize(16, 16));
	_actionsToolbar->setFixedHeight(22);
	_actionsToolbar->setStyleSheet("QToolBar{spacing: 0px;}");
	_actionsToolbar->setFloatable(false);
	_actionsToolbar->setSizePolicy(QSizePolicy::Maximum,
				       QSizePolicy::Maximum);
	_actionsToolbar->setStyleSheet(
		"QToolButton {padding: 0px; margin-left: 2px; margin-right: 2px;}");

	_vis = new QCheckBox();
	_vis->setProperty("visibilityCheckBox", true);
	_vis->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
	_vis->setChecked(sourceVisible);
	_vis->setStyleSheet("background: none");
	_vis->setAccessibleName("Source Visibility");
	_vis->setAccessibleDescription("Source Visibility");

	auto actionTransform = new QAction(this);
	actionTransform->setObjectName(QStringLiteral("actionTransform"));
	actionTransform->setShortcutContext(Qt::WidgetWithChildrenShortcut);
	actionTransform->setProperty("themeID", "cogsIcon");
	actionTransform->setText(QT_UTF8(obs_module_text("Transformation")));
	connect(actionTransform, SIGNAL(triggered()), this,
		SLOT(on_actionTransform_triggered()));
	_actionsToolbar->addAction(actionTransform);

	_layout->addWidget(_iconLabel);
	_layout->setSpacing(10);
	_layout->addWidget(_label);
	_layout->addWidget(_vis);
	_layout->addWidget(_actionsToolbar);
	setLayout(_layout);
	// Themes need the QAction dynamic properties
	for (QAction *x : _actionsToolbar->actions()) {
		QWidget *temp = _actionsToolbar->widgetForAction(x);

		for (QByteArray &y : x->dynamicPropertyNames()) {
			temp->setProperty(y, x->property(y));
		}
	}

	auto setItemVisible = [this](bool val) {
		//obs_scene_t *scene = obs_sceneitem_get_scene(_sceneItem);
		//obs_source_t *scenesource = obs_scene_get_source(scene);
		//int64_t id = obs_sceneitem_get_id(_sceneItem);
		//const char *name = obs_source_get_name(scenesource);
		//const char *uuid = obs_source_get_uuid(scenesource);
		//obs_source_t *source = obs_sceneitem_get_source(_sceneItem);

		//auto undo_redo = [](const std::string &uuid, int64_t id,
		//		    bool val) {
		//	auto s = obs_get_source_by_uuid(uuid.c_str());
		//	obs_scene_t *sc = obs_group_or_scene_from_source(s);
		//	obs_sceneitem_t *si =
		//		obs_scene_find_sceneitem_by_id(sc, id);
		//	if (si)
		//		obs_sceneitem_set_visible(si, val);
		//};

		//QString str = QTStr(val ? "Undo.ShowSceneItem"
		//			: "Undo.HideSceneItem");

		//OBSBasic *main = OBSBasic::Get();
		//main->undo_s.add_action(
		//	str.arg(obs_source_get_name(source), name),
		//	std::bind(undo_redo, std::placeholders::_1, id, !val),
		//	std::bind(undo_redo, std::placeholders::_1, id, val),
		//	uuid, uuid);

		//QSignalBlocker sourcesSignalBlocker(this);
		obs_sceneitem_set_visible(_sceneItem, val);
	};
	connect(_vis, &QAbstractButton::clicked, setItemVisible);
}

QuickAccessSceneItem::~QuickAccessSceneItem()
{
	obs_sceneitem_release(_sceneItem);
}

void QuickAccessSceneItem::on_actionTransform_triggered()
{
	obs_frontend_open_sceneitem_edit_transform(_sceneItem);
}

void QuickAccessSceneItem::mouseReleaseEvent(QMouseEvent *e)
{
	// This is to suppress mouse event propegating down
	// to QMenu.
	UNUSED_PARAMETER(e);
}

bool AddSourceToWidget(void *data, obs_source_t *source)
{
	auto qa = static_cast<QuickAccess *>(data);
	qa->AddSourceMenuItem(source);
	return true;
}
