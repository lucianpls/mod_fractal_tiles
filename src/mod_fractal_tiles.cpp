/*
* (C) Lucian Plesea 2019-2024
*
*/

#include <ahtse.h>
#include <http_request.h>
#include <apr_strings.h>
#include <http_log.h>
#include <complex>

//#include <chrono>

NS_AHTSE_USE
NS_ICD_USE

// Defined at the bottom, needs extern to avoid redefinition
extern module AP_MODULE_DECLARE_DATA fractal_tiles_module;
#if defined(APLOG_USE_MODULE)
APLOG_USE_MODULE(fractal_tiles);
#endif

typedef struct {
    apr_array_header_t* arr_rxp;
    TiledRaster raster;

    size_t max_size; // Compressed size
    int indirect;
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
    // Check that the type is byte and it's single band
    if (c->raster.dt != ICDT_Byte || c->raster.pagesize.c != 1)
        return "Only byte data type is supported for now";
    // Check that we want square tiles
    if (c->raster.pagesize.x != c->raster.pagesize.y)
        return "Only square tiles are supported for now";

    if (c->raster.format != IMG_PNG)
        return "Only PNG format is supported for now";
    c->max_size = MAX_TILE_SIZE;
    return nullptr;
}

// Build a fractal image, return the error code or SUCCESS
// This is basic Mandelbrot, no speed-ups
void* generateTile(sz5 tile, fractal_conf* cfg, void* data)
{
    auto size = cfg->raster.pagesize.x; // Size of the tile, always square
    //auto bands = cfg->raster.pagesize.c; // Number of bands, always 1
    auto pixels = (unsigned char*)data;
    auto resolution = cfg->raster.rsets[tile.l].rx;
    for (size_t y = 0; y < size; y++) {
        double imaginary = cfg->raster.bbox.ymax - (tile.y * size + y) * resolution;
        std::complex<double> c(0, imaginary);
        for (size_t x = 0; x < size; x++) {
            c.real(cfg->raster.bbox.xmin + (tile.x * size + x) * resolution);
            std::complex<double> z(0, 0);
            int i = 0;
            for (; i < 255; i++) {
                z = z * z + c;
                if (std::norm(z) > 4)
                    break;
            }
            pixels[y * size + x] = i;
        }
    }
    return data;
}

static int handler(request_rec *r)
{
    const char* message;
    if (r->method_number != M_GET)
        return DECLINED;
    auto* cfg = get_conf<fractal_conf>(r, &fractal_tiles_module);
    // This always returns a configuration, even if it's the default one
    if (!cfg || cfg->indirect )
        return DECLINED;

    apr_array_header_t* tokens = tokenize(r->pool, r->uri);
    if (tokens->nelts < 3)
        return DECLINED; // At least level row column

    sz5 tile;
    memset(&tile, 0, sizeof(tile));


    tile.x = apr_atoi64(ARRAY_POP(tokens, char*)); RETURN_ERR_IF(errno);
    tile.y = apr_atoi64(ARRAY_POP(tokens, char*)); RETURN_ERR_IF(errno);
    tile.l = apr_atoi64(ARRAY_POP(tokens, char*)); RETURN_ERR_IF(errno);

    // Z is optional, defaults to zero
    if (cfg->raster.size.z != 1 && tokens->nelts > 0)
        tile.z = apr_atoi64(ARRAY_POP(tokens, char*));

    // Outside of bounds tile
    if (tile.l >= cfg->raster.n_levels ||
        tile.x >= cfg->raster.rsets[tile.l].w ||
        tile.y >= cfg->raster.rsets[tile.l].h)
        return sendEmptyTile(r, cfg->raster.missing);

    // Adjust the level to the full pyramid one
    tile.l += cfg->raster.skip;

    // Same is true for outside of input bounds
    if (tile.l >= cfg->raster.n_levels ||
        tile.x >= cfg->raster.rsets[tile.l].w ||
        tile.y >= cfg->raster.rsets[tile.l].h)
        return sendEmptyTile(r, cfg->raster.missing);

    size_t tile_size = cfg->raster.pagebytes();
    auto size = cfg->raster.pagesize.x; // Size of the tile, always square
    auto bands = cfg->raster.pagesize.c; // Number of bands, always 1
    // We know it's a byte data type, allocate it
    auto data = apr_palloc(r->pool, tile_size);
    SERVER_ERR_IF(!data, r, "Allocation error");

    //auto t1 = std::chrono::high_resolution_clock::now();
    data = generateTile(tile, cfg, data);
    SERVER_ERR_IF(!data, r, "Rendering error");
    //auto time_span = std::chrono::duration_cast<std::chrono::milliseconds>
    //    (std::chrono::high_resolution_clock::now() - t1).count();
    //LOG(r, "Generated tile %d %d %d %d in %d ms", tile.x, tile.y, tile.l, tile.z, time_span);

    storage_manager src(data, cfg->raster.pagebytes());
    storage_manager dst(apr_palloc(r->pool, cfg->max_size), cfg->max_size);

    const char* out_mime = "image/png";
    // Convert it to a PNG using libicd
    switch (cfg->raster.format) {
    case IMG_PNG: {
        png_params out_params(cfg->raster);
        out_params.raster.size = cfg->raster.pagesize;
        out_params.reset();

        message = png_encode(out_params, src, dst);
        SERVER_ERR_IF(message, r, "%s", message);
        break;
    }
    default:
        SERVER_ERR_IF(true, r, "Unsupported format");
    }

    // Send the tile
    return sendImage(r, dst, out_mime);
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
