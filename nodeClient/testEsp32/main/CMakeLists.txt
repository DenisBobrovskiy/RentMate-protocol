
idf_component_register(SRCS "client.c" "socketThread.c" INCLUDE_DIRS ".")

spiffs_create_partition_image(storage ../spiffFolder FLASH_IN_PROJECT)


