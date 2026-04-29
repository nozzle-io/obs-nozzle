#include "nozzle-plugin.h"

extern "C" {

OBS_DECLARE_MODULE()
OBS_MODULE_AUTHOR("nozzle")
OBS_MODULE_USE_DEFAULT_LOCALE("obs-nozzle", "en-US")

obs_source_info nozzle_source_info;
obs_output_info nozzle_output_info;

extern obs_source_info create_nozzle_source_info();
extern obs_output_info create_nozzle_output_info();

bool obs_module_load(void)
{
    nozzle_source_info = create_nozzle_source_info();
    obs_register_source(&nozzle_source_info);

    nozzle_output_info = create_nozzle_output_info();
    obs_register_output(&nozzle_output_info);

    blog(LOG_INFO, "[nozzle] obs-nozzle plugin loaded");
    return true;
}

void obs_module_unload(void)
{
    blog(LOG_INFO, "[nozzle] obs-nozzle plugin unloaded");
}

const char *obs_module_name(void)
{
    return "obs-nozzle";
}

const char *obs_module_description(void)
{
    return "Nozzle GPU texture sharing plugin for OBS Studio";
}

} // extern "C"
