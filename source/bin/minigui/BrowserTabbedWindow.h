/*
** BrowserTabbedWindow.h -- The declaration of BrowserTabbedWindow.
**
** Copyright (C) 2022, 2023 FMSoft <http://www.fmsoft.cn>
**
** Author: Vincent Wei <https://github.com/VincentWei>
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

#ifndef BrowserTabbedWindow_h
#define BrowserTabbedWindow_h

#include <webkit2/webkit2.h>

G_BEGIN_DECLS

#define BROWSER_TYPE_TABBED_WINDOW            (browser_tabbed_window_get_type())
#define BROWSER_TABBED_WINDOW(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), BROWSER_TYPE_TABBED_WINDOW, BrowserTabbedWindow))
#define BROWSER_TABBED_WINDOW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  BROWSER_TYPE_TABBED_WINDOW, BrowserTabbedWindowClass))
#define BROWSER_IS_TABBED_WINDOW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), BROWSER_TYPE_TABBED_WINDOW))
#define BROWSER_IS_TABBED_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  BROWSER_TYPE_TABBED_WINDOW))
#define BROWSER_TABBED_WINDOW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  BROWSER_TYPE_TABBED_WINDOW, BrowserTabbedWindowClass))

typedef struct _BrowserTabbedWindow        BrowserTabbedWindow;
typedef struct _BrowserTabbedWindowClass   BrowserTabbedWindowClass;

GType browser_tabbed_window_get_type(void);

HWND browser_tabbed_window_new(HWND, WebKitWebContext*,
        const char*, const char*, gint, gint);

WebKitWebContext* browser_tabbed_window_get_web_context(BrowserTabbedWindow*);

/* Create or get the only toolbar widget */
HWND browser_tabbed_window_create_or_get_toolbar(BrowserTabbedWindow *window);

/* Create a layout container. */
HWND browser_tabbed_window_create_layout_container(BrowserTabbedWindow*,
        HWND, const char *klass, const RECT*);

/* Create a pane container for BrowserPane widgets. */
HWND browser_tabbed_window_create_pane_container(BrowserTabbedWindow*,
        HWND, const char *klass, const RECT*);

/* Create a BrowserPane widget in the specific pane container. */
HWND browser_tabbed_window_append_view_pane(BrowserTabbedWindow*,
        HWND, WebKitWebView*, const RECT*);

/* Create a container for BrowserTab widgets */
HWND browser_tabbed_window_create_tab_container(BrowserTabbedWindow*,
        HWND, const RECT*);

/* Append a webview to the tab container. */
HWND browser_tabbed_window_append_view_tab(BrowserTabbedWindow*,
        HWND, WebKitWebView*);

/* Try to close all webViews in the container */
void browser_tabbed_window_clear_container(BrowserTabbedWindow*, HWND);

/* Try to close the webView in the BrowserPane or BrowserTab */
void browser_tabbed_window_clear_pane_or_tab(BrowserTabbedWindow*, HWND);

/* Load URI in the specific widget
   (or the active BrowserTab if the widget is NULL). */
void browser_tabbed_window_load_uri(BrowserTabbedWindow*, HWND,
        const char *uri);

void browser_tabbed_window_set_background_color(BrowserTabbedWindow*, GAL_Color*);

G_END_DECLS

#endif
