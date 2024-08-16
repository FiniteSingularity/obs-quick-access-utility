#pragma once
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <string>
#include <map>
#include <vector>
#include <set>
#include <mutex>
#include <thread>
#include <QObject>
#include <QVariant>
#include <QAbstractListModel>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QMetaType>
#include <QDataStream>

class QuickAccessDock;
class QuickAccessSource;

enum class SearchType { None, Source, Type, Filters, Url, File };

enum class SourceClass { Source, Scene, Group };

class QuickAccessSourceDelegate : public QStyledItemDelegate {
	Q_OBJECT
public:
	QuickAccessSourceDelegate(QObject *parent = nullptr,
				  QuickAccessDock *dock = nullptr);

	virtual void paint(QPainter *painter,
			   const QStyleOptionViewItem &option,
			   const QModelIndex &index) const;

	virtual QSize sizeHint(const QStyleOptionViewItem &option,
			       const QModelIndex &index) const;

	bool editorEvent(QEvent *event, QAbstractItemModel *model,
			 const QStyleOptionViewItem &option,
			 const QModelIndex &index);

signals:
	void openPropertiesClicked(const QModelIndex &index);
	void openParentScenesClicked(const QModelIndex &index);
	void openFiltersClicked(const QModelIndex &index);
	void activateScene(const QModelIndex &index);
	void itemSelected(const QModelIndex &index);

private:
	QStyle::State _propertiesState;
	QStyle::State _filtersState;
	QStyle::State _parentScenesState;
	QuickAccessDock *_dock;
};

class QuickAccessSourceModel : public QAbstractListModel {
	Q_OBJECT

public:
	explicit QuickAccessSourceModel(
		QObject *parent = nullptr,
		SearchType searchType = SearchType::None);
	inline void setSources(std::vector<QuickAccessSource *> *newData)
	{
		_data = newData;
	}
	void addSource(QuickAccessSource *source);
	void removeSource(QuickAccessSource *source);
	void setSearchTerm(std::string searchTerm);
	QuickAccessSource *item(int row) const;

	int rowCount(const QModelIndex &parent = QModelIndex()) const override;
	QVariant data(const QModelIndex &index,
		      int role = Qt::DisplayRole) const override;

	bool setData(const QModelIndex &index, const QVariant &value,
		     int role = Qt::DisplayRole) override;
	bool insertRows(int row, int count, const QModelIndex &parent) override;
	bool removeRows(int row, int count, const QModelIndex &parent) override;

	void swapRows(int rowA, int rowB);

	Qt::DropActions supportedDropActions() const override;
	Qt::ItemFlags flags(const QModelIndex &index) const override;

private:
	std::vector<QuickAccessSource *> *_data;
	SearchType _searchType;
};

typedef std::map<SearchType, std::vector<std::string>> SearchTermMap;

class QuickAccessSource {
public:
	QuickAccessSource(obs_source_t *);
	~QuickAccessSource();

	void BuildSearchTerms();

	std::string getName() const;
	std::string getUUID() const;
	obs_source_t *get();
	void addDock(QuickAccessDock *);
	void removeDock(QuickAccessDock *);
	inline bool removing() { return _removing; }
	inline void addParent(QuickAccessSource *parent)
	{
		_parents.push_back(parent);
	}
	inline void addChild(QuickAccessSource *child)
	{
		_children.push_back(child);
	}
	inline void markForRemoval() { _removing = true; }
	void removeParent(QuickAccessSource *parent);
	void removeChild(QuickAccessSource *child);
	signal_handler_t* getSignalHandler();
	inline std::vector<QuickAccessSource *> children() { return _children; }
	inline std::vector<QuickAccessSource *> parents() { return _parents; }
	inline SourceClass sourceType() { return _sourceClass; }
	std::vector<SearchType> search(std::string searchTerm);
	bool hasMatch(std::string &searchTerm, SearchType st);
	void update();
	void openProperties() const;
	void openFilters() const;
	void openInteract() const;
	void refreshBrowser() const;
	void toggleActivation() const;
	void activateScene() const;
	bool hasProperties() const;
	bool hasInteract() const;
	bool hasRefresh() const;
	std::string activeState() const;

	bool save(obs_data_t *itemObj);
	inline bool isSource(obs_source_t *source)
	{
		return obs_weak_source_references_source(_source, source);
	}
	void rename(std::string name);
	QIcon icon() const;

private:
	obs_weak_source_t *_source;
	SourceClass _sourceClass;
	SearchTermMap _searchTerms;
	bool _removing = false;
	std::vector<QuickAccessSource *> _parents;
	std::vector<QuickAccessSource *> _children;
	std::set<QuickAccessDock *> _docks;
	std::mutex _m;
	// TODO: Get rid of _tmpName
	std::string _tmpName;
	static bool registered;
};

Q_DECLARE_METATYPE(QuickAccessSource *)

QDataStream &operator<<(QDataStream &out, QuickAccessSource *const &rhs);
QDataStream &operator>>(QDataStream &in, QuickAccessSource *&rhs);

void GetFilters(obs_source_t *parentScene, obs_source_t *filter, void *param);
