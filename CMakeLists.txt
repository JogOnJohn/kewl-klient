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

# ---------------------------------------------------------------------------------------------------
# kewlklient.dll -- injected into the game
# ---------------------------------------------------------------------------------------------------
add_library(kewlklient SHARED client/dllmain.cpp)
target_include_directories(kewlklient PRIVATE client ${JNI_INCLUDE_DIRS})
target_link_libraries(kewlklient PRIVATE user32 gdi32)
set_target_properties(kewlklient PROPERTIES PREFIX "" OUTPUT_NAME "kewlklient")

# ---------------------------------------------------------------------------------------------------
# KewlKlient.exe -- the launcher with the button
# ---------------------------------------------------------------------------------------------------
add_executable(KewlKlient WIN32 launcher/main.cpp)
target_link_libraries(KewlKlient PRIVATE user32)

# Drop both next to each other so the launcher can find the DLL by sitting beside it.
set_target_properties(kewlklient KewlKlient PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/dist"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/dist")
