/* Copyright © 2018 MAKER.                                                    */
/* This code is licensed under the MIT License.                               */
/* See: LICENSE.md                                                            */

// main.c - The Hideo compositor and window manager.

#include <stdlib.h>

#include <math.h>
#include <string.h>

#include <skift/io.h>
#include <skift/drawing.h>
#include <skift/list.h>
#include <skift/lock.h>

#include "hideo.h"

bool check_colision(int x0, int y0, int w0, int h0, int x1, int y1, int w1, int h1)
{
    return x0 < x1 + w1 && x1 < x0 + w0 && y0 < y1 + h1 && y1 < y0 + h0;
}

/* --- Windows -------------------------------------------------------------- */

hideo_window_t *hideo_create_window(hideo_context_t* ctx, char *title, int x, int y, int w, int h)
{
    hideo_window_t *win = MALLOC(hideo_window_t);

    win->title = title;

    win->x = x;
    win->y = y;
    win->width = w;
    win->height = h;

    list_pushback(ctx->windows, (void *)win);
    ctx->focus = win;

    printf("Window@%x create", win);

    return win;
}

void hideo_window_update(hideo_context_t *ctx, hideo_window_t *w, hideo_cursor_t *c)
{
    UNUSED(ctx);
    UNUSED(w);
    UNUSED(c);

    if (ctx->dragstate.dragged == w)
    {
        ctx->dragstate.dragged->x = c->x + ctx->dragstate.offx;
        ctx->dragstate.dragged->y = c->y + ctx->dragstate.offy;
    }
}

void hideo_window_draw(hideo_context_t *ctx, hideo_window_t *w)
{
    drawing_fillrect(ctx->screen, w->x, w->y, w->width, w->height, 0xf5f5f5);

    if (w == ctx->focus)
    {
        drawing_fillrect(ctx->screen, w->x, w->y, w->width, 32, 0xffffff);
        drawing_rect(ctx->screen, w->x, w->y, w->width, w->height, 1, 0x0A64CD);
        drawing_text(ctx->screen, w->title, w->x + (w->width / 2) - (strlen(w->title) * 8) / 2, w->y + 8, 0x0);
    }
    else
    {
        drawing_rect(ctx->screen, w->x, w->y, w->width, w->height, 1, 0x939393);
        drawing_text(ctx->screen, w->title, w->x + (w->width / 2) - (strlen(w->title) * 8) / 2, w->y + 8, 0x939393);
    }    
}

/* --- Mouse cursor --------------------------------------------------------- */

hideo_window_t *hideo_window_at(hideo_context_t *ctx, int x, int y, bool header)
{
    hideo_window_t *result = NULL;

    FOREACH(w, ctx->windows)
    {
        hideo_window_t *window = (hideo_window_t *)w->value;

        if (check_colision(window->x, window->y, window->width, header ? HEADER_HEIGHT : window->height, x, y, 1, 1))
        {
            result = window;
        }
    }

    return result;
}

void hideo_cursor_update(hideo_context_t *ctx, hideo_cursor_t *c)
{
    static mouse_state_t old_state;
    static mouse_state_t new_state;

    old_state = new_state;
    sk_io_mouse_get_state(&new_state);

    new_state.x = max(min(new_state.x, (int)ctx->width - 1), 0);
    new_state.y = max(min(new_state.y, (int)ctx->height - 1), 0);


    sk_io_mouse_set_state(&new_state);
    c->x = new_state.x;
    c->y = new_state.y;

    c->rightbtn = new_state.right ? BTN_DOWN : BTN_UP;
    c->middlebtn = new_state.middle ? BTN_DOWN : BTN_UP;

    // Left button
    if (new_state.left && !old_state.left)
    {
        c->leftbtn = BTN_PRESSED;
    }
    else if (!new_state.left && old_state.left)
    {
        c->leftbtn = BTN_RELEASED;
    }
    else
    {
        c->leftbtn = new_state.left ? BTN_DOWN : BTN_UP;
    }

    // Right button
    if (new_state.right && !old_state.right)
    {
        c->rightbtn = BTN_PRESSED;
    }
    else if (!new_state.right && old_state.right)
    {
        c->rightbtn = BTN_RELEASED;
    }
    else
    {
        c->rightbtn = new_state.right ? BTN_DOWN : BTN_UP;
    }


    // middle button.
    if (new_state.middle && !old_state.middle)
    {
        c->middlebtn = BTN_PRESSED;
    }
    else if (!new_state.middle && old_state.middle)
    {
        c->middlebtn = BTN_RELEASED;
    }
    else
    {
        c->middlebtn = new_state.middle ? BTN_DOWN : BTN_UP;
    }

    hideo_window_t * win = hideo_window_at(ctx, c->x, c->y, true);
    
    if (win != NULL)
    {
        if (c->leftbtn == BTN_PRESSED)
        {
            ctx->focus = win;
            list_remove(ctx->windows, win);
            list_pushback(ctx->windows, win);

            ctx->dragstate.dragged = win;
            ctx->dragstate.offx = win->x - c->x;
            ctx->dragstate.offy = win->y - c->y;
        }
    }

    if (ctx->dragstate.dragged != NULL && c->leftbtn == BTN_RELEASED)
    {
        ctx->dragstate.dragged = NULL;
    }
}

void hideo_cursor_draw(hideo_context_t *ctx, hideo_cursor_t *c)
{
#define SCALE 2

    drawing_filltri(ctx->screen, c->x, c->y, c->x, c->y + 12 * SCALE, c->x + 8 * SCALE, c->y + 8 * SCALE, 0xffffff);

    drawing_line(ctx->screen, c->x, c->y, c->x, c->y + 12 * SCALE, 1, 0x0);
    drawing_line(ctx->screen, c->x, c->y, c->x + 8 * SCALE, c->y + 8 * SCALE, 1, 0x0);
    drawing_line(ctx->screen, c->x, c->y + 12 * SCALE, c->x + 8 * SCALE, c->y + 8 * SCALE, 1, 0x0);
}

/* --- Hideo ---------------------------------------------------------------- */

hideo_context_t *hideo_ctor(uint screen_width, uint screen_height)
{
    hideo_context_t *ctx = MALLOC(hideo_context_t);

    ctx->width = screen_width;
    ctx->height = screen_height;

    ctx->screen = bitmap_ctor(screen_width, screen_height);

    ctx->windows = list_alloc();
    ctx->focus = NULL;

    return ctx;
}

int main(int argc, char const *argv[])
{
    UNUSED(argc);
    UNUSED(argv);

    sk_io_print("Hideo compositor and window manager");

    uint width, height = 0;
    sk_io_graphic_size(&width, &height);
    printf("Graphic context created %dx%d", width, height);

    hideo_context_t *ctx = hideo_ctor(width, height);

    hideo_cursor_t cur = {.x = ctx->width / 2, .y = ctx->height / 2};

    hideo_create_window(ctx, "Hello, world!", 54, 96, 256, 128);
    hideo_create_window(ctx, "Good bye!", 300, 128, 256, 128);

    while (1)
    {
        // Update
        hideo_cursor_update(ctx, &cur);

        FOREACH(w, ctx->windows)
        {
            hideo_window_t *window = (hideo_window_t *)w->value;
            hideo_window_update(ctx, window, &cur);
        }

        // Draw
        drawing_clear(ctx->screen, 0xe5e5e5);

        FOREACH(w, ctx->windows)
        {
            hideo_window_t *window = (hideo_window_t *)w->value;
            hideo_window_draw(ctx, window);
        }

        hideo_cursor_draw(ctx, &cur);

        // Blit
        sk_io_graphic_blit(ctx->screen->buffer);
    }

    return 0;
}
