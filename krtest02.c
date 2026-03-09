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
 * Author: Kruthers <kruthers@kruthers.net>
 *
 * This demonstrates a problem with doing a scale operation before a
 * GEGL_OP_AREA_FILTER node which adds padding with the left/right/top/bottom
 * attributes.  This is using gegl:noise-spread as an example but the symptom
 * should be visible with any area filter that uses GEGL_ABYSS_CLAMP and
 * makes use of the data in the padding area.
 * 
 * To see the problem, slowly scub the scale slider and watch the bottom
 * edge flicker.  For some scale values the clamping works, for others it
 * appears to lose data in the bottom edge padding area.
 * 
 * This is using GEGL_SAMPLER_NEAREST by default on the scale node, and the
 * problem seems fixed by switching to linear or the other smooth samplers.
 * However, the problem actually still exists but just shows up less often.
 * With an input image of 1200 x 800, try a scale value of 0.149 and 0.035
 * for example.
 * 
 * Another possible data point is that the problem also happens to the
 * right edge of the image if you disable clamping on the scale node,
 * but at different scale values than the bottom edge.  Set Abyss Policy
 * to none in the UI to see this.
 */

#include "config.h"
#include <glib/gi18n-lib.h>

#ifdef GEGL_PROPERTIES

property_double (scale, _("Scale"), 0.500)
    description (_("Scale"))
    value_range (0.001, 1.0)
    ui_range    (0.001, 1.0)
    ui_steps    (0.001,  0.001)
    ui_digits   (3)

property_enum (sampler, _("Resampling method"),
    GeglSamplerType, gegl_sampler_type, GEGL_SAMPLER_NEAREST)
    description (_("Algorithm to use for scaling"))

property_enum (abyss_policy, _("Abyss policy"),
    GeglAbyssPolicy, gegl_abyss_policy, GEGL_ABYSS_CLAMP)
    description (_("Abyss policy used by scale node"))

#else

#define GEGL_OP_META
#define GEGL_OP_NAME     krtest02
#define GEGL_OP_C_SOURCE krtest02.c

#include "gegl-op.h"

typedef struct
{
    GeglNode *input;
    GeglNode *scale;
    GeglNode *area;
    GeglNode *output;
} Nodes;

static void
update (GeglOperation *operation)
{
    GeglProperties *o     = GEGL_PROPERTIES (operation);
    Nodes          *nodes = o->user_data;

    if (!nodes)
        return;

    gegl_node_set (nodes->scale,  "x", o->scale, "y", o->scale, NULL);
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

    nodes->scale  = gegl_node_new_child (gegl, "operation", "gegl:scale-ratio",
        NULL
    );
    nodes->area = gegl_node_new_child (gegl, "operation", "gegl:noise-spread",
        "amount_x", 100,
        "amount_y", 100,
        NULL
    );

    gegl_node_link_many (
        nodes->input,
        nodes->scale,
        nodes->area,
        nodes->output,
        NULL
    );

    gegl_operation_meta_redirect (operation, "abyss-policy", nodes->scale, "abyss-policy");
    gegl_operation_meta_redirect (operation, "sampler",      nodes->scale, "sampler");
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
    operation_meta_class->update = update;
    object_class->dispose        = dispose; 

    gegl_operation_class_set_keys (operation_class,
        "name",             "kruthers:krtest02",
        "title",          _("krtest02"),
        "categories",       "distort",
        "description",    _(
            "Demonstration of a problem with GEGL_ABYSS_CLAMP failing when using\n"
            "GEGL_OP_AREA_FILTER and a scale node, but only for some scale values."
        ),
        "gimp:menu-path",   "<Image>/Filters/Kruthers",
        "gimp:menu-label",  "krtest02",
        NULL);
}

#endif
