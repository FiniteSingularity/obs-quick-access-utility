#pragma once

#define PROJECT_NAME "Quick Access Utility"
#define PROJECT_VERSION "0.0.2"
#define PROJECT_VERSION_MAJOR 0
#define PROJECT_VERSION_MINOR 0
#define PROJECT_VERSION_PATCH 2

#define blog(level, msg, ...) \
	blog(level, "[" PROJECT_NAME "] " msg, ##__VA_ARGS__)
