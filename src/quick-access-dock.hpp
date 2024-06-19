#pragma once
#include <obs-module.h>
#include <QDockWidget>
#include <QWidget>
#include "quick-access.hpp"

typedef const char *(*translateFunc)(const char *);

class QuickAccessDock : public QWidget {
	Q_OBJECT

public:
	QuickAccessDock(QWidget *parent = nullptr);
	void Load(obs_data_t *data);
	void Save(obs_data_t *data);
	QIcon GetIconFromType(const char* type) const;
	QIcon GetSceneIcon() const;
	QIcon GetGroupIcon() const;
private:
	QuickAccess *_widget;
};

void frontendSaveLoad(obs_data_t* save_data, bool saving, void* data);

