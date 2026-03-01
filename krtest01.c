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
 * Author: Kruthers <kruthers@kruthers.net>
 *
 * This demonstrates a problem with trying to do math with the input
 * size in a meta operation.  The information is not available until
 * after the first update, so the first draw is bad.  Even if you tweak
 * things so the first open of the tool "looks good", the problem will
 * show when you edit a filter already applied to a layer.
 */

#include "config.h"
#include <glib/gi18n-lib.h>

#ifdef GEGL_PROPERTIES

property_double (scale, _("Scale"), 1)
    description (_("Scale"))
    value_range (1.0, 100.0)
    ui_range    (1.0,  10.0)
    ui_steps    (0.01,  0.01)
    ui_digits   (2)


#else

#define GEGL_OP_META
#define GEGL_OP_NAME     krtest01
#define GEGL_OP_C_SOURCE krtest01.c

#include "gegl-op.h"

typedef struct
{
    GeglNode *input;
    GeglNode *prescale;
    GeglNode *output;
} Nodes;

static void
update (GeglOperation *operation)
{
    GeglProperties *o     = GEGL_PROPERTIES (operation);
    Nodes          *nodes = o->user_data;

    if (!nodes)
        return;

    GeglRectangle inrect = gegl_node_get_bounding_box(nodes->input);
    gegl_node_set (nodes->prescale,  "x", inrect.width / o->scale, "y", inrect.height / o->scale, NULL);
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

    nodes->prescale  = gegl_node_new_child (gegl, "operation", "gegl:scale-size",
        "abyss-policy", GEGL_ABYSS_CLAMP,
        "sampler", GEGL_SAMPLER_NEAREST,
        NULL
    );

    gegl_node_link_many (
        nodes->input,
        nodes->prescale,
        nodes->output,
        NULL
    );
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
        "name",             "kruthers:krtest01",
        "title",          _("krtest01"),
        "categories",       "distort",
        "description",    _("Demonstrate a bug or problem."),
        "gimp:menu-path",   "<Image>/Filters/Kruthers",
        "gimp:menu-label",  "krtest01",
        NULL);
}

#endif
