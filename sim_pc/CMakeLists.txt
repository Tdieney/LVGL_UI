cmake_minimum_required(VERSION 3.16)
project(sim_pc C)

set(CMAKE_C_STANDARD 11)

include(FetchContent)
# Once LVGL has been cloned, configure/build must stay deterministic and work
# offline. Initial population is still allowed when the source is absent.
set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "Do not update cached dependencies" FORCE)
FetchContent_Declare(
    lvgl
    GIT_REPOSITORY https://github.com/lvgl/lvgl.git
    GIT_TAG v8.4.0
    GIT_SHALLOW TRUE
)

# lv_conf.h lives in this directory; LVGL finds it via LV_CONF_INCLUDE_SIMPLE.
# LV_LVGL_H_INCLUDE_SIMPLE makes lv_font_conv/png2lvgl output include "lvgl.h".
# These directory-level settings propagate into the lvgl target created below.
add_compile_definitions(LV_CONF_INCLUDE_SIMPLE LV_LVGL_H_INCLUDE_SIMPLE)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

FetchContent_MakeAvailable(lvgl)

option(SIM_PERF_MONITOR "Show the LVGL FPS/CPU overlay" OFF)
if(SIM_PERF_MONITOR)
    add_compile_definitions(SIM_PERF_MONITOR)
endif()

# The UI sources under test live at the repo root (and fonts/ once generated)
get_filename_component(UI_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/.. ABSOLUTE)
file(GLOB UI_SOURCES CONFIGURE_DEPENDS ${UI_ROOT}/*.c ${UI_ROOT}/fonts/*.c)

add_executable(sim_pc main.c ${UI_SOURCES})
target_include_directories(sim_pc PRIVATE ${UI_ROOT} ${lvgl_SOURCE_DIR})
target_compile_definitions(sim_pc PRIVATE UI_DEMO_SIM=1)
target_link_libraries(sim_pc PRIVATE lvgl gdi32 user32)
