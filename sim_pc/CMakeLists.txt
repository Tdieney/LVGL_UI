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

# The UI sources under test live at the repo root. Keep this manifest explicit:
# CMake's CONFIGURE_DEPENDS glob check can hang on some Windows/Ninja setups,
# and an explicit list also prevents retired generated assets entering builds.
get_filename_component(UI_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/.. ABSOLUTE)
set(UI_SOURCES
    ${UI_ROOT}/actions.c
    ${UI_ROOT}/demo_sim.c
    ${UI_ROOT}/screens.c
    ${UI_ROOT}/ui.c
    ${UI_ROOT}/ui_icon_bolt.c
    ${UI_ROOT}/ui_icon_ccw.c
    ${UI_ROOT}/ui_icon_check.c
    ${UI_ROOT}/ui_icon_chevron.c
    ${UI_ROOT}/ui_icon_ctrl.c
    ${UI_ROOT}/ui_icon_cw.c
    ${UI_ROOT}/ui_icon_dash.c
    ${UI_ROOT}/ui_icon_diag.c
    ${UI_ROOT}/ui_icon_graph.c
    ${UI_ROOT}/ui_icon_link.c
    ${UI_ROOT}/ui_icon_mon.c
    ${UI_ROOT}/ui_icon_set.c
    ${UI_ROOT}/ui_icon_thermo.c
    ${UI_ROOT}/ui_icon_xmark.c
    ${UI_ROOT}/ui_image_logo.c
    ${UI_ROOT}/ui_img_topbar.c
    ${UI_ROOT}/ui_img_topbar_chip.c
    ${UI_ROOT}/ui_img_dashboard_gauge.c
    ${UI_ROOT}/fonts/ui_font_mono20.c
    ${UI_ROOT}/fonts/ui_font_mono22.c
    ${UI_ROOT}/fonts/ui_font_mono30.c
    ${UI_ROOT}/fonts/ui_font_mono38.c
    ${UI_ROOT}/fonts/ui_font_mono44.c
    ${UI_ROOT}/fonts/ui_font_mono56.c
    ${UI_ROOT}/fonts/ui_font_mono66.c
    ${UI_ROOT}/fonts/ui_font_sans20.c
    ${UI_ROOT}/fonts/ui_font_sans22.c
)

add_executable(sim_pc main.c ${UI_SOURCES})
target_include_directories(sim_pc PRIVATE ${UI_ROOT} ${lvgl_SOURCE_DIR})
target_compile_definitions(sim_pc PRIVATE UI_DEMO_SIM=1)
target_link_libraries(sim_pc PRIVATE lvgl gdi32 user32)
