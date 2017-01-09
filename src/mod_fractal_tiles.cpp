#include "fractal_tiles.h"

#include <httpd.h>
#include <http_config.h>
#include <http_request.h>

using namespace std;

static void *create_dir_conf(apr_pool_t *p, char *dummy)
{
  return NULL;
}

static int handler(request_rec *r)
{
  return 0;
}

static const command_rec cmds[] =
{
  { NULL }
};

static void register_hooks(apr_pool_t *p)
{
  ap_hook_handler(handler, NULL, NULL, APR_HOOK_MIDDLE);
}

module AP_MODULE_DECLARE_DATA fractal_tiles_module = {
  STANDARD20_MODULE_STUFF,
  create_dir_conf,
  0,
  0,
  0,
  cmds,
  register_hooks
};
