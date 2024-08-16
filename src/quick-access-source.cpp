#include "quick-access-source.hpp"
#include "quick-access-utility.hpp"
#include "quick-access-dock.hpp"
#include "version.h"

#include <algorithm>
#include <QApplication>
#include <QEvent>
#include <QMouseEvent>
#include <QStringListModel>

#define BROWSER_SOURCE_ID "browser_source"
#define IMAGE_SOURCE_ID "image_source"
#define MEDIA_SOURCE_ID "ffmpeg_source"

#define START_LOC 35
#define INC 30

extern QuickAccessUtility *qau;
const std::vector<SearchType> SearchTypes{SearchType::Source, SearchType::Type,
					  SearchType::File, SearchType::Url,
					  SearchType::Filters};
const std::map<SearchType, std::string> SearchTypeNames{
	{SearchType::Source, "Source"},
	{SearchType::Type, "Source Type"},
	{SearchType::File, "File Path"},
	{SearchType::Url, "URL"},
	{SearchType::Filters, "Filters"}};

bool QuickAccessSource::registered = false;

QuickAccessSourceDelegate::QuickAccessSourceDelegate(QObject *parent,
						     QuickAccessDock *dock)
	: QStyledItemDelegate(parent),
	  _dock(dock)
{
	_propertiesState = QStyle::State_Enabled;
	_filtersState = QStyle::State_Enabled;
	_parentScenesState = QStyle::State_Enabled;
}

void QuickAccessSourceDelegate::paint(QPainter *painter,
				      const QStyleOptionViewItem &option,
				      const QModelIndex &index) const
{
	const auto model =
		static_cast<const QuickAccessSourceModel *>(index.model());
	auto item = model->item(index.row());
	std::string mode = obs_frontend_is_theme_dark() ? "theme:Dark/"
							: "theme:Light/";
	QString text = item->getName().c_str();
	QRect rect = option.rect;
	if (option.state & QStyle::State_Selected)
		painter->fillRect(option.rect, option.palette.highlight());
	if (option.state & QStyle::State_MouseOver)
		painter->fillRect(option.rect, option.palette.highlight());
	auto icon = item->icon();
	QRect iconRect(rect);
	iconRect.setWidth(22);
	iconRect.setHeight(22);
	iconRect.setX(rect.x() + 5);
	iconRect.setY(rect.y() + 5);
	icon.paint(painter, iconRect);
	// Paint the icon

	QRect textRect(rect);
	textRect.setWidth(rect.width() - 107);
	textRect.setHeight(30);
	textRect.setX(32);
	painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, text);

	const QWidget *widget = option.widget;
	QListWidget *lw = new QListWidget();
	QStyle *style = lw ? lw->style() : QApplication::style();

	int loc = START_LOC;
	int inc = INC;

	if (_dock->ShowScenes()) {
		QRect ParentScenesButtonRect(rect);
		ParentScenesButtonRect.setX(rect.width() - loc);
		ParentScenesButtonRect.setHeight(30);
		ParentScenesButtonRect.setWidth(30);
		QStyleOptionButton parentScenesButton;
		parentScenesButton.rect = ParentScenesButtonRect;
		QIcon scenesIcon;
		std::string scenesIconPath = mode + "sources/scene.svg";
		scenesIcon.addFile(scenesIconPath.c_str(), QSize(),
				   QIcon::Normal, QIcon::Off);
		parentScenesButton.icon = scenesIcon;
		parentScenesButton.iconSize = QSize(16, 16);
		parentScenesButton.state = _filtersState |
					   QStyle::State_Enabled;
		style->drawControl(QStyle::CE_PushButtonLabel,
				   &parentScenesButton, painter, widget);
		loc += inc;
	}

	if (_dock->ShowFilters()) {
		QRect filtersButtonRect(rect);
		filtersButtonRect.setX(rect.width() - loc);
		filtersButtonRect.setHeight(30);
		filtersButtonRect.setWidth(30);
		QStyleOptionButton filtersButton;
		filtersButton.rect = filtersButtonRect;
		QIcon filterIcon;
		std::string filtersIconPath = mode + "filter.svg";
		filterIcon.addFile(filtersIconPath.c_str(), QSize(),
				   QIcon::Normal, QIcon::Off);
		filtersButton.icon = filterIcon;
		filtersButton.iconSize = QSize(16, 16);
		filtersButton.state = _filtersState | QStyle::State_Enabled;
		style->drawControl(QStyle::CE_PushButtonLabel, &filtersButton,
				   painter, widget);
		loc += inc;
	}

	if (_dock->ShowProperties() && item->hasProperties()) {
		QRect propertiesButtonRect(rect);
		propertiesButtonRect.setX(rect.width() - loc);
		propertiesButtonRect.setHeight(30);
		propertiesButtonRect.setWidth(30);
		QStyleOptionButton propertiesButton;
		propertiesButton.rect = propertiesButtonRect;
		QIcon propsIcon;
		std::string propertiesIconPath = mode + "settings/general.svg";
		propsIcon.addFile(propertiesIconPath.c_str(), QSize(),
				  QIcon::Normal, QIcon::Off);
		propertiesButton.icon = propsIcon;
		propertiesButton.iconSize = QSize(16, 16);
		propertiesButton.state = _propertiesState |
					 QStyle::State_Enabled;
		style->drawControl(QStyle::CE_PushButtonLabel,
				   &propertiesButton, painter, widget);
		loc += inc;
	}
	textRect.setWidth(rect.width() - (loc + 2));

	//style->drawControl(QStyle::CE_ItemViewItem, &option, painter, widget);
	delete lw;
}

QSize QuickAccessSourceDelegate::sizeHint(
	const QStyleOptionViewItem & /*option*/,
	const QModelIndex & /*index*/) const
{
	//hard coding size for test purpose,
	//actual size hint can be calculated from option param
	return QSize(200, 30);
}

bool QuickAccessSourceDelegate::editorEvent(QEvent *event,
					    QAbstractItemModel *model,
					    const QStyleOptionViewItem &option,
					    const QModelIndex &index)
{
	UNUSED_PARAMETER(model);
	if (event->type() == QEvent::MouseButtonPress ||
	    event->type() == QEvent::MouseButtonRelease ||
	    event->type() == QEvent::MouseButtonDblClick) {

	} else {
		//ignoring other mouse event and reseting button's state
		_propertiesState = QStyle::State_Raised;
		_filtersState = QStyle::State_Raised;
		_parentScenesState = QStyle::State_Raised;
		return true;
	}

	QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);

	int loc = START_LOC;
	int inc = INC;

	if (event->type() == QEvent::MouseButtonRelease) {
		emit itemSelected(index);
	}

	if (_dock->ShowScenes()) {
		QRect parentScenesButtonRect(option.rect);
		parentScenesButtonRect.setX(option.rect.width() - loc);
		parentScenesButtonRect.setWidth(30);
		parentScenesButtonRect.setHeight(30);

		if (!parentScenesButtonRect.contains(mouseEvent->pos())) {
			_parentScenesState = QStyle::State_Raised;
		} else if (event->type() == QEvent::MouseButtonPress) {
			_parentScenesState = QStyle::State_Sunken;
		} else if (event->type() == QEvent::MouseButtonRelease) {
			_parentScenesState = QStyle::State_Raised;
			emit openParentScenesClicked(index);
			return true;
		}
		loc += inc;
	}

	if (_dock->ShowFilters()) {

		QRect filtersButtonRect(option.rect);
		filtersButtonRect.setX(option.rect.width() - loc);
		filtersButtonRect.setWidth(30);
		filtersButtonRect.setHeight(30);

		if (!filtersButtonRect.contains(mouseEvent->pos())) {
			_filtersState = QStyle::State_Raised;
		} else if (event->type() == QEvent::MouseButtonPress) {
			_filtersState = QStyle::State_Sunken;
		} else if (event->type() == QEvent::MouseButtonRelease) {
			_filtersState = QStyle::State_Raised;
			emit openFiltersClicked(index);
			return true;
		}
		loc += inc;
	}

	if (_dock->ShowProperties()) {
		QRect propertiesButtonRect(option.rect);
		propertiesButtonRect.setX(option.rect.width() - loc);
		propertiesButtonRect.setWidth(30);
		propertiesButtonRect.setHeight(30);

		if (!propertiesButtonRect.contains(mouseEvent->pos())) {
			_propertiesState = QStyle::State_Raised;
		} else if (event->type() == QEvent::MouseButtonPress) {
			_propertiesState = QStyle::State_Sunken;
		} else if (event->type() == QEvent::MouseButtonRelease) {
			_propertiesState = QStyle::State_Raised;
			emit openPropertiesClicked(index);
			return true;
		}
	}

	if (_dock->ClickableScenes() &&
	    event->type() == QEvent::MouseButtonDblClick) {
		blog(LOG_INFO, "DBL CLICK!!!!!");
	} else if (_dock->ClickableScenes() &&
		   event->type() == QEvent::MouseButtonRelease) {
		blog(LOG_INFO, "SINGLE CLICK!!!!");
		emit activateScene(index);
	}

	return true;
}

QuickAccessSourceModel::QuickAccessSourceModel(QObject *parent,
					       SearchType searchType)
	: QAbstractListModel(parent),
	  _searchType(searchType)
{
}

Qt::DropActions QuickAccessSourceModel::supportedDropActions() const
{
	return Qt::CopyAction | Qt::MoveAction;
}

Qt::ItemFlags QuickAccessSourceModel::flags(const QModelIndex &index) const
{
	Qt::ItemFlags defaultFlags = QAbstractListModel::flags(index);

	if (index.isValid())
		return Qt::ItemIsSelectable |
		       defaultFlags; //return Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | defaultFlags;
	else
		return Qt::ItemIsSelectable |
		       defaultFlags; //Qt::ItemIsDropEnabled | Qt::ItemIsDragEnabled | defaultFlags;
}

bool QuickAccessSourceModel::setData(const QModelIndex &index,
				     const QVariant &value, int role)
{
	UNUSED_PARAMETER(role);
	//auto dat = value.value<QuickAccessSource*>();
	auto dat = static_cast<QuickAccessSource *>(value.value<void *>());
	_data->at(index.row()) = dat;
	return true;
}

bool QuickAccessSourceModel::insertRows(int row, int count,
					const QModelIndex &parent)
{
	if (parent.isValid())
		return false;

	for (int i = 0; i != count; ++i) {
		auto it = _data->begin() + row + i;
		auto var = nullptr;
		_data->insert(it, var);
	}
	return true;
}

bool QuickAccessSourceModel::removeRows(int row, int count,
					const QModelIndex &parent)
{
	if (parent.isValid())
		return false;
	beginRemoveRows(parent, row, row + count - 1);
	for (int i = 0; i != count; ++i) {
		auto it = _data->begin() + row;
		_data->erase(it);
	}
	endRemoveRows();
	return true;
}

void QuickAccessSourceModel::swapRows(int rowA, int rowB)
{
	auto &data = *_data;
	beginResetModel();
	std::swap(data[rowA], data[rowB]);
	endResetModel();
}

void QuickAccessSourceModel::addSource(QuickAccessSource *source)
{
	// TODO: Insert alphabatized
	beginInsertRows(QModelIndex(), rowCount(), rowCount());
	_data->push_back(source);
	endInsertRows();
}

void QuickAccessSourceModel::removeSource(QuickAccessSource *source)
{
	auto it = std::find(_data->begin(), _data->end(), source);
	if (it == _data->end()) {
		return;
	}
	int idx = static_cast<int>(it - _data->begin());
	beginRemoveRows(QModelIndex(), idx, idx);
	_data->erase(it);
	endRemoveRows();
}

void QuickAccessSourceModel::setSearchTerm(std::string searchTerm)
{
	UNUSED_PARAMETER(searchTerm);
}

int QuickAccessSourceModel::rowCount(const QModelIndex &parent) const
{
	UNUSED_PARAMETER(parent);
	return static_cast<int>(_data->size());
}

QuickAccessSource *QuickAccessSourceModel::item(int row) const
{
	if (row < 0 || row >= rowCount()) {
		return nullptr;
	}
	return _data->at(row);
}

QVariant QuickAccessSourceModel::data(const QModelIndex &index, int role) const
{
	UNUSED_PARAMETER(role);
	if (index.row() < 0 || index.row() >= rowCount()) {
		return {};
	}

	QuickAccessSource *ptr = _data->at(index.row());
	QVariant qv(QVariant::fromValue(static_cast<void *>(ptr)));
	return qv;
}

QuickAccessSource::QuickAccessSource(obs_source_t *source)
{
	if (!QuickAccessSource::registered) {
		qRegisterMetaType<QuickAccessSource *>();
	}
	_source = obs_source_get_weak_source(source);
	_tmpName = obs_source_get_name(source);
	blog(LOG_INFO, "!!!!! QAS:Grabbed\t%s", _tmpName.c_str());
	_sourceClass = obs_source_is_group(source)   ? SourceClass::Group
		       : obs_source_is_scene(source) ? SourceClass::Scene
						     : SourceClass::Source;
	BuildSearchTerms();
}

QuickAccessSource::~QuickAccessSource()
{
	blog(LOG_INFO, "!!!!! QAS:Released\t%s", _tmpName.c_str());
	for (auto &dock : _docks) {
		dock->RemoveSource(this, false);
	}
	for (auto &parent : _parents) {
		parent->removeChild(this);
	}
	for (auto &child : _children) {
		child->removeParent(this);
	}

	obs_weak_source_release(_source);
}

obs_source_t *QuickAccessSource::get()
{
	return obs_weak_source_get_source(_source);
}

QIcon QuickAccessSource::icon() const
{
	auto source = obs_weak_source_get_source(_source);
	if (!source) {
		return qau->GetSceneIcon();
	}
	const char *id = obs_source_get_id(source);
	obs_source_release(source);

	if (strcmp(id, "scene") == 0)
		return qau->GetSceneIcon();
	else if (strcmp(id, "group") == 0)
		return qau->GetGroupIcon();
	return qau->GetIconFromType(id);
}

signal_handler_t *QuickAccessSource::getSignalHandler()
{
	auto source = obs_weak_source_get_source(_source);
	auto signalHandler = obs_source_get_signal_handler(source);
	obs_source_release(source);
	return signalHandler;
}

std::string QuickAccessSource::getName() const
{
	auto source = obs_weak_source_get_source(_source);
	if (!source) {
		return std::string("");
	}
	std::string name = obs_source_get_name(source);
	obs_source_release(source);
	return name;
}

std::string QuickAccessSource::getUUID() const
{
	auto source = obs_weak_source_get_source(_source);
	if (!source) {
		return std::string("");
	}
	std::string name = obs_source_get_uuid(source);
	obs_source_release(source);
	return name;
}

void QuickAccessSource::rename(std::string name)
{
	obs_source_t *source = obs_weak_source_get_source(_source);
	obs_source_set_name(source, name.c_str());
	obs_source_release(source);
}

bool QuickAccessSource::save(obs_data_t *itemObj)
{
	if (removing()) {
		return false;
	}
	std::string sourceName = getName();
	obs_data_set_string(itemObj, "source_name", sourceName.c_str());
	return true;
}

std::vector<SearchType> QuickAccessSource::search(std::string searchTerm)
{
	std::vector<SearchType> hits;
	for (auto &st : SearchTypes) {
		for (auto &haystack : _searchTerms[st]) {
			auto it = std::search(haystack.begin(), haystack.end(),
					      searchTerm.begin(),
					      searchTerm.end(),
					      [](char a, char b) {
						      return tolower(a) ==
							     tolower(b);
					      });
			if (it != haystack.end()) {
				hits.push_back(st);
				break;
			}
		}
	}
	return hits;
}

bool QuickAccessSource::hasMatch(std::string &searchTerm, SearchType st)
{
	for (auto &haystack : _searchTerms[st]) {
		auto it = std::search(haystack.begin(), haystack.end(),
				      searchTerm.begin(), searchTerm.end(),
				      [](char a, char b) {
					      return tolower(a) == tolower(b);
				      });
		if (it != haystack.end()) {
			return true;
		}
	}
	return false;
}

void QuickAccessSource::BuildSearchTerms()
{
	std::unique_lock lock(_m);
	auto source = obs_weak_source_get_source(_source);
	_searchTerms[SearchType::Source].clear();
	_searchTerms[SearchType::Type].clear();
	_searchTerms[SearchType::Filters].clear();
	_searchTerms[SearchType::Url].clear();
	_searchTerms[SearchType::File].clear();

	if (!source) {
		return;
	}

	// Source Name
	_searchTerms[SearchType::Source].push_back(obs_source_get_name(source));
	_searchTerms[SearchType::Source].push_back(obs_source_get_uuid(source));

	// Source Type Id and Name
	const char *source_id = obs_source_get_id(source);
	// blog(LOG_INFO, "Source ID: %s", source_id);
	const char *source_type_name = obs_source_get_display_name(source_id);
	if (!source_type_name) {
		return;
	}
	_searchTerms[SearchType::Type].push_back(source_id);
	_searchTerms[SearchType::Type].push_back(
		obs_source_get_display_name(source_id));

	std::vector<obs_source_t *> filters;
	// Source Filters
	obs_source_enum_filters(source, GetFilters, &filters);

	for (auto filter : filters) {
		const char *filter_id = obs_source_get_id(filter);
		const char *name = obs_source_get_display_name(filter_id);
		if (!name) {
			continue;
		}
		_searchTerms[SearchType::Filters].push_back(filter_id);
		_searchTerms[SearchType::Filters].push_back(
			obs_source_get_display_name(filter_id));
		_searchTerms[SearchType::Filters].push_back(
			obs_source_get_name(filter));
	}

	obs_data_t *data = obs_source_get_settings(source);
	// Browser source urls
	if (strcmp(source_id, BROWSER_SOURCE_ID) == 0) {
		std::string url = obs_data_get_string(data, "url");
		_searchTerms[SearchType::Url].push_back(url);
	}

	// Media file path/input
	if (strcmp(source_id, MEDIA_SOURCE_ID) == 0) {
		bool localFile = obs_data_get_bool(data, "is_local_file");
		if (localFile) {
			_searchTerms[SearchType::File].push_back(
				obs_data_get_string(data, "local_file"));
		} else {
			_searchTerms[SearchType::Url].push_back(
				obs_data_get_string(data, "input"));
		}
	}

	// Image file path
	if (strcmp(source_id, IMAGE_SOURCE_ID) == 0) {
		std::string file = obs_data_get_string(data, "file");
		_searchTerms[SearchType::File].push_back(file);
	}
	obs_data_release(data);

	obs_source_release(source);
}

void QuickAccessSource::update()
{
	BuildSearchTerms();
}

void QuickAccessSource::addDock(QuickAccessDock *dock)
{
	std::unique_lock lock(_m);
	_docks.insert(dock);
}

void QuickAccessSource::removeDock(QuickAccessDock *dock)
{
	std::unique_lock lock(_m);
	if (auto it = _docks.find(dock); it != _docks.end()) {
		_docks.erase(it);
	}
}

void QuickAccessSource::removeParent(QuickAccessSource *parent)
{
	//std::unique_lock lock(_m);
	if (_parents.size() == 0) {
		return;
	}
	auto it = std::find(_parents.begin(), _parents.end(), parent);
	while (it != _parents.end()) {
		_parents.erase(it);
		it = std::find(_parents.begin(), _parents.end(), parent);
	}
}

void QuickAccessSource::removeChild(QuickAccessSource *child)
{
	//std::unique_lock lock(_m);
	if (_children.size() == 0) {
		return;
	}
	// Child can be added multiple times to a parent scene/group.
	auto it = std::find(_children.begin(), _children.end(), child);
	while (it != _children.end()) {
		_children.erase(it);
		it = std::find(_children.begin(), _children.end(), child);
	}
}

void QuickAccessSource::openProperties() const
{
	obs_source_t *source = obs_weak_source_get_source(_source);
	obs_frontend_open_source_properties(source);
	obs_source_release(source);
}

void QuickAccessSource::openFilters() const
{
	obs_source_t *source = obs_weak_source_get_source(_source);
	obs_frontend_open_source_filters(source);
	obs_source_release(source);
}

void QuickAccessSource::openInteract() const
{
	obs_source_t *source = obs_weak_source_get_source(_source);
	obs_frontend_open_source_interaction(source);
	obs_source_release(source);
}

void QuickAccessSource::activateScene() const
{
	obs_source_t *source = obs_weak_source_get_source(_source);
	if (obs_source_is_scene(source)) {
		bool studioMode = obs_frontend_preview_program_mode_active();
		if (studioMode) {
			obs_frontend_set_current_preview_scene(source);
		} else {
			obs_frontend_set_current_scene(source);
		}
	}
	obs_source_release(source);
}

bool QuickAccessSource::hasProperties() const
{
	obs_source_t *source = obs_weak_source_get_source(_source);
	bool props = obs_source_configurable(source);
	obs_source_release(source);
	return props;
}

bool QuickAccessSource::hasInteract() const
{
	obs_source_t *source = obs_weak_source_get_source(_source);
	uint32_t flags = obs_source_get_output_flags(source);
	obs_source_release(source);
	return (flags & OBS_SOURCE_INTERACTION) == OBS_SOURCE_INTERACTION;
}

bool QuickAccessSource::hasRefresh() const
{
	obs_source_t *source = obs_weak_source_get_source(_source);
	std::string sourceType = obs_source_get_id(source);
	obs_source_release(source);
	return sourceType == "browser_source";
}

void QuickAccessSource::refreshBrowser() const
{
	obs_source_t *source = obs_weak_source_get_source(_source);
	obs_properties_t *props = obs_source_properties(source);

	obs_property_t *p = obs_properties_get(props, "refreshnocache");
	obs_property_button_clicked(p, source);
	obs_source_release(source);
}

std::string QuickAccessSource::activeState() const
{
	obs_source_t *source = obs_weak_source_get_source(_source);
	std::string type_str = obs_source_get_id(source);
	std::string ret = "";
	if (type_str == "dshow_input") {
		auto settings = obs_source_get_settings(source);
		bool now_active = obs_data_get_bool(settings, "active");
		ret = now_active ? "Deactivate" : "Activate";
		obs_data_release(settings);
	}
	// macos-avcapture-fast and av_capture_input on macos
	obs_source_release(source);
	return ret;
}

void QuickAccessSource::toggleActivation() const
{
	obs_source_t *source = obs_weak_source_get_source(_source);
	if (!source) {
		return;
	}
	auto settings = obs_source_get_settings(source);
	bool nowActive = obs_data_get_bool(settings, "active");

	obs_data_release(settings);
	obs_source_release(source);

	calldata_t cd = {};
	calldata_set_bool(&cd, "active", !nowActive);
	proc_handler_t *ph = obs_source_get_proc_handler(source);
	proc_handler_call(ph, "activate", &cd);
	calldata_free(&cd);
}

QDataStream &operator<<(QDataStream &out, QuickAccessSource *const &rhs)
{
	out.writeRawData(reinterpret_cast<const char *>(&rhs), sizeof(rhs));
	return out;
}

QDataStream &operator>>(QDataStream &in, QuickAccessSource *&rhs)
{
	in.readRawData(reinterpret_cast<char *>(&rhs), sizeof(rhs));
	return in;
}

void GetFilters(obs_source_t *parentScene, obs_source_t *filter, void *param)
{
	UNUSED_PARAMETER(parentScene);
	std::vector<obs_source_t *> *filters =
		static_cast<std::vector<obs_source_t *> *>(param);
	filters->push_back(filter);
}
