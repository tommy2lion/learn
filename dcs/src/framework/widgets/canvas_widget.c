#include "canvas_widget.h"
#include "../core/color.h"
#include <stdlib.h>

vec2_t canvas_widget_screen_to_world(const canvas_widget_t *c, vec2_t s) {
    return (vec2_t){
        (s.x - c->cam_offset.x) / c->cam_zoom + c->cam_target.x,
        (s.y - c->cam_offset.y) / c->cam_zoom + c->cam_target.y,
    };
}

vec2_t canvas_widget_world_to_screen(const canvas_widget_t *c, vec2_t w) {
    return (vec2_t){
        (w.x - c->cam_target.x) * c->cam_zoom + c->cam_offset.x,
        (w.y - c->cam_target.y) * c->cam_zoom + c->cam_offset.y,
    };
}

static void canvas_draw(widget_t *self, igraph_t *g) {
    canvas_widget_t *c = (canvas_widget_t *)self;

    /* Pan: poll the button each frame so a drag continues even if the
       cursor leaves the widget bounds; press starts a pan only when over us. */
    vec2_t mp = g->mouse_position(g->self);
    int over = widget_hits(self, mp);
    int down = g->mouse_down(g->self, c->pan_button);
    if (!c->panning && down && over) {
        if (g->mouse_pressed(g->self, c->pan_button)) c->panning = 1;
    }
    if (c->panning) {
        if (!down) {
            c->panning = 0;
        } else {
            vec2_t d = g->mouse_delta(g->self);
            c->cam_target.x -= d.x / c->cam_zoom;
            c->cam_target.y -= d.y / c->cam_zoom;
        }
    }

    if (c->bg_color) g->draw_rect(g->self, self->bounds, c->bg_color);

    g->push_scissor(g->self, self->bounds);
    g->push_camera2d(g->self, c->cam_target, c->cam_offset, c->cam_zoom);
    if (c->draw_world) c->draw_world(g, c->draw_user);
    g->pop_camera2d(g->self);
    g->pop_scissor(g->self);
}

static int canvas_handle_event(widget_t *self, const event_t *ev) {
    canvas_widget_t *c = (canvas_widget_t *)self;

    /* Zoom toward cursor on wheel events. */
    if (c->zoom_enabled && ev->kind == EV_MOUSE_WHEEL && ev->wheel.dy != 0.0f) {
        vec2_t mp = ev->wheel.pos;
        vec2_t mw = canvas_widget_screen_to_world(c, mp);
        c->cam_offset = mp;
        c->cam_target = mw;
        c->cam_zoom += ev->wheel.dy * 0.1f * c->cam_zoom;
        if (c->cam_zoom < c->zoom_min) c->cam_zoom = c->zoom_min;
        if (c->cam_zoom > c->zoom_max) c->cam_zoom = c->zoom_max;
        return 1;
    }

    /* Press over canvas → maybe begin pan. The actual pan motion is
       polled in draw (see above). We just consume the press so it
       doesn't fall through to anything beneath. */
    if (ev->kind == EV_MOUSE_PRESS && ev->mouse.btn == c->pan_button) {
        return 1;
    }
    return 0;
}

static void canvas_destroy(widget_t *self) { free(self); }

static const widget_vt_t CANVAS_VT = {
    .draw         = canvas_draw,
    .handle_event = canvas_handle_event,
    .destroy      = canvas_destroy,
};

canvas_widget_t *canvas_widget_create(rect_t bounds,
                                      canvas_draw_world_t draw_world, void *user) {
    canvas_widget_t *c = (canvas_widget_t *)calloc(1, sizeof(canvas_widget_t));
    if (!c) return NULL;
    c->base.vt      = &CANVAS_VT;
    c->base.bounds  = bounds;
    c->base.visible = 1;
    /* Default camera: world origin sits at the widget's top-left.
       canvas_widget_set_camera() is the recommended way to position it. */
    c->cam_target   = (vec2_t){0, 0};
    c->cam_offset   = (vec2_t){bounds.x, bounds.y};
    c->cam_zoom     = 1.0f;
    c->zoom_min     = 0.1f;
    c->zoom_max     = 5.0f;
    c->draw_world   = draw_world;
    c->draw_user    = user;
    c->pan_button   = IM_MIDDLE;
    c->zoom_enabled = 1;
    c->bg_color     = 0;   /* transparent by default */
    return c;
}

void canvas_widget_set_camera(canvas_widget_t *self, vec2_t target, float zoom) {
    self->cam_target = target;
    self->cam_zoom   = zoom;
    /* offset stays at bounds top-left unless caller updates it */
}

void canvas_widget_set_pan_btn (canvas_widget_t *self, igraph_mouse_btn_t btn) { self->pan_button = btn; }
void canvas_widget_set_zoom_lim(canvas_widget_t *self, float mn, float mx)     { self->zoom_min = mn; self->zoom_max = mx; }
void canvas_widget_set_bg      (canvas_widget_t *self, uint32_t color)         { self->bg_color = color; }
