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
description(_("Some patterns have \"background pixels\" or \"holes\" that are not R, G or B "
    "so are drawn as always black.  This toggles them to be clear."
))

property_int (orientation, _("Orientation"), 0)
description(_("Change the orientation of the pattern."))
    value_range (0, 7)
    ui_range    (0, 7)
    ui_steps    (1, 1)

property_double (scale, _("Scale"), 1)
    description (_("Increase size of video pixels"))
    value_range (1.0, 100.0)
    ui_range    (1.0,  10.0)
    ui_steps    (0.01,  0.01)
    ui_digits   (2)

property_enum (sampler_type, _("Resampling method"),
    GeglSamplerType, gegl_sampler_type, GEGL_SAMPLER_NEAREST)
    description (_("Algorithm to use for scaling"))


#else

#define GEGL_OP_META
#define GEGL_OP_NAME     video_pixelize
#define GEGL_OP_C_SOURCE video-pixelize.c

#include "gegl-op.h"

typedef struct
{
    GeglNode *input;
    GeglNode *prescale;
    GeglNode *videopix;
    GeglNode *postcrop;
    GeglNode *postscale;
    GeglNode *output;
} Nodes;


static void
update (GeglOperation *operation)
{
    static gfloat cache_width  = 0;
    static gfloat cache_height = 0;
    GeglProperties *o     = GEGL_PROPERTIES (operation);
    Nodes          *nodes = o->user_data;

    if (!nodes)
        return;

    gfloat scale_inv = 1.0 / o->scale;
    gegl_node_set (nodes->prescale,  "x", scale_inv, "y", scale_inv, NULL);
    gegl_node_set (nodes->postscale, "x", o->scale,  "y", o->scale,  NULL);
}


static void
attach (GeglOperation *operation)
{
    GeglProperties  *o     = GEGL_PROPERTIES (operation);
    GeglNode        *gegl  = operation->node;

    Nodes *nodes = g_malloc0 (sizeof (Nodes));
    o->user_data = nodes;

    nodes->input  = gegl_node_get_input_proxy (gegl, "input");
    nodes->output = gegl_node_get_output_proxy (gegl, "output");

    nodes->prescale  = gegl_node_new_child (gegl, "operation", "gegl:scale-ratio",
        "x", 1.0, "y", 1.0,
        "abyss-policy", GEGL_ABYSS_CLAMP,
        "sampler", GEGL_SAMPLER_NEAREST,
        NULL
    );

    nodes->videopix  = gegl_node_new_child (gegl, "operation", "kruthers:video-pixelize-core", NULL);

    nodes->postcrop  = gegl_node_new_child (gegl, "operation", "gegl:crop", NULL);
    nodes->postscale = gegl_node_new_child (gegl, "operation", "gegl:scale-ratio",
        "x", 1.0, "y", 1.0,
        "abyss-policy", GEGL_ABYSS_CLAMP,
        "sampler", GEGL_SAMPLER_NEAREST,
        NULL
    );

    // before scaling back up, we crop to remove the extra padding added by video-pixelie-core
    gegl_node_connect(nodes->prescale, "output", nodes->postcrop, "aux");

    gegl_node_link_many (
        nodes->input,
        nodes->prescale,
        nodes->videopix,
        nodes->postcrop,
        nodes->postscale,
        nodes->output,
        NULL
    );

    gegl_operation_meta_redirect (operation, "pattern",      nodes->videopix,  "pattern");
    gegl_operation_meta_redirect (operation, "color-style",  nodes->videopix,  "color-style");
    gegl_operation_meta_redirect (operation, "clear-bg",     nodes->videopix,  "clear-bg");
    gegl_operation_meta_redirect (operation, "orientation",  nodes->videopix,  "orientation");
    gegl_operation_meta_redirect (operation, "sampler-type", nodes->postscale, "sampler");
}

static void
prepare (GeglOperation *operation)
{
    static gboolean initialized = FALSE;

    if (! initialized) {
        GeglNode       *gegl = operation->node;
        GeglRectangle   inrect;
        GeglNode       *input;

        input  = gegl_node_get_input_proxy (gegl, "input");
        inrect = gegl_node_get_bounding_box (input);

        if (inrect.width > 0) {
            update (operation);
            initialized = TRUE;
        }
    }
}

static void
dispose (GObject *object)
{
   GeglProperties  *o = GEGL_PROPERTIES (object);
   g_clear_pointer (&o->user_data, g_free);
   G_OBJECT_CLASS (gegl_op_parent_class)->dispose (object);
}

static void
gegl_op_class_init (GeglOpClass *klass)
{
    GObjectClass           *object_class         = G_OBJECT_CLASS (klass); 
    GeglOperationClass     *operation_class      = GEGL_OPERATION_CLASS (klass);
    GeglOperationMetaClass *operation_meta_class = GEGL_OPERATION_META_CLASS (klass);

    operation_class->threaded    = FALSE;
    operation_class->attach      = attach;
//    operation_class->prepare     = prepare;
    operation_meta_class->update = update;
    object_class->dispose        = dispose; 

    gegl_operation_class_set_keys (operation_class,
        "name",             "kruthers:video-pixelize",
        "title",          _("Video Pixelize"),
        "categories",       "distort",
        "description",    _("This filter is like a combination of Video Degradation and Pixelize, "
                            "creating a chunky pixel effect similar to various video displays."),
        "gimp:menu-path",   "<Image>/Filters/Kruthers",
        "gimp:menu-label",  "Video Pixelize",
        NULL);
}

#endif
