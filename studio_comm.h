#ifndef STUDIO_COMM_H
#define STUDIO_COMM_H

/* Typed commands exchanged between the UI and the firmware's UART layer.
 *
 * The firmware parses newline-delimited JSON from the PC, validates it, fills
 * a studio_cmd_t and passes it to ui_apply() in GUI context. Local LCD actions
 * arrive through the ui_action_cb_t callback as the same type; the firmware
 * turns them into JSON events. Wire mapping (every line ends with "\n"):
 *
 *   {"cmd":"LIGHT","power":1,"bright":80}   STUDIO_CMD_LIGHT  power 0/1, bright 0..100
 *   {"cmd":"AC","power":1,"temp":25}        STUDIO_CMD_AC     power 0/1, temp 16..30
 *   {"cmd":"ENV","temp":27,"humid":60}      STUDIO_CMD_ENV    whole ints: temp int16, humid 0..100
 *   {"cmd":"SCENE","mode":"WELCOME"}        STUDIO_CMD_SCENE  WELCOME->HOME, SLEEP, LEAVE->AWAY
 *   {"cmd":"BAUD","val":115200}             STUDIO_CMD_BAUD   one of STUDIO_BAUD_RATES
 *
 * Events use "event" instead of "cmd" with the same fields. */

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    STUDIO_CMD_LIGHT = 0,
    STUDIO_CMD_AC,
    STUDIO_CMD_ENV,
    STUDIO_CMD_SCENE,
    STUDIO_CMD_BAUD
} studio_cmd_kind_t;

typedef enum {
    STUDIO_SCENE_NONE = 0,
    STUDIO_SCENE_HOME,
    STUDIO_SCENE_SLEEP,
    STUDIO_SCENE_AWAY
} studio_scene_t;

/* Numeric fields stay int until validation has completed. */
typedef struct {
    studio_cmd_kind_t kind;
    union {
        struct { int power; int bright; } light;
        struct { int power; int temp; } ac;
        struct { int temp; int humid; bool valid; } env;
        studio_scene_t scene;
        uint32_t baud;
    } data;
} studio_cmd_t;

#define STUDIO_BAUD_COUNT 8u
#define STUDIO_BAUD_RATES { 9600u, 19200u, 38400u, 57600u, 115200u, 230400u, 460800u, 921600u }
#define STUDIO_BAUD_DEFAULT 115200u

#ifdef __cplusplus
}
#endif

#endif /* STUDIO_COMM_H */
