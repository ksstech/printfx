# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.18

# Default target executed when no arguments are given to make.
default_target: all

.PHONY : default_target

# Allow only one "make -f Makefile2" at a time, but pass parallelism.
.NOTPARALLEL:


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
CMAKE_COMMAND = /usr/local/Cellar/cmake/3.18.2/bin/cmake

# The command to remove a file.
RM = /usr/local/Cellar/cmake/3.18.2/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /Users/andremaree/Dropbox/devs/ws/osx/ComponentTest

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /Users/andremaree/Dropbox/devs/ws/osx/ComponentTest

#=============================================================================
# Targets provided globally by CMake.

# Special rule for the target rebuild_cache
rebuild_cache:
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --cyan "Running CMake to regenerate build system..."
	/usr/local/Cellar/cmake/3.18.2/bin/cmake --regenerate-during-build -S$(CMAKE_SOURCE_DIR) -B$(CMAKE_BINARY_DIR)
.PHONY : rebuild_cache

# Special rule for the target rebuild_cache
rebuild_cache/fast: rebuild_cache

.PHONY : rebuild_cache/fast

# Special rule for the target edit_cache
edit_cache:
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --cyan "Running CMake cache editor..."
	/usr/local/Cellar/cmake/3.18.2/bin/ccmake -S$(CMAKE_SOURCE_DIR) -B$(CMAKE_BINARY_DIR)
.PHONY : edit_cache

# Special rule for the target edit_cache
edit_cache/fast: edit_cache

.PHONY : edit_cache/fast

# The main all target
all: cmake_check_build_system
	cd /Users/andremaree/Dropbox/devs/ws/osx/ComponentTest && $(CMAKE_COMMAND) -E cmake_progress_start /Users/andremaree/Dropbox/devs/ws/osx/ComponentTest/CMakeFiles /Users/andremaree/Dropbox/devs/ws/z-comp/printf//CMakeFiles/progress.marks
	cd /Users/andremaree/Dropbox/devs/ws/osx/ComponentTest && $(MAKE) $(MAKESILENT) -f CMakeFiles/Makefile2 /Users/andremaree/Dropbox/devs/ws/z-comp/printf/all
	$(CMAKE_COMMAND) -E cmake_progress_start /Users/andremaree/Dropbox/devs/ws/osx/ComponentTest/CMakeFiles 0
.PHONY : all

# The main clean target
clean:
	cd /Users/andremaree/Dropbox/devs/ws/osx/ComponentTest && $(MAKE) $(MAKESILENT) -f CMakeFiles/Makefile2 /Users/andremaree/Dropbox/devs/ws/z-comp/printf/clean
.PHONY : clean

# The main clean target
clean/fast: clean

.PHONY : clean/fast

# Prepare targets for installation.
preinstall: all
	cd /Users/andremaree/Dropbox/devs/ws/osx/ComponentTest && $(MAKE) $(MAKESILENT) -f CMakeFiles/Makefile2 /Users/andremaree/Dropbox/devs/ws/z-comp/printf/preinstall
.PHONY : preinstall

# Prepare targets for installation.
preinstall/fast:
	cd /Users/andremaree/Dropbox/devs/ws/osx/ComponentTest && $(MAKE) $(MAKESILENT) -f CMakeFiles/Makefile2 /Users/andremaree/Dropbox/devs/ws/z-comp/printf/preinstall
.PHONY : preinstall/fast

# clear depends
depend:
	cd /Users/andremaree/Dropbox/devs/ws/osx/ComponentTest && $(CMAKE_COMMAND) -S$(CMAKE_SOURCE_DIR) -B$(CMAKE_BINARY_DIR) --check-build-system CMakeFiles/Makefile.cmake 1
.PHONY : depend

# Convenience name for target.
/Users/andremaree/Dropbox/devs/ws/z-comp/printf/CMakeFiles/common.dir/rule:
	cd /Users/andremaree/Dropbox/devs/ws/osx/ComponentTest && $(MAKE) $(MAKESILENT) -f CMakeFiles/Makefile2 /Users/andremaree/Dropbox/devs/ws/z-comp/printf/CMakeFiles/common.dir/rule
.PHONY : /Users/andremaree/Dropbox/devs/ws/z-comp/printf/CMakeFiles/common.dir/rule

# Convenience name for target.
common: /Users/andremaree/Dropbox/devs/ws/z-comp/printf/CMakeFiles/common.dir/rule

.PHONY : common

# fast build rule for target.
common/fast:
	cd /Users/andremaree/Dropbox/devs/ws/osx/ComponentTest && $(MAKE) $(MAKESILENT) -f /Users/andremaree/Dropbox/devs/ws/z-comp/printf/CMakeFiles/common.dir/build.make /Users/andremaree/Dropbox/devs/ws/z-comp/printf/CMakeFiles/common.dir/build
.PHONY : common/fast

x_errors_events.o: x_errors_events.c.o

.PHONY : x_errors_events.o

# target to build an object file
x_errors_events.c.o:
	cd /Users/andremaree/Dropbox/devs/ws/osx/ComponentTest && $(MAKE) $(MAKESILENT) -f /Users/andremaree/Dropbox/devs/ws/z-comp/printf/CMakeFiles/common.dir/build.make /Users/andremaree/Dropbox/devs/ws/z-comp/printf/CMakeFiles/common.dir/x_errors_events.c.o
.PHONY : x_errors_events.c.o

x_errors_events.i: x_errors_events.c.i

.PHONY : x_errors_events.i

# target to preprocess a source file
x_errors_events.c.i:
	cd /Users/andremaree/Dropbox/devs/ws/osx/ComponentTest && $(MAKE) $(MAKESILENT) -f /Users/andremaree/Dropbox/devs/ws/z-comp/printf/CMakeFiles/common.dir/build.make /Users/andremaree/Dropbox/devs/ws/z-comp/printf/CMakeFiles/common.dir/x_errors_events.c.i
.PHONY : x_errors_events.c.i

x_errors_events.s: x_errors_events.c.s

.PHONY : x_errors_events.s

# target to generate assembly for a file
x_errors_events.c.s:
	cd /Users/andremaree/Dropbox/devs/ws/osx/ComponentTest && $(MAKE) $(MAKESILENT) -f /Users/andremaree/Dropbox/devs/ws/z-comp/printf/CMakeFiles/common.dir/build.make /Users/andremaree/Dropbox/devs/ws/z-comp/printf/CMakeFiles/common.dir/x_errors_events.c.s
.PHONY : x_errors_events.c.s

x_stdio.o: x_stdio.c.o

.PHONY : x_stdio.o

# target to build an object file
x_stdio.c.o:
	cd /Users/andremaree/Dropbox/devs/ws/osx/ComponentTest && $(MAKE) $(MAKESILENT) -f /Users/andremaree/Dropbox/devs/ws/z-comp/printf/CMakeFiles/common.dir/build.make /Users/andremaree/Dropbox/devs/ws/z-comp/printf/CMakeFiles/common.dir/x_stdio.c.o
.PHONY : x_stdio.c.o

x_stdio.i: x_stdio.c.i

.PHONY : x_stdio.i

# target to preprocess a source file
x_stdio.c.i:
	cd /Users/andremaree/Dropbox/devs/ws/osx/ComponentTest && $(MAKE) $(MAKESILENT) -f /Users/andremaree/Dropbox/devs/ws/z-comp/printf/CMakeFiles/common.dir/build.make /Users/andremaree/Dropbox/devs/ws/z-comp/printf/CMakeFiles/common.dir/x_stdio.c.i
.PHONY : x_stdio.c.i

x_stdio.s: x_stdio.c.s

.PHONY : x_stdio.s

# target to generate assembly for a file
x_stdio.c.s:
	cd /Users/andremaree/Dropbox/devs/ws/osx/ComponentTest && $(MAKE) $(MAKESILENT) -f /Users/andremaree/Dropbox/devs/ws/z-comp/printf/CMakeFiles/common.dir/build.make /Users/andremaree/Dropbox/devs/ws/z-comp/printf/CMakeFiles/common.dir/x_stdio.c.s
.PHONY : x_stdio.c.s

x_terminal.o: x_terminal.c.o

.PHONY : x_terminal.o

# target to build an object file
x_terminal.c.o:
	cd /Users/andremaree/Dropbox/devs/ws/osx/ComponentTest && $(MAKE) $(MAKESILENT) -f /Users/andremaree/Dropbox/devs/ws/z-comp/printf/CMakeFiles/common.dir/build.make /Users/andremaree/Dropbox/devs/ws/z-comp/printf/CMakeFiles/common.dir/x_terminal.c.o
.PHONY : x_terminal.c.o

x_terminal.i: x_terminal.c.i

.PHONY : x_terminal.i

# target to preprocess a source file
x_terminal.c.i:
	cd /Users/andremaree/Dropbox/devs/ws/osx/ComponentTest && $(MAKE) $(MAKESILENT) -f /Users/andremaree/Dropbox/devs/ws/z-comp/printf/CMakeFiles/common.dir/build.make /Users/andremaree/Dropbox/devs/ws/z-comp/printf/CMakeFiles/common.dir/x_terminal.c.i
.PHONY : x_terminal.c.i

x_terminal.s: x_terminal.c.s

.PHONY : x_terminal.s

# target to generate assembly for a file
x_terminal.c.s:
	cd /Users/andremaree/Dropbox/devs/ws/osx/ComponentTest && $(MAKE) $(MAKESILENT) -f /Users/andremaree/Dropbox/devs/ws/z-comp/printf/CMakeFiles/common.dir/build.make /Users/andremaree/Dropbox/devs/ws/z-comp/printf/CMakeFiles/common.dir/x_terminal.c.s
.PHONY : x_terminal.c.s

x_time.o: x_time.c.o

.PHONY : x_time.o

# target to build an object file
x_time.c.o:
	cd /Users/andremaree/Dropbox/devs/ws/osx/ComponentTest && $(MAKE) $(MAKESILENT) -f /Users/andremaree/Dropbox/devs/ws/z-comp/printf/CMakeFiles/common.dir/build.make /Users/andremaree/Dropbox/devs/ws/z-comp/printf/CMakeFiles/common.dir/x_time.c.o
.PHONY : x_time.c.o

x_time.i: x_time.c.i

.PHONY : x_time.i

# target to preprocess a source file
x_time.c.i:
	cd /Users/andremaree/Dropbox/devs/ws/osx/ComponentTest && $(MAKE) $(MAKESILENT) -f /Users/andremaree/Dropbox/devs/ws/z-comp/printf/CMakeFiles/common.dir/build.make /Users/andremaree/Dropbox/devs/ws/z-comp/printf/CMakeFiles/common.dir/x_time.c.i
.PHONY : x_time.c.i

x_time.s: x_time.c.s

.PHONY : x_time.s

# target to generate assembly for a file
x_time.c.s:
	cd /Users/andremaree/Dropbox/devs/ws/osx/ComponentTest && $(MAKE) $(MAKESILENT) -f /Users/andremaree/Dropbox/devs/ws/z-comp/printf/CMakeFiles/common.dir/build.make /Users/andremaree/Dropbox/devs/ws/z-comp/printf/CMakeFiles/common.dir/x_time.c.s
.PHONY : x_time.c.s

x_utilities.o: x_utilities.c.o

.PHONY : x_utilities.o

# target to build an object file
x_utilities.c.o:
	cd /Users/andremaree/Dropbox/devs/ws/osx/ComponentTest && $(MAKE) $(MAKESILENT) -f /Users/andremaree/Dropbox/devs/ws/z-comp/printf/CMakeFiles/common.dir/build.make /Users/andremaree/Dropbox/devs/ws/z-comp/printf/CMakeFiles/common.dir/x_utilities.c.o
.PHONY : x_utilities.c.o

x_utilities.i: x_utilities.c.i

.PHONY : x_utilities.i

# target to preprocess a source file
x_utilities.c.i:
	cd /Users/andremaree/Dropbox/devs/ws/osx/ComponentTest && $(MAKE) $(MAKESILENT) -f /Users/andremaree/Dropbox/devs/ws/z-comp/printf/CMakeFiles/common.dir/build.make /Users/andremaree/Dropbox/devs/ws/z-comp/printf/CMakeFiles/common.dir/x_utilities.c.i
.PHONY : x_utilities.c.i

x_utilities.s: x_utilities.c.s

.PHONY : x_utilities.s

# target to generate assembly for a file
x_utilities.c.s:
	cd /Users/andremaree/Dropbox/devs/ws/osx/ComponentTest && $(MAKE) $(MAKESILENT) -f /Users/andremaree/Dropbox/devs/ws/z-comp/printf/CMakeFiles/common.dir/build.make /Users/andremaree/Dropbox/devs/ws/z-comp/printf/CMakeFiles/common.dir/x_utilities.c.s
.PHONY : x_utilities.c.s

# Help Target
help:
	@echo "The following are some of the valid targets for this Makefile:"
	@echo "... all (the default if no target is provided)"
	@echo "... clean"
	@echo "... depend"
	@echo "... edit_cache"
	@echo "... rebuild_cache"
	@echo "... common"
	@echo "... x_errors_events.o"
	@echo "... x_errors_events.i"
	@echo "... x_errors_events.s"
	@echo "... x_stdio.o"
	@echo "... x_stdio.i"
	@echo "... x_stdio.s"
	@echo "... x_terminal.o"
	@echo "... x_terminal.i"
	@echo "... x_terminal.s"
	@echo "... x_time.o"
	@echo "... x_time.i"
	@echo "... x_time.s"
	@echo "... x_utilities.o"
	@echo "... x_utilities.i"
	@echo "... x_utilities.s"
.PHONY : help



#=============================================================================
# Special targets to cleanup operation of make.

# Special rule to run CMake to check the build system integrity.
# No rule that depends on this can have commands that come from listfiles
# because they might be regenerated.
cmake_check_build_system:
	cd /Users/andremaree/Dropbox/devs/ws/osx/ComponentTest && $(CMAKE_COMMAND) -S$(CMAKE_SOURCE_DIR) -B$(CMAKE_BINARY_DIR) --check-build-system CMakeFiles/Makefile.cmake 0
.PHONY : cmake_check_build_system

