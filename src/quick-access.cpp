#include "quick-access.hpp"
#include "quick-access-dock.hpp"
#include <obs-module.h>
#include <QToolBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QWidgetAction>
#include <QLineEdit>
#include <obs-frontend-api.h>

#define QT_UTF8(str) QString::fromUtf8(str)
#define QT_TO_UTF8(str) str.toUtf8().constData()

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

	setAttribute(Qt::WA_TranslucentBackground);
	setMouseTracking(true);
	setStyleSheet("background: none");

	_label = new QLabel(this);
	_label->setText(obs_source_get_name(source));
	_label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	_label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	_label->setAttribute(Qt::WA_TranslucentBackground);

	auto layout = new QHBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(_label);

	_actionsToolbar = new QToolBar(this);
	_actionsToolbar->setObjectName(QStringLiteral("actionsToolbar"));
	_actionsToolbar->setIconSize(QSize(20, 20));
	_actionsToolbar->setFixedHeight(22);
	_actionsToolbar->setStyleSheet("QToolBar{spacing:0px}");
	_actionsToolbar->setFloatable(false);
	_actionsToolbar->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);

	if (!obs_source_is_scene(source)) {
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

	// Themes need the QAction dynamic properties
	for (QAction *x : _actionsToolbar->actions()) {
		QWidget *temp = _actionsToolbar->widgetForAction(x);

		for (QByteArray &y : x->dynamicPropertyNames()) {
			temp->setProperty(y, x->property(y));
		}
	}

	layout->addWidget(_actionsToolbar);
}

QuickAccessItem::~QuickAccessItem()
{
	delete _label;
	delete _iconLabel;
	delete _actionsToolbar;
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

QuickAccess::QuickAccess(QWidget *parent, QString name)
  : QWidget(parent)
{
	setObjectName(name);
	auto layout = new QVBoxLayout(this);
	layout->setSpacing(0);
	layout->setContentsMargins(0, 0, 0, 0);

	_sourceList = new QListWidget(this);
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
	blog(LOG_INFO, "Saving Quick Access Dock");
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
	auto item = new QListWidgetItem(_sourceList);
	_sourceList->addItem(item);
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
		QString qname = name;
		QAction *popupItem = new QAction(qname, this);
		connect(popupItem, &QAction::triggered,
			[this, name]() { AddSource(name); });

		//QIcon icon;

		//if (strcmp(type, "scene") == 0)
		//	icon = GetSceneIcon();
		//else
		//	icon = GetSourceIcon(type);

		//popupItem->setIcon(icon);

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
	QListWidgetItem* oldItem = _sourceList->takeItem(index);
	auto item = new QListWidgetItem(_sourceList);
	_sourceList->addItem(item);
	QuickAccessItem* newWidget = new QuickAccessItem(this, widget);
	_sourceList->setItemWidget(item, newWidget);
	_sourceList->setCurrentRow(index-1);
	//item->setSelected(true);
	_sourceList->blockSignals(false);
	delete oldItem;
	delete widget;
}

void QuickAccess::on_actionSourceDown_triggered()
{
	//int index = _sourceList->currentRow();
	//QListWidgetItem *item = _sourceList->takeItem(index);
	//_sourceList->insertItem(index, item);
}

void QuickAccess::on_sourceList_itemSelectionChanged()
{
	bool itemActions = _sourceList->currentItem() != nullptr;
	_actionRemoveSource->setEnabled(itemActions);
	_actionSourceUp->setEnabled(itemActions);
	_actionSourceDown->setEnabled(itemActions);

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

bool AddSourceToWidget(void* data, obs_source_t* source) {
	auto qa = static_cast<QuickAccess *>(data);
	qa->AddSourceMenuItem(source);
	return true;
}
