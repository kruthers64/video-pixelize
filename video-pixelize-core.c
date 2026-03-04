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
 * Inspired by Video Degradation and Pixelize, creating a chunky effect
 * similar to various video displays.
 *
 * Author: Kruthers <kruthers@kruthers.net>
 *
 */

#include "config.h"
#include <glib/gi18n-lib.h>

#ifdef GEGL_PROPERTIES

#include "video-pixelize-core-gegl-enum.h"

property_double (color_style, _("Color style:  RGB phosphors <-> Full color"), 0.0)
    description(_("Adjust video pixels to be colored from phosophor-style RGB to full color."))
    value_range (0.0, 1.0)
    ui_range    (0.0, 1.0)

property_boolean (clear_bg, _("Transparent backround pixels"), FALSE)
description(_("Some patterns have \"background pixels\" or \"holes\" that are not R, G or B "
    "so are drawn as always black.  This toggles them to be clear."
))

property_int (orientation, _("Orientation"), 0)
description(_("Change the orientation of the pattern."))
    value_range (0, 7)
    ui_range    (0, 7)
    ui_steps    (1, 1)

#else

#define GEGL_OP_AREA_FILTER
#define GEGL_OP_NAME     video_pixelize_core
#define GEGL_OP_C_SOURCE video-pixelize-core.c

#include "gegl-op.h"


// ---- patterns --------------------------------------------------------------------------------------

typedef struct _Pattern
{
    gint    vw, vh;     // full size of vixmap (larger than grid size)
    gint    gw, gh;     // grid cell size
    gint    gx, gy;     // grid start within pattern
    gint    vixn;       // number of vixels (ie. video pixels) in pattern + 1 for "background" pixels
    gint   *vixmap;     // vixel layout
    gint   *colmap;     // color layout; maps vixels to phosphors (0 = bg, 1,2,3 = r,b,g)
} Pattern;

static Pattern *
pattern_clone(Pattern *in_pat) {
    // allocate & build struct
    Pattern *out_pat = g_malloc0 (sizeof(Pattern));
    gint *vixmap = (gint *) g_malloc0 (sizeof(gint) * in_pat->vw * in_pat->vh);
    gint *colmap = (gint *) g_malloc0 (sizeof(gint) * in_pat->vixn);
    out_pat->vixmap = vixmap;
    out_pat->colmap = colmap;

    // copy values
    out_pat->vw = in_pat->vw;
    out_pat->vh = in_pat->vh;
    out_pat->gw = in_pat->gw;
    out_pat->gh = in_pat->gh;
    out_pat->gx = in_pat->gx;
    out_pat->gy = in_pat->gy;
    out_pat->vixn = in_pat->vixn;
    for (gint i = 0 ; i < in_pat->vw * in_pat->vh ; i++)
        out_pat->vixmap[i] = in_pat->vixmap[i];
    for (gint i = 0 ; i < in_pat->vixn ; i++)
        out_pat->colmap[i] = in_pat->colmap[i];

    return out_pat;
}

static void
pattern_free(Pattern *pat) {
    g_free(pat->vixmap);
    g_free(pat->colmap);
    g_free(pat);
}

// A B C D              A E I
// E F G H  transpose   B F J
// I J K L     ->       C G K
//                      D H L
static void
pattern_transpose(Pattern *in_pat, Pattern *out_pat) {
    // ignore colmap/vixn since they never change
    out_pat->vw = in_pat->vh;
    out_pat->vh = in_pat->vw;
    out_pat->gw = in_pat->gh;
    out_pat->gh = in_pat->gw;
    out_pat->gx = in_pat->gy;
    out_pat->gy = in_pat->gx;
    for (gint y = 0 ; y < in_pat->vh ; y++)
        for (gint x = 0 ; x < in_pat->vw ; x++)
            out_pat->vixmap[ x * in_pat->vh + y ] = in_pat->vixmap[ y * in_pat->vw + x ];
}

// A B C D            D C B A
// E F G H  flip h.   H G F E
// I J K L     ->     L K J I
static void
pattern_flip_h(Pattern *in_pat, Pattern *out_pat) {
    // ignore colmap/vixn since they never change
    out_pat->vw = in_pat->vw;
    out_pat->vh = in_pat->vh;
    out_pat->gw = in_pat->gw;
    out_pat->gh = in_pat->gh;
    out_pat->gx = in_pat->vw - in_pat->gx - in_pat->gw;
    out_pat->gy = in_pat->gy;
    for (gint y = 0 ; y < in_pat->vh ; y++)
        for (gint x = 0 ; x < in_pat->vw ; x++)
            out_pat->vixmap[ y * in_pat->vw + in_pat->vw - 1 - x ] = in_pat->vixmap[ y * in_pat->vw + x ];
}

// A B C D            L K J I
// E F G H  rot 180   H G F E
// I J K L     ->     D C B A
static void
pattern_rot_180(Pattern *in_pat, Pattern *out_pat) {
    // ignore colmap/vixn since they never change
    out_pat->vw = in_pat->vw;
    out_pat->vh = in_pat->vh;
    out_pat->gw = in_pat->gw;
    out_pat->gh = in_pat->gh;
    out_pat->gx = in_pat->vw - in_pat->gx - in_pat->gw;
    out_pat->gy = in_pat->vh - in_pat->gy - in_pat->gh;
    gint len = in_pat->vw * in_pat->vh;
    for (gint i = 0 ; i < len ; i++)
        out_pat->vixmap[ len - 1 - i ] = in_pat->vixmap[ i ];
}

// Flip & rotate the pattern to get 8 orientations
// This flip-rotate-flip-etc progression is the most pleasing when using a diagonally biased pattern
static Pattern *
pattern_orient (
    Pattern *in_pat,
    gint orientation
) {
    Pattern *out_pat = pattern_clone(in_pat);
    Pattern *scratch = NULL;

    // flip
    if (orientation == 1) {
        pattern_flip_h(in_pat, out_pat);
    // 90 deg
    } else if (orientation == 2) {
        scratch = pattern_clone(in_pat);
        pattern_transpose(in_pat, scratch);
        pattern_flip_h(scratch, out_pat);
    // 90 + flip
    } else if (orientation == 3) {
        scratch = pattern_clone(in_pat);
        pattern_flip_h(in_pat, out_pat);
        pattern_transpose(out_pat, scratch);
        pattern_flip_h(scratch, out_pat);
    // 180
    } else if (orientation == 4) {
        pattern_rot_180(in_pat, out_pat);
    // 180 + flip
    } else if (orientation == 5) {
        scratch = pattern_clone(in_pat);
        pattern_rot_180(in_pat, scratch);
        pattern_flip_h(scratch, out_pat);
    // 270
    } else if (orientation == 6) {
        scratch = pattern_clone(in_pat);
        pattern_flip_h(in_pat, scratch);
        pattern_transpose(scratch, out_pat);
    // 270 + flip
    } else if (orientation == 7) {
        scratch = pattern_clone(in_pat);
        pattern_flip_h(in_pat, out_pat);
        pattern_transpose(out_pat, scratch);
        pattern_flip_h(scratch, out_pat);
    }

    if (scratch) {
        pattern_free(scratch);
    }

    return out_pat;
}

#include "video-pixelize-patterns.h"


// ---- video pixelize --------------------------------------------------------------------------------

static void
get_vixel_colors(
    GeglProperties  *o,
    Pattern         *pat,
    gfloat          *src_buf,
    gfloat          *vix_rgb,   // return values; float[4] provided by caller
    gint            *scratch    // scratch buffer for mean divisors
) {
    gint channel;

    // zero out color arrays
    gint i;
    for (i = 0 ; i < pat->vixn * 4 ; i++) {
        vix_rgb[i] = 0.0;
    }
    for (i = 0 ; i < pat->vixn ; i++)
        scratch[i] = 0;

    // get per-vixel sums and counts to calculate mean vixel color
    gint u, v, c, vix;
    for (v = 0 ; v < pat->vh ; v++) {
        for (u = 0 ; u < pat->vw ; u++) {
            // determine which vix we are sampling; skip bg and unknown
            vix = pat->vixmap[v * pat->vw + u];
            if (vix > 0) {
                scratch[vix]++;
                for (c = 0 ; c < 4 ; c++) {
                    vix_rgb[vix * 4 + c] += src_buf[(v * pat->vw + u) * 4 + c];
                }
            }
        }
    }

    for (vix = 1 ; vix < pat->vixn ; vix++) {
        channel = pat->colmap[vix];

        // get mean colors (and alpha), used as-is for full color style
        for (c = 0 ; c < 4 ; c++) {
            vix_rgb[vix * 4 + c] /= scratch[vix];
        }

        // dial in RGB phosphor style depending on slider
        if (o->color_style < 1.0) {
            for (c = 0 ; c < 3 ; c++) {
                if (channel - 1 != c) {
                    vix_rgb[vix * 4 + c] *= o->color_style;
                }
            }
        }
    }
}

static gfloat
set_pixel_color (
    GeglProperties *o,
    gfloat *dst_buf, gint dst_idx,
    gfloat *vix_rgb, gint vix_idx,
    gint channel,
    gfloat src_alpha
) {
    // color
    gint c;
    for (c = 0 ; c < 3 ; c++) {
        dst_buf[dst_idx + c] = vix_rgb[vix_idx + c];
    }

    // alpha
    if (channel == 0) {
        if (o->clear_bg) {
            dst_buf[dst_idx + 3] = 0.0;
        } else {
            // alpha channel for bg color must be set to source alpha; otherwise all bg pixels
            // in a grid cell will get the same alpha value, causing bad artifacting
            dst_buf[dst_idx + 3] = src_alpha;
        }
    } else {
        dst_buf[dst_idx + 3] = vix_rgb[vix_idx + 3];
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
    return roi_offset - ((roi_offset - world_origin) % grid_size);
}

static gboolean
process (GeglOperation       *operation,
         GeglBuffer          *input,
         GeglBuffer          *output,
         const GeglRectangle *roi,
         gint                 level)
{
    GeglProperties *o = GEGL_PROPERTIES (operation);
    const Babl *format = gegl_operation_get_format(operation, "output");

    Pattern *pat = pattern_orient(patterns[o->pattern], o->orientation);

    // align grid
    GeglRectangle *world = gegl_operation_source_get_bounding_box(operation, "input");
    gint startx = align_grid(pat->gx, pat->gw, roi->x, world->x);
    gint starty = align_grid(pat->gy, pat->gh, roi->y, world->y);

    // allocations
    GeglRectangle *src_rect = gegl_rectangle_new(0, 0, pat->vw, pat->vh);
    GeglRectangle *dst_rect = gegl_rectangle_new(0, 0, pat->gw, pat->gh);
    GeglRectangle *clp_rect = gegl_rectangle_new(0, 0, 0, 0);
    GeglRectangle *grid_rect = gegl_rectangle_new(0, 0, pat->gw, pat->gh);
    GeglRectangle *gclp_rect = gegl_rectangle_new(0, 0, 0, 0);
    gfloat *src_buf = g_new(gfloat, pat->vw * pat->vh * 4);
    gfloat *dst_buf = g_new(gfloat, pat->gw * pat->gh * 4);
    GeglBuffer *inter_gbuf = gegl_buffer_new(grid_rect, format);

    // arrays for calculating vixel colors
    gfloat *vix_rgb = g_new(gfloat, pat->vixn * 4);
    gint *scratch = g_new(gint, pat->vixn);
    gfloat src_alpha;

    // step through grid cells
    gint gridx, gridy;
    gint x, y, u, v, vix;
    for (gridy = starty ; gridy < roi->y + roi->height ; gridy += pat->gh) {
        for (gridx = startx ; gridx < roi->x + roi->width ; gridx += pat->gw) {
            src_rect->x = gridx - pat->gx;
            src_rect->y = gridy - pat->gy;
            dst_rect->x = gridx;
            dst_rect->y = gridy;

            // get vixel colors
            gegl_buffer_get(input, src_rect, 1.0, format, src_buf, GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_CLAMP);
            get_vixel_colors(o, pat, src_buf, vix_rgb, scratch);

            // process each pixel
            for (y = 0 ; y < pat->gh ; y++) {
                v = y + pat->gy;
                for (x = 0 ; x < pat->gw ; x++) {
                    u = x + pat->gx;
                    // determine which vix we are drawing; since we are drawing only within the grid cell
                    // we should not see -1 indexes
                    vix = pat->vixmap[v * pat->vw + u];
                    src_alpha = src_buf[(v * pat->vw + u) * 4 + 3];
                    set_pixel_color(o, dst_buf, (y * pat->gw + x) * 4, vix_rgb, vix * 4, pat->colmap[vix], src_alpha);
                }
            }

            // go through an intermediate gegl buffer to more easily handle clipping to roi
            gegl_buffer_set(inter_gbuf, grid_rect, 0, format, dst_buf, GEGL_AUTO_ROWSTRIDE);
            gegl_rectangle_intersect(clp_rect, roi, dst_rect);
            gegl_rectangle_set(gclp_rect,
                clp_rect->x - dst_rect->x, clp_rect->y - dst_rect->y,
                clp_rect->width, clp_rect->height
            );

            // write result
            gegl_buffer_copy(inter_gbuf, gclp_rect, GEGL_ABYSS_CLAMP, output, clp_rect);
        }
    }

    g_free(src_buf);
    g_free(dst_buf);
    g_free(vix_rgb);
    g_free(scratch);
    g_free(src_rect);
    g_free(dst_rect);
    g_free(clp_rect);
    g_free(grid_rect);
    g_free(gclp_rect);
    g_object_unref(inter_gbuf);

    pattern_free(pat);

    return TRUE;
}

static void
prepare (GeglOperation *operation)
{
    const Babl *format = babl_format_with_space ("R'G'B'A float", gegl_operation_get_source_space (operation, "input"));

    GeglOperationAreaFilter *op_area;
    op_area = GEGL_OPERATION_AREA_FILTER (operation);

    // this needs to be some non-zero amount or we get artifacting when scaling up
    // TODO: determine if the amount needed here depends on the pattern size
    op_area->left   = 
    op_area->right  = 100;
    op_area->top    = 
    op_area->bottom = 100;

    gegl_operation_set_format (operation, "input", format);
    gegl_operation_set_format (operation, "output", format);
}

static void
gegl_op_class_init (GeglOpClass *klass)
{
    GeglOperationClass           *operation_class = GEGL_OPERATION_CLASS (klass);
    GeglOperationFilterClass     *filter_class    = GEGL_OPERATION_FILTER_CLASS (klass);

    operation_class->prepare = prepare;
    filter_class->process    = process;

    gegl_operation_class_set_keys (operation_class,
        "name",             "kruthers:video-pixelize-core",
        "title",          _("Video Pixelize Core"),
        "categories",       "hidden",
        "description",    _("This function is like a combination of Video Degradation and Pixelize, "
                            "creating a chunky pixel effect similar to various video displays."),
        NULL);
}

#endif
