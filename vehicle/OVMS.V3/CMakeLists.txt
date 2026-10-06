
# (Automatically converted from project Makefile by convert_to_cmake.py.)

# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

message(AUTHOR_WARNING "WARNING - OVMSv3 build is only supported on ESP-IDF 3.3.x")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# On the new xtensa-esp-elf toolchains uint32_t is typedef'd as 'long unsigned int',
# so the legacy OVMS logging/format code (which uses plain %u/%d/%x specifiers) would
# trigger -Wformat warnings. Keep those as warnings, not errors, across the whole build.
idf_build_set_property(COMPILE_OPTIONS "-Wno-error=format;-Wno-error=format-truncation;-Wno-error=format-overflow;-Wno-error=misleading-indentation;-Wno-error=int-in-bool-context" APPEND)

project(ovms3)
