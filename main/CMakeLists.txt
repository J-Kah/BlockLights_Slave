idf_component_register(SRCS "main.cpp"
                    INCLUDE_DIRS ".")
littlefs_create_partition_image(storage ../partition FLASH_IN_PROJECT)