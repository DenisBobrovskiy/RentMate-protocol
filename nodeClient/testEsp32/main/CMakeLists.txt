
idf_component_register(SRCS "client.c" 
"socketThread.c"  "./smartLock/matrixKeyboard.c" "./smartLock/smartLockCore.c"
"./commands/sharedCommands/sharedCommands.c" 
"./commands/smartLock/commands.c"
 INCLUDE_DIRS ".")

spiffs_create_partition_image(storage ../spiffFolder FLASH_IN_PROJECT)


