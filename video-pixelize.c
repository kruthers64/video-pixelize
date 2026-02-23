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
 * Inspired by Video Degradation and Pixelize, creating a chunky effect
 * similar to various video displays.
 *
 * Author: Kruthers <kruthers@kruthers.net>
 *
 */

#include "config.h"
#include <glib/gi18n-lib.h>

#ifdef GEGL_PROPERTIES

#include "video-pixelize-gegl-enum.h"

property_double (color_style, _("Color style:  RGB phosphors <-> Full color"), 0.0)
    description(_("Adjust video pixels to be colored from phosophor-style RGB to full color."))
    value_range (0.0, 1.0)
    ui_range    (0.0, 1.0)

property_boolean (clear_bg, _("Transparent backround pixels"), FALSE)
description(_("Some patterns have \"background pixels\" or \"holes\" that are not R, G or B. "
    "They are usually drawn black or white depending on the style; this toggles them to be clear."
))

property_boolean (rotate, _("Rotate"), FALSE)
description(_("Rotate the pattern by ninety degrees."))

property_int    (scale, _("Scale"), 1)
    description (_("Increase size of video pixels"))
    value_range (1, 20)
    ui_range    (1, 20)

property_enum (sampler_type, _("Resampling method"),
    GeglSamplerType, gegl_sampler_type, GEGL_SAMPLER_NEAREST)
    description (_("Algorithm to use for scaling"))


#else

/* #define GEGL_OP_POINT_FILTER */
#define GEGL_OP_AREA_FILTER
#define GEGL_OP_NAME     video_pixelize
#define GEGL_OP_C_SOURCE video-pixelize.c

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
prepare (GeglOperation *operation)
{
    const Babl     *format = babl_format_with_space ("R'G'B'A float", gegl_operation_get_source_space (operation, "input"));

    GeglOperationAreaFilter *op_area;
    op_area = GEGL_OPERATION_AREA_FILTER (operation);

    op_area->left   =
    op_area->right  = 0;//25;  TODO: FIGURE OUT IF PADDING IS NECESSARY
    op_area->top    =
    op_area->bottom = 0;//25;

    gegl_operation_set_format (operation, "input", format);
    gegl_operation_set_format (operation, "output", format);
}

const gfloat RLUM = 0.2126;
const gfloat GLUM = 0.7152;
const gfloat BLUM = 0.0722;

static void
get_vixel_colors(
    GeglProperties  *o,
    Pattern         *pat,
    gfloat          *src_buf,
    gfloat          *vix_rgb,   // return values; float[4] provided by caller
    gint            *scratch    // scratch buffer for mean divisors
) {
    gint channel;
    gfloat lum;

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

static gfloat
set_pixel_color (
    GeglProperties *o,
    gfloat *dst_buf, gint dst_idx,
    gfloat *vix_rgb, gint vix_idx,
    gint channel,
    gfloat src_alpha
) {
    gint c;

    // color
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

static gboolean
process (GeglOperation       *operation,
         GeglBuffer          *input,
         GeglBuffer          *output,
         const GeglRectangle *roi,
         gint                 level)
{
    GeglProperties *o = GEGL_PROPERTIES (operation);

    Pattern *pat = patterns[o->pattern];

    const Babl *format = gegl_operation_get_format(operation, "output");

    GeglRectangle *world = gegl_operation_source_get_bounding_box(operation, "input");
    gint startx = align_grid(pat->gx, pat->gw, roi->x, world->x);
    gint starty = align_grid(pat->gy, pat->gh, roi->y, world->y);

    // alloc now and reuse as much as possible
    GeglRectangle *src_rect = gegl_rectangle_new(0, 0, pat->vw, pat->vh);
    GeglRectangle *dst_rect = gegl_rectangle_new(0, 0, pat->gw, pat->gh);
    GeglRectangle *clp_rect = gegl_rectangle_new(0, 0, 0, 0);
    // dst is always size of grid cell, src is larger to sample pattern overlaps
    gfloat *src_buf = g_new(gfloat, pat->vw * pat->vh * 4);
    gfloat *dst_buf = g_new(gfloat, pat->gw * pat->gh * 4);

    GeglRectangle *grid_rect = gegl_rectangle_new(0, 0, pat->gw, pat->gh);
    GeglRectangle *gclp_rect = gegl_rectangle_new(0, 0, 0, 0);
    GeglBuffer *buftmp = gegl_buffer_new(grid_rect, format);

    // arrays for calculating vixel colors
    gfloat *vix_rgb = g_new(gfloat, pat->vixn * 4);
    gint *scratch = g_new(gint, pat->vixn);
    gfloat src_alpha;

    // step through grid cells
    gint gridx, gridy;
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
            gint x, y, u, v, vix;
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
            gegl_buffer_set(buftmp, grid_rect, 0, format, dst_buf, GEGL_AUTO_ROWSTRIDE);
            gegl_rectangle_intersect(clp_rect, roi, dst_rect);
            gegl_rectangle_set(gclp_rect,
                clp_rect->x - dst_rect->x, clp_rect->y - dst_rect->y,
                clp_rect->width, clp_rect->height
            );

            // TODO: remove; in theory this should not happen, but leave for bullet-proofing for now...
            if (clp_rect->width < 1 || clp_rect->height < 1) {
                printf("dst_rect = %d,%d,%d,%d   roi = %d,%d,%d,%d\n",
                    dst_rect->x, dst_rect->y, dst_rect->width, dst_rect->height,
                    roi->x, roi->y, roi->width, roi->height);
                continue;
            }

            gegl_buffer_copy(buftmp, gclp_rect, GEGL_ABYSS_WHITE, output, clp_rect);
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
    g_object_unref(buftmp);

    return TRUE;
}

static void
gegl_op_class_init (GeglOpClass *klass)
{
    GeglOperationClass       *operation_class;
    GeglOperationFilterClass *filter_class;

    operation_class = GEGL_OPERATION_CLASS (klass);
    filter_class    = GEGL_OPERATION_FILTER_CLASS (klass);

    operation_class->prepare = prepare;
    filter_class->process    = process;

    gegl_operation_class_set_keys (operation_class,
        "name",             "kruthers:video-pixelize",
        "title",          _("Video Pixelize"),
        "categories",       "distort",
        "description",    _("This function is a like combination of Video Degradation and Pixelize, "
                            "creating a chunky pixel effect similar to various video displays."),
        "gimp:menu-path",   "<Image>/Filters/Kruthers",
        "gimp:menu-label",  "Video Pixelize",
        NULL);
}

#endif
