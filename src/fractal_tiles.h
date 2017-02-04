/*
* mod_fractal_tiles.cpp
* Lucian Plesea
* (C) 2017
*/

#if !defined(MOD_FRACTAL_TILES)

struct sz {
  apr_int64_t x, y, z, c, l;
};

typedef struct {
  apr_array_header_t *regexp;

  // Full raster size in pixels
  struct sz size;
  // Page size in pixels
  struct sz pagesize;

  // Levels to skip at the top
  int skip_levels;
  int n_levels;
  const char *mime_type;

} fractal_conf;

#endif
