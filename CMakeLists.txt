cmake_minimum_required(VERSION 3.20)
project(KewlKlient CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The game is 64-bit, so we must be too. A 32-bit DLL will simply be refused at injection time, which
# looks like "nothing happened" and wastes an evening.
if(CMAKE_SIZEOF_VOID_P EQUAL 4)
  message(FATAL_ERROR "KewlKlient must be built 64-bit -- the game is x64 and will reject a 32-bit DLL.")
endif()

# JNI headers. We only need the headers to compile; jvm.dll is loaded at runtime from the path in
# kewlklient.ini, so the machine that BUILDS this does not have to be the one that runs it.
find_package(JNI REQUIRED)

# TARGET NAMES MUST NOT DIFFER ONLY BY CASE. They used to be `kewlklient` and `KewlKlient`, which the
# Makefile and Ninja generators handle fine -- but the Visual Studio generator writes one .vcxproj per
# target, named after it, and Windows filenames are case-insensitive. `KewlKlient.vcxproj` therefore
# OVERWROTE `kewlklient.vcxproj`, both targets became the launcher, and no DLL was ever produced. The
# build still reported success, because from MSBuild's point of view both projects built perfectly.
#
# The internal names below are distinct; OUTPUT_NAME keeps the shipped filenames exactly as they were.

# ---------------------------------------------------------------------------------------------------
# kewlklient.dll -- injected into the game
# ---------------------------------------------------------------------------------------------------
add_library(kewl_dll SHARED client/dllmain.cpp)
target_include_directories(kewl_dll PRIVATE client ${JNI_INCLUDE_DIRS})
target_link_libraries(kewl_dll PRIVATE user32 gdi32)
set_target_properties(kewl_dll PROPERTIES PREFIX "" OUTPUT_NAME "kewlklient")

# ---------------------------------------------------------------------------------------------------
# KewlKlient.exe -- the launcher with the button
# ---------------------------------------------------------------------------------------------------
add_executable(kewl_launcher WIN32 launcher/main.cpp)
target_link_libraries(kewl_launcher PRIVATE user32)
set_target_properties(kewl_launcher PROPERTIES OUTPUT_NAME "KewlKlient")

# Drop both next to each other so the launcher can find the DLL by sitting beside it, and so the DLL
# can find kewlklient.jar the same way. Gradle passes KEWL_DIST; a bare cmake run gets a sane default.
if(NOT DEFINED KEWL_DIST)
  set(KEWL_DIST "${CMAKE_BINARY_DIR}/dist")
endif()

# Multi-config generators (Visual Studio, Xcode) APPEND the config name to the plain
# RUNTIME_OUTPUT_DIRECTORY, so setting only that puts the binaries in dist/Release while the jar and
# the ini land in dist/ -- the DLL then loads and cannot find the jar, which surfaces as
# "kewl/KewlKlient not found" and looks like a Java problem. Setting the per-config variables as well
# suppresses that suffix, so every generator agrees on one directory.
foreach(cfg "" _DEBUG _RELEASE _RELWITHDEBINFO _MINSIZEREL)
  set_target_properties(kewl_dll kewl_launcher PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY${cfg} "${KEWL_DIST}"
      LIBRARY_OUTPUT_DIRECTORY${cfg} "${KEWL_DIST}")
endforeach()
