cmake_minimum_required(VERSION 3.16)
project(smart_studio_sim C)

set(CMAKE_C_STANDARD 11)

include(FetchContent)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "Do not update cached dependencies" FORCE)
FetchContent_Declare(
    lvgl
    GIT_REPOSITORY https://github.com/lvgl/lvgl.git
    GIT_TAG v8.4.0
    GIT_SHALLOW TRUE
)

add_compile_definitions(LV_CONF_INCLUDE_SIMPLE LV_LVGL_H_INCLUDE_SIMPLE)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
FetchContent_MakeAvailable(lvgl)

get_filename_component(UI_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/.. ABSOLUTE)

set(UI_SOURCES
    ${UI_ROOT}/ui.c
    ${UI_ROOT}/ui_model.c
    ${UI_ROOT}/ui_theme.c
    ${UI_ROOT}/ui_icons.c
    ${UI_ROOT}/ui_fonts.c
)

# Simulator with test hooks: interactive window, regression, screenshots
add_executable(sim_pc main.c ${UI_SOURCES})
target_compile_definitions(sim_pc PRIVATE UI_TEST_HOOKS)
target_include_directories(sim_pc PRIVATE ${UI_ROOT} ${lvgl_SOURCE_DIR})
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(sim_pc PRIVATE -Wall -Wextra -Wpedantic -Werror -g)
    target_link_options(sim_pc PRIVATE -Wl,--wrap=lv_mem_alloc -Wl,--wrap=lv_mem_realloc)
endif()
target_link_libraries(sim_pc PRIVATE lvgl gdi32 user32)

# Production smoke test: the exported sources as strict C99, no test hooks
add_executable(smoke_production smoke_production.c ${UI_SOURCES})
set_target_properties(smoke_production PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
target_include_directories(smoke_production PRIVATE ${UI_ROOT} ${lvgl_SOURCE_DIR})
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(smoke_production PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
target_link_libraries(smoke_production PRIVATE lvgl)
