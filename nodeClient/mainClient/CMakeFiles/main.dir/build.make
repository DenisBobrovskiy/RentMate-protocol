# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.23

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Disable VCS-based implicit rules.
% : %,v

# Disable VCS-based implicit rules.
% : RCS/%

# Disable VCS-based implicit rules.
% : RCS/%,v

# Disable VCS-based implicit rules.
% : SCCS/s.%

# Disable VCS-based implicit rules.
% : s.%

.SUFFIXES: .hpux_make_needs_suffix_list

# Command-line flag to silence nested $(MAKE).
$(VERBOSE)MAKESILENT = -s

#Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:
.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/main/Projects/iot_system/nodeClient/mainClient

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/main/Projects/iot_system/nodeClient/mainClient

# Include any dependencies generated for this target.
include CMakeFiles/main.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include CMakeFiles/main.dir/compiler_depend.make

# Include the progress variables for this target.
include CMakeFiles/main.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/main.dir/flags.make

CMakeFiles/main.dir/client.c.o: CMakeFiles/main.dir/flags.make
CMakeFiles/main.dir/client.c.o: client.c
CMakeFiles/main.dir/client.c.o: CMakeFiles/main.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/main/Projects/iot_system/nodeClient/mainClient/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building C object CMakeFiles/main.dir/client.c.o"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -MD -MT CMakeFiles/main.dir/client.c.o -MF CMakeFiles/main.dir/client.c.o.d -o CMakeFiles/main.dir/client.c.o -c /home/main/Projects/iot_system/nodeClient/mainClient/client.c

CMakeFiles/main.dir/client.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/main.dir/client.c.i"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/main/Projects/iot_system/nodeClient/mainClient/client.c > CMakeFiles/main.dir/client.c.i

CMakeFiles/main.dir/client.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/main.dir/client.c.s"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/main/Projects/iot_system/nodeClient/mainClient/client.c -o CMakeFiles/main.dir/client.c.s

CMakeFiles/main.dir/socketThread.c.o: CMakeFiles/main.dir/flags.make
CMakeFiles/main.dir/socketThread.c.o: socketThread.c
CMakeFiles/main.dir/socketThread.c.o: CMakeFiles/main.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/main/Projects/iot_system/nodeClient/mainClient/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building C object CMakeFiles/main.dir/socketThread.c.o"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -MD -MT CMakeFiles/main.dir/socketThread.c.o -MF CMakeFiles/main.dir/socketThread.c.o.d -o CMakeFiles/main.dir/socketThread.c.o -c /home/main/Projects/iot_system/nodeClient/mainClient/socketThread.c

CMakeFiles/main.dir/socketThread.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/main.dir/socketThread.c.i"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/main/Projects/iot_system/nodeClient/mainClient/socketThread.c > CMakeFiles/main.dir/socketThread.c.i

CMakeFiles/main.dir/socketThread.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/main.dir/socketThread.c.s"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/main/Projects/iot_system/nodeClient/mainClient/socketThread.c -o CMakeFiles/main.dir/socketThread.c.s

CMakeFiles/main.dir/commands/sharedCommands/sharedCommands.c.o: CMakeFiles/main.dir/flags.make
CMakeFiles/main.dir/commands/sharedCommands/sharedCommands.c.o: commands/sharedCommands/sharedCommands.c
CMakeFiles/main.dir/commands/sharedCommands/sharedCommands.c.o: CMakeFiles/main.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/main/Projects/iot_system/nodeClient/mainClient/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Building C object CMakeFiles/main.dir/commands/sharedCommands/sharedCommands.c.o"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -MD -MT CMakeFiles/main.dir/commands/sharedCommands/sharedCommands.c.o -MF CMakeFiles/main.dir/commands/sharedCommands/sharedCommands.c.o.d -o CMakeFiles/main.dir/commands/sharedCommands/sharedCommands.c.o -c /home/main/Projects/iot_system/nodeClient/mainClient/commands/sharedCommands/sharedCommands.c

CMakeFiles/main.dir/commands/sharedCommands/sharedCommands.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/main.dir/commands/sharedCommands/sharedCommands.c.i"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/main/Projects/iot_system/nodeClient/mainClient/commands/sharedCommands/sharedCommands.c > CMakeFiles/main.dir/commands/sharedCommands/sharedCommands.c.i

CMakeFiles/main.dir/commands/sharedCommands/sharedCommands.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/main.dir/commands/sharedCommands/sharedCommands.c.s"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/main/Projects/iot_system/nodeClient/mainClient/commands/sharedCommands/sharedCommands.c -o CMakeFiles/main.dir/commands/sharedCommands/sharedCommands.c.s

# Object files for target main
main_OBJECTS = \
"CMakeFiles/main.dir/client.c.o" \
"CMakeFiles/main.dir/socketThread.c.o" \
"CMakeFiles/main.dir/commands/sharedCommands/sharedCommands.c.o"

# External object files for target main
main_EXTERNAL_OBJECTS =

main: CMakeFiles/main.dir/client.c.o
main: CMakeFiles/main.dir/socketThread.c.o
main: CMakeFiles/main.dir/commands/sharedCommands/sharedCommands.c.o
main: CMakeFiles/main.dir/build.make
main: libpckDataLib.a
main: libaesGcmLib.a
main: libarrayListLib.a
main: libtinyECDHLib.a
main: /usr/local/lib/libwebsockets.so
main: /home/main/Projects/iot_system/misc/json-c/build/libjson-c.so
main: /home/main/Projects/iot_system/misc/mbedtls/library/libmbedcrypto.a
main: CMakeFiles/main.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/main/Projects/iot_system/nodeClient/mainClient/CMakeFiles --progress-num=$(CMAKE_PROGRESS_4) "Linking C executable main"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/main.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/main.dir/build: main
.PHONY : CMakeFiles/main.dir/build

CMakeFiles/main.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/main.dir/cmake_clean.cmake
.PHONY : CMakeFiles/main.dir/clean

CMakeFiles/main.dir/depend:
	cd /home/main/Projects/iot_system/nodeClient/mainClient && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/main/Projects/iot_system/nodeClient/mainClient /home/main/Projects/iot_system/nodeClient/mainClient /home/main/Projects/iot_system/nodeClient/mainClient /home/main/Projects/iot_system/nodeClient/mainClient /home/main/Projects/iot_system/nodeClient/mainClient/CMakeFiles/main.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/main.dir/depend
