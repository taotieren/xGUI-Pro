/*
** layouter.h -- The module interface for page/window layouter.
**
** Copyright (C) 2022 FMSoft (http://www.fmsoft.cn)
**
** Author: Vincent Wei (https://github.com/VincentWei)
**
** This file is part of xGUI Pro, an advanced HVML renderer.
**
** xGUI Pro is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
**
** xGUI Pro is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
** You should have received a copy of the GNU General Public License
** along with this program.  If not, see http://www.gnu.org/licenses/.
*/

#ifndef XGUIPRO_LAYOUTER_LAYOUTER_H
#define XGUIPRO_LAYOUTER_LAYOUTER_H

#include <purc/purc-variant.h>

#define DEF_LAYOUT_CSS  "assets/workspace-layouter.css"

/* The layouter object; `ws` means workspace */
struct ws_layouter;

struct ws_metrics {
    unsigned int    width;
    unsigned int    height;
    unsigned int    dpi;
    unsigned int    density;
};

typedef enum {
    WS_WIDGET_TYPE_NONE  = 0,       /* not-existing */
    WS_WIDGET_TYPE_PLAINWINDOW,     /* a plain main window for a webview */
    WS_WIDGET_TYPE_TABBEDWINDOW,    /* a tabbede main window for webviews */
    WS_WIDGET_TYPE_HEADER,  /* the renderer defiend header widget */
    WS_WIDGET_TYPE_MENUBAR, /* the renderer defined menu bar widget */
    WS_WIDGET_TYPE_TOOLBAR, /* the renderer defined tool bar widget */
    WS_WIDGET_TYPE_SIDEBAR, /* the renderer defined aside widget */
    WS_WIDGET_TYPE_FOOTER,  /* the renderer defiend footer widget */
    WS_WIDGET_TYPE_PANEHOST,   /* the container widget of plain pages */
    WS_WIDGET_TYPE_TABHOST, /* the container widget of tabbed pages */
    WS_WIDGET_TYPE_PANEDPAGE,   /* a plain page for a webview */
    WS_WIDGET_TYPE_TABBEDPAGE,     /* a tabbed page for a webview */
} ws_widget_type_t;

#define WSWS_FLAG_NAME      0x00000001
#define WSWS_FLAG_TITLE     0x00000002
#define WSWS_FLAG_GEOMETRY  0x00000004
#define WSWS_FLAG_TOOLKIT   0x00000008

struct ws_widget_style {
    unsigned int flags;

    const char *name;
    const char *title;

    /* other styles */
    const char *backgroundColor;
    bool        darkMode;
    bool        fullScreen;

    int         x, y;
    unsigned    w, h;

    int         ml, mt, mr, mb; /* margins */
    int         pl, pt, pr, pb; /* paddings */
    float       bl, bt, br, bb; /* borders */
    float       brlt, brtr, brrb, brbl; /* border radius */

    float       opacity;
};

typedef void (*wsltr_convert_style_fn)(struct ws_widget_style *style,
        purc_variant_t widget_style);

typedef void *(*wsltr_create_widget_fn)(void *ws_ctxt, ws_widget_type_t type,
        void *parent, const struct ws_widget_style *style);

typedef int  (*wsltr_destroy_widget_fn)(void *ws_ctxt, void *widget,
        ws_widget_type_t type);

typedef void (*wsltr_update_widget_fn)(void *ws_ctxt, void *widget,
        ws_widget_type_t type, const struct ws_widget_style *style);

#ifdef __cplusplus
extern "C" {
#endif

/* Create a new layouter */
struct ws_layouter *ws_layouter_new(struct ws_metrics *metrics,
        const char *html_contents, size_t sz_html_contents, void *ws_ctxt,
        wsltr_convert_style_fn cb_convert_style,
        wsltr_create_widget_fn cb_create_widget,
        wsltr_destroy_widget_fn cb_destroy_widget,
        wsltr_update_widget_fn cb_update_widget, int *retv);

/* Destroy a layouter */
void ws_layouter_delete(struct ws_layouter *layouter);

/* Add new page groups */
int ws_layouter_add_page_groups(struct ws_layouter *layouter,
        const char *html_fragment, size_t sz_html_fragment);

/* Remove a page group */
int ws_layouter_remove_page_group(struct ws_layouter *layouter,
        const char *group_id);

/* Add a plain window into a group */
void *ws_layouter_add_plain_window(struct ws_layouter *layouter,
        const char *group_id, const char *window_name,
        const char *class_name, const char *title, const char *layout_style,
        purc_variant_t widget_style, int *retv);

/* Remove a plain window by identifier */
int ws_layouter_remove_plain_window_by_id(struct ws_layouter *layouter,
        const char *group_id, const char *window_name);

/* Remove a plain window by widget */
int ws_layouter_remove_plain_window_by_widget(struct ws_layouter *layouter,
        void *widget);

/* Add a page into a group */
void *ws_layouter_add_page(struct ws_layouter *layouter,
        const char *group_id, const char *page_name,
        const char *class_name, const char *title, const char *layout_style,
        purc_variant_t widget_style, int *retv);

/* Remove a page by identifier */
int ws_layouter_remove_page_by_id(struct ws_layouter *layouter,
        const char *group_id, const char *page_name);

/* Remove a page by widget */
int ws_layouter_remove_page_by_widget(struct ws_layouter *layouter,
        void *widget);

/* Update a widget */
int ws_layouter_update_widget(struct ws_layouter *layouter,
        void *widget, const char *property, purc_variant_t value);

/* Retrieve a widget */
ws_widget_type_t ws_layouter_retrieve_widget(struct ws_layouter *layouter,
        void *widget);

/* Retrieve a widget by group identifier and page/window name */
ws_widget_type_t
ws_layouter_retrieve_widget_by_id(struct ws_layouter *layouter,
        const char *group_id, const char *page_name);

#ifdef __cplusplus
}
#endif

#endif /* !XGUIPRO_LAYOUTER_LAYOUTER_H*/

