#include <httpd.h>
#include <http_config.h>
#include <http_request.h>
#include <apr_strings.h>

#define APR_WANT_STRFUNC
#define APR_WANT_MEMFUNC
#include <apr_want.h>

#include "fractal_tiles.h"

// using namespace std;

static void *create_dir_conf(apr_pool_t *p, char *dummy)
{
  fractal_conf *c =
    (fractal_conf *)apr_pcalloc(p, sizeof(fractal_conf));
  return NULL;
}

// Returns a table read from a file, or NULL and an error message
static apr_table_t *read_pKVP_from_file(apr_pool_t *pool, const char *fname, char **err_message)
{
  // Should parse it here and initialize the configuration structure
  ap_configfile_t *cfg_file;
  apr_status_t s = ap_pcfg_openfile(&cfg_file, pool, fname);

  if (APR_SUCCESS != s) { // %pm means print status error string
    *err_message = apr_psprintf(pool, "%s - %pm", fname, &s);
    return NULL;
  }

  char buffer[MAX_STRING_LEN];
  apr_table_t *table = apr_table_make(pool, 8);
  // This can return ENOSPC if lines are too long
  while (APR_SUCCESS == (s = ap_cfg_getline(buffer, MAX_STRING_LEN, cfg_file))) {
    if ((strlen(buffer) == 0) || buffer[0] == '#')
      continue;
    const char *value = buffer;
    char *key = ap_getword_white(pool, &value);
    apr_table_add(table, key, value);
  }

  ap_cfg_closefile(cfg_file);
  if (s == APR_ENOSPC) {
    *err_message = apr_psprintf(pool, "%s lines should be smaller than %d", fname, MAX_STRING_LEN);
    return NULL;
  }

  return table;
}

// Allow for one or more RegExp guard
// One of them has to match if the request is to be considered
static const char *set_regexp(cmd_parms *cmd, fractal_conf *c, const char *pattern)
{
    char *err_message = NULL;
    if (c->regexp == 0)
        c->regexp = apr_array_make(cmd->pool, 2, sizeof(ap_regex_t));
    ap_regex_t *m = (ap_regex_t *)apr_array_push(c->regexp);
    int error = ap_regcomp(m, pattern, 0);
    if (error) {
        int msize = 2048;
        err_message = (char *)apr_pcalloc(cmd->pool, msize);
        ap_regerror(error, m, err_message, msize);
        return apr_pstrcat(cmd->pool, "Reproject Regexp incorrect ", err_message, NULL);
    }
    return NULL;
}

// Returns NULL if it worked as expected, returns a four integer value from "x y", "x y z" or "x y z c"
static char *get_xyzc_size(apr_pool_t *p, struct sz *size, const char *value, const char*err_prefix) {
  char *s;
  if (!value)
    return apr_psprintf(p, "%s directive missing", err_prefix);
  size->x = apr_strtoi64(value, &s, 0);
  size->y = apr_strtoi64(s, &s, 0);
  size->c = 3;
  size->z = 1;
  if (errno == 0 && *s) { // Read optional third and fourth integers
    size->z = apr_strtoi64(s, &s, 0);
    if (*s)
      size->c = apr_strtoi64(s, &s, 0);
  } // Raster size is 4 params max
  if (errno || *s)
    return apr_psprintf(p, "%s incorrect", err_prefix);
  return NULL;
}

static const char *file_set(cmd_parms *cmd, void *dconf, const char *fname)
{
  ap_assert(sizeof(apr_off_t) == 8);
  fractal_conf *c = (fractal_conf *)dconf;
  char *err_message;
  apr_table_t *kvp = read_pKVP_from_file(cmd->temp_pool, fname, &err_message);
  if (NULL == kvp) return err_message;

  // Got the parsed kvp table, parse the configuration items
  const char *line;
  char *err_prefix;

  line = apr_table_get(kvp, "Size");
  if (!line)
    return "Size directive is mandatory";
  err_prefix = apr_psprintf(cmd->temp_pool, "%s Size", fname);
  err_message = get_xyzc_size(cmd->temp_pool, &(c->size), line, err_prefix);
  if (err_message) return err_message;

  // PageSize is optional, use reasonable defaults
  c->pagesize.x = c->pagesize.z = 512;
  c->pagesize.c = c->size.c;
  c->pagesize.z = 1;
  line = apr_table_get(kvp, "PageSize");
  if (line) {
    err_prefix = "PageSize error";
    err_message = get_xyzc_size(cmd->temp_pool, &(c->pagesize), line, err_prefix);
    if (err_message) return err_message;
  }

  if (c->pagesize.c != c->size.c || c->pagesize.z != 1)
    return "PageSize has invalid parameters";

  // Mime type is jpeg if not provided
  line = apr_table_get(kvp, "MimeType");
  if (line)
      c->mime_type = apr_pstrdup(cmd->pool, line);
  else
      c->mime_type = "image/jpeg";

  // Skip levels
  line = apr_table_get(kvp, "SkippedLevels");
  if (line)
    c->skip_levels = atoi(line);

  return NULL;
}

static int handler(request_rec *r)
{
  return 0;
}


#define CMD_FUNC (cmd_func)

static const command_rec cmds[] =
{
  AP_INIT_TAKE1(
  "FractalTiles_ConfigurationFile",
  CMD_FUNC file_set, // Callback
  0, // Self-pass argument
  ACCESS_CONF, // availability
  "The configuration file for this module"
  ),

  AP_INIT_TAKE1(
  "FractalTiles_RegExp",
  (cmd_func)set_regexp,
  0, // Self-pass argument
  ACCESS_CONF, // availability
  "Regular expression that the URL has to match.  At least one is required."
  ),

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
