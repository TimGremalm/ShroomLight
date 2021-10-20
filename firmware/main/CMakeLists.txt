set(srcs "main.c"
    "apa106.c"
    "coordinates.c"
    "light.c"
    "pir.c"
    "shroom_settings.c"
    "statusblink.c"
    )

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS  ".")
