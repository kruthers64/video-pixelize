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

// TODO: REMOVE!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!11
#include "stdio.h"

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

property_int (direction, _("Direction"), 0)
description(_("Change the orientation of the pattern."))
    value_range (0, 3)
    ui_range    (0, 3)
    ui_steps    (1, 1)

#else

#define GEGL_OP_AREA_FILTER
#define GEGL_OP_NAME     video_pixelize_core
#define GEGL_OP_C_SOURCE video-pixelize-core.c

#include "gegl-op.h"


typedef struct _Pattern
{
    gint    gx, gy;     // grid start within pattern
    gint    gw, gh;     // grid cell size
    gint    vixn;       // number of vixels (ie. video pixels) in pattern + 1 for "background" pixels
    gint   *vixmap;     // vixel layout
    gint    vw, vh;     // full size of vixmap (larger than grid size)
    gint   *colmap;     // color layout; maps vixels to phosphors (0 = bg, 1,2,3 = r,b,g)
} Pattern;

#include "video-pixelize-patterns.h"


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

// ---- pattern helper routines -----------------------------------------------------------------------

/*
static Pattern *
new_pattern(gint w, gint h) {
    gint *vixmap = (gint *) g_malloc0 (sizeof(gint) * w * h);
    Pattern *pat = g_malloc (sizeof (Pattern));
    pat->vixmap = vixmap;
    return pat;
}

static Pattern *
free_pattern(Pattern *pat) {
    g_free(pat->vixmap);
    g_free(pat);
}
*/

// A B C D  xpose  A E I   flipH   I E A
// E F G H         B F J           J F B
// I J K L         C G K           K G C
//                 D H L           L H D

static void
pat_transpose(Pattern *pat, gint *vixmap_in, gint *vixmap_out) {
    for (gint y = 0 ; y < pat->vh ; y++)
        for (gint x = 0 ; x < pat->vw ; x++)
            vixmap_out[ x * pat->vh + y ] = vixmap_in[ y * pat->vw + x ];
}

// A:   0,0     ->      2,0
// idx: 0       ->      2
//      0*3

static void
pat_flip_h(Pattern *pat, gint *vixmap_in, gint *vixmap_out) {
    for (gint y = 0 ; y < pat->vh ; y++)
        for (gint x = 0 ; x < pat->vw ; x++)
            vixmap_out[ y * pat->vw + pat->vw - 1 - x ] = vixmap_in[ y * pat->vw + x ];
}

static void
pat_copy(Pattern *pat, gint *vixmap_in, gint *vixmap_out) {
    gint len = pat->vw * pat->vh;
    for (gint i = 0 ; i < len ; i++)
        vixmap_out[ i ] = pat->vixmap[ i ];
}

static void
pat_reverse(Pattern *pat, gint *vixmap_in, gint *vixmap_out) {
    gint len = pat->vw * pat->vh;
    for (gint i = 0 ; i < len ; i++)
        vixmap_out[ len - 1 - i ] = pat->vixmap[ i ];
}

//  Flip & rotate the pattern to get 4 directions:
//    0 = no change
//    1 = flip
//    2 = rotate
//    3 = flip & rotate
static Pattern *
pattern_set_direction (
    Pattern *in_pat,
    gint direction
) {
//printf("pattern_set_direction\n");
    gint *vixmap = (gint *) g_malloc (sizeof(gint) * in_pat->vw * in_pat->vh);
    gint *scratch = (gint *) g_malloc (sizeof(gint) * in_pat->vw * in_pat->vh);

    Pattern *out_pat = g_malloc (sizeof (Pattern));
    out_pat->vixmap = vixmap;

    // never change
    out_pat->colmap = in_pat->colmap;
    out_pat->vixn   = in_pat->vixn;

//    gboolean flp = (direction == 1 || direction == 3) ? TRUE : FALSE;
//    gboolean rot = (direction == 2 || direction == 3) ? TRUE : FALSE;
    gint rot = direction % 4;

    // rotate 90 & 270 swaps width/height
    out_pat->gw = (rot == 1 || rot == 3) ? in_pat->gh : in_pat->gw;
    out_pat->gh = (rot == 1 || rot == 3) ? in_pat->gw : in_pat->gh;
    out_pat->vw = (rot == 1 || rot == 3) ? in_pat->vh : in_pat->vw;
    out_pat->vh = (rot == 1 || rot == 3) ? in_pat->vw : in_pat->vh;

    if (rot == 0) {
        pat_copy(in_pat, in_pat->vixmap, vixmap);
        out_pat->gx = in_pat->gx;
        out_pat->gy = in_pat->gy;

    // 90 deg
    } else if (rot == 1) {
        pat_transpose(in_pat, in_pat->vixmap, scratch);
        pat_flip_h(in_pat, scratch, vixmap);
        // gx=0 gy=2 gw=6 gh=12 vw=6 vh=16
        // x: 16 - 2 - 12 = 2
        // y: 0
        out_pat->gx = in_pat->vh - in_pat->gy - in_pat->gh;
        out_pat->gy = in_pat->gx;

    // 180 degrees
    } else if (rot == 2) {
        pat_reverse(in_pat, in_pat->vixmap, vixmap);
        out_pat->gx = in_pat->vw - in_pat->gx - in_pat->gw;
        out_pat->gy = in_pat->vh - in_pat->gy - in_pat->gh;

    // 270 degrees
    } else {
        pat_transpose(in_pat, in_pat->vixmap, scratch);
        pat_flip_h(in_pat, scratch, vixmap);
        out_pat->gx = in_pat->gy;
        out_pat->gy = in_pat->vw - in_pat->gx - in_pat->gw;
    }

/*
    // update the grid offset
    if (rot && ! flp) {
        printf("  rotate\n");
        out_pat->gx = in_pat->vh - in_pat->gy - in_pat->gh;
        out_pat->gy = in_pat->vw - in_pat->gx - in_pat->gw;
    } else if (! rot && flp) {
        printf("  flip\n");
        out_pat->gx = in_pat->vw - in_pat->gx - in_pat->gw;
        out_pat->gy = in_pat->vh - in_pat->gy - in_pat->gh;
    } else if (rot && flp) {
        printf("  rotate & flip\n");
        out_pat->gx = in_pat->gy;
        out_pat->gy = in_pat->gx;
    } else {
        printf("  normal\n");
        out_pat->gx = in_pat->gx;
        out_pat->gy = in_pat->gy;
    }
out_pat->gx = 0;
out_pat->gy = 0;

    gint r_idx, w_idx;
//    printf("  reorder: ");
    for (gint y = 0 ; y < in_pat->vh ; y++) {
        for (gint x = 0 ; x < in_pat->vw ; x++) {
            r_idx = y * in_pat->vw + x;

            if (rot && ! flp)
                w_idx = x * in_pat->vh + in_pat->vh - 1 - y;
            else if (! rot && flp)
                w_idx = y * in_pat->vw + in_pat->vw - 1 - x;
            else if (rot && flp)
                w_idx = x * in_pat->vh + y;
            else
                w_idx = r_idx;
//            printf(" %d", w_idx);

            vixmap[w_idx] = in_pat->vixmap[r_idx];
        }
    }
//    printf("\n");
*/

    return out_pat;
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

    Pattern *base_pat = patterns[o->pattern];
    Pattern *pat = pattern_set_direction(base_pat, o->direction);

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

    g_free(pat->vixmap);
    g_free(pat);

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
// TODO: REMOVE!?!?!??!?!?!?!?!?!??!?!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!111
    operation_class->threaded         = FALSE;

    gegl_operation_class_set_keys (operation_class,
        "name",             "kruthers:video-pixelize-core",
        "title",          _("Video Pixelize Core"),
        "categories",       "hidden",
        "description",    _("This function is like a combination of Video Degradation and Pixelize, "
                            "creating a chunky pixel effect similar to various video displays."),
        NULL);
}

#endif
