# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.5

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list


# Suppress display of executed commands.
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
RM = /usr/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/raoxu/ffmpeg-2.5.11/raoxu/work2

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/raoxu/ffmpeg-2.5.11/raoxu/work2

# Include any dependencies generated for this target.
include CMakeFiles/player.dir/depend.make

# Include the progress variables for this target.
include CMakeFiles/player.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/player.dir/flags.make

CMakeFiles/player.dir/main.c.o: CMakeFiles/player.dir/flags.make
CMakeFiles/player.dir/main.c.o: main.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/raoxu/ffmpeg-2.5.11/raoxu/work2/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building C object CMakeFiles/player.dir/main.c.o"
	/usr/bin/cc  $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/player.dir/main.c.o   -c /home/raoxu/ffmpeg-2.5.11/raoxu/work2/main.c

CMakeFiles/player.dir/main.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/player.dir/main.c.i"
	/usr/bin/cc  $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/raoxu/ffmpeg-2.5.11/raoxu/work2/main.c > CMakeFiles/player.dir/main.c.i

CMakeFiles/player.dir/main.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/player.dir/main.c.s"
	/usr/bin/cc  $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/raoxu/ffmpeg-2.5.11/raoxu/work2/main.c -o CMakeFiles/player.dir/main.c.s

CMakeFiles/player.dir/main.c.o.requires:

.PHONY : CMakeFiles/player.dir/main.c.o.requires

CMakeFiles/player.dir/main.c.o.provides: CMakeFiles/player.dir/main.c.o.requires
	$(MAKE) -f CMakeFiles/player.dir/build.make CMakeFiles/player.dir/main.c.o.provides.build
.PHONY : CMakeFiles/player.dir/main.c.o.provides

CMakeFiles/player.dir/main.c.o.provides.build: CMakeFiles/player.dir/main.c.o


# Object files for target player
player_OBJECTS = \
"CMakeFiles/player.dir/main.c.o"

# External object files for target player
player_EXTERNAL_OBJECTS =

player: CMakeFiles/player.dir/main.c.o
player: CMakeFiles/player.dir/build.make
player: CMakeFiles/player.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/raoxu/ffmpeg-2.5.11/raoxu/work2/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking C executable player"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/player.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/player.dir/build: player

.PHONY : CMakeFiles/player.dir/build

CMakeFiles/player.dir/requires: CMakeFiles/player.dir/main.c.o.requires

.PHONY : CMakeFiles/player.dir/requires

CMakeFiles/player.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/player.dir/cmake_clean.cmake
.PHONY : CMakeFiles/player.dir/clean

CMakeFiles/player.dir/depend:
	cd /home/raoxu/ffmpeg-2.5.11/raoxu/work2 && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/raoxu/ffmpeg-2.5.11/raoxu/work2 /home/raoxu/ffmpeg-2.5.11/raoxu/work2 /home/raoxu/ffmpeg-2.5.11/raoxu/work2 /home/raoxu/ffmpeg-2.5.11/raoxu/work2 /home/raoxu/ffmpeg-2.5.11/raoxu/work2/CMakeFiles/player.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/player.dir/depend

