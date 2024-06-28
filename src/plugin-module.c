#include <obs-module.h>
#include <obs-frontend-api.h>

#include "version.h"

OBS_DECLARE_MODULE()
OBS_MODULE_AUTHOR("FiniteSingularity");
OBS_MODULE_USE_DEFAULT_LOCALE("obs-quick-access-utility", "en-US")

typedef const char *(*translateFunc)(const char *);

void InitializeQAU(obs_module_t *, translateFunc);
void ShutdownQAU();
void LoadSourceItems();
void frontend_save_load(obs_data_t *save_data, bool saving, void *);

bool obs_module_load()
{
	obs_frontend_push_ui_translation(obs_module_get_string);
	InitializeQAU(obs_current_module(), obs_module_text);
	blog(LOG_INFO, "Loaded version %s", PROJECT_VERSION);
	return true;
}

void obs_module_unload()
{
	ShutdownQAU();
}

MODULE_EXPORT const char *obs_module_description(void)
{
	return "A dock to quickly access stuff.";
}

MODULE_EXPORT const char *obs_module_name(void)
{
	return "Quick Access Dock";
}
