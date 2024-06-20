#include "quick-access.hpp"
#include "quick-access-dock.hpp"
#include <obs-module.h>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QWidgetAction>
#include <QLineEdit>
#include <obs-frontend-api.h>

#define QT_UTF8(str) QString::fromUtf8(str)
#define QT_TO_UTF8(str) str.toUtf8().constData()

extern QuickAccessDock *dock;

QuickAccessList::QuickAccessList(QWidget* parent)
 : QListWidget(parent)
{
	_qa = dynamic_cast<QuickAccess *>(parent);
}

void QuickAccessList::dropEvent(QDropEvent* event)
{
	QListWidget::dropEvent(event);
	_qa->updateEnabled();
}

QuickAccessItem::QuickAccessItem(QWidget *parent, QuickAccessItem* original)
 : QFrame(parent)
{
	obs_source_t *source = obs_weak_source_get_source(original->_source);
	_source = obs_source_get_weak_source(source);
	obs_source_release(source);
}

QuickAccessItem::QuickAccessItem(QWidget *parent, obs_source_t *source)
  : QFrame(parent)
{
	_source = obs_source_get_weak_source(source);

	const char *id = obs_source_get_id(source);

	setAttribute(Qt::WA_TranslucentBackground);
	setMouseTracking(true);
	setStyleSheet("background: none");

	QIcon icon;

	if(strcmp(id, "scene") == 0)
		icon = dock->GetSceneIcon();
	else if (strcmp(id, "group") == 0)
		icon = dock->GetGroupIcon();
	else
		icon = dock->GetIconFromType(id);
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

	_actionsToolbar = new QToolBar(this);
	_actionsToolbar->setObjectName(QStringLiteral("actionsToolbar"));
	_actionsToolbar->setIconSize(QSize(16, 16));
	_actionsToolbar->setFixedHeight(22);
	_actionsToolbar->setStyleSheet("QToolBar{spacing: 0px;}");
	_actionsToolbar->setFloatable(false);
	_actionsToolbar->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
	_actionsToolbar->setStyleSheet(
		"QToolButton {padding: 0px; margin-left: 2px; margin-right: 2px;}"
	);
	if (obs_source_configurable(source)) {
		auto actionProperties = new QAction(this);
		actionProperties->setObjectName(QStringLiteral("actionProperties"));
		actionProperties->setProperty("themeID", "propertiesIconSmall");
		actionProperties->setText(QT_UTF8(obs_module_text("Properties")));
		connect(actionProperties, SIGNAL(triggered()), this,
			SLOT(on_actionProperties_triggered()));
		_actionsToolbar->addAction(actionProperties);
	}

	auto actionFilters = new QAction(this);
	actionFilters->setObjectName(QStringLiteral("actionFilters"));
	actionFilters->setShortcutContext(Qt::WidgetWithChildrenShortcut);
	actionFilters->setProperty("themeID", "filtersIcon");
	actionFilters->setText(QT_UTF8(obs_module_text("Filters")));
	connect(actionFilters, SIGNAL(triggered()), this,
		SLOT(on_actionFilters_triggered()));
	_actionsToolbar->addAction(actionFilters);

	auto actionScenes = new QAction(this);
	actionScenes->setObjectName(QStringLiteral("actionScenes"));
	actionScenes->setShortcutContext(Qt::WidgetWithChildrenShortcut);
	QIcon sceneIcon;
	sceneIcon = dock->GetSceneIcon();
	actionScenes->setIcon(sceneIcon);
	//actionScenes->setProperty("themeID", "sceneIcon");
	actionScenes->setText(QT_UTF8(obs_module_text("Scenes")));
	connect(actionScenes, SIGNAL(triggered()), this,
		SLOT(on_actionScenes_triggered()));
	_actionsToolbar->addAction(actionScenes);

	// Themes need the QAction dynamic properties
	for (QAction *x : _actionsToolbar->actions()) {
		QWidget *temp = _actionsToolbar->widgetForAction(x);

		for (QByteArray &y : x->dynamicPropertyNames()) {
			temp->setProperty(y, x->property(y));
		}
	}
	layout->addWidget(_actionsToolbar);
	setLayout(layout);
}

const char* QuickAccessItem::GetSourceName()
{
	obs_source_t *source = obs_weak_source_get_source(_source);
	const char* name = obs_source_get_name(source);
	obs_source_release(source);
	return name;
}

QuickAccessItem::~QuickAccessItem()
{
	delete _label;
	delete _iconLabel;
	delete _actionsToolbar;
	_clearSceneItems();
	obs_weak_source_release(_source);
}

void QuickAccessItem::Save(obs_data_t * itemObj)
{
	obs_source_t *source = obs_weak_source_get_source(_source);
	if (!source) {
		return;
	}

	const char * sourceName = obs_source_get_name(source);
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

void QuickAccessItem::_getSceneItems() {
	_clearSceneItems();
	obs_enum_scenes(QuickAccessItem::GetSceneItemsFromScene, this);
}

bool QuickAccessItem::GetSceneItemsFromScene(void* data, obs_source_t* s) {
	obs_scene_t *scene = obs_scene_from_source(s);
	obs_scene_enum_items(scene, QuickAccessItem::AddSceneItems, data);
	return true;
}

bool QuickAccessItem::AddSceneItems(obs_scene_t* scene, obs_sceneitem_t* sceneItem, void* data) {
	auto qai = static_cast<QuickAccessItem*>(data);
	auto source = obs_sceneitem_get_source(sceneItem);
	if (obs_source_is_group(source)) {
		obs_scene_t * group = obs_group_from_source(source);
		obs_scene_enum_items(group, QuickAccessItem::AddSceneItems, data);
	}
	if (obs_weak_source_references_source(qai->_source, source)) {
		obs_sceneitem_addref(sceneItem);
		qai->_sceneItems.push_back(sceneItem);
		const char* sceneName = obs_source_get_name(obs_scene_get_source(scene));
		// blog(LOG_INFO, "Scene Found: %s", sceneName);
	}
	return true;
}

QMenu* QuickAccessItem::_CreateSceneMenu() {
	QMenu *popup = new QMenu("SceneMenu", this);
	auto wa = new QWidgetAction(popup);
	auto t = new QLineEdit;
	t->connect(t, &QLineEdit::textChanged, [popup](const QString text) {
		foreach(auto action, popup->actions())
			action->setVisible(action->text().isEmpty() || action->text().contains(text, Qt::CaseInsensitive));
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

	auto addSource = [this, getActionAfter](QMenu *pop, obs_sceneitem_t *sceneItem) {
		auto scene = obs_sceneitem_get_scene(sceneItem);
		obs_source_t *sceneSource = obs_scene_get_source(scene);
		const char* name = obs_source_get_name(sceneSource);
		const char* type = obs_source_get_unversioned_id(sceneSource);
		QString qname = name;

		QWidgetAction *popupItem = new QWidgetAction(this);
		QWidget *itemWidget = new QuickAccessSceneItem(this, sceneItem);
		popupItem->setDefaultWidget(itemWidget);

	/*	connect(popupItem, &QAction::triggered,
			[this, name]() { AddSource(name); });*/

		//QIcon icon;

		//if (strcmp(type, "scene") == 0)
		//	icon = dock->GetSceneIcon();
		//else if (strcmp(type, "group") == 0)
		//	icon = dock->GetGroupIcon();
		//else
		//	icon = dock->GetIconFromType(type);

		//popupItem->setIcon(icon);

		QAction *after = getActionAfter(pop, qname);
		pop->insertAction(after, popupItem);
		return true;
	};
	for (auto& src : _sceneItems) {
		addSource(popup, src);
	}

	return popup;
}

void QuickAccessItem::_AddScenePopupMenu(const QPoint& pos)
{
	QScopedPointer<QMenu> popup(_CreateSceneMenu());
	if (popup) {
		popup->exec(pos);
	}
}


void QuickAccessItem::_clearSceneItems() {
	for (auto& sceneItem : _sceneItems) {
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

QuickAccess::QuickAccess(QWidget *parent, QString name)
  : QListWidget(parent)
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
	sizePolicy.setHeightForWidth(_sourceList->sizePolicy().hasHeightForWidth());
	_sourceList->setSizePolicy(sizePolicy);
	_sourceList->setContextMenuPolicy(Qt::CustomContextMenu);
	_sourceList->setFrameShape(QFrame::NoFrame);
	_sourceList->setFrameShadow(QFrame::Plain);
	_sourceList->setProperty("showDropIndicator", QVariant(true));
	_sourceList->setDragEnabled(true);
	_sourceList->setDragDropMode(QAbstractItemView::InternalMove);
	_sourceList->setDefaultDropAction(Qt::TargetMoveAction);
	connect(_sourceList, SIGNAL(itemSelectionChanged()), this,
		SLOT(on_sourceList_itemSelectionChanged()));

	layout->addWidget(_sourceList);

	_actionsToolbar = new QToolBar(this);
	_actionsToolbar->setObjectName(QStringLiteral("actionsToolbar"));
	_actionsToolbar->setIconSize(QSize(16, 16));
	_actionsToolbar->setFloatable(false);

	_actionAddSource = new QAction(this);
	_actionAddSource->setObjectName(QStringLiteral("actionAddSource"));
	_actionAddSource->setProperty("themeID", "addIconSmall");
	_actionAddSource->setText(QT_UTF8(obs_module_text("Add")));
	connect(_actionAddSource, SIGNAL(triggered()), this,
		SLOT(on_actionAddSource_triggered()));
	_actionsToolbar->addAction(_actionAddSource);

	_actionRemoveSource = new QAction(this);
	_actionRemoveSource->setObjectName(QStringLiteral("actionRemoveSource"));
	_actionRemoveSource->setShortcutContext(Qt::WidgetWithChildrenShortcut);
	_actionRemoveSource->setProperty("themeID", "removeIconSmall");
	_actionRemoveSource->setText(QT_UTF8(obs_module_text("Remove")));
	connect(_actionRemoveSource, SIGNAL(triggered()), this,
		SLOT(on_actionRemoveSource_triggered()));
	_actionsToolbar->addAction(_actionRemoveSource);

	_actionsToolbar->addSeparator();

	_actionSourceUp = new QAction(this);
	_actionSourceUp->setObjectName(QStringLiteral("actionSourceUp"));
	_actionSourceUp->setProperty("themeID", "upArrowIconSmall");
	_actionSourceUp->setText(QT_UTF8(obs_module_text("MoveUp")));
	connect(_actionSourceUp, SIGNAL(triggered()), this,
		SLOT(on_actionSourceUp_triggered()));
	_actionsToolbar->addAction(_actionSourceUp);

	_actionSourceDown = new QAction(this);
	_actionSourceDown->setObjectName(QStringLiteral("actionSourceDown"));
	_actionSourceDown->setProperty("themeID", "downArrowIconSmall");
	_actionSourceDown->setText(QT_UTF8(obs_module_text("MoveDown")));
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

	layout->addWidget(_actionsToolbar);
	layout->addItem(new QSpacerItem(150, 0, QSizePolicy::Fixed,
					QSizePolicy::Minimum));
}

void QuickAccess::Save(obs_data_t *saveObj) {
	obs_data_set_string(saveObj, "dock_name", "default");
	obs_data_set_string(saveObj, "dock_type", "manual");
	auto itemsArr = obs_data_array_create();
	for (int i=0; i<_sourceList->count(); ++i) {
		auto itemObj = obs_data_create();
		QListWidgetItem* item = _sourceList->item(i);
		QuickAccessItem* widget = dynamic_cast<QuickAccessItem*>(_sourceList->itemWidget(item));
		widget->Save(itemObj);
		obs_data_array_push_back(itemsArr, itemObj);
		obs_data_release(itemObj);
	}
	obs_data_set_array(saveObj, "dock_sources", itemsArr);
	obs_data_array_release(itemsArr);
}

void QuickAccess::Load(obs_data_t *loadObj) {
	obs_data_array_t *items = obs_data_get_array(loadObj, "dock_sources");
	auto numItems = obs_data_array_count(items);
	for (size_t i = 0; i < numItems; i++) {
		auto item = obs_data_array_item(items, i);
		auto sourceName = obs_data_get_string(item, "source_name");
		AddSource(sourceName);
	}
	obs_data_array_release(items);
}

void QuickAccess::AddSource(const char* sourceName)
{
	obs_source_t *source = obs_get_source_by_name(sourceName);
	auto item = new QListWidgetItem();
	_sourceList->addItem(item);
	auto row = new QuickAccessItem(this, source);
	_sourceList->setItemWidget(item, row);
	obs_source_release(source);
}

void QuickAccess::AddSourceAtIndex(const char* sourceName, int index)
{
	obs_source_t *source = obs_get_source_by_name(sourceName);
	auto item = new QListWidgetItem();
	_sourceList->insertItem(index, item);
	auto row = new QuickAccessItem(this, source);
	_sourceList->setItemWidget(item, row);
	obs_source_release(source);
}

void QuickAccess::AddSourceMenuItem(obs_source_t* source) {
	_menuSources.push_back(source);
}

void QuickAccess::_ClearMenuSources() {
	_menuSources.clear();
}

QMenu* QuickAccess::CreateAddSourcePopupMenu()
{
	size_t idx = 0;
	QMenu *popup = new QMenu("Add", this);
	auto wa = new QWidgetAction(popup);
	auto t = new QLineEdit;
	t->connect(t, &QLineEdit::textChanged, [popup](const QString text) {
		foreach(auto action, popup->actions())
			action->setVisible(action->text().isEmpty() || action->text().contains(text, Qt::CaseInsensitive));
	});
	wa->setDefaultWidget(t);
	popup->addAction(wa);
	// MenuSources *should* be empty, but just in case
	_ClearMenuSources();
	obs_enum_sources(AddSourceToWidget, this);
	obs_enum_scenes(AddSourceToWidget, this);
	
	auto getActionAfter = [](QMenu *menu, const QString &name) {
		QList<QAction *> actions = menu->actions();

		for (QAction *menuAction : actions) {
			if (menuAction->text().compare(
				    name, Qt::CaseInsensitive) >= 0)
				return menuAction;
		}

		return (QAction *)nullptr;
	};

	auto addSource = [this, getActionAfter](QMenu *pop, obs_source_t *source) {
		const char* name = obs_source_get_name(source);
		const char* type = obs_source_get_unversioned_id(source);
		QString qname = name;
		QAction *popupItem = new QAction(qname, this);
		connect(popupItem, &QAction::triggered,
			[this, name]() { AddSource(name); });

		QIcon icon;

		if (strcmp(type, "scene") == 0)
			icon = dock->GetSceneIcon();
		else if (strcmp(type, "group") == 0)
			icon = dock->GetGroupIcon();
		else
			icon = dock->GetIconFromType(type);

		popupItem->setIcon(icon);

		QAction *after = getActionAfter(pop, qname);
		pop->insertAction(after, popupItem);
		return true;
	};

	for (auto& src : _menuSources) {
		addSource(popup, src);
	}
	_ClearMenuSources();
	return popup;
}

void QuickAccess::AddSourcePopupMenu(const QPoint& pos)
{
	QScopedPointer<QMenu> popup(CreateAddSourcePopupMenu());
	if (popup) {
		popup->exec(pos);
	}
}

void QuickAccess::on_actionAddSource_triggered()
{
	AddSourcePopupMenu(QCursor::pos());
}

void QuickAccess::on_actionRemoveSource_triggered()
{
	auto item = _sourceList->currentItem();
	auto index = _sourceList->row(item);
	if (!item)
		return;
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

	QListWidgetItem* widgetItem = _sourceList->item(index);
	QuickAccessItem* widget = dynamic_cast<QuickAccessItem*>(_sourceList->itemWidget(widgetItem));
	const char* sourceName = widget->GetSourceName();
	QListWidgetItem* toDelete = _sourceList->takeItem(index);
	AddSourceAtIndex(sourceName, index-1);
	_sourceList->blockSignals(false);
	_sourceList->setCurrentRow(index-1);
	delete toDelete;
}

void QuickAccess::on_actionSourceDown_triggered()
{
	int index = _sourceList->currentRow();
	if (index == -1) {
		return;
	}
	_sourceList->blockSignals(true);

	QListWidgetItem* widgetItem = _sourceList->item(index);
	QuickAccessItem* widget = dynamic_cast<QuickAccessItem*>(_sourceList->itemWidget(widgetItem));
	const char* sourceName = widget->GetSourceName();
	QListWidgetItem* toDelete = _sourceList->takeItem(index);
	AddSourceAtIndex(sourceName, index+1);
	_sourceList->blockSignals(false);
	_sourceList->setCurrentRow(index+1);
	delete toDelete;
}

void QuickAccess::updateEnabled()
{
	bool itemActions = _sourceList->currentItem() != nullptr;
	bool firstElement = itemActions && _sourceList->currentRow() == 0;
	bool lastElement = itemActions && _sourceList->currentRow() == _sourceList->count() - 1;
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
	updateEnabled();
}

QuickAccessSceneItem::QuickAccessSceneItem(QWidget* parent, obs_sceneitem_t* sceneItem)
: QWidget(parent), _sceneItem(sceneItem)
{
	obs_sceneitem_addref(_sceneItem);
	bool sourceVisible = obs_sceneitem_visible(_sceneItem);
	auto scene = obs_sceneitem_get_scene(sceneItem);
	obs_source_t *sceneSource = obs_scene_get_source(scene);
	const char* name = obs_source_get_name(sceneSource);
	const char* type = obs_source_get_unversioned_id(sceneSource);
	QString qname = name;

	setAttribute(Qt::WA_TranslucentBackground);
	setMouseTracking(true);
	setStyleSheet("background: none");

	QIcon icon;

	if(strcmp(type, "scene") == 0)
		icon = dock->GetSceneIcon();
	else if (strcmp(type, "group") == 0)
		icon = dock->GetGroupIcon();
	else
		icon = dock->GetIconFromType(type);

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
	_actionsToolbar->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
	_actionsToolbar->setStyleSheet(
		"QToolButton {padding: 0px; margin-left: 2px; margin-right: 2px;}"
	);

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

QuickAccessSceneItem::~QuickAccessSceneItem() {
	obs_sceneitem_release(_sceneItem);
}

void QuickAccessSceneItem::on_actionTransform_triggered() {
	obs_frontend_open_sceneitem_edit_transform(_sceneItem);
}

void QuickAccessSceneItem::mouseReleaseEvent(QMouseEvent* e) {
	// This is to suppress mouse event propegating down
	// to QMenu.
}

bool AddSourceToWidget(void* data, obs_source_t* source) {
	auto qa = static_cast<QuickAccess *>(data);
	qa->AddSourceMenuItem(source);
	return true;
}
