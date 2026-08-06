# LVGL UI Style & Architecture Guide

This document defines the architectural rules, coding standards, and design guidelines for developing LVGL v8.4.0 interfaces in this repository.

## Target Hardware Specifications

- Embedded GUI using C/C++ with LVGL v8.4.0.
- Target display: 800x480 resolution, 16-bit RGB565 color format.

## Code Architecture

The UI codebase is structured into modular components:

- **`ui.h` / `ui.c` (Core Coordinator):**
  - `ui_init()`: Initializes UI resources and loads the initial screen.
  - `ui_tick()`: Invoked periodically from the main loop to execute rate-limited widget refreshes.
  - `ui_build_tab()`: Centralized screen switching coordinator. Executes `lv_obj_clean()` and instantiates the target screen.
- **`screens.h` / `screens.c` (Layouts & Widgets):**
  - Implements screen construction (`create_screen_<name>()`) and refresh routines (`tick_screen_<name>()`).
  - Dynamic widgets (labels, charts, bars) are maintained via static widget pointers scoped within `screens.c`.
- **`actions.h` / `actions.c` (Event Handlers & Dispatch):**
  - Contains user input callbacks (e.g., `action_tab_monitor`, `action_motor_start`).
  - Handles command dispatch to `motorCmd` wire bitfields.
- **`images.h` / `ui_image_*.c` / `ui_icon_*.c` (Graphic Assets):**
  - Contains C-array image descriptors and 4-bit alpha icon masks.

## Development Rules & Best Practices

1. **Plan Tracking:** Keep `PLAN.md` up to date with completed milestones and pending tasks.
2. **Object Creation & Scoping:**
    - Instantiate widgets using `lv_obj_create(parent)`.
    - Apply precise positioning with `lv_obj_set_pos()` and dimensions with `lv_obj_set_size()`.
3. **Widget Flags Management:**
    - Static elements (backgrounds, cards, decorative frames) MUST clear click and scroll flags to minimize processing overhead:
      `lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);`
    - Interactive buttons MUST enable clickable flags and set explicit touch hit areas:
      `lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);`
      `lv_obj_set_ext_click_area(obj, 6);`
4. **Styling Standards:**
    - Prefer local styling unless sharing reusable patterns across widgets.
    - Specify explicit target parts and states: `LV_PART_MAIN | LV_STATE_DEFAULT`.
5. **Data Refresh & Active Screen Guards:**
    - Never update UI elements directly from ISR or DMA contexts. Update telemetry structures in the background and process visual updates inside `ui_tick()`.
    - **Active Screen Guard:** Screen switches teardown previous widgets. Refresh routines MUST guard execution using `if (ui_current_tab != TAB_ID) return;` at the start of every `tick_screen_<name>()` to prevent invalid memory dereferences.
6. **Layout & Box Model Alignment:**
    - Coordinates set via `lv_obj_set_pos(obj, x, y)` are relative to the parent container's Content Area (excluding parent padding). Take parent padding into account when positioning child widgets.
    - Components with extended visual bounding boxes (such as slider knobs) require `lv_obj_add_flag(parent, LV_OBJ_FLAG_OVERFLOW_VISIBLE)` to prevent visual clipping.
