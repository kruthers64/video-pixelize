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

