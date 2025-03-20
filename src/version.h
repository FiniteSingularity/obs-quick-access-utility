#pragma once

#define PROJECT_NAME "Quick Access Utility"
#define PROJECT_VERSION "1.0.3"
#define PROJECT_VERSION_MAJOR 1
#define PROJECT_VERSION_MINOR 0
#define PROJECT_VERSION_PATCH 3

#define blog(level, msg, ...) \
	blog(level, "[" PROJECT_NAME "] " msg, ##__VA_ARGS__)
