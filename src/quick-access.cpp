#if !defined(_WIN32) && !defined(__APPLE__) && defined(__x86_64__)
#include <smmintrin.h>
#endif

#include "quick-access.hpp"
#include "quick-access-dock.hpp"
#include "quick-access-utility.hpp"

#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QWidgetAction>
#include <QMessageBox>
#include <QLineEdit>
#include <QApplication>
#include <QThread>
#include <QMetaObject>
#include <QScrollArea>
#include <QDialog>
#include <QMouseEvent>
#include <QInputDialog>
#include <QMainWindow>
#include <QString>
#include <QScrollArea>
#include <QTextStream>
#include <QStyleOption>
#include <QPainter>
#include <QMenu>
#include <QCursor>

#include <algorithm>
#include "version.h"

#define QT_UTF8(str) QString::fromUtf8(str)
#define QT_TO_UTF8(str) str.toUtf8().constData()

#define BROWSER_SOURCE_ID "browser_source"
#define IMAGE_SOURCE_ID "image_source"
#define MEDIA_SOURCE_ID "ffmpeg_source"

extern QuickAccessUtility *qau;

QuickAccessSourceList::QuickAccessSourceList(QWidget *parent,
					     SearchType searchType)
	: QListView(parent),
	  _searchType(searchType),
	  _activeSearch(false),
	  _numActive(0)
{
	_qaParent = dynamic_cast<QuickAccess *>(parent);
	setContextMenuPolicy(Qt::CustomContextMenu);
	setMouseTracking(true);
	setSelectionMode(QAbstractItemView::SingleSelection);
	setSelectionBehavior(QAbstractItemView::SelectItems);
	setProperty("showDropIndicator", QVariant(true));
	setDragEnabled(false);
	setAcceptDrops(false);
	setDragDropMode(QAbstractItemView::InternalMove);
	setDefaultDropAction(Qt::TargetMoveAction);
	connect(this, &QListView::customContextMenuRequested, this,
		[this](const QPoint &pos) {
			UNUSED_PARAMETER(pos);
			_displayContextMenu();
		});
	//setAttribute(Qt::WA_TranslucentBackground);
}

QList<QString> QuickAccessSourceList::_getProjectorMenuMonitorsFormatted()
{
	QList<QString> projectorsFormatted;
	QList<QScreen *> screens = QGuiApplication::screens();
	for (int i = 0; i < screens.size(); i++) {
		QScreen *screen = screens[i];
		QRect screenGeometry = screen->geometry();
		qreal ratio = screen->devicePixelRatio();
		QString name = "";
#if defined(_WIN32) && QT_VERSION < QT_VERSION_CHECK(6, 4, 0)
		QTextStream fullname(&name);
		fullname << GetMonitorName(screen->name());
		fullname << " (";
		fullname << (i + 1);
		fullname << ")";
#elif defined(__APPLE__) || defined(_WIN32)
		name = screen->name();
#else
		name = screen->model().simplified();

		if (name.length() > 1 && name.endsWith("-"))
			name.chop(1);
#endif
		name = name.simplified();

		if (name.length() == 0) {
			name = QString("%1 %2").arg("Display").arg(
				QString::number(i + 1));
		}
		QString str =
			QString("%1: %2x%3 @ %4,%5")
				.arg(name,
				     QString::number(screenGeometry.width() *
						     ratio),
				     QString::number(screenGeometry.height() *
						     ratio),
				     QString::number(screenGeometry.x()),
				     QString::number(screenGeometry.y()));
		projectorsFormatted.push_back(str);
	}
	return projectorsFormatted;
}

void QuickAccessSourceList::_displayContextMenu()
{
	QMenu context(this);

	auto idx = currentIndex();

	auto sourceModel = dynamic_cast<QuickAccessSourceModel *>(model());
	if (!sourceModel) {
		return;
	}

	auto source = sourceModel->item(idx.row());
	if (!source) {
		return;
	}

	auto addCurrent = new QAction("Add to Current Scene", this);
	connect(addCurrent, &QAction::triggered, this, [this, source]() {
		obs_source_t *src = source->get();

		bool studio = obs_frontend_preview_program_mode_active();
		obs_source_t *currentScene =
			studio ? obs_frontend_get_current_preview_scene()
			       : obs_frontend_get_current_scene();

		obs_scene_t *scene = obs_scene_from_source(currentScene);
		obs_scene_add(scene, src);
		obs_source_release(src);
		obs_source_release(currentScene);
		_qaParent->DismissModal();
	});
	context.addAction(addCurrent);

	if (qau->SourceCloneInstalled()) {
		auto addCurrentClone =
			new QAction("Add Clone to Current Scene", this);
		connect(addCurrentClone, &QAction::triggered, this, [this, source]() {
			bool studio =
				obs_frontend_preview_program_mode_active();
			obs_source_t *sceneSrc =
				studio ? obs_frontend_get_current_preview_scene()
				       : obs_frontend_get_current_scene();

			const char *sourceCloneId = "source-clone";
			const char *vId =
				obs_get_latest_input_type_id(sourceCloneId);

			std::string sourceName = source->getName();
			std::string newSourceName = sourceName + " CLONE";

			// Pop open QDialog to ask for new cloned source name.
			bool ok;
			QString text = QInputDialog::getText(
				this, "Name of new source",
				"Clone Name:", QLineEdit::Normal,
				newSourceName.c_str(), &ok);
			if (ok && !text.isEmpty()) {
				newSourceName = text.toStdString();
			} else {
				return;
			}

			obs_source_t *newSource = obs_source_create(
				vId, newSourceName.c_str(), NULL, NULL);
			obs_data_t *settings =
				obs_source_get_settings(newSource);
			obs_data_set_string(settings, "clone",
					    sourceName.c_str());
			obs_source_update(newSource, settings);
			obs_data_release(settings);

			obs_scene_t *scene = obs_scene_from_source(sceneSrc);
			obs_scene_add(scene, newSource);

			obs_source_release(sceneSrc);
			obs_source_release(newSource);
			_qaParent->DismissModal();
		});
		context.addAction(addCurrentClone);
	}

	context.addSeparator();

	if (source->hasProperties()) {
		auto properties = new QAction("Properties", this);
		connect(properties, &QAction::triggered, this,
			[this, source]() { source->openProperties(); });
		context.addAction(properties);
	}

	auto filters = new QAction("Filters", this);
	connect(filters, &QAction::triggered, this,
		[this, source]() { source->openFilters(); });
	context.addAction(filters);

	auto rename = new QAction("Rename", this);
	connect(rename, &QAction::triggered, this, [this, source]() {
		std::string currentName = source->getName();

		bool ok;
		QString text = QInputDialog::getText(
			this, "Rename Source", "New name:", QLineEdit::Normal,
			currentName.c_str(), &ok);
		if (ok && !text.isEmpty()) {
			std::string newSourceName = text.toStdString();
			source->rename(newSourceName);
		} else {
			return;
		}
	});
	context.addAction(rename);

	if (source->hasInteract()) {
		auto interact = new QAction("Interact", this);
		connect(interact, &QAction::triggered, this,
			[this, source]() { source->openInteract(); });
		context.addAction(interact);
	}

	if (source->hasRefresh()) {
		auto refresh = new QAction("Refresh", this);
		connect(refresh, &QAction::triggered, this,
			[this, source]() { source->refreshBrowser(); });
		context.addAction(refresh);
	}

	auto activateState = source ? source->activeState() : "";
	if (activateState != "") {
		auto activate = new QAction(activateState.c_str(), this);
		connect(activate, &QAction::triggered, this,
			[this, source]() { source->toggleActivation(); });
		context.addAction(activate);
	}

	context.addSeparator();

	auto windowedProjector = new QAction("Open Windowed Projector", this);
	connect(windowedProjector, &QAction::triggered, this,
		[this, source]() { source->openWindowedProjector(); });
	context.addAction(windowedProjector);

	auto monitors = _getProjectorMenuMonitorsFormatted();
	auto fsProjector = new QMenu("Full Screen Projector", this);
	for (int i = 0; i < monitors.size(); i++) {
		auto monitor = new QAction(monitors[i], this);
		fsProjector->addAction(monitor);
		connect(monitor, &QAction::triggered, this,
			[this, source, i]() {
				source->openFullScreenProjector(i);
			});
	}
	context.addMenu(fsProjector);

	context.exec(QCursor::pos());
}

QuickAccessSource *QuickAccessSourceList::currentSource()
{
	QModelIndex index = currentIndex();
	auto m = dynamic_cast<QuickAccessSourceModel *>(model());
	return m->item(index.row());
}

void QuickAccessSourceList::mousePressEvent(QMouseEvent *event)
{
	auto idx = indexAt(event->pos());
	if (idx.isValid()) {
		setCurrentIndex(idx);
		_qaParent->ClearSelections(this);
	}
	QListView::mousePressEvent(event);
}

QSize QuickAccessSourceList::sizeHint() const
{
	QMargins margins = contentsMargins();
	int rows = _activeSearch ? _numActive : model()->rowCount();
	if (rows == 0)
		return QSize(width(), 0);
	//int minHeight = minimumHeight();
	int height =
		rows * sizeHintForRow(0) + margins.top() + margins.bottom();
	return QSize(width(), height);
}

void QuickAccessSourceList::search(std::string searchTerm)
{
	auto m = dynamic_cast<QuickAccessSourceModel *>(model());
	if (!m) {
		return;
	}
	int rows = m->rowCount();
	if (searchTerm == "") {
		_activeSearch = false;
		for (int i = 0; i < rows; i++) {
			setRowHidden(i, false);
		}
		_numActive = rows;
	} else {
		_activeSearch = true;
		_numActive = 0;
		setUpdatesEnabled(false);
		for (int i = 0; i < rows; i++) {
			auto source = m->item(i);
			bool match = source->hasMatch(searchTerm, _searchType);
			if (match) {
				_numActive++;
			}
			setRowHidden(i, !(match));
		}
		setUpdatesEnabled(true);
	}
	updateGeometry();
}

DockMessage::DockMessage(QWidget *parent, std::string messageText,
			 std::string iconPath)
	: QWidget(parent)
{
	QLabel *message = new QLabel(this);
	message->setText(messageText.c_str());
	message->setWordWrap(true);
	message->setAlignment(Qt::AlignCenter);

	QVBoxLayout *messageLayout = new QVBoxLayout(this);
	QWidget *spacerTop = new QWidget(this);
	QWidget *spacerBot = new QWidget(this);
	spacerTop->setSizePolicy(QSizePolicy::Preferred,
				 QSizePolicy::Expanding);
	spacerBot->setSizePolicy(QSizePolicy::Preferred,
				 QSizePolicy::Expanding);
	messageLayout->addWidget(spacerTop);
	if (iconPath != "") {
		QPixmap magImgPixmap =
			QIcon(iconPath.c_str()).pixmap(QSize(48, 48));
		QLabel *messageIcon = new QLabel(this);
		messageIcon->setPixmap(magImgPixmap);
		messageIcon->setAlignment(Qt::AlignCenter);
		messageLayout->addWidget(messageIcon);
	}

	messageLayout->addWidget(message);
	messageLayout->addWidget(spacerBot);
}

QuickAccess::QuickAccess(QWidget *parent, QuickAccessDock *dock, QString name)
	: QWidget(parent),
	  _dock(dock)
{
	std::string imageBaseDir =
		obs_get_module_data_path(obs_current_module());
	imageBaseDir += "/images/";
	setObjectName(name);
	auto layout = new QVBoxLayout(this);
	setAttribute(Qt::WA_TranslucentBackground);
	layout->setSpacing(0);
	layout->setContentsMargins(0, 0, 0, 0);
	_contents = nullptr;
	_listsContainer = new QScrollArea(this);
	_createListContainer();

	std::string emptySearchText =
		"Type in the search bar above to search for either a source name, source type, filter name, filter type, file path, or URL.";
	std::string searchImgPath =
		imageBaseDir + "magnifying-glass-solid-white.svg";
	_emptySearch = new DockMessage(this, emptySearchText, searchImgPath);

	std::string emptyManualText =
		"You don't have any sources in your dock. Click the + below to add one.";
	_emptyManual = new DockMessage(this, emptyManualText);

	std::string emptyDynamicText =
		"There are no sources in the current scene. Add one to the scene and it will appear here.";
	_emptyDynamic = new DockMessage(this, emptyDynamicText);

	std::string noSearchResultsText =
		"No sources found matching your search terms.";
	_noSearchResults =
		new DockMessage(this, noSearchResultsText, searchImgPath);

	auto sourceCount = _dock->SourceCount();

	_contents = new QStackedWidget(this);
	_contents->addWidget(_listsContainer);  //0
	_contents->addWidget(_emptySearch);     //1
	_contents->addWidget(_emptyManual);     //2
	_contents->addWidget(_emptyDynamic);    //3
	_contents->addWidget(_noSearchResults); //4
	int index = 1;
	if (_dock->GetType() == "Source Search") {
		index = 1;
	} else if (_dock->GetType() == "Manual") {
		index = sourceCount > 0 ? 0 : 2;
	} else if (_dock->GetType() == "Dynamic") {
		index = sourceCount > 0 ? 0 : 3;
	}
	_contents->setCurrentIndex(index);

	std::string dockType = _dock->GetType();

	if (dockType == "Source Search" /* || dockType == "Dynamic"*/) {
		_searchText = new QLineEdit;
		_searchText->setPlaceholderText("Search...");
		_searchText->setClearButtonEnabled(true);
		_searchText->setFocusPolicy(Qt::StrongFocus);
		_searchText->connect(
			_searchText, &QLineEdit::textChanged,
			[this, dockType](const QString text) {
				if (dockType == "Source Search") {
					_noSearch = text.size() == 0;
					int totalMatches = 0;
					for (auto &qa : _qaLists) {
						qa.listView->search(
							text.toStdString());
						int numMatches =
							qa.listView
								->visibleCount();
						totalMatches += numMatches;
						qa.listView->setHidden(
							numMatches == 0);
						qa.headerLabel->setHidden(
							numMatches == 0);
					}
					if (_noSearch) {
						_contents->setCurrentIndex(1);
					} else if (totalMatches == 0) {
						_contents->setCurrentIndex(4);
					} else {
						_contents->setCurrentIndex(0);
					}
				}
			});
		layout->addWidget(_searchText);
	}

	layout->addWidget(_contents);
	_actionsToolbar = new QToolBar(this);
	_actionsToolbar->setObjectName(QStringLiteral("actionsToolbar"));
	_actionsToolbar->setIconSize(QSize(16, 16));
	_actionsToolbar->setFloatable(false);

	if (_dock->GetType() == "Manual") {
		_actionAddSource = new QAction(this);
		_actionAddSource->setObjectName(
			QStringLiteral("actionAddSource"));
		_actionAddSource->setProperty("themeID", "addIconSmall");
		_actionAddSource->setProperty("class", "icon-plus");
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
		_actionRemoveSource->setProperty("class", "icon-minus");
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
		_actionSourceUp->setProperty("class", "icon-up");
		_actionSourceUp->setText(QT_UTF8(obs_module_text("MoveUp")));
		connect(_actionSourceUp, SIGNAL(triggered()), this,
			SLOT(on_actionSourceUp_triggered()));
		_actionsToolbar->addAction(_actionSourceUp);

		_actionSourceDown = new QAction(this);
		_actionSourceDown->setObjectName(
			QStringLiteral("actionSourceDown"));
		_actionSourceDown->setProperty("themeID", "downArrowIconSmall");
		_actionSourceDown->setProperty("class", "icon-down");
		_actionSourceDown->setText(
			QT_UTF8(obs_module_text("MoveDown")));
		connect(_actionSourceDown, SIGNAL(triggered()), this,
			SLOT(on_actionSourceDown_triggered()));
		_actionsToolbar->addAction(_actionSourceDown);

		_actionRemoveSource->setEnabled(false);
		_actionSourceUp->setEnabled(false);
		_actionSourceDown->setEnabled(false);
	}

	QWidget *spacer = new QWidget();
	spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	_actionsToolbar->addWidget(spacer);

	_actionDockProperties = new QAction(this);
	_actionDockProperties->setObjectName(QStringLiteral("actionSourceUp"));
	_actionDockProperties->setProperty("themeID", "propertiesIconSmall");
	_actionDockProperties->setProperty("class", "icon-gear");
	_actionDockProperties->setText(QT_UTF8(obs_module_text("Dock Props")));
	connect(_actionDockProperties, SIGNAL(triggered()), this,
		SLOT(on_actionDockProperties_triggered()));
	_actionsToolbar->addAction(_actionDockProperties);

	// Themes need the QAction dynamic properties
	for (QAction *x : _actionsToolbar->actions()) {
		QWidget *temp = _actionsToolbar->widgetForAction(x);

		for (QByteArray &y : x->dynamicPropertyNames()) {
			temp->setProperty(y, x->property(y));
		}
	}

	layout->addWidget(_actionsToolbar);
	layout->addItem(new QSpacerItem(150, 0, QSizePolicy::Fixed,
					QSizePolicy::Minimum));
}

QuickAccess::~QuickAccess()
{
	if (_current) {
		obs_weak_source_release(_current);
	}
	_clearSceneItems();
	source_signal_handler = nullptr;
}

void QuickAccess::_createListContainer()
{
	_qaLists.clear();
	_sourceModels.clear();
	for (auto &dg : _dock->DisplayGroups()) {
		_sourceModels.emplace_back(
			std::make_unique<QuickAccessSourceModel>());
		auto model = _sourceModels[_sourceModels.size() - 1].get();
		_qaLists.push_back(
			{dg.name, nullptr,
			 new QuickAccessSourceList(this, dg.searchType),
			 model});
		model->setSources(&dg.sources2);
	}

	auto widget = _listsContainer->widget();
	if (widget) {
		delete widget;
	}
	_listsContainer->setStyleSheet("QWidget {background: transparent;}");
	_listsContainer->setContentsMargins(0, 0, 0, 0);
	_listsContainer->setWidgetResizable(true);
	_listsContainer->setAttribute(Qt::WA_TranslucentBackground);
	auto listsWidget = new QWidget(this);
	listsWidget->setContentsMargins(0, 0, 0, 0);
	auto lcLayout = new QVBoxLayout();
	lcLayout->setContentsMargins(2, 2, 2, 2);
	lcLayout->setSpacing(0);
	int i = 0;
	for (auto &qa : _qaLists) {
		auto header = new QLabel(this);
		header->setContentsMargins(0, 10, 0, 4);
		header->setAlignment(Qt::AlignHCenter);
		header->setText(qa.header.c_str());
		header->setStyleSheet("QLabel { font-size: 18px }");
		header->setSizePolicy(QSizePolicy::Preferred,
				      QSizePolicy::Fixed);
		header->setHidden(qa.header == "Manual");
		qa.headerLabel = header;
		lcLayout->addWidget(header);
		qa.listView->setModel(qa.model);
		qa.listView->setSizePolicy(QSizePolicy::Preferred,
					   QSizePolicy::Fixed);
		qa.listView->setStyleSheet("border: none");
		qa.listView->setContentsMargins(0, 0, 0, 10);
		qa.listView->setMinimumHeight(0);
		QuickAccessSourceDelegate *itemDelegate =
			new QuickAccessSourceDelegate(qa.listView, _dock);
		qa.listView->setItemDelegate(itemDelegate);
		connect(itemDelegate,
			&QuickAccessSourceDelegate::openPropertiesClicked, this,
			[this](const QModelIndex &index) {
				const QuickAccessSourceModel *model =
					dynamic_cast<
						const QuickAccessSourceModel *>(
						index.model());
				QuickAccessSource *source =
					model->item(index.row());
				source->openProperties();
			});
		connect(itemDelegate,
			&QuickAccessSourceDelegate::openFiltersClicked, this,
			[this, qa](const QModelIndex &index) {
				const QuickAccessSourceModel *model =
					dynamic_cast<
						const QuickAccessSourceModel *>(
						index.model());
				QuickAccessSource *source =
					model->item(index.row());
				source->openFilters();
			});
		connect(itemDelegate,
			&QuickAccessSourceDelegate::openParentScenesClicked,
			this, [this, qa](const QModelIndex &index) {
				const QuickAccessSourceModel *model =
					dynamic_cast<
						const QuickAccessSourceModel *>(
						index.model());
				QuickAccessSource *source =
					model->item(index.row());
				_currentSource = source;
				_getSceneItems();
				auto pos = QCursor::pos();
				QScopedPointer<QMenu> popup(
					_CreateParentSceneMenu());
				if (popup) {
					popup->exec(pos);
				}
			});
		connect(itemDelegate, &QuickAccessSourceDelegate::itemSelected,
			this, [this, i](const QModelIndex &index) {
				if (_dock->GetType() != "Manual") {
					return;
				}
				_activeIndex = i;
				size_t numRows = index.model()->rowCount();
				size_t currentRow = index.row();
				_actionRemoveSource->setEnabled(true);
				_actionSourceUp->setEnabled(currentRow > 0);
				_actionSourceDown->setEnabled(currentRow <
							      numRows - 1);

				for (auto x : _actionsToolbar->actions()) {
					auto wdgt =
						_actionsToolbar->widgetForAction(
							x);

					if (!wdgt) {
						continue;
					}
					wdgt->style()->unpolish(wdgt);
					wdgt->style()->polish(wdgt);
				}
			});
		connect(itemDelegate, &QuickAccessSourceDelegate::activateScene,
			this, [this](const QModelIndex &index) {
				const QuickAccessSourceModel *model =
					dynamic_cast<
						const QuickAccessSourceModel *>(
						index.model());
				QuickAccessSource *source =
					model->item(index.row());
				source->activateScene();
			});
		lcLayout->addWidget(qa.listView);
		++i;
	}

	QWidget *lSpacer = new QWidget(this);
	lSpacer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
	lcLayout->addWidget(lSpacer);
	listsWidget->setLayout(lcLayout);
	_listsContainer->setWidget(listsWidget);
}

void QuickAccess::paintEvent(QPaintEvent *)
{
	QStyleOption opt;
	opt.initFrom(this);
	QPainter p(this);
	style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

void QuickAccess::SearchFocus()
{
	if (_searchText) {
		_searchText->setFocus();
		_searchText->selectAll();
	}
}

void QuickAccess::ClearSelections(QuickAccessSourceList *skip)
{
	for (auto &qa : _qaLists) {
		if (qa.listView != skip) {
			qa.listView->clearSelection();
		}
	}
}

void QuickAccess::_clearSceneItems()
{
	for (auto &sceneItem : _sceneItems) {
		obs_sceneitem_release(sceneItem);
	}
	_sceneItems.clear();
}

void QuickAccess::_getSceneItems()
{
	_clearSceneItems();
	obs_enum_scenes(QuickAccess::GetSceneItemsFromScene, this);
}

bool QuickAccess::GetSceneItemsFromScene(void *data, obs_source_t *s)
{
	obs_scene_t *scene = obs_scene_from_source(s);
	obs_scene_enum_items(scene, QuickAccess::AddSceneItems, data);
	return true;
}

bool QuickAccess::AddSceneItems(obs_scene_t *scene, obs_sceneitem_t *sceneItem,
				void *data)
{
	UNUSED_PARAMETER(scene);

	auto qa = static_cast<QuickAccess *>(data);
	auto source = obs_sceneitem_get_source(sceneItem);
	if (obs_source_is_group(source)) {
		obs_scene_t *group = obs_group_from_source(source);
		obs_scene_enum_items(group, QuickAccess::AddSceneItems, data);
	}
	if (qa->_currentSource->isSource(source)) {
		obs_sceneitem_addref(sceneItem);
		qa->_sceneItems.push_back(sceneItem);
	}
	return true;
}

QMenu *QuickAccess::_CreateParentSceneMenu()
{
	QMenu *popup = new QMenu("SceneMenu", this);
	bool dark = obs_frontend_is_theme_dark();
	auto title = new QWidgetAction(popup);
	auto titleText = new QLabel("Parent Scenes", this);
	titleText->setStyleSheet("QLabel { font-size: 12pt; margin: 10px; }");
	titleText->setAlignment(Qt::AlignCenter);
	title->setDefaultWidget(titleText);
	popup->addAction(title);
	popup->addSeparator();
	if (_sceneItems.size() == 0) {
		auto noItems = new QWidgetAction(popup);
		auto noItemsText = new QLabel("No Parent Scenes", this);
		noItemsText->setStyleSheet("QLabel {margin: 30px;}");
		noItemsText->setAlignment(Qt::AlignCenter);
		noItems->setDefaultWidget(noItemsText);
		popup->addAction(noItems);
	}
	if (_sceneItems.size() > 1) {
		auto wa = new QWidgetAction(popup);
		auto t = new QLineEdit;
		t->setPlaceholderText("Search...");
		t->connect(
			t, &QLineEdit::textChanged,
			[popup](const QString text) {
				foreach(auto action, popup->actions())
					action->setVisible(
						action->text().isEmpty() ||
						action->text().contains(
							text,
							Qt::CaseInsensitive));
			});
		wa->setDefaultWidget(t);
		popup->addAction(wa);
	}
	popup->setStyleSheet("QMenu { menu-scrollable: 1; }");
	auto getActionAfter = [](QMenu *menu, const QString &name) {
		QList<QAction *> actions = menu->actions();

		for (QAction *menuAction : actions) {
			if (menuAction->text().compare(
				    name, Qt::CaseInsensitive) >= 0)
				return menuAction;
		}

		return (QAction *)nullptr;
	};

	auto addSource = [this, getActionAfter,
			  dark](QMenu *pop, obs_sceneitem_t *sceneItem) {
		auto scene = obs_sceneitem_get_scene(sceneItem);
		obs_source_t *sceneSource = obs_scene_get_source(scene);
		const char *name = obs_source_get_name(sceneSource);
		QString qname = name;

		QWidgetAction *popupItem = new QWidgetAction(this);
		QWidget *itemWidget = new QuickAccessSceneItem(this, sceneItem);
		if (dark) {
			itemWidget->setStyleSheet(
				"QuickAccessSceneItem:hover {background: #333333;}");
		} else {
			itemWidget->setStyleSheet(
				"QuickAccessSceneItem:hover {background: #aaaaaa;}");
		}

		popupItem->setDefaultWidget(itemWidget);
		popupItem->setParent(pop);

		connect(popupItem, &QWidgetAction::triggered, this,
			[this, name]() {
				obs_source_t *sceneClicked =
					obs_get_source_by_name(name);
				if (obs_frontend_preview_program_mode_active()) {
					obs_frontend_set_current_preview_scene(
						sceneClicked);
				} else {
					obs_frontend_set_current_scene(
						sceneClicked);
				}

				obs_source_release(sceneClicked);
			});

		QAction *after = getActionAfter(pop, qname);
		pop->insertAction(after, popupItem);
		return true;
	};
	for (auto &src : _sceneItems) {
		addSource(popup, src);
	}

	connect(popup, &QMenu::hovered, this, [popup](QAction *act) {
		QList<QWidgetAction *> menuActions =
			popup->findChildren<QWidgetAction *>();
		for (auto menuAction : menuActions) {
			auto widget = static_cast<QuickAccessSceneItem *>(
				menuAction->defaultWidget());
			widget->setHighlight(menuAction == act);
		}
	});

	return popup;
}

void QuickAccess::Save(obs_data_t *saveObj)
{

	if (_dock->GetType() == "Manual") {
		auto itemsArr = obs_data_array_create();
		auto &displayGroups = _dock->DisplayGroups();
		if (displayGroups.size() == 0) {
			return;
		}
		auto &dg = displayGroups[0];
		for (auto &source : dg.sources2) {
			auto itemObj = obs_data_create();
			source->save(itemObj);
			obs_data_array_push_back(itemsArr, itemObj);
			obs_data_release(itemObj);
		}
		obs_data_set_array(saveObj, "dock_sources", itemsArr);
		obs_data_array_release(itemsArr);
	}
}

void QuickAccess::Redraw()
{
	for (auto &qa : _qaLists) {
		qa.listView->repaint();
	}
}

void QuickAccess::Load()
{
	if (_dock->GetType() == "Dynamic") {
		_createListContainer();
		auto sourceCount = _dock->SourceCount();
		int index = sourceCount > 0 ? 0 : 3;
		_contents->setCurrentIndex(index);
	}
}

void QuickAccess::UpdateVisibility()
{
	bool active = false;
	for (auto &group : _dock->DisplayGroups()) {
		group.headerItem->setHidden(!group.headerVisible);
		if (group.headerVisible) {
			active = true;
		}
		for (auto &source : group.sources) {
			source.listItem->setHidden(!source.visible);
		}
	}
	if (_noSearch) {
		_contents->setCurrentIndex(1);
		return;
	}
	_contents->setCurrentIndex(active ? 0 : 4);
}

bool QuickAccess::AddSourceName(void *data, obs_source_t *source)
{
	auto qa = static_cast<QuickAccess *>(data);
	std::string sourceName = obs_source_get_name(source);
	qa->_allSourceNames.push_back(sourceName);
	return true;
}

void QuickAccess::AddSource(QuickAccessSource *source, std::string groupName)
{
	bool update = false;
	auto it = std::find_if(
		_qaLists.begin(), _qaLists.end(),
		[groupName](QuickAccessSourceListView const &list) {
			return list.header == groupName;
		});
	if (it != _qaLists.end()) {
		// Add the new thing.
		it->model->addSource(source);
		it->listView->setVisible(true);
		it->listView->updateGeometry();
		update = true;
	}
	if (_dock->GetType() == "Manual" && update) {
		_contents->setCurrentIndex(0);
	}
}

void QuickAccess::RemoveSource(QuickAccessSource *source, std::string groupName)
{
	auto it = std::find_if(
		_qaLists.begin(), _qaLists.end(),
		[groupName](QuickAccessSourceListView const &list) {
			return list.header == groupName;
		});
	if (it != _qaLists.end()) {
		it->model->removeSource(source);
		it->listView->updateGeometry();
	}
	auto sourceCount = _dock->SourceCount();
	if (_dock->GetType() == "Manual" && sourceCount == 0) {
		_contents->setCurrentIndex(2);
	}
}

void QuickAccess::SetItemsButtonVisibility()
{
	for (auto &qa : _qaLists) {
		qa.listView->repaint();
	}
}

void QuickAccess::AddSourceMenuItem(obs_source_t *source)
{
	_menuSources.push_back(source);
}

void QuickAccess::_ClearMenuSources()
{
	_menuSources.clear();
}

void QuickAccess::DismissModal()
{
	if (_dock)
		_dock->DismissModal();
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
	// TODO: Get list of current manual source names
	for (auto &source : _dock->DisplayGroups()[0].sources2) {
		_manualSourceNames.push_back(source->getName());
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

	/*
	 *  Handle selection changes in lists.
	 */
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

	/*
	 *  Handle Add/Remove button clicks.
	 */
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

	/*
	 *  Handle Double-clicking of items in lists.
	 */
	connect(allSourcesList, &QListWidget::itemDoubleClicked, popup,
		[dockSourcesList, allSourcesList,
		 addButton](QListWidgetItem *item) {
			allSourcesList->takeItem(allSourcesList->row(item));
			dockSourcesList->addItem(item);
			allSourcesList->clearSelection();
			addButton->setDisabled(true);
		});

	connect(dockSourcesList, &QListWidget::itemDoubleClicked, popup,
		[dockSourcesList, allSourcesList, removeButton,
		 getItemInsert](QListWidgetItem *item) {
			dockSourcesList->takeItem(dockSourcesList->row(item));
			auto insertIdx =
				getItemInsert(allSourcesList, item->text());
			allSourcesList->insertItem(insertIdx, item);
			dockSourcesList->clearSelection();
			removeButton->setDisabled(true);
		});

	/*
	 *  Handle Cancel/Save button clicks.
	 */
	connect(cancelButton, &QPushButton::released, popup,
		[popup]() { popup->reject(); });

	connect(saveButton, &QPushButton::released, popup,
		[this, popup, dockSourcesList]() {
			for (int i = 0; i < dockSourcesList->count(); i++) {
				auto item = dockSourcesList->item(i);
				auto sceneName = item->text();
				std::string sceneNameStr =
					sceneName.toStdString();
				obs_source_t *source = obs_get_source_by_name(
					sceneNameStr.c_str());
				std::string uuid = obs_source_get_uuid(source);
				auto qaSource = qau->GetSource(uuid);
				obs_source_release(source);
				_dock->AddSource(qaSource, i);
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
	auto &qa = _qaLists[_activeIndex];

	auto source = qa.listView->currentSource();

	if (!source)
		return;

	std::string sourceName = source->getName();
	std::string dockName = _dock->GetName();
	std::string message = "Are you sure you want to remove " + sourceName +
			      " from " + dockName + "?";
	QMessageBox confirm(this);
	confirm.setText(message.c_str());
	confirm.setIcon(QMessageBox::Question);
	confirm.setWindowTitle(message.c_str());
	confirm.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
	int ret = confirm.exec();
	if (ret == QMessageBox::Yes) {
		_dock->RemoveSource(source);
	}
}

void QuickAccess::on_actionDockProperties_triggered()
{
	const auto main_window =
		static_cast<QMainWindow *>(obs_frontend_get_main_window());
	UpdateDockDialog *dockDialog = new UpdateDockDialog(_dock, main_window);
	dockDialog->show();
}

void QuickAccess::on_actionSourceUp_triggered()
{
	if (_qaLists.size() == 0) {
		return;
	}
	QuickAccessSourceListView qa = _qaLists[0];
	auto model = qa.model;
	auto index = qa.listView->currentIndex();
	auto newRow = index.row() - 1;
	model->swapRows(index.row(), newRow);
	auto newIdx = model->index(newRow);
	qa.listView->setCurrentIndex(newIdx);
	_actionSourceUp->setEnabled(newIdx.row() != 0);
	_actionSourceDown->setEnabled(newIdx.row() < model->rowCount() - 1);
	for (auto x : _actionsToolbar->actions()) {
		auto widget = _actionsToolbar->widgetForAction(x);

		if (!widget) {
			continue;
		}
		widget->style()->unpolish(widget);
		widget->style()->polish(widget);
	}
}

void QuickAccess::on_actionSourceDown_triggered()
{
	if (_qaLists.size() == 0) {
		return;
	}
	QuickAccessSourceListView qa = _qaLists[0];
	auto model = qa.model;
	auto index = qa.listView->currentIndex();
	auto newRow = index.row() + 1;
	model->swapRows(index.row(), newRow);
	auto newIdx = model->index(newRow);
	qa.listView->setCurrentIndex(newIdx);
	_actionSourceUp->setEnabled(newIdx.row() != 0);
	_actionSourceDown->setEnabled(newIdx.row() < model->rowCount() - 1);
	for (auto x : _actionsToolbar->actions()) {
		auto widget = _actionsToolbar->widgetForAction(x);

		if (!widget) {
			continue;
		}
		widget->style()->unpolish(widget);
		widget->style()->polish(widget);
	}
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
	//setStyleSheet("background: none");

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
	_actionsToolbar->setFloatable(false);
	_actionsToolbar->setSizePolicy(QSizePolicy::Maximum,
				       QSizePolicy::Maximum);
	_actionsToolbar->setStyleSheet(
		"QToolBar{spacing: 0px;} QToolButton {margin-left: 0px; margin-right: 0px; background: none; border: none;}");

	_vis = new QCheckBox();
	_vis->setProperty("visibilityCheckBox", true);
	_vis->setProperty("class", "indicator-visibility");
	_vis->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
	_vis->setChecked(sourceVisible);
	_vis->setStyleSheet("background: none");
	_vis->setAccessibleName("Source Visibility");
	_vis->setAccessibleDescription("Source Visibility");
	_vis->setToolTip("Source Visibility");

	auto actionTransform = new QAction(this);
	actionTransform->setObjectName(QStringLiteral("actionTransform"));
	actionTransform->setShortcutContext(Qt::WidgetWithChildrenShortcut);
	actionTransform->setProperty("themeID", "cogsIcon");
	actionTransform->setProperty("class", "icon-gear");
	actionTransform->setText(QT_UTF8(obs_module_text("Transformation")));
	connect(actionTransform, SIGNAL(triggered()), this,
		SLOT(on_actionTransform_triggered()));
	_actionsToolbar->addAction(actionTransform);
	//_actionsToolbar->setStyleSheet("QToolButton {background: none; border: none;} ");

	_layout->addWidget(_iconLabel);
	_layout->setSpacing(4);
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

void QuickAccessSceneItem::setHighlight(bool h)
{
	setBackgroundRole(h ? QPalette::Highlight : QPalette::Window);
	setAutoFillBackground(h);
}

void QuickAccessSceneItem::paintEvent(QPaintEvent *)
{
	QStyleOption opt;
	opt.initFrom(this);
	QPainter p(this);
	style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

UpdateDockDialog::UpdateDockDialog(QuickAccessDock *dock, QWidget *parent)
	: QDialog(parent),
	  _dock(dock)
{
	setWindowModality(Qt::WindowModal);
	setAttribute(Qt::WA_DeleteOnClose, true);
	setWindowTitle(QString("Add Quick Access Dock"));
	setMinimumWidth(400);
	setMinimumHeight(300);
	int labelWidth = 120;
	_layout = new QVBoxLayout();

	// Form layout
	_layout2 = new QVBoxLayout();

	auto layoutName = new QHBoxLayout();

	auto inputLabel = new QLabel(this);
	inputLabel->setText("Dock Name:");
	inputLabel->setFixedWidth(labelWidth);

	_inputName = new QLineEdit(this);
	_inputName->setPlaceholderText("Dock Name");
	_inputName->setText(_dock->GetName().c_str());
	_inputName->connect(_inputName, &QLineEdit::textChanged,
			    [this](const QString text) {
				    _buttonBox->button(QDialogButtonBox::Ok)
					    ->setEnabled(text.length() > 0);
			    });

	layoutName->addWidget(inputLabel);
	layoutName->addWidget(_inputName);

	_layout2->addItem(layoutName);

	auto optionsLabel = new QLabel(this);
	optionsLabel->setText("Dock Options:");
	_layout2->addWidget(optionsLabel);

	_showProperties = new QCheckBox(this);
	_showProperties->setText("Show Properties");
	_showProperties->setChecked(_dock->ShowProperties());
	_layout2->addWidget(_showProperties);

	_showFilters = new QCheckBox(this);
	_showFilters->setText("Show Filters");
	_showFilters->setChecked(_dock->ShowFilters());
	_layout2->addWidget(_showFilters);

	_showScenes = new QCheckBox(this);
	_showScenes->setText("Show Parent Scenes");
	_showScenes->setChecked(_dock->ShowScenes());
	_layout2->addWidget(_showScenes);

	_clickThroughScenes = new QCheckBox(this);
	_clickThroughScenes->setText("Clickable Scenes");
	_clickThroughScenes->setChecked(_dock->ClickableScenes());
	_layout2->addWidget(_clickThroughScenes);

	_buttonBox = new QDialogButtonBox(this);
	_buttonBox->setStandardButtons(QDialogButtonBox::Cancel |
				       QDialogButtonBox::Ok);

	connect(_buttonBox, SIGNAL(accepted()), this, SLOT(on_update_dock()));
	connect(_buttonBox, SIGNAL(rejected()), this, SLOT(on_cancel()));

	_layout->addItem(_layout2);

	// Spacer to push buttons to bottom of widget
	QWidget *spacer = new QWidget(this);
	spacer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
	spacer->setVisible(true);
	_layout->addWidget(spacer);

	_layout->addWidget(_buttonBox);

	setLayout(_layout);
}

void UpdateDockDialog::on_update_dock()
{
	_dock->SetName(QT_TO_UTF8(_inputName->text()));
	_dock->SetProperties(_showProperties->isChecked());
	_dock->SetFilters(_showFilters->isChecked());
	_dock->SetScenes(_showScenes->isChecked());
	_dock->SetClickableScenes(_clickThroughScenes->isChecked());
	_dock->SetItemsButtonVisibility();
	done(DialogCode::Accepted);
}

void UpdateDockDialog::on_cancel()
{
	done(DialogCode::Rejected);
}

bool AddSourceToWidget(void *data, obs_source_t *source)
{
	auto qa = static_cast<QuickAccess *>(data);
	qa->AddSourceMenuItem(source);
	return true;
}

void EnumerateFilters(obs_source_t *parentScene, obs_source_t *filter,
		      void *param)
{
	UNUSED_PARAMETER(parentScene);
	std::vector<obs_source_t *> *filters =
		static_cast<std::vector<obs_source_t *> *>(param);
	filters->push_back(filter);
}
