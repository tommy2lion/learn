#ifndef DCS_FW_CANVAS_WIDGET_H
#define DCS_FW_CANVAS_WIDGET_H

#include "widget.h"
#include <stdint.h>

/* Pannable / zoomable area. The widget owns its 2D camera; subclasses
   (or callers) provide a draw_world callback that paints in world
   coordinates. The camera transform + scissor clipping are handled by
   the widget. Default pan = middle-mouse drag; zoom = wheel toward
   cursor. */

typedef void (*canvas_draw_world_t)(igraph_t *g, void *user);

class tagt_canvas_widget {
    widget_t base;
    /* camera */
    vec2_t   cam_target;
    vec2_t   cam_offset;
    float    cam_zoom;
    float    zoom_min, zoom_max;
    /* drawing callback */
    canvas_draw_world_t draw_world;
    void                *draw_user;
    /* options */
    igraph_mouse_btn_t   pan_button;
    int                  zoom_enabled;
    /* runtime state */
    int                  panning;
    /* visuals */
    uint32_t             bg_color;
};
typedef class tagt_canvas_widget canvas_widget_t;

canvas_widget_t *canvas_widget_create(rect_t bounds,
                                      canvas_draw_world_t draw_world, void *user);

void   canvas_widget_set_camera   (canvas_widget_t *self, vec2_t target, float zoom);
void   canvas_widget_set_pan_btn  (canvas_widget_t *self, igraph_mouse_btn_t btn);
void   canvas_widget_set_zoom_lim (canvas_widget_t *self, float min, float max);
void   canvas_widget_set_bg       (canvas_widget_t *self, uint32_t color);

vec2_t canvas_widget_screen_to_world(const canvas_widget_t *self, vec2_t s);
vec2_t canvas_widget_world_to_screen(const canvas_widget_t *self, vec2_t w);

#endif /* DCS_FW_CANVAS_WIDGET_H */
