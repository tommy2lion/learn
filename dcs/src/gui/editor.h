#ifndef DCS_EDITOR_H
#define DCS_EDITOR_H

#include "raylib.h"
#include "canvas.h"

#define EDITOR_SIDEBAR_W   110
#define EDITOR_HEADER_H    30
#define EDITOR_STATUS_H    24
#define EDITOR_FILE_PATH_LEN 256
#define EDITOR_STATUS_LEN  128

typedef enum {
    MODE_IDLE,
    MODE_PLACING,         /* user picked a sidebar item; next canvas click places it */
    MODE_WIRING,          /* user clicked an output pin; next click on input pin wires */
    MODE_DRAGGING,        /* user is dragging an existing node */
} EditMode;

typedef enum {
    PLACE_NONE,
    PLACE_AND, PLACE_OR, PLACE_NOT,
    PLACE_INPUT, PLACE_OUTPUT,
} PlaceKind;

typedef struct {
    EditMode  mode;
    PlaceKind place_kind;

    /* WIRING state */
    int wire_src_node;

    /* DRAGGING state */
    int     drag_node;
    Vector2 drag_offset;

    /* Hover (recomputed each frame) */
    int hover_node;
    int hover_wire;

    /* Auto-name counters */
    int counter_in;
    int counter_out;
    int counter_gate;

    /* Save target + transient status message */
    char   file_path[EDITOR_FILE_PATH_LEN];
    char   status[EDITOR_STATUS_LEN];
    double status_until;
} EditorState;

void editor_init  (EditorState *e, const CanvasState *cs, const char *file_path);
void editor_update(EditorState *e, CanvasState *cs, Camera2D *cam);
void editor_draw  (const EditorState *e, const CanvasState *cs, Camera2D cam,
                   int screen_w, int screen_h);

/* Build a Circuit from the CanvasState, serialize, write to file.
   Returns 0 on success, -1 on error (writes message to err_out). */
int  editor_save  (const CanvasState *cs, const char *path,
                   char *err_out, int err_len);

#endif /* DCS_EDITOR_H */
