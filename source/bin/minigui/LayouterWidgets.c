/*
** LayouterWidgets.c -- The management of widgets for layouter.
**
** Copyright (C) 2022 FMSoft <http://www.fmsoft.cn>
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

#include "config.h"
#include "main.h"
#include "BrowserPlainWindow.h"
#include "BrowserTabbedWindow.h"
#include "BrowserPane.h"
#include "BuildRevision.h"
#include "PurcmcCallbacks.h"
#include "LayouterWidgets.h"

#include "purcmc/purcmc.h"
#include "layouter/layouter.h"

#include <errno.h>
#include <assert.h>
#include <string.h>
#include <webkit2/webkit2.h>

void gtk_imp_convert_style(struct ws_widget_info *style,
        purc_variant_t toolkit_style)
{
    style->darkMode = false;
    style->fullScreen = false;
    style->withToolbar = false;
    style->backgroundColor = NULL;
    style->flags |= WSWS_FLAG_TOOLKIT;

    if (toolkit_style == PURC_VARIANT_INVALID)
        return;

    purc_variant_t tmp;
    if ((tmp = purc_variant_object_get_by_ckey(toolkit_style, "darkMode")) &&
            purc_variant_is_true(tmp)) {
        style->darkMode = true;
    }

    if ((tmp = purc_variant_object_get_by_ckey(toolkit_style, "fullScreen")) &&
            purc_variant_is_true(tmp)) {
        style->fullScreen = true;
    }

    if ((tmp = purc_variant_object_get_by_ckey(toolkit_style, "withToolbar")) &&
            purc_variant_is_true(tmp)) {
        style->withToolbar = true;
    }

    if ((tmp = purc_variant_object_get_by_ckey(toolkit_style,
                    "backgroundColor"))) {
        const char *value = purc_variant_get_string_const(tmp);
        if (value) {
            style->backgroundColor = value;
        }
    }
}

static BrowserPlainWindow *
create_plainwin(purcmc_workspace *workspace, purcmc_session *sess,
        WebKitWebViewParam *web_view_param, const struct ws_widget_info *style)
{
    BrowserPlainWindow *plainwin;
    plainwin = BROWSER_PLAIN_WINDOW(browser_plain_window_new(NULL,
                sess->web_context, style->name, style->title));

    HWND hwnd = browser_plain_window_get_hwnd(plainwin);
    RECT rect;
    GetWindowRect(hwnd, &rect);

    if (style->flags & WSWS_FLAG_GEOMETRY) {
        LOG_DEBUG("the SIZE of creating plainwin: %d, %d; %u x %u\n",
                style->x, style->y, style->w, style->h);

        unsigned w = RECTW(rect);
        unsigned h = RECTH(rect);
        if (style->w > 0 && style->h > 0) {
            w = style->w;
            h = style->h;
        }

        MoveWindow(hwnd, rect.left, rect.top, w, h, false);
    }

    browser_plain_window_set_view(plainwin, web_view_param);
    WebKitWebView *web_view = browser_plain_window_get_view(plainwin);

#if 0
    if (style->darkMode) {
        g_object_set(gtk_widget_get_settings(GTK_WIDGET(plainwin)),
                "gtk-application-prefer-dark-theme", TRUE, NULL);
    }

    if (style->fullScreen) {
        gtk_window_fullscreen(GTK_WINDOW(plainwin));
    }

    if (style->backgroundColor) {
        GdkRGBA rgba;
        if (gdk_rgba_parse(&rgba, style->backgroundColor)) {
            browser_plain_window_set_background_color(plainwin, &rgba);
        }
    }
    if (editorMode)
        webkit_web_view_set_editable(web_view, TRUE);
#endif

    g_object_set_data(G_OBJECT(web_view), "purcmc-container", plainwin);

    return plainwin;
}

static void post_tabbedwindow_event(purcmc_session *sess, void *window,
        bool create_or_destroy)
{
    purcmc_endpoint *endpoint = purcmc_get_endpoint_by_session(sess);
    /* endpoint might be deleted already. */
    if (endpoint) {
        char buff[64];
        sprintf(buff, "%llx", (unsigned long long)PTR2U64(window));

        pcrdr_msg event = { };
        event.type = PCRDR_MSG_TYPE_EVENT;
        event.target = PCRDR_MSG_TARGET_WORKSPACE;
        event.targetValue = 0;  /* XXX: only one workspace */
        event.eventName =
            purc_variant_make_string_static(create_or_destroy ?
                    "create:tabbedwindow" : "destroy:tabbedwindow", false);
        /* TODO: use real URI for the sourceURI */
        event.sourceURI = purc_variant_make_string_static(PCRDR_APP_RENDERER,
                false);
        event.elementType = PCRDR_MSG_ELEMENT_TYPE_HANDLE;
        event.elementValue = purc_variant_make_string_static(buff, false);
        event.property = PURC_VARIANT_INVALID;
        event.dataType = PCRDR_MSG_DATA_TYPE_VOID;

        purcmc_endpoint_post_event(sess->srv, endpoint, &event);
    }
}

static void
on_destroy_tabbed_window(BrowserTabbedWindow *window, purcmc_session *sess)
{
    void *data;
    if (!sorted_array_find(sess->all_handles, PTR2U64(window), &data)
            || (uintptr_t)data != HT_TABBEDWIN) {
        LOG_ERROR("ODD tabbede window: %p\n", window);
        return;
    }

    post_tabbedwindow_event(sess, window, false);
    sorted_array_remove(sess->all_handles, PTR2U64(window));
}

static void on_destroy_container(HWND container, purcmc_session *sess)
{
    void *data;
    if (!sorted_array_find(sess->all_handles, PTR2U64(container), &data)
            || (uintptr_t)data != HT_CONTAINER) {
        LOG_ERROR("ODD container: %p\n", container);
        return;
    }

    sorted_array_remove(sess->all_handles, PTR2U64(container));
}

static BrowserTabbedWindow *
create_tabbedwin(purcmc_workspace *workspace, purcmc_session *sess,
        void *init_arg, const struct ws_widget_info *style)
{
    BrowserTabbedWindow *window;
    window = BROWSER_TABBED_WINDOW(browser_tabbed_window_new(NULL,
                sess->web_context, style->name, style->title,
                style->w, style->h));

#if 0
    GtkApplication *application;
    application = g_object_get_data(G_OBJECT(sess->webkit_settings),
            KEY_XGUI_APPLICATION);

    gtk_application_add_window(GTK_APPLICATION(application),
            GTK_WINDOW(window));

    if (style->withToolbar) {
        browser_tabbed_window_create_or_get_toolbar(window);
    }

    if (style->darkMode) {
        g_object_set(gtk_widget_get_settings(GTK_WIDGET(window)),
                "gtk-application-prefer-dark-theme", TRUE, NULL);
    }

    if (style->fullScreen) {
        gtk_window_fullscreen(GTK_WINDOW(window));
    }

    if (style->backgroundColor) {
        GdkRGBA rgba;
        if (gdk_rgba_parse(&rgba, style->backgroundColor)) {
            browser_tabbed_window_set_background_color(window, &rgba);
        }
    }
#endif

    sorted_array_add(sess->all_handles, PTR2U64(window), INT2PTR(HT_TABBEDWIN));
#if 0
    g_signal_connect(window, "destroy",
            G_CALLBACK(on_destroy_tabbed_window), sess);
#endif


    post_tabbedwindow_event(sess, window, true);
    return window;
}

static WNDPROC old_layout_container_proc;
static LRESULT layout_container_proc(HWND hWnd, UINT message, WPARAM wParam,
        LPARAM lParam)
{
    if (message == MSG_DESTROY) {
        purcmc_session *sess = (purcmc_session *)
            GetWindowAdditionalData(hWnd);
        if (sess) {
            on_destroy_container(hWnd, sess);
        }
    }
    return (*old_layout_container_proc) (hWnd, message, wParam, lParam);
}

static HWND
create_layout_container(purcmc_workspace *workspace, purcmc_session *sess,
        BrowserTabbedWindow *window, HWND container,
        const struct ws_widget_info *style)
{
    RECT geometry = {style->x, style->y, style->x + style->w, style->y + style->h};

    HWND widget = browser_tabbed_window_create_layout_container(window,
            container, style->klass, &geometry);
    if (widget) {
        sorted_array_add(sess->all_handles, PTR2U64(widget),
                INT2PTR(HT_CONTAINER));
        SetWindowAdditionalData(widget, (DWORD)sess);
        old_layout_container_proc = SetWindowCallbackProc(widget, layout_container_proc);
    }

    return widget;
}

static HWND
create_pane_container(purcmc_workspace *workspace, purcmc_session *sess,
        BrowserTabbedWindow *window, HWND container,
        const struct ws_widget_info *style)
{
    RECT geometry = {style->x, style->y, style->x + style->w, style->y + style->h};

    HWND widget = browser_tabbed_window_create_pane_container(window,
            container, style->klass, &geometry);
    if (widget) {

        sorted_array_add(sess->all_handles, PTR2U64(widget),
                INT2PTR(HT_CONTAINER));
#if 0
        g_signal_connect(widget, "destroy",
                G_CALLBACK(on_destroy_container), sess);
#endif
    }

    return widget;
}

static HWND
create_tab_container(purcmc_workspace *workspace, purcmc_session *sess,
        BrowserTabbedWindow *window, HWND container,
        const struct ws_widget_info *style)
{
    RECT geometry = {style->x, style->y, style->x + style->w, style->y + style->h};

    HWND widget = browser_tabbed_window_create_tab_container(window,
            container, &geometry);
    if (widget) {
        sorted_array_add(sess->all_handles, PTR2U64(widget),
                INT2PTR(HT_CONTAINER));
#if 0 // TODO
        g_signal_connect(widget, "destroy",
                G_CALLBACK(on_destroy_container), sess);
#endif
    }

    return widget;
}

static BrowserPane *
create_pane(purcmc_workspace *workspace, purcmc_session *sess,
        BrowserTabbedWindow *window, HWND container,
        WebKitWebViewParam *web_view_param, const struct ws_widget_info *style)
{
    RECT geometry = {style->x, style->y, style->x + style->w, style->y + style->h};

    BrowserPane *pane = browser_tabbed_window_append_view_pane(window,
            container, web_view_param, &geometry);
    if (pane) {
        WebKitWebView *web_view = browser_pane_get_web_view(pane);
        g_object_set_data(G_OBJECT(web_view), "purcmc-container", pane);
    }

    return pane;
}

static BrowserTab *
create_tab(purcmc_workspace *workspace, purcmc_session *sess,
        BrowserTabbedWindow *window, HWND container,
        WebKitWebViewParam *web_view_param, const struct ws_widget_info *style)
{
    BrowserTab *tab = browser_tabbed_window_append_view_tab(window,
            container, web_view_param);
    if (tab) {
        WebKitWebView *web_view = browser_tab_get_web_view(tab);
        g_object_set_data(G_OBJECT(web_view), "purcmc-container", tab);
    }

    return tab;
}

void *
gtk_imp_create_widget(void *workspace, void *session, ws_widget_type_t type,
        void *window, void *container, void *init_arg,
        const struct ws_widget_info *style)
{
    switch (type) {
    case WS_WIDGET_TYPE_PLAINWINDOW:
        return create_plainwin(workspace, session, init_arg, style);

    case WS_WIDGET_TYPE_TABBEDWINDOW:
        return create_tabbedwin(workspace, session, init_arg, style);

    case WS_WIDGET_TYPE_CONTAINER:
        return create_layout_container(workspace, session,
                window, container, style);

    case WS_WIDGET_TYPE_PANEHOST:
        return create_pane_container(workspace, session,
                window, container, style);

    case WS_WIDGET_TYPE_TABHOST:
        return create_tab_container(workspace, session,
                window, container, style);

    case WS_WIDGET_TYPE_PANEDPAGE:
        return create_pane(workspace, session, window,
                container, init_arg, style);

    case WS_WIDGET_TYPE_TABBEDPAGE:
        return create_tab(workspace, session, window,
                container, init_arg, style);

    default:
        break;
    }

    return NULL;
}

static int
destroy_plainwin(purcmc_workspace *workspace, purcmc_session *sess,
        HWND plain_win)
{
    void *data;
    if (!sorted_array_find(sess->all_handles, PTR2U64(plain_win), &data)) {
        return PCRDR_SC_NOT_FOUND;
    }

    if ((uintptr_t)data != HT_PLAINWIN) {
        return PCRDR_SC_BAD_REQUEST;
    }

    BrowserPlainWindow *gtkwin = BROWSER_PLAIN_WINDOW(plain_win);
    webkit_web_view_try_close(browser_plain_window_get_view(gtkwin));
    return PCRDR_SC_OK;
}

static int
destroy_container_in_tabbedwin(purcmc_workspace *workspace,
        purcmc_session *sess, BrowserTabbedWindow *window, HWND container)
{
    void *data;
    if (!sorted_array_find(sess->all_handles, PTR2U64(window), &data)) {
        LOG_INFO("The tabbed window (%p) has been destroyed.\n", window);
        return PCRDR_SC_OK;
    }
    assert((uintptr_t)data == HT_TABBEDWIN);

    if (container != NULL && (uintptr_t)container != (uintptr_t)window) {
        if (!sorted_array_find(sess->all_handles,
                    PTR2U64(container), &data)) {
            LOG_INFO("The container (%p) has been destroyed.\n", container);
            return PCRDR_SC_OK;
        }
        assert((uintptr_t)data == HT_CONTAINER);
    }

    browser_tabbed_window_clear_container(window, container);
    return PCRDR_SC_OK;
}

static int
destroy_pane_or_tab_in_tabbedwin(purcmc_workspace *workspace,
        purcmc_session *sess, BrowserTabbedWindow *window, HWND pane_or_tab)
{
    void *data;
    if (!sorted_array_find(sess->all_handles, PTR2U64(window), &data)) {
        LOG_INFO("The tabbed window (%p) has been destroyed.\n", window);
        return PCRDR_SC_OK;
    }
    assert((uintptr_t)data == HT_TABBEDWIN);

    if (!sorted_array_find(sess->all_handles, PTR2U64(pane_or_tab), &data)) {
        LOG_INFO("The pane or tab (%p) has been destroyed.\n", pane_or_tab);
        return PCRDR_SC_OK;
    }
    assert((uintptr_t)data == HT_CONTAINER);

    browser_tabbed_window_clear_pane_or_tab(window, pane_or_tab);
    return PCRDR_SC_OK;
}

int
gtk_imp_destroy_widget(void *workspace, void *session, void *window, void *widget,
        ws_widget_type_t type)
{
    switch (type) {
    case WS_WIDGET_TYPE_PLAINWINDOW:
        return destroy_plainwin(workspace, session, widget);

    case WS_WIDGET_TYPE_TABBEDWINDOW:
    case WS_WIDGET_TYPE_CONTAINER:
    case WS_WIDGET_TYPE_PANEHOST:
    case WS_WIDGET_TYPE_TABHOST:
        return destroy_container_in_tabbedwin(workspace, session,
                window, widget);

    case WS_WIDGET_TYPE_PANEDPAGE:
    case WS_WIDGET_TYPE_TABBEDPAGE:
        return destroy_pane_or_tab_in_tabbedwin(workspace, session,
                window, widget);

    default:
        break;
    }

    return PCRDR_SC_OK;
}

void
gtk_imp_update_widget(void *workspace, void *session, void *widget,
        ws_widget_type_t type, const struct ws_widget_info *style)
{
}

