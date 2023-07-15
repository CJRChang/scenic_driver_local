#pragma once

#include <cairo.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include "ops/script_ops.h"

typedef struct {
  FT_Library ft_library;
  float font_size;
  text_align_t text_align;
  text_base_t text_base;
  cairo_surface_t* surface;
  cairo_t* cr;
} CAIROcontext_t;

typedef struct {
  cairo_surface_t* surface;
  cairo_pattern_t* pattern;
} image_data_t;
