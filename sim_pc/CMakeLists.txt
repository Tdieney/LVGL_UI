cmake_minimum_required(VERSION 3.16)
project(smart_hub_sim C)

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
add_executable(sim_pc main.c ${UI_ROOT}/ui.c)
target_include_directories(sim_pc PRIVATE ${UI_ROOT} ${lvgl_SOURCE_DIR})
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(sim_pc PRIVATE -Wall -Wextra -Werror)
endif()
target_link_libraries(sim_pc PRIVATE lvgl gdi32 user32)
