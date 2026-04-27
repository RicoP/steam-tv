cmake_minimum_required(VERSION 3.21)

project(steam_tv C CXX)

set(CMAKE_CONFIGURATION_TYPES Debug Release)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CEF_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/external/cef")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CEF_ROOT}/cmake")

find_package(CEF REQUIRED)

add_subdirectory(external/raylib EXCLUDE_FROM_ALL)
add_subdirectory(${CEF_LIBCEF_DLL_WRAPPER_PATH} libcef_dll_wrapper)

ADD_LOGICAL_TARGET("libcef_lib" "${CEF_LIB_DEBUG}" "${CEF_LIB_RELEASE}")

set(CEF_TARGET steam_tv)
SET_CEF_TARGET_OUT_DIR()

add_executable(steam_tv
    src/main.cpp
)

SET_EXECUTABLE_TARGET_PROPERTIES(steam_tv)

target_include_directories(steam_tv PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/external/raylib/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/cef/include"
)

target_compile_definitions(steam_tv PRIVATE
    STEAM_TV_STATIC_SITE_FILE="${CMAKE_CURRENT_SOURCE_DIR}/src/static/index.html"
)

target_link_libraries(steam_tv
    raylib
    libcef_lib
    libcef_dll_wrapper
    ${CEF_STANDARD_LIBS}
)

add_dependencies(steam_tv libcef_dll_wrapper)

set_target_properties(steam_tv PROPERTIES
    INSTALL_RPATH "$ORIGIN"
    BUILD_WITH_INSTALL_RPATH TRUE
    RUNTIME_OUTPUT_DIRECTORY ${CEF_TARGET_OUT_DIR}
)

COPY_FILES("steam_tv" "${CEF_BINARY_FILES}" "${CEF_BINARY_DIR}" "${CEF_TARGET_OUT_DIR}")
COPY_FILES("steam_tv" "${CEF_RESOURCE_FILES}" "${CEF_RESOURCE_DIR}" "${CEF_TARGET_OUT_DIR}")

if(EXISTS "${CEF_BINARY_DIR}/libminigbm.so")
    COPY_FILES("steam_tv" "libminigbm.so" "${CEF_BINARY_DIR}" "${CEF_TARGET_OUT_DIR}")
endif()
