#include "stdio.h"

/* This file is an image processing operation for GEGL
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 *
 * Exchange one color with the other (settable threshold to convert from
 * one color-shade to another...might do wonders on certain images, or be
 * totally useless on others).
 *
 * Author: Adam D. Moss <adam@foxbox.org>
 *
 * GEGL port: Thomas Manni <thomas.manni@free.fr>
 *
 */

#include "config.h"
#include <glib/gi18n-lib.h>

#ifdef GEGL_PROPERTIES

enum_start (gegl_video_degradation_type)
  enum_value (GEGL_VIDEO_DEGRADATION_TYPE_STAGGERED, "staggered",
              N_("Staggered"))
  enum_value (GEGL_VIDEO_DEGRADATION_TYPE_LARGE_STAGGERED, "large-staggered",
              N_("Large staggered"))
  enum_value (GEGL_VIDEO_DEGRADATION_TYPE_STRIPED, "striped",
              N_("Striped"))
  enum_value (GEGL_VIDEO_DEGRADATION_TYPE_WIDE_STRIPED, "wide-striped",
              N_("Wide striped"))
  enum_value (GEGL_VIDEO_DEGRADATION_TYPE_LONG_STAGGERED, "long-staggered",
              N_("Long staggered"))
  enum_value (GEGL_VIDEO_DEGRADATION_TYPE_3X3, "3x3",
              N_("3x3"))
  enum_value (GEGL_VIDEO_DEGRADATION_TYPE_LARGE_3X3, "large-3x3",
              N_("Large 3x3"))
  enum_value (GEGL_VIDEO_DEGRADATION_TYPE_LARGE_2X3, "large-2x3",
              N_("Large 2x3"))
  enum_value (GEGL_VIDEO_DEGRADATION_TYPE_Hex, "hex",
              N_("Hex"))
  enum_value (GEGL_VIDEO_DEGRADATION_TYPE_DOTS, "dots",
              N_("Dots"))
enum_end (GeglVideoDegradationPlusType)

property_enum (pattern, _("Pattern"), GeglVideoDegradationPlusType,
               gegl_video_degradation_type,
               GEGL_VIDEO_DEGRADATION_TYPE_LARGE_2X3)
  description (_("Type of RGB pattern to use"))

property_boolean (additive, _("Additive"), TRUE)
  description(_("Whether the function adds the result to the original image."))
property_boolean (additive2, _("Additive2"), TRUE)
  description(_("Whether the function adds the result to the original image."))

property_boolean (rotated, _("Rotated"), FALSE)
  description(_("Whether to rotate the RGB pattern by ninety degrees."))

#else

/* #define GEGL_OP_POINT_FILTER */
#define GEGL_OP_AREA_FILTER
#define GEGL_OP_NAME     video_pixels
#define GEGL_OP_C_SOURCE video-pixels.c

#include "gegl-op.h"

#define MAX_PATTERNS      10
#define MAX_PATTERN_SIZE 108

static const gint   pattern_width[MAX_PATTERNS] = { 2, 4, 1, 1, 2, 3, 6, 6, 6, 5 };
static const gint   pattern_height[MAX_PATTERNS] = { 6, 12, 3, 6, 12, 3, 6, 4, 18, 15 };

static const gint pattern[MAX_PATTERNS][MAX_PATTERN_SIZE] =
{
  {
    0, 1,
    0, 2,
    1, 2,
    1, 0,
    2, 0,
    2, 1,
  },
  {
    0, 0, 1, 1,
    0, 0, 1, 1,
    0, 0, 2, 2,
    0, 0, 2, 2,
    1, 1, 2, 2,
    1, 1, 2, 2,
    1, 1, 0, 0,
    1, 1, 0, 0,
    2, 2, 0, 0,
    2, 2, 0, 0,
    2, 2, 1, 1,
    2, 2, 1, 1,
  },
  {
    0,
    1,
    2,
  },
  {
    0,
    0,
    1,
    1,
    2,
    2,
  },
  {
    0, 1,
    0, 1,
    0, 2,
    0, 2,
    1, 2,
    1, 2,
    1, 0,
    1, 0,
    2, 0,
    2, 0,
    2, 1,
    2, 1,
  },
  {
    0, 1, 2,
    2, 0, 1,
    1, 2, 0,
  },
  {
    0, 0, 1, 1, 2, 2,
    0, 0, 1, 1, 2, 2,
    2, 2, 0, 0, 1, 1,
    2, 2, 0, 0, 1, 1,
    1, 1, 2, 2, 0, 0,
    1, 1, 2, 2, 0, 0,
  },
  {
    0, 0, 1, 1, 2, 2,
    0, 0, 1, 1, 2, 2,
    1, 2, 2, 0, 0, 1,
    1, 2, 2, 0, 0, 1,
  },
  {
    2, 2, 0, 0, 0, 0,
    2, 2, 2, 0, 0, 2,
    2, 2, 2, 2, 2, 2,
    2, 2, 2, 1, 1, 2,
    2, 2, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1,
    0, 0, 1, 1, 1, 1,
    0, 0, 0, 1, 1, 0,
    0, 0, 0, 0, 0, 0,
    0, 0, 0, 2, 2, 0,
    0, 0, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2,
    1, 1, 2, 2, 2, 2,
    1, 1, 1, 2, 2, 1,
    1, 1, 1, 1, 1, 1,
    1, 1, 1, 0, 0, 1,
    1, 1, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0,
  },
  {
    0, 1, 2, 0, 0,
    1, 1, 1, 2, 0,
    0, 1, 2, 2, 2,
    0, 0, 1, 2, 0,
    0, 1, 1, 1, 2,
    2, 0, 1, 2, 2,
    0, 0, 0, 1, 2,
    2, 0, 1, 1, 1,
    2, 2, 0, 1, 2,
    2, 0, 0, 0, 1,
    1, 2, 0, 1, 1,
    2, 2, 2, 0, 1,
    1, 2, 0, 0, 0,
    1, 1, 2, 0, 1,
    1, 2, 2, 2, 0,
  }
};

static GeglRectangle
get_bounding_box (GeglOperation *self)
{
  GeglRectangle  result = { 0, 0, 0, 0 };
  GeglRectangle *in_rect;

  in_rect = gegl_operation_source_get_bounding_box (self, "input");
  if (in_rect)
    {
      result = *in_rect;
    }

  return result;
}

static void
prepare (GeglOperation *operation)
{
  const Babl     *format = babl_format_with_space ("R'G'B'A float", gegl_operation_get_source_space (operation, "input"));

  GeglOperationAreaFilter *op_area;
  op_area = GEGL_OPERATION_AREA_FILTER (operation);

  op_area->left   =
  op_area->right  = 0;//25;
  op_area->top    =
  op_area->bottom = 0;//25;

  gegl_operation_set_format (operation, "input", format);
  gegl_operation_set_format (operation, "output", format);

//    printf("\nkruthers:video-degradation-plus\n");
//    printf("\n");

}

static gboolean
process (GeglOperation       *operation,
         void                *in_buf,
         void                *out_buf,
         glong                n_pixels,
         const GeglRectangle *roi,
         gint                 level)
{
    GeglProperties *o = GEGL_PROPERTIES (operation);
    gfloat *input  = in_buf;
    gfloat *output = out_buf;
    float value;
    gint x, y;
    gint real_x, real_y;
    gint b;
    gint sel_b;
    gint idx;

    for (y = 0 ; y < roi->height ; y++) {
        real_y = roi->y + y;
        for (x = 0 ; x < roi->width ; x++) {
            real_x = roi->x + x;

            if (o->rotated) {
                sel_b = pattern[o->pattern][pattern_width[o->pattern]*
                (real_x % pattern_height[o->pattern]) +
                real_y % pattern_width[o->pattern] ];
            } else {
                sel_b = pattern[o->pattern][pattern_width[o->pattern]*
                (real_y % pattern_height[o->pattern]) +
                real_x % pattern_width[o->pattern] ];
            }

            for (b = 0; b < 4; b++) {
                idx = (x + y * roi->width) * 4 + b;
                if (b < 3 ) {
                    value = (sel_b == b) ? input[idx] : 0.f;
                    if (o->additive) {
                        gfloat temp = value + input[idx];
                        value = MIN (temp, 1.0);
                    } else if (o->additive2) {
                        gfloat temp = value + (input[idx] * 2.0f / 3.0f);
                        value = MIN (temp, 1.0);
                    }
                    output[idx] = value;
                } else {
                    output[idx] = input[idx];
                }
            }
        }
    }

    return TRUE;
}

static gboolean
process2 (GeglOperation       *operation,
         GeglBuffer          *in_buf,
         GeglBuffer          *out_buf,
         const GeglRectangle *roi,
         gint                 level)
{
    GeglProperties *o = GEGL_PROPERTIES (operation);
    GeglBuffer *input  = in_buf;
    GeglBuffer *output = out_buf;
    gint x, y;

    gint sx = 10;
    gint sy = 15;
    gfloat tmp = 0.0f;

    GeglRectangle *whole_region = gegl_operation_source_get_bounding_box (operation, "input");

    GeglColor *color = gegl_color_new ("white");
    GeglRandom *r = gegl_random_new();

    gegl_color_set_rgba(
        color,
        gegl_random_float(r, x, y, 0, 0),
        gegl_random_float(r, x, y, 0, 1),
        gegl_random_float(r, x, y, 0, 2),
        1.0
    );
    gegl_buffer_set_color (output, roi, color);

    printf("rx,ry = %4d,%-4d  rw,rh = %4d,%-4d  x,y = %4d,%-4d  w,h = %4d,%-4d  area = %d\n",
        whole_region->x, whole_region->y, whole_region->width, whole_region->height,
        roi->x, roi->y, roi->width, roi->height, roi->width * roi->height);
    return TRUE;

    for (y = 0 ; y < roi->height ; y += sy) {
        for (x = 0 ; x < roi->width ; x += sx) {

            // color
            tmp = (x/sx + y/sy) % 2 ? 1.0 : 0.0;
            gegl_color_set_rgba(
                color,
                tmp, tmp, tmp,
                /*
                gegl_random_float(r, x, y, 0, 0),
                gegl_random_float(r, x, y, 0, 1),
                gegl_random_float(r, x, y, 0, 2),
                */
                1.0
            );

            // rectangle
            GeglRectangle rect = { roi->x + x, roi->y + y, sx, sy };
            gegl_rectangle_intersect (&rect, whole_region, &rect);
            if (rect.width < 1 || rect.height < 1)
                continue;
            gegl_buffer_set_color (output, &rect, color);
        }
    }
    gegl_random_free(r);

    return TRUE;
}

// TODO: if gw != vw you're gonna have a bad day... (maybe, or it might be fixed)
typedef struct _Pattern
{
    gint    gx, gy;     // grid start within pattern
    gint    gw, gh;     // grid cell size
    gint    vixn;       // number of vixels (ie. video pixels) in pattern + 1 for black
    gint   *vixmap;     // vixel layout
    gint    vw, vh;     // full size of vixmap (larger than grid size)
    gint   *colmap;     // color layout; maps vixels to phosphors
} Pattern;

/*
    dev pattern 1:

    colors      vixels
                            color map
                  44
    rrgg        1144        r = 1, 6
    rrgg        1144        g = 2, 4, 7
    rrBB        1155        b = 3, 5
    ggBB        2255
    ggBB        2255        grid
    ggrr        2266
    BBrr        3366        x, y = 0, 1
    BBrr        3366        w, h = 4, 9
    BBgg        3377
                  77
                  77
*/
    gint vixmap1[4*12] = {
        -1,-1, 4, 4,
         1, 1, 4, 4,
         1, 1, 4, 4,
         1, 1, 5, 5,
         2, 2, 5, 5,
         2, 2, 5, 5,
         2, 2, 6, 6,
         3, 3, 6, 6,
         3, 3, 6, 6,
         3, 3, 7, 7,
        -1,-1, 7, 7,
        -1,-1, 7, 7,
    };
    // 1,2,3 = r,g,b    0  1  2  3  4  5  6  7
    // 1,2,3 = r,g,b       r  g  b  g  b  r  g
    gint colmap1[8] = { 0, 1, 2, 3, 2, 3, 1, 2 };
    Pattern dev1 = { 0, 1, 4, 9, 8, vixmap1, 4, 12, colmap1 };

/*
    dev pattern 2:

    vixels                          color map

    -1  -1  -1  -1  13  13  -1      r = 1, 5, 8, 12, 13, 16, 17
    -1  4   4   0   13  13  -1      g = 3, 4, 7, 11, 15, 19
    -1  4   4   10  10  0   -1      b = 2, 6, 9, 10, 14, 18
    1   1   0   10  10  17  17
    1   1   7   7   0   17  17      grid & vixmap
    -1  0   7   7   14  14  -1
    -1  5   5   0   14  14  -1      gx, gy = 1, 1
    -1  5   5   11  11  0   -1      gw, gh = 5, 15
    2   2   0   11  11  18  18
    2   2   8   8   0   18  18      vw, vw = 7, 17
    -1  0   8   8   15  15  -1
    -1  6   6   0   15  15  -1
    -1  6   6   12  12  0   -1
    3   3   0   12  12  19  19
    3   3   9   9   0   19  19
    -1  0   9   9   16  16  -1
    -1  -1  -1  -1  16  16  -1
*/

    gint vixmap2[7*17] = {
        -1, -1, -1, -1, 13, 13, -1,
        -1, 4 , 4 , 0 , 13, 13, -1,
        -1, 4 , 4 , 10, 10, 0 , -1,
        1 , 1 , 0 , 10, 10, 17, 17,
        1 , 1 , 7 , 7 , 0 , 17, 17,
        -1, 0 , 7 , 7 , 14, 14, -1,
        -1, 5 , 5 , 0 , 14, 14, -1,
        -1, 5 , 5 , 11, 11, 0 , -1,
        2 , 2 , 0 , 11, 11, 18, 18,
        2 , 2 , 8 , 8 , 0 , 18, 18,
        -1, 0 , 8 , 8 , 15, 15, -1,
        -1, 6 , 6 , 0 , 15, 15, -1,
        -1, 6 , 6 , 12, 12, 0 , -1,
        3 , 3 , 0 , 12, 12, 19, 19,
        3 , 3 , 9 , 9 , 0 , 19, 19,
        -1, 0 , 9 , 9 , 16, 16, -1,
        -1, -1, -1, -1, 16, 16, -1,
    };
    // 1,2,3 = r,g,b     0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19
    gint colmap2[20] = { 0, 1, 3, 2, 2, 1, 3, 2, 1, 3, 3, 2, 1, 1, 3, 2, 1, 1, 3, 2 };
    Pattern dev2 = { 1, 1, 5, 15, 20, vixmap2, 7, 17, colmap2 };



static void
get_cell_mean_values(
    Pattern             *pat,
    GeglRectangle       *src_rect,
    gfloat              *src_buf,
    gfloat              *means,     // return; float[4] provided by caller
    gint                *meanc      // scratch buffer to count mean samples, for mean divisor
) {
    // zero mean arrays
    gint i;
    for (i = 0 ; i < pat->vixn * 4 ; i++)
        means[i] = 0;
    for (i = 0 ; i < pat->vixn ; i++)
        meanc[i] = 0;

    // get per-vixel sums and counts for means
    gint u, v, c, vix;
    for (v = 0 ; v < pat->vh ; v++) {
        for (u = 0 ; u < pat->vw ; u++) {
            // determine which vix we are sampling; ignore indexes of -1
            vix = pat->vixmap[v * pat->vw + u];
            if (vix > -1) {
                meanc[vix]++;
                for (c = 0 ; c < 4 ; c++) {
                    means[vix * 4 + c] += src_buf[(v * pat->vw + u) * 4 + c];
                }
            }
        }
    }
    // means = sums / counts
    for (vix = 1 ; vix < pat->vixn ; vix++) {
        for (c = 0 ; c < 4 ; c++) {
//            if (vix == 3 && c < 3) {
//                means[vix * 4 + c] = 0;
//            } else {
                means[vix * 4 + c] /= meanc[vix];
//            }
        }
    }
}

//  Calculate the position of our pattern's grid with respect to the current ROI, using
//  the world origin as the canonical start of our grid (ie. keep it aligned to the full
//  image beyond the current selection).
//
//  We should return the closest grid point to the left and above the current ROI, or
//  the start of the ROI itself if it happens to align perfectly with the grid.
static gint
align_grid(gint grid_offset, gint grid_size, gint roi_offset, gint world_origin) {
    return roi_offset - ((roi_offset - world_origin) % grid_size); // - grid_offset;   TODO - REMOVE!
}

// TEMP!!!
static void
cpy(gfloat src[4], gfloat dst[4]) {
    for (gint i = 0 ; i < 4 ; i++)
        dst[i] = src[i];
}

static void
dumprect(char *name, const GeglRectangle *rect) {
    printf("%s: w,h = %d,%d, x,y = %d,%d\n", name, rect->width, rect->height, rect->x, rect->y);
}

static gboolean
process3 (GeglOperation       *operation,
         GeglBuffer          *input,
         GeglBuffer          *output,
         const GeglRectangle *roi,
         gint                 level)
{
    Pattern *pat = &dev1;

    GeglRectangle *world = gegl_operation_source_get_bounding_box(operation, "input");
    const Babl *format = gegl_operation_get_format(operation, "output");

    gint startx = align_grid(pat->gx, pat->gw, roi->x, world->x);
    gint starty = align_grid(pat->gy, pat->gh, roi->y, world->y);

    /* * /
    GeglColor *color = gegl_color_new ("white");
    GeglRandom *r = gegl_random_new();
    gegl_color_set_rgba(
        color,
        gegl_random_float(r, roi->x, roi->y, 0, 0),
        gegl_random_float(r, roi->x, roi->y, 0, 1),
        gegl_random_float(r, roi->x, roi->y, 0, 2),
        1.0
    );
    gegl_buffer_set_color (output, roi, color);
    /* */

    /* * /
    printf("wx,wy = %5d,%-5d ; rx,ry = %5d,%-5d ; rw,rh = %5d,%-5d ; startx,starty = %5d,%-5d\n",
        world->x, world->y,
        roi->x, roi->y, roi->width, roi->height, startx, starty);
    /* */

    // dst is size of grid cell, src is larger to sample pattern overlaps
    // (always same size so reuse rects & bufs in loop)
    GeglRectangle *src_rect = gegl_rectangle_new(0, 0, pat->vw, pat->vh);
    GeglRectangle *dst_rect = gegl_rectangle_new(0, 0, pat->gw, pat->gh);
    GeglRectangle *clp_rect = gegl_rectangle_new(0, 0, 0, 0);

    gfloat *src_buf = g_new(gfloat, pat->vw * pat->vh * 4);
    gfloat *dst_buf = g_new(gfloat, pat->gw * pat->gh * 4);

    GeglRectangle *grid_rect = gegl_rectangle_new(0, 0, pat->gw, pat->gh);
    GeglRectangle *gclp_rect = gegl_rectangle_new(0, 0, 0, 0);
    GeglBuffer *buftmp = gegl_buffer_new(grid_rect, format);

    // arrays for calculating vixel mean colors
    gfloat *means = g_new(gfloat, pat->vixn * 4);
    gint *meanc = g_new(gint, pat->vixn);

    gfloat red[4] = { 1.0, 0.0, 0.0, 1.0};
    gfloat blu[4] = { 0.0, 1.0, 0.0, 1.0};
    gfloat grn[4] = { 0.0, 0.0, 1.0, 1.0};
    gfloat cyn[4] = { 0.0, 1.0, 1.0, 1.0};
    gfloat mag[4] = { 1.0, 0.0, 1.0, 1.0};
    gfloat ylo[4] = { 1.0, 1.0, 0.0, 1.0};
    gfloat wht[4] = { 1.0, 1.0, 1.0, 1.0};
    gfloat blk[4] = { 0.0, 0.0, 0.0, 1.0};
    gfloat nul[4] = { 0.0, 0.0, 0.0, 0.0};

GeglRandom *r = gegl_random_new();
gfloat al = gegl_random_float(r, 9, 1240, 0, 0);
gfloat col[4];
gfloat tgl = 0;
gfloat cr = gegl_random_float_range(r, 0, 0, 0, 0, 0.7, 1.0);
gfloat cg = gegl_random_float_range(r, 0, 0, 0, 1, 0.7, 1.0);
gfloat cb = gegl_random_float_range(r, 0, 0, 0, 2, 0.7, 1.0);

GeglColor *color = gegl_color_new ("black");
gegl_buffer_set_color (output, roi, color);

    // step through grid cells
    gint gridx, gridy;
//if (! (roi->x == 0 && roi->y == 0))
    for (gridy = starty ; gridy < roi->y + roi->height ; gridy += pat->gh) {
        for (gridx = startx ; gridx < roi->x + roi->width ; gridx += pat->gw) {
            src_rect->x = gridx - pat->gx;
            src_rect->y = gridy - pat->gy;
            dst_rect->x = gridx;
            dst_rect->y = gridy;

//printf("%4d,%-4d ", gridx, gridy);
//tgl = ((gridx-world->x)/pat->gw + (gridy-world->y)/pat->gh) % 2 ? 1.0 : 0.7;

            // get vixel mean colors
            gegl_buffer_get(input, src_rect, 1.0, format, src_buf, GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_CLAMP);
            get_cell_mean_values(pat, src_rect, src_buf, means, meanc);

            // TBD: draw_vixels(pat, src_rect, src_buf, dst_rect, dst_buf, means);
            gint x, y, u, v, c, vix;
            for (y = 0 ; y < pat->gh ; y++) {
                v = y + pat->gy;
                for (x = 0 ; x < pat->gw ; x++) {
                    u = x + pat->gx;
                    // determine which vix we are drawing; since we are drawing only within the grid cell
                    // we should not see -1 vixel indexes
                    vix = pat->vixmap[v * pat->vw + u];
/*
                    switch(vix) {
                        case 1:
                            cpy(red, col); break;
                        case 2:
                            cpy(blu, col); break;
                        case 3:
                            cpy(grn, col); break;
                        case 4:
                            cpy(cyn, col); break;
                        case 5:
                            cpy(mag, col); break;
                        case 6:
                            cpy(ylo, col); break;
                        case 7:
                            cpy(wht, col); break;
                        case 0:
                            cpy(blk, col); break;
                        default:
                            cpy(nul, col); break;
                    }
*/
                    /* */
                    gfloat phosphor = means[vix * 4 + pat->colmap[vix] - 1];
                    for (c = 0 ; c < 3 ; c++) {
                        if (pat->colmap[vix] - 1 == c) {
                            dst_buf[(y * pat->gw + x) * 4 + c] = means[vix * 4 + c];
                        } else {
                            dst_buf[(y * pat->gw + x) * 4 + c] = 0.0;
                        }
                    }
                    dst_buf[(y * pat->gw + x) * 4 + c] = means[vix * 4 + 3];

                    /* * /
                    dst_buf[(y * pat->gw + x) * 4 + 0] *= cr;
                    dst_buf[(y * pat->gw + x) * 4 + 1] *= cg;
                    dst_buf[(y * pat->gw + x) * 4 + 2] *= cb;
                    /* */
                    /* * /
                    dst_buf[(y * pat->gw + x) * 4 + 0] = cr * tgl;
                    dst_buf[(y * pat->gw + x) * 4 + 1] = cg * tgl;
                    dst_buf[(y * pat->gw + x) * 4 + 2] = cb * tgl;
                    dst_buf[(y * pat->gw + x) * 4 + 3] = 1.0;
                    /* */
                }
            }

/* */
            // blit to output
            gegl_buffer_set(buftmp, grid_rect, 0, format, dst_buf, GEGL_AUTO_ROWSTRIDE);
            gegl_rectangle_intersect(clp_rect, roi, dst_rect);
            gegl_rectangle_set(gclp_rect,
                clp_rect->x - dst_rect->x, clp_rect->y - dst_rect->y,
                clp_rect->width, clp_rect->height);

/*
            printf("roi:       wxh x,y = %3dx%-3d %3d,%-3d   ", roi->width, roi->height, roi->x, roi->y);
            printf("src_rect:  wxh x,y = %3dx%-3d %3d,%-3d   ", src_rect->width, src_rect->height, src_rect->x, src_rect->y);
            printf("dst_rect:  wxh x,y = %3dx%-3d %3d,%-3d   ", dst_rect->width, dst_rect->height, dst_rect->x, dst_rect->y);
            if (gegl_rectangle_equal(clp_rect, dst_rect)) {
                printf("noclip");
            } else {
                printf("clp_rect:  wxh x,y = %3dx%-3d %3d,%-3d   ", clp_rect->width, clp_rect->height, clp_rect->x, clp_rect->y);
            }
            printf("\n");
*/
            if (clp_rect->width < 1 || clp_rect->height < 1) {
                printf("dst_rect = %d,%d,%d,%d   roi = %d,%d,%d,%d\n",
                    dst_rect->x, dst_rect->y, dst_rect->width, dst_rect->height, 
                    roi->x, roi->y, roi->width, roi->height);
                continue;
            }
/* */
            //gegl_buffer_set(output, clp_rect, 0, format, dst_buf, GEGL_AUTO_ROWSTRIDE);
            gegl_buffer_copy(buftmp, gclp_rect, GEGL_ABYSS_WHITE, output, clp_rect);


            /* * /
            for (gint vix = 0 ; vix < pat->vixn ; vix++) {
                printf("vix=%d ; mean r,b,g,a = %5f, %5f, %5f, %5f\n",
                    vix, means[vix * 4], means[vix * 4 + 1], means[vix * 4 + 2], means[vix * 4 + 3]);
            }
            /* */

            /* * /
            GeglColor *color = gegl_color_new("white");
            gegl_color_set_rgba(color, means[0], means[1], means[2], means[3]);
            printf("gx,gy = %5d,%-5d ; r,g,b,a = %5f, %5f, %5f, %5f\n",
                gridx, gridy, means[0], means[1], means[2], means[3]);
            gegl_buffer_set_color(output, roi, color);
            /* */

        }
    }
//printf("\n");

    g_free(src_buf);
    g_free(dst_buf);
    g_free(means);
    g_free(meanc);
    g_free(src_rect);
    g_free(dst_rect);
    g_free(clp_rect);
    g_free(grid_rect);
    g_free(gclp_rect);
    g_object_unref(buftmp);

    return TRUE;
}

static void
gegl_op_class_init (GeglOpClass *klass)
{
  GeglOperationClass            *operation_class;
  /* GeglOperationPointFilterClass *filter_class; */
  GeglOperationFilterClass *filter_class;

  operation_class = GEGL_OPERATION_CLASS (klass);
  /* filter_class    = GEGL_OPERATION_POINT_FILTER_CLASS (klass); */
  filter_class    = GEGL_OPERATION_FILTER_CLASS (klass);

  operation_class->prepare = prepare;
  /* operation_class->get_bounding_box = get_bounding_box; */

  filter_class->process    = process3;

  gegl_operation_class_set_keys (operation_class,
    "name",           "kruthers:video-pixels",
    "title",          _("Video Pixels"),
    "categories",     "distort",
    "description", _("This function is a mash-up of Video Degradation and Pixelize, "
                     "creating a chunky effect similar to various video displays."),
    "gimp:menu-path", "<Image>/Filters/Kruthers",
    "gimp:menu-label", "Video Pixels",
    NULL);
}

#endif
