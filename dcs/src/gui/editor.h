#ifndef DCS_EDITOR_H
#define DCS_EDITOR_H

#include "raylib.h"
#include "canvas.h"
#include "waveform.h"

#define EDITOR_SIDEBAR_W      110
#define EDITOR_HEADER_H        30
#define EDITOR_STATUS_H        24
#define EDITOR_PANEL_H        240
#define EDITOR_FILE_PATH_LEN  256
#define EDITOR_STATUS_LEN     128

#define EDITOR_MAX_TOGGLES     16
#define EDITOR_MAX_TRACKS      32
#define EDITOR_MAX_STEPS       64

typedef enum {
    MODE_IDLE,
    MODE_PLACING,
    MODE_WIRING,
    MODE_DRAGGING,
} EditMode;

typedef enum {
    PLACE_NONE,
    PLACE_AND, PLACE_OR, PLACE_NOT,
    PLACE_INPUT, PLACE_OUTPUT,
} PlaceKind;

typedef struct {
    char   name[SIM_NAME_LEN];
    Signal value;
} InputToggle;

typedef struct {
    EditMode  mode;
    PlaceKind place_kind;

    int wire_src_node;

    int     drag_node;
    Vector2 drag_offset;

    int hover_node;
    int hover_wire;

    int counter_in;
    int counter_out;
    int counter_gate;

    char   file_path[EDITOR_FILE_PATH_LEN];
    char   status[EDITOR_STATUS_LEN];
    double status_until;

    /* ── Step 1.6: simulation state ──────────────────────── */
    InputToggle   toggles[EDITOR_MAX_TOGGLES];
    int           toggle_count;
    int           steps;
    Signal        track_values[EDITOR_MAX_TRACKS][EDITOR_MAX_STEPS];
    WaveformTrack tracks[EDITOR_MAX_TRACKS];
    int           track_count;
    int           has_run;
} EditorState;

void editor_init  (EditorState *e, const CanvasState *cs, const char *file_path);
void editor_update(EditorState *e, CanvasState *cs, Camera2D *cam);
void editor_draw  (const EditorState *e, const CanvasState *cs, Camera2D cam,
                   int screen_w, int screen_h);

/* The screen-space region where the canvas is rendered (used by main.c
   for scissor clipping). */
Rectangle editor_canvas_rect(int screen_w, int screen_h);

/* Center the camera on the bounding box of all nodes so the whole circuit
   is visible inside the canvas region. Falls back to the world origin if
   the canvas is empty. Does not change zoom. */
void editor_fit_camera(Camera2D *cam, const CanvasState *cs,
                       int screen_w, int screen_h);

/* Build a Circuit from the CanvasState, serialize, write to file.
   Returns 0 on success, -1 on error (writes message to err_out). */
int  editor_save  (const CanvasState *cs, const char *path,
                   char *err_out, int err_len);

#endif /* DCS_EDITOR_H */
