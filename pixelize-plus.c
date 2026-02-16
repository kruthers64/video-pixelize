#include "stdio.h"

/* This file is an image processing operation for GEGL
 *
 * GEGL is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * GEGL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GEGL; if not, see <https://www.gnu.org/licenses/>.
 *
 * Copyright 2006 Øyvind Kolås <pippin@gimp.org>
 * Copyright 2013 Téo Mazars   <teo.mazars@ensimag.fr>
 */

#include "config.h"
#include <glib/gi18n-lib.h>

#ifdef GEGL_PROPERTIES

enum_start (gegl_pixelize_norm)
  enum_value (GEGL_PIXELIZE_NORM_MANHATTAN, "diamond", N_("Diamond"))
  enum_value (GEGL_PIXELIZE_NORM_EUCLIDEAN, "round",   N_("Round"))
  enum_value (GEGL_PIXELIZE_NORM_INFINITY,  "square",  N_("Square"))
enum_end (GeglPixelizeNormPlus)

property_enum   (norm, _("Shape"),
    GeglPixelizeNormPlus, gegl_pixelize_norm, GEGL_PIXELIZE_NORM_INFINITY)
    description (_("The shape of pixels"))

property_int    (size_x, _("Block width"), 16)
    description (_("Width of blocks in pixels"))
    value_range (2, G_MAXINT)
    ui_range    (2, 2048)
    ui_gamma    (1.5)
    ui_meta     ("unit", "pixel-distance")
    ui_meta     ("axis", "x")

property_int    (size_y, _("Block height"), 16)
    description (_("Height of blocks in pixels"))
    value_range (2, G_MAXINT)
    ui_range    (2, 2048)
    ui_gamma    (1.5)
    ui_meta     ("unit", "pixel-distance")
    ui_meta     ("axis", "y")

property_int    (offset_x, _("Offset X"), 0)
    description (_("Horizontal offset of blocks in pixels"))
    value_range (G_MININT, G_MAXINT)
    ui_range    (0, 2048)
    ui_meta     ("unit", "pixel-coordinate")
    ui_meta     ("axis", "x")

property_int    (offset_y, _("Offset Y"), 0)
    description (_("Vertical offset of blocks in pixels"))
    value_range (G_MININT, G_MAXINT)
    ui_range    (0, 2048)
    ui_meta     ("unit", "pixel-coordinate")
    ui_meta     ("axis", "y")

property_double (ratio_x, _("Size ratio X"), 1.0)
    description (_("Horizontal size ratio of a pixel inside each block"))
    value_range (0.0, 1.0)
    ui_meta     ("axis", "x")

property_double (ratio_y, _("Size ratio Y"), 1.0)
    description (_("Vertical size ratio of a pixel inside each block"))
    value_range (0.0, 1.0)
    ui_meta     ("axis", "y")

property_color  (background, _("Background color"), "white")
    description (_("Color used to fill the background"))
    ui_meta     ("role", "color-secondary")

#else

#define GEGL_OP_AREA_FILTER
#define GEGL_OP_NAME     pixelize_plus
#define GEGL_OP_C_SOURCE pixelize-plus.c

#include "gegl-op.h"

#define CHUNK_SIZE           (1024)
#define ALLOC_THRESHOLD_SIZE (64)
#define SQR(x)               ((x)*(x))

static void
prepare (GeglOperation *operation)
{
  const Babl *space = gegl_operation_get_source_space (operation, "input");
  GeglProperties              *o;
  GeglOperationAreaFilter *op_area;

  op_area = GEGL_OPERATION_AREA_FILTER (operation);
  o       = GEGL_PROPERTIES (operation);

  op_area->left   =
  op_area->right  = o->size_x;
  op_area->top    =
  op_area->bottom = o->size_y;

  gegl_operation_set_format (operation, "input",
                             babl_format_with_space ("RaGaBaA float", space));
  gegl_operation_set_format (operation, "output",
                             babl_format_with_space ("RaGaBaA float", space));
}

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
mean_rectangle_noalloc (GeglBuffer    *input,
                        GeglRectangle *rect,
                        GeglColor     *color,
                        const Babl    *format)
{
  GeglBufferIterator *gi;
  gfloat              col[] = {0.0, 0.0, 0.0, 0.0};
  gint                c;

  gi = gegl_buffer_iterator_new (input, rect, 0, format,
                                 GEGL_ACCESS_READ, GEGL_ABYSS_CLAMP, 1);

  while (gegl_buffer_iterator_next (gi))
    {
      gint    k;
      gfloat *data = (gfloat*) gi->items[0].data;

      for (k = 0; k < gi->length; k++)
        {
          for (c = 0; c < 4; c++)
            col[c] += data[c];

          data += 4;
        }
    }

  for (c = 0; c < 4; c++)
    col[c] /= rect->width * rect->height;

  gegl_color_set_pixel (color, format, col);
}


static void
set_rectangle_noalloc (GeglBuffer      *output,
                       GeglRectangle   *rect,
                       GeglRectangle   *rect_shape,
                       GeglColor       *color,
                       GeglPixelizeNormPlus norm,
                       const Babl      *format)
{
  if (norm == GEGL_PIXELIZE_NORM_INFINITY)
    {
      GeglRectangle rect2;
      gegl_rectangle_intersect (&rect2, rect, rect_shape);
      gegl_buffer_set_color (output, &rect2, color);
    }
  else
    {
      GeglBufferIterator *gi;
      gint                c, x, y;
      gfloat              col[4];
      gfloat              center_x, center_y;
      gfloat              shape_area = rect_shape->width * rect_shape->height;

      center_x = rect_shape->x + rect_shape->width / 2.0f;
      center_y = rect_shape->y + rect_shape->height / 2.0f;

      gegl_color_get_pixel (color, format, col);

      gi = gegl_buffer_iterator_new (output, rect, 0, format,
                                     GEGL_ACCESS_READWRITE, GEGL_ABYSS_CLAMP, 1);

      while (gegl_buffer_iterator_next (gi))
        {
          gfloat       *data = (gfloat*) gi->items[0].data;
          GeglRectangle roi = gi->items[0].roi;

          switch (norm)
            {
            case (GEGL_PIXELIZE_NORM_EUCLIDEAN):

              for (y = 0; y < roi.height; y++)
                for (x = 0; x < roi.width; x++)
                  if (SQR ((x + roi.x - center_x) / (gfloat) rect_shape->width) +
                      SQR ((y + roi.y - center_y) / (gfloat) rect_shape->height) <= 1.0f)
                    for (c = 0; c < 4; c++)
                      data [4 * (y * roi.width + x) + c] = col[c];
              break;

            case (GEGL_PIXELIZE_NORM_MANHATTAN):

              for (y = 0; y < roi.height; y++)
                for (x = 0; x < roi.width; x++)
                  if (fabsf (x + roi.x - center_x) * rect_shape->height +
                      fabsf (y + roi.y - center_y) * rect_shape->width
                      < shape_area)
                    for (c = 0; c < 4; c++)
                      data [4 * (y * roi.width + x) + c] = col[c];
              break;

            case (GEGL_PIXELIZE_NORM_INFINITY):
              break;
            }
        }
    }
}

static gint
align_offset (gint offset,
              gint size) {
  gint align = abs(offset) % size;
  return offset <= 0 ? align : (size - align);
}

static int
block_index (int pos,
             int size) {
  return pos < 0 ? ((pos + 1) / size - 1) : (pos / size);
}

static void
pixelize_noalloc (GeglBuffer          *input,
                  GeglBuffer          *output,
                  const GeglRectangle *roi,
                  const GeglRectangle *whole_region,
                  GeglProperties      *o,
                  const Babl          *format)
{
  gint align_x = align_offset(o->offset_x, o->size_x);
  gint align_y = align_offset(o->offset_y, o->size_y);

  gint start_x = block_index (roi->x, o->size_x) * o->size_x - align_x;
  gint start_y = block_index (roi->y, o->size_y) * o->size_y - align_y;
  gint x, y;
  gint off_shape_x, off_shape_y;

  GeglColor *color = gegl_color_new ("white");

  GeglRectangle rect_shape;

  rect_shape.width  = ceilf (o->size_x * (gfloat)o->ratio_x);
  rect_shape.height = ceilf (o->size_y * (gfloat)o->ratio_y);

  off_shape_x = floorf ((o->size_x - (gfloat)o->ratio_x * o->size_x) / 2.0f);
  off_shape_y = floorf ((o->size_y - (gfloat)o->ratio_y * o->size_y) / 2.0f);

  printf("start_x = %5d, start_y = %5d, width = %5d, height = %5d\n", start_x, start_y, roi->width, roi->height);

  for (y = start_y; y < roi->y + roi->height; y += o->size_y)
    for (x = start_x; x < roi->x + roi->width; x += o->size_x)
      {
        GeglRectangle rect = {x, y, o->size_x, o->size_y};
//        printf("%d,%d ", x, y);

        gegl_rectangle_intersect (&rect, whole_region, &rect);

        if (rect.width < 1 || rect.height < 1)
          continue;

        mean_rectangle_noalloc (input, &rect, color, format);

        gegl_rectangle_intersect (&rect, roi, &rect);

        rect_shape.x = x + off_shape_x;
        rect_shape.y = y + off_shape_y;

        set_rectangle_noalloc (output, &rect, &rect_shape, color, o->norm, format);
      }
//    printf("\n");

  g_object_unref (color);
}


static gboolean
process (GeglOperation       *operation,
         GeglBuffer          *input,
         GeglBuffer          *output,
         const GeglRectangle *roi,
         gint                 level)
{
  GeglRectangle            src_rect;
  GeglRectangle           *whole_region;
  GeglProperties          *o = GEGL_PROPERTIES (operation);
  const Babl *format = gegl_operation_get_format (operation, "output");

  whole_region = gegl_operation_source_get_bounding_box (operation, "input");

  gegl_buffer_set_color (output, roi, o->background);
  pixelize_noalloc (input, output, roi, whole_region, o, format);

  return  TRUE;
}


static void
gegl_op_class_init (GeglOpClass *klass)
{
  GeglOperationClass       *operation_class;
  GeglOperationFilterClass *filter_class;

  operation_class = GEGL_OPERATION_CLASS (klass);
  filter_class    = GEGL_OPERATION_FILTER_CLASS (klass);

  operation_class->prepare          = prepare;
  operation_class->get_bounding_box = get_bounding_box;

  filter_class->process           = process;

  gegl_operation_class_set_keys (operation_class,
    "name",           "kruthers:pixelize-plus",
    "categories",         "blur:scramble",
    "position-dependent", "true",
    "title",              _("Pixelize Plus"),
    "gimp:menu-path", "<Image>/Filters/Kruthers",
    "gimp:menu-label", "Pixelize Plus",
    "description", _("Simplify image into an array of solid-colored rectangles"),
    NULL);
}

#endif
