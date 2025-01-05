/*
* (C) Lucian Plesea 2019-2024
*
*/

#include <ahtse.h>
#include <http_request.h>
#include <apr_strings.h>

NS_AHTSE_USE

typedef struct {
    apr_array_header_t* arr_rxp;
    TiledRaster raster;

} fractal_conf;

static const char *read_config(cmd_parms *cmd, void *dconf, const char *src)
{
    const char* err_message;
    fractal_conf *c = (fractal_conf *)dconf;

    apr_table_t *kvp = readAHTSEConfig(cmd->temp_pool, src, &err_message);
    if (nullptr == kvp)
        return err_message;
    err_message = configRaster(cmd->pool, kvp, c->raster);
    if (err_message)
        return err_message;

  return nullptr;
}

static int handler(request_rec *r)
{
  return 0;
}

static const command_rec cmds[] =
{
  AP_INIT_TAKE1(
      "FractalTiles_ConfigurationFile",
      (cmd_func) read_config, // Callback
      0, // Self-pass argument
      ACCESS_CONF, // availability
      "The AHTSE configuration file for this module"
  ),

  AP_INIT_TAKE1(
      "FractalTiles_RegExp",
      (cmd_func)set_regexp<fractal_conf>,
      0, // Self-pass argument
      ACCESS_CONF, // availability
      "Regular expression for triggering mod_fractal_tiles"
  ),

  { NULL }
};

static void register_hooks(apr_pool_t *p) {
  ap_hook_handler(handler, nullptr, nullptr, APR_HOOK_MIDDLE);
}

module AP_MODULE_DECLARE_DATA fractal_tiles_module = {
  STANDARD20_MODULE_STUFF,
  pcreate<fractal_conf>, // Per-directory configuration
  nullptr, // Merge handler
  nullptr, // Server configuration
  nullptr, // Merge handler
  cmds, // Configuration directives
  register_hooks // Register hooks
};
