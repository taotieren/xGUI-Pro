/*
 * Copyright (C) 2006, 2007 Apple Inc.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2011 Igalia S.L.
 * Copyright (C) 2022 FMSoft <https://www.fmsoft.cn>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "main.h"
#include "BrowserWindow.h"
#include "BuildRevision.h"
#include "PurcmcCallbacks.h"
#include "HVMLURISchema.h"

#include "purcmc/purcmc.h"

#include <errno.h>
#include <gtk/gtk.h>
#include <string.h>
#include <webkit2/webkit2.h>

#define APP_NAME        "cn.fmsoft.hvml.xGUIPro"
#define RUNNER_NAME     "purcmc"

static purcmc_server_config pcmc_srvcfg;
static purcmc_server *pcmc_srv;

static const gchar **uriArguments = NULL;
static const gchar **ignoreHosts = NULL;
#if WEBKIT_CHECK_VERSION(2, 30, 0)
static WebKitAutoplayPolicy autoplayPolicy = WEBKIT_AUTOPLAY_ALLOW_WITHOUT_SOUND;
#endif
static GdkRGBA *backgroundColor;
static gboolean editorMode;
static const char *sessionFile;
static char *geometry;
static gboolean privateMode;
static gboolean automationMode;
static gboolean fullScreen;
static gboolean ignoreTLSErrors;
static const char *contentFilter;
static const char *cookiesFile;
static const char *cookiesPolicy;
#if WEBKIT_CHECK_VERSION(2, 32, 0)
static const char *proxy;
#endif
static gboolean darkMode;
#if WEBKIT_CHECK_VERSION(2, 30, 0)
static gboolean enableITP;
#endif
static gboolean enableSandbox;
static gboolean exitAfterLoad;
static gboolean webProcessCrashed;
static gboolean printVersion;

static gchar *argumentToURL(const char *filename)
{
    if (g_str_equal(filename, "about:gpu"))
        filename = "webkit://gpu";

    GFile *gfile = g_file_new_for_commandline_arg(filename);
    gchar *fileURL = g_file_get_uri(gfile);
    g_object_unref(gfile);

    return fileURL;
}

static WebKitWebView *createBrowserTab(BrowserWindow *window
        ,WebKitSettings *webkitSettings
        ,WebKitUserContentManager *userContentManager
#if WEBKIT_CHECK_VERSION(2, 30, 0)
        ,WebKitWebsitePolicies *defaultWebsitePolicies
#endif
        )
{
    WebKitWebView *webView = WEBKIT_WEB_VIEW(g_object_new(WEBKIT_TYPE_WEB_VIEW,
        "web-context", browser_window_get_web_context(window),
        "settings", webkitSettings,
        "user-content-manager", userContentManager,
        "is-controlled-by-automation", automationMode,
#if WEBKIT_CHECK_VERSION(2, 30, 0)
        "website-policies", defaultWebsitePolicies,
#endif
        NULL));

    if (editorMode)
        webkit_web_view_set_editable(webView, TRUE);

    browser_window_append_view(window, webView);
    return webView;
}

#if WEBKIT_CHECK_VERSION(2, 30, 0)
static gboolean parseAutoplayPolicy(const char *optionName, const char *value, gpointer data, GError **error)
{
    if (!g_strcmp0(value, "allow")) {
        autoplayPolicy = WEBKIT_AUTOPLAY_ALLOW;
        return TRUE;
    }

    if (!g_strcmp0(value, "allow-without-sound")) {
        autoplayPolicy = WEBKIT_AUTOPLAY_ALLOW_WITHOUT_SOUND;
        return TRUE;
    }

    if (!g_strcmp0(value, "deny")) {
        autoplayPolicy = WEBKIT_AUTOPLAY_DENY;
        return TRUE;
    }

    g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_FAILED, "Failed to parse '%s' as an autoplay policy, valid options are allow, allow-without-sound, and deny", value);
    return FALSE;
}
#endif /* WebKit >= 2.30.0 */

static gboolean parseBackgroundColor(const char *optionName, const char *value, gpointer data, GError **error)
{
    GdkRGBA rgba;
    if (gdk_rgba_parse(&rgba, value)) {
        backgroundColor = gdk_rgba_copy(&rgba);
        return TRUE;
    }

    g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_FAILED, "Failed to parse '%s' as RGBA color", value);
    return FALSE;
}

static const GOptionEntry commandLineOptions[] =
{
    { "pcmc-nowebsocket", 0, 0, G_OPTION_ARG_NONE, &pcmc_srvcfg.nowebsocket, "Without support for WebSocket", NULL },
    { "pcmc-accesslog", 0, 0, G_OPTION_ARG_NONE, &pcmc_srvcfg.accesslog, "Logging the verbose socket access information", NULL },
    { "pcmc-unixsocket", 0, 0, G_OPTION_ARG_STRING, &pcmc_srvcfg.unixsocket, "The path of the Unix-domain socket to listen on", "PATH" },
    { "pcmc-addr", 0, 0, G_OPTION_ARG_STRING, &pcmc_srvcfg.addr, "The IPv4 address to bind to for WebSocket", NULL },
    { "pcmc-port", 0, 0, G_OPTION_ARG_STRING, &pcmc_srvcfg.port, "The port to bind to for WebSocket", NULL },
    { "pcmc-origin", 0, 0, G_OPTION_ARG_STRING, &pcmc_srvcfg.origin, "The origin to ensure clients send the specified origin header upon the WebSocket handshake", "FQDN" },
#if HAVE(LIBSSL)
    { "pcmc-sslcert", 0, 0, G_OPTION_ARG_STRING, &pcmc_srvcfg.sslcert, "The path to SSL certificate", "FILE" },
    { "pcmc-sslkey", 0, 0, G_OPTION_ARG_STRING, &pcmc_srvcfg.sslkey, "The path to SSL private key", "FILE" },
#endif
    { "pcmc-maxfrmsize", 0, 0, G_OPTION_ARG_INT, &pcmc_srvcfg.max_frm_size, "The maximum size of a socket frame", "BYTES" },
    { "pcmc-backlog", 0, 0, G_OPTION_ARG_INT, &pcmc_srvcfg.backlog, "The maximum length to which the queue of pending connections.", "NUMBER" },

#if WEBKIT_CHECK_VERSION(2, 30, 0)
    { "autoplay-policy", 0, 0, G_OPTION_ARG_CALLBACK, parseAutoplayPolicy, "Autoplay policy. Valid options are: allow, allow-without-sound, and deny", NULL },
#endif
    { "bg-color", 0, 0, G_OPTION_ARG_CALLBACK, parseBackgroundColor, "Background color", NULL },
    { "editor-mode", 'e', 0, G_OPTION_ARG_NONE, &editorMode, "Run in editor mode", NULL },
    { "dark-mode", 'd', 0, G_OPTION_ARG_NONE, &darkMode, "Run in dark mode", NULL },
    { "session-file", 's', 0, G_OPTION_ARG_FILENAME, &sessionFile, "Session file", "FILE" },
    { "geometry", 'g', 0, G_OPTION_ARG_STRING, &geometry, "Unused. Kept for backwards-compatibility only", "GEOMETRY" },
    { "full-screen", 'f', 0, G_OPTION_ARG_NONE, &fullScreen, "Set the window to full-screen mode", NULL },
    { "private", 'p', 0, G_OPTION_ARG_NONE, &privateMode, "Run in private browsing mode", NULL },
    { "automation", 0, 0, G_OPTION_ARG_NONE, &automationMode, "Run in automation mode", NULL },
    { "cookies-file", 'c', 0, G_OPTION_ARG_FILENAME, &cookiesFile, "Persistent cookie storage database file", "FILE" },
    { "cookies-policy", 0, 0, G_OPTION_ARG_STRING, &cookiesPolicy, "Cookies accept policy (always, never, no-third-party). Default: no-third-party", "POLICY" },
#if WEBKIT_CHECK_VERSION(2, 32, 0)
    { "proxy", 0, 0, G_OPTION_ARG_STRING, &proxy, "Set proxy", "PROXY" },
#endif
    { "ignore-host", 0, 0, G_OPTION_ARG_STRING_ARRAY, &ignoreHosts, "Set proxy ignore hosts", "HOSTS" },
    { "ignore-tls-errors", 0, 0, G_OPTION_ARG_NONE, &ignoreTLSErrors, "Ignore TLS errors", NULL },
    { "content-filter", 0, 0, G_OPTION_ARG_FILENAME, &contentFilter, "JSON with content filtering rules", "FILE" },
#if WEBKIT_CHECK_VERSION(2, 30, 0)
    { "enable-itp", 0, 0, G_OPTION_ARG_NONE, &enableITP, "Enable Intelligent Tracking Prevention (ITP)", NULL },
#endif
    { "enable-sandbox", 0, 0, G_OPTION_ARG_NONE, &enableSandbox, "Enable web process sandbox support", NULL },
    { "exit-after-load", 0, 0, G_OPTION_ARG_NONE, &exitAfterLoad, "Quit the browser after the load finishes", NULL },
    { "version", 'v', 0, G_OPTION_ARG_NONE, &printVersion, "Print the WebKitGTK version", NULL },
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &uriArguments, 0, "[URL…]" },
    { 0, 0, 0, 0, 0, 0, 0 }
};

static gboolean parseOptionEntryCallback(const gchar *optionNameFull, const gchar *value, WebKitSettings *webSettings, GError **error)
{
    if (strlen(optionNameFull) <= 2) {
        g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_FAILED, "Invalid option %s", optionNameFull);
        return FALSE;
    }

    /* We have two -- in option name so remove them. */
    const gchar *optionName = optionNameFull + 2;
    GParamSpec *spec = g_object_class_find_property(G_OBJECT_GET_CLASS(webSettings), optionName);
    if (!spec) {
        g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_FAILED, "Cannot find web settings for option %s", optionNameFull);
        return FALSE;
    }

    switch (G_PARAM_SPEC_VALUE_TYPE(spec)) {
    case G_TYPE_BOOLEAN: {
        gboolean propertyValue = !(value && g_ascii_strcasecmp(value, "true") && strcmp(value, "1"));
        g_object_set(G_OBJECT(webSettings), optionName, propertyValue, NULL);
        break;
    }
    case G_TYPE_STRING:
        g_object_set(G_OBJECT(webSettings), optionName, value, NULL);
        break;
    case G_TYPE_INT: {
        glong propertyValue;
        gchar *end;

        errno = 0;
        propertyValue = g_ascii_strtoll(value, &end, 0);
        if (errno == ERANGE || propertyValue > G_MAXINT || propertyValue < G_MININT) {
            g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE, "Integer value '%s' for %s out of range", value, optionNameFull);
            return FALSE;
        }
        if (errno || value == end) {
            g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE, "Cannot parse integer value '%s' for %s", value, optionNameFull);
            return FALSE;
        }
        g_object_set(G_OBJECT(webSettings), optionName, propertyValue, NULL);
        break;
    }
    case G_TYPE_FLOAT: {
        gdouble propertyValue;
        gchar *end;

        errno = 0;
        propertyValue = g_ascii_strtod(value, &end);
        if (errno == ERANGE || propertyValue > G_MAXFLOAT || propertyValue < G_MINFLOAT) {
            g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE, "Float value '%s' for %s out of range", value, optionNameFull);
            return FALSE;
        }
        if (errno || value == end) {
            g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE, "Cannot parse float value '%s' for %s", value, optionNameFull);
            return FALSE;
        }
        g_object_set(G_OBJECT(webSettings), optionName, propertyValue, NULL);
        break;
    }
    default:
        g_assert_not_reached();
    }

    return TRUE;
}

static gboolean isValidParameterType(GType gParamType)
{
    return (gParamType == G_TYPE_BOOLEAN || gParamType == G_TYPE_STRING || gParamType == G_TYPE_INT
            || gParamType == G_TYPE_FLOAT);
}

static GOptionEntry* getOptionEntriesFromWebKitSettings(WebKitSettings *webSettings)
{
    GParamSpec **propertySpecs;
    GOptionEntry *optionEntries;
    guint numProperties, numEntries, i;

    propertySpecs = g_object_class_list_properties(G_OBJECT_GET_CLASS(webSettings), &numProperties);
    if (!propertySpecs)
        return NULL;

    optionEntries = g_new0(GOptionEntry, numProperties + 1);
    numEntries = 0;
    for (i = 0; i < numProperties; i++) {
        GParamSpec *param = propertySpecs[i];

        /* Fill in structures only for writable and not construct-only properties. */
        if (!param || !(param->flags & G_PARAM_WRITABLE) || (param->flags & G_PARAM_CONSTRUCT_ONLY))
            continue;

        GType gParamType = G_PARAM_SPEC_VALUE_TYPE(param);
        if (!isValidParameterType(gParamType))
            continue;

        GOptionEntry *optionEntry = &optionEntries[numEntries++];
        optionEntry->long_name = g_param_spec_get_name(param);

        /* There is no easy way to figure our short name for generated option entries.
           optionEntry.short_name=*/
        /* For bool arguments "enable" type make option argument not required. */
        if (gParamType == G_TYPE_BOOLEAN && (strstr(optionEntry->long_name, "enable")))
            optionEntry->flags = G_OPTION_FLAG_OPTIONAL_ARG;
        optionEntry->arg = G_OPTION_ARG_CALLBACK;
        optionEntry->arg_data = parseOptionEntryCallback;
        optionEntry->description = g_param_spec_get_blurb(param);
        optionEntry->arg_description = g_type_name(gParamType);
    }
    g_free(propertySpecs);

    return optionEntries;
}

static gboolean addSettingsGroupToContext(GOptionContext *context, WebKitSettings* webkitSettings)
{
    GOptionEntry *optionEntries = getOptionEntriesFromWebKitSettings(webkitSettings);
    if (!optionEntries)
        return FALSE;

    GOptionGroup *webSettingsGroup = g_option_group_new("websettings",
                                                        "WebKitSettings writable properties for default WebKitWebView",
                                                        "WebKitSettings properties",
                                                        webkitSettings,
                                                        NULL);
    g_option_group_add_entries(webSettingsGroup, optionEntries);
    g_free(optionEntries);

    /* Option context takes ownership of the group. */
    g_option_context_add_group(context, webSettingsGroup);

    return TRUE;
}

typedef struct {
    WebKitURISchemeRequest *request;
    GList *dataList;
    GHashTable *dataMap;
} AboutDataRequest;

static GHashTable *aboutDataRequestMap;

static void aboutDataRequestFree(AboutDataRequest *request)
{
    if (!request)
        return;

    g_list_free_full(request->dataList, g_object_unref);

    if (request->request)
        g_object_unref(request->request);
    if (request->dataMap)
        g_hash_table_destroy(request->dataMap);

    g_free(request);
}

static AboutDataRequest* aboutDataRequestNew(WebKitURISchemeRequest *uriRequest)
{
    if (!aboutDataRequestMap)
        aboutDataRequestMap = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, (GDestroyNotify)aboutDataRequestFree);

    AboutDataRequest *request = g_new0(AboutDataRequest, 1);
    request->request = g_object_ref(uriRequest);
    g_hash_table_insert(aboutDataRequestMap, GUINT_TO_POINTER(webkit_web_view_get_page_id(webkit_uri_scheme_request_get_web_view(request->request))), request);

    return request;
}

static AboutDataRequest *aboutDataRequestForView(guint64 pageID)
{
    return aboutDataRequestMap ? g_hash_table_lookup(aboutDataRequestMap, GUINT_TO_POINTER(pageID)) : NULL;
}

static void websiteDataRemovedCallback(WebKitWebsiteDataManager *manager, GAsyncResult *result, AboutDataRequest *dataRequest)
{
    if (webkit_website_data_manager_remove_finish(manager, result, NULL))
        webkit_web_view_reload(webkit_uri_scheme_request_get_web_view(dataRequest->request));
}

static void websiteDataClearedCallback(WebKitWebsiteDataManager *manager, GAsyncResult *result, AboutDataRequest *dataRequest)
{
    if (webkit_website_data_manager_clear_finish(manager, result, NULL))
        webkit_web_view_reload(webkit_uri_scheme_request_get_web_view(dataRequest->request));
}

static void aboutDataScriptMessageReceivedCallback(WebKitUserContentManager *userContentManager, WebKitJavascriptResult *message, WebKitWebContext *webContext)
{
    char *messageString = jsc_value_to_string(webkit_javascript_result_get_js_value(message));
    char **tokens = g_strsplit(messageString, ":", 3);
    g_free(messageString);

    unsigned tokenCount = g_strv_length(tokens);
    if (!tokens || tokenCount < 2) {
        g_strfreev(tokens);
        return;
    }

    guint64 pageID = g_ascii_strtoull(tokens[0], NULL, 10);
    AboutDataRequest *dataRequest = aboutDataRequestForView(pageID);
    if (!dataRequest) {
        g_strfreev(tokens);
        return;
    }

    WebKitWebsiteDataManager *manager = webkit_web_context_get_website_data_manager(webContext);
    guint64 types = g_ascii_strtoull(tokens[1], NULL, 10);
    if (tokenCount == 2)
        webkit_website_data_manager_clear(manager, types, 0, NULL, (GAsyncReadyCallback)websiteDataClearedCallback, dataRequest);
    else {
        guint64 domainID = g_ascii_strtoull(tokens[2], NULL, 10);
        GList *dataList = g_hash_table_lookup(dataRequest->dataMap, GUINT_TO_POINTER(types));
        WebKitWebsiteData *data = g_list_nth_data(dataList, domainID);
        if (data) {
            GList dataList = { data, NULL, NULL };
            webkit_website_data_manager_remove(manager, types, &dataList, NULL, (GAsyncReadyCallback)websiteDataRemovedCallback, dataRequest);
        }
    }
    g_strfreev(tokens);
}

static void domainListFree(GList *domains)
{
    g_list_free_full(domains, (GDestroyNotify)webkit_website_data_unref);
}

static void aboutDataFillTable(GString *result, AboutDataRequest *dataRequest, GList* dataList, const char *title, WebKitWebsiteDataTypes types, const char *dataPath, guint64 pageID)
{
    guint64 totalDataSize = 0;
    GList *domains = NULL;
    GList *l;
    for (l = dataList; l; l = g_list_next(l)) {
        WebKitWebsiteData *data = (WebKitWebsiteData *)l->data;

        if (webkit_website_data_get_types(data) & types) {
            domains = g_list_prepend(domains, webkit_website_data_ref(data));
            totalDataSize += webkit_website_data_get_size(data, types);
        }
    }
    if (!domains)
        return;

    if (!dataRequest->dataMap)
        dataRequest->dataMap = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, (GDestroyNotify)domainListFree);
    g_hash_table_insert(dataRequest->dataMap, GUINT_TO_POINTER(types), domains);

    if (totalDataSize) {
        char *totalDataSizeStr = g_format_size(totalDataSize);
        g_string_append_printf(result, "<h1>%s (%s)</h1>\n<table>\n", title, totalDataSizeStr);
        g_free(totalDataSizeStr);
    } else
        g_string_append_printf(result, "<h1>%s</h1>\n<table>\n", title);
    if (dataPath)
        g_string_append_printf(result, "<tr><td colspan=\"2\">Path: %s</td></tr>\n", dataPath);

    unsigned index;
    for (l = domains, index = 0; l; l = g_list_next(l), index++) {
        WebKitWebsiteData *data = (WebKitWebsiteData *)l->data;
        const char *displayName = webkit_website_data_get_name(data);
        guint64 dataSize = webkit_website_data_get_size(data, types);
        if (dataSize) {
            char *dataSizeStr = g_format_size(dataSize);
            g_string_append_printf(result, "<tr><td>%s (%s)</td>", displayName, dataSizeStr);
            g_free(dataSizeStr);
        } else
            g_string_append_printf(result, "<tr><td>%s</td>", displayName);
        g_string_append_printf(result, "<td><input type=\"button\" value=\"Remove\" onclick=\"removeData('%"G_GUINT64_FORMAT":%u:%u');\"></td></tr>\n",
            pageID, types, index);
    }
    g_string_append_printf(result, "<tr><td><input type=\"button\" value=\"Clear all\" onclick=\"clearData('%"G_GUINT64_FORMAT":%u');\"></td></tr></table>\n",
        pageID, types);
}

static void gotWebsiteDataCallback(WebKitWebsiteDataManager *manager, GAsyncResult *asyncResult, AboutDataRequest *dataRequest)
{
    GList *dataList = webkit_website_data_manager_fetch_finish(manager, asyncResult, NULL);

    GString *result = g_string_new(
        "<html><head>"
        "<script>"
        "  function removeData(domain) {"
        "    window.webkit.messageHandlers.aboutData.postMessage(domain);"
        "  }"
        "  function clearData(dataType) {"
        "    window.webkit.messageHandlers.aboutData.postMessage(dataType);"
        "  }"
        "</script></head><body>\n");

    guint64 pageID = webkit_web_view_get_page_id(webkit_uri_scheme_request_get_web_view(dataRequest->request));
    aboutDataFillTable(result, dataRequest, dataList, "Cookies", WEBKIT_WEBSITE_DATA_COOKIES, NULL, pageID);
    aboutDataFillTable(result, dataRequest, dataList, "Device Id Hash Salt", WEBKIT_WEBSITE_DATA_DEVICE_ID_HASH_SALT, NULL, pageID);
    aboutDataFillTable(result, dataRequest, dataList, "Memory Cache", WEBKIT_WEBSITE_DATA_MEMORY_CACHE, NULL, pageID);
    aboutDataFillTable(result, dataRequest, dataList, "Disk Cache", WEBKIT_WEBSITE_DATA_DISK_CACHE, webkit_website_data_manager_get_disk_cache_directory(manager), pageID);
    aboutDataFillTable(result, dataRequest, dataList, "Session Storage", WEBKIT_WEBSITE_DATA_SESSION_STORAGE, NULL, pageID);
    aboutDataFillTable(result, dataRequest, dataList, "Local Storage", WEBKIT_WEBSITE_DATA_LOCAL_STORAGE, webkit_website_data_manager_get_local_storage_directory(manager), pageID);
    aboutDataFillTable(result, dataRequest, dataList, "IndexedDB Databases", WEBKIT_WEBSITE_DATA_INDEXEDDB_DATABASES, webkit_website_data_manager_get_indexeddb_directory(manager), pageID);
    aboutDataFillTable(result, dataRequest, dataList, "Plugins Data", WEBKIT_WEBSITE_DATA_PLUGIN_DATA, NULL, pageID);
    aboutDataFillTable(result, dataRequest, dataList, "Offline Web Applications Cache", WEBKIT_WEBSITE_DATA_OFFLINE_APPLICATION_CACHE, webkit_website_data_manager_get_offline_application_cache_directory(manager), pageID);
    aboutDataFillTable(result, dataRequest, dataList, "HSTS Cache", WEBKIT_WEBSITE_DATA_HSTS_CACHE, webkit_website_data_manager_get_hsts_cache_directory(manager), pageID);
#if WEBKIT_CHECK_VERSION(2, 30, 0)
    aboutDataFillTable(result, dataRequest, dataList, "ITP data", WEBKIT_WEBSITE_DATA_ITP, webkit_website_data_manager_get_itp_directory(manager), pageID);
    aboutDataFillTable(result, dataRequest, dataList, "Service Worker Registratations", WEBKIT_WEBSITE_DATA_SERVICE_WORKER_REGISTRATIONS,
        webkit_website_data_manager_get_service_worker_registrations_directory(manager), pageID);
    aboutDataFillTable(result, dataRequest, dataList, "DOM Cache", WEBKIT_WEBSITE_DATA_DOM_CACHE, webkit_website_data_manager_get_dom_cache_directory(manager), pageID);
#endif

    result = g_string_append(result, "</body></html>");
    gsize streamLength = result->len;
    GInputStream *stream = g_memory_input_stream_new_from_data(g_string_free(result, FALSE), streamLength, g_free);
    webkit_uri_scheme_request_finish(dataRequest->request, stream, streamLength, "text/html");
    g_list_free_full(dataList, (GDestroyNotify)webkit_website_data_unref);
}

static void aboutDataHandleRequest(WebKitURISchemeRequest *request, WebKitWebContext *webContext)
{
    AboutDataRequest *dataRequest = aboutDataRequestNew(request);
    WebKitWebsiteDataManager *manager = webkit_web_context_get_website_data_manager(webContext);
    webkit_website_data_manager_fetch(manager, WEBKIT_WEBSITE_DATA_ALL, NULL, (GAsyncReadyCallback)gotWebsiteDataCallback, dataRequest);
}

#if WEBKIT_CHECK_VERSION(2, 30, 0)
typedef struct {
    WebKitURISchemeRequest *request;
    GList *thirdPartyList;
} AboutITPRequest;

static AboutITPRequest *aboutITPRequestNew(WebKitURISchemeRequest *request)
{
    AboutITPRequest *itpRequest = g_new0(AboutITPRequest, 1);
    itpRequest->request = g_object_ref(request);
    return itpRequest;
}

static void aboutITPRequestFree(AboutITPRequest *itpRequest)
{
    g_clear_object(&itpRequest->request);
    g_list_free_full(itpRequest->thirdPartyList, (GDestroyNotify)webkit_itp_third_party_unref);
    g_free(itpRequest);
}

static void gotITPSummaryCallback(WebKitWebsiteDataManager *manager, GAsyncResult *asyncResult, AboutITPRequest *itpRequest)
{
    GList *thirdPartyList = webkit_website_data_manager_get_itp_summary_finish(manager, asyncResult, NULL);
    GString *result = g_string_new("<html><body>\n<h1>Trackers</h1>\n");
    GList *l;
    for (l = thirdPartyList; l && l->data; l = g_list_next(l)) {
        WebKitITPThirdParty *thirdParty = (WebKitITPThirdParty *)l->data;
        result = g_string_append(result, "<details>\n");
        g_string_append_printf(result, "<summary>%s</summary>\n", webkit_itp_third_party_get_domain(thirdParty));
        result = g_string_append(result, "<table border='1'><tr><th>First Party</th><th>Website data access granted</th><th>Last updated</th></tr>\n");
        GList *firstPartyList = webkit_itp_third_party_get_first_parties(thirdParty);
        GList *ll;
        for (ll = firstPartyList; ll && ll->data; ll = g_list_next(ll)) {
            WebKitITPFirstParty *firstParty = (WebKitITPFirstParty *)ll->data;
            char *updatedTime = g_date_time_format(webkit_itp_first_party_get_last_update_time(firstParty), "%Y-%m-%d %H:%M:%S");
            g_string_append_printf(result, "<tr><td>%s</td><td>%s</td><td>%s</td></tr>\n", webkit_itp_first_party_get_domain(firstParty),
                webkit_itp_first_party_get_website_data_access_allowed(firstParty) ? "yes" : "no", updatedTime);
            g_free(updatedTime);
        }
        result = g_string_append(result, "</table></details>\n");
    }

    result = g_string_append(result, "</body></html>");
    gsize streamLength = result->len;
    GInputStream *stream = g_memory_input_stream_new_from_data(g_string_free(result, FALSE), streamLength, g_free);
    webkit_uri_scheme_request_finish(itpRequest->request, stream, streamLength, "text/html");
    aboutITPRequestFree(itpRequest);
}

static void aboutITPHandleRequest(WebKitURISchemeRequest *request, WebKitWebContext *webContext)
{
    AboutITPRequest *itpRequest = aboutITPRequestNew(request);
    WebKitWebsiteDataManager *manager = webkit_web_context_get_website_data_manager(webContext);
    webkit_website_data_manager_get_itp_summary(manager, NULL, (GAsyncReadyCallback)gotITPSummaryCallback, itpRequest);
}
#endif /* WebKit >= 2.30.0 */

#define HTML_REDIRECT_TO_ABOUT                                  \
    "<head>"                                                    \
    "  <meta http-equiv=\"Refresh\" content=\"0; URL="          \
    "hvml://localhost/_renderer/_builtin/-/assets/about.html"   \
    "\">"                                                       \
    "</head>"

static void aboutURISchemeRequestCallback(WebKitURISchemeRequest *request,
        WebKitWebContext *webContext)
{
    const gchar *path;

    path = webkit_uri_scheme_request_get_path(request);
    printf("%s: path: %s\n", __func__, path);

    if (!g_strcmp0(path, "xguipro")) {
        GInputStream *stream;
        const gsize streamLength = sizeof(HTML_REDIRECT_TO_ABOUT) - 1;

        stream = g_memory_input_stream_new_from_data(
                HTML_REDIRECT_TO_ABOUT, streamLength, NULL);
        webkit_uri_scheme_request_finish(request,
                stream, streamLength, "text/html");
        g_object_unref(stream);
    }
    else if (!g_strcmp0(path, "data"))
        aboutDataHandleRequest(request, webContext);
#if WEBKIT_CHECK_VERSION(2, 30, 0)
    else if (!g_strcmp0(path, "itp"))
        aboutITPHandleRequest(request, webContext);
#endif
    else {
        GError *error;

        error = g_error_new(XGUI_PRO_ERROR, XGUI_PRO_ERROR_INVALID_ABOUT_PATH,
                "Invalid about:%s page.", path);
        webkit_uri_scheme_request_finish_error(request, error);
        g_error_free(error);
    }
}

static GtkWidget *createWebViewForAutomationInWindowCallback(WebKitAutomationSession* session, GtkApplication *application)
{
    GtkWindow *window = gtk_application_get_active_window(application);
    return BROWSER_IS_WINDOW(window) ? GTK_WIDGET(browser_window_get_or_create_web_view_for_automation(BROWSER_WINDOW(window))) : NULL;
}

static GtkWidget *createWebViewForAutomationInTabCallback(WebKitAutomationSession* session, GtkApplication *application)
{
    GtkWindow *window = gtk_application_get_active_window(application);
    return BROWSER_IS_WINDOW(window) ? GTK_WIDGET(browser_window_create_web_view_in_new_tab_for_automation(BROWSER_WINDOW(window))) : NULL;
}

static void automationStartedCallback(WebKitWebContext *webContext, WebKitAutomationSession *session, GtkApplication *application)
{
    WebKitApplicationInfo *info = webkit_application_info_new();
    webkit_application_info_set_version(info, WEBKIT_MAJOR_VERSION, WEBKIT_MINOR_VERSION, WEBKIT_MICRO_VERSION);
    webkit_automation_session_set_application_info(session, info);
    webkit_application_info_unref(info);

    g_signal_connect(session, "create-web-view::window", G_CALLBACK(createWebViewForAutomationInWindowCallback), application);
    g_signal_connect(session, "create-web-view::tab", G_CALLBACK(createWebViewForAutomationInTabCallback), application);
}

static gboolean quitApplication(GApplication *application)
{
    g_application_quit(application);
    return FALSE;
}

static void exitAfterWebViewLoadFinishesCallback(WebKitWebView *webView, WebKitLoadEvent loadEvent, GApplication *application)
{
    if (loadEvent != WEBKIT_LOAD_FINISHED)
        return;

    g_idle_add_full(G_PRIORITY_DEFAULT_IDLE, (GSourceFunc)quitApplication, g_object_ref(application), g_object_unref);
}

static void exitAfterWebProcessCrashed(WebKitWebView *webView, WebKitWebProcessTerminationReason reason, GApplication *application)
{
    if (reason == WEBKIT_WEB_PROCESS_CRASHED) {
        webProcessCrashed = TRUE;
        exitAfterWebViewLoadFinishesCallback(webView, WEBKIT_LOAD_FINISHED, application);
    }
}

static void exitAfterWebViewLoadFinishes(WebKitWebView *webView, GApplication *application)
{
    g_signal_connect_object(webView, "load-changed", G_CALLBACK(exitAfterWebViewLoadFinishesCallback), application, G_CONNECT_AFTER);
    g_signal_connect_object(webView, "web-process-terminated", G_CALLBACK(exitAfterWebProcessCrashed), application, G_CONNECT_AFTER);
}

typedef struct {
    GMainLoop *mainLoop;
    WebKitUserContentFilter *filter;
    GError *error;
} FilterSaveData;

static void filterSavedCallback(WebKitUserContentFilterStore *store, GAsyncResult *result, FilterSaveData *data)
{
    data->filter = webkit_user_content_filter_store_save_finish(store, result, &data->error);
    g_main_loop_quit(data->mainLoop);
}

static void setDefaultWebsiteDataManager(WebKitSettings *webkitSettings)
{
    WebKitWebsiteDataManager *manager;

    char *dataDirectory = g_build_filename(g_get_user_data_dir(),
            "webkitgtk-" WEBKITGTK_API_VERSION_STRING, "xGUIPro", NULL);
    char *cacheDirectory = g_build_filename(g_get_user_cache_dir(),
            "webkitgtk-" WEBKITGTK_API_VERSION_STRING, "xGUIPro", NULL);
    manager = webkit_website_data_manager_new(
            "base-data-directory", dataDirectory,
            "base-cache-directory", cacheDirectory,
            NULL);
    g_free(dataDirectory);
    g_free(cacheDirectory);

#if WEBKIT_CHECK_VERSION(2, 30, 0)
    webkit_website_data_manager_set_itp_enabled(manager, enableITP);
#endif
#if WEBKIT_CHECK_VERSION(2, 32, 0)
    if (proxy) {
        WebKitNetworkProxySettings *webkitProxySettings
            = webkit_network_proxy_settings_new(proxy, ignoreHosts);
        webkit_website_data_manager_set_network_proxy_settings(manager,
                WEBKIT_NETWORK_PROXY_MODE_CUSTOM, webkitProxySettings);
        webkit_network_proxy_settings_free(webkitProxySettings);
    }

    if (ignoreTLSErrors)
        webkit_website_data_manager_set_tls_errors_policy(manager,
                WEBKIT_TLS_ERRORS_POLICY_IGNORE);
#endif

    g_object_set_data(G_OBJECT(webkitSettings),
            "default-website-data-manager", manager);
    g_object_ref(manager);

}

#if WEBKIT_CHECK_VERSION(2, 30, 0)
static void setDefaultWebsitePolicies(WebKitSettings *webkitSettings)
{
    WebKitWebsitePolicies *policies
        = webkit_website_policies_new_with_policies(
                "autoplay", autoplayPolicy,
                NULL);

    g_object_set_data(G_OBJECT(webkitSettings),
            "default-website-policies", policies);
    g_object_ref(policies);
}
#endif

static void startup(GApplication *application, WebKitSettings *webkitSettings)
{
    const char *actionAccels[] = {
        "win.reload", "F5", "<Ctrl>R", NULL,
        "win.reload-no-cache", "<Ctrl>F5", "<Ctrl><Shift>R", NULL,
        "win.toggle-inspector", "<Ctrl><Shift>I", "F12", NULL,
        "win.open-private-window", "<Ctrl><Shift>P", NULL,
        "win.focus-location", "<Ctrl>L", NULL,
        "win.stop-load", "F6", "Escape", NULL,
        "win.load-homepage", "<Alt>Home", NULL,
        "win.zoom-in", "<Ctrl>plus", "<Ctrl>equal", "<Ctrl>KP_Add", NULL,
        "win.zoom-out", "<Ctrl>minus", "<Ctrl>KP_Subtract", NULL,
        "win.zoom-default", "<Ctrl>0", "<Ctrl>KP_0", NULL,
        "win.find", "<Ctrl>F", NULL,
        "win.new-tab", "<Ctrl>T", NULL,
        "win.toggle-fullscreen", "F11", NULL,
        "win.print", "<Ctrl>P", NULL,
        "win.close", "<Ctrl>W", NULL,
        "win.quit", "<Ctrl>Q", NULL,
        "find.next", "F3", "<Ctrl>G", NULL,
        "find.previous", "<Shift>F3", "<Ctrl><Shift>G", NULL,
        NULL
    };

    for (const gchar **it = actionAccels; it[0]; it += g_strv_length((gchar **)it) + 1)
        gtk_application_set_accels_for_action(GTK_APPLICATION(application), it[0], &it[1]);

    /* use webkitSettings to store some global data */
    g_object_set_data(G_OBJECT(webkitSettings), "gtk-application", application);
    setDefaultWebsiteDataManager(webkitSettings);
#if WEBKIT_CHECK_VERSION(2, 30, 0)
    setDefaultWebsitePolicies(webkitSettings);
#endif

    purcmc_server_callbacks cbs = {
        .prepare = pcmc_gtk_prepare,
        .cleanup = pcmc_gtk_cleanup,

        .create_session = gtk_create_session,
        .remove_session = gtk_remove_session,

        .find_page = gtk_find_page,

        .get_special_plainwin = gtk_get_special_plainwin,
        .create_plainwin = gtk_create_plainwin,
        .update_plainwin = gtk_update_plainwin,
        .destroy_plainwin = gtk_destroy_plainwin,

        .set_page_groups = gtk_set_page_groups,
        .add_page_groups = gtk_add_page_groups,
        .remove_page_group = gtk_remove_page_group,

        .get_special_widget = gtk_get_special_widget,
        .create_widget = gtk_create_widget,
        .update_widget = gtk_update_widget,
        .destroy_widget = gtk_destroy_widget,

        .load = gtk_load_or_write,
        .write = gtk_load_or_write,
        .register_crtn = gtk_register_crtn,
        .revoke_crtn = gtk_revoke_crtn,

        .update_dom = gtk_update_dom,

        .call_method_in_dom = gtk_call_method_in_dom,
        .get_property_in_dom = gtk_get_property_in_dom,
        .set_property_in_dom = gtk_set_property_in_dom,

        .pend_response = gtk_pend_response,
    };

    pcmc_srvcfg.app_name = APP_NAME;
    pcmc_srvcfg.runner_name = RUNNER_NAME;

    pcmc_srv = purcmc_rdrsrv_init(&pcmc_srvcfg, webkitSettings,
            &cbs, "HTML:5.3", 0, -1, -1, -1);
    if (pcmc_srv == NULL) {
        fprintf(stderr, "Failed call to purcmc_rdrsrv_init()\n");
        exit(EXIT_FAILURE);
    }

    GMainContext *context = g_main_context_default();

    GSource *source;
    source = g_timeout_source_new(10);
    g_source_set_callback(source,
            G_SOURCE_FUNC(purcmc_rdrsrv_check), pcmc_srv, NULL);
    g_source_attach(source, context);
    g_source_unref(source);
}

static void shutdown(GApplication *application, WebKitSettings *webkitSettings)
{
    g_source_remove_by_user_data(pcmc_srv);
    purcmc_rdrsrv_deinit(pcmc_srv);

    WebKitWebsiteDataManager *manager;
    manager = g_object_get_data(G_OBJECT(webkitSettings),
            "default-website-data-manager");
    g_object_unref(manager);

#if WEBKIT_CHECK_VERSION(2, 30, 0)
    WebKitWebsitePolicies *policies;
    policies = g_object_get_data(G_OBJECT(webkitSettings),
            "default-website-policies");
    g_object_unref(policies);
#endif

    g_object_unref(webkitSettings);
}

static void activate(GApplication *application, WebKitSettings *webkitSettings)
{
    WebKitWebsiteDataManager *manager;
    if (privateMode || automationMode)
        manager = webkit_website_data_manager_new_ephemeral();
    else
        manager = g_object_get_data(G_OBJECT(webkitSettings),
                "default-website-data-manager");

    WebKitWebContext *webContext = g_object_new(WEBKIT_TYPE_WEB_CONTEXT,
            "website-data-manager", manager,
            "process-swap-on-cross-site-navigation-enabled", TRUE,
#if !GTK_CHECK_VERSION(3, 98, 0) && WEBKIT_CHECK_VERSION(2, 30, 0)
            "use-system-appearance-for-scrollbars", FALSE,
#endif
            NULL);
    g_object_unref(manager);

    g_signal_connect(webContext, "initialize-web-extensions",
            G_CALLBACK(initializeWebExtensionsCallback), NULL);

    if (enableSandbox)
        webkit_web_context_set_sandbox_enabled(webContext, TRUE);

    if (cookiesPolicy) {
        WebKitCookieManager *cookieManager = webkit_web_context_get_cookie_manager(webContext);
        GEnumClass *enumClass = g_type_class_ref(WEBKIT_TYPE_COOKIE_ACCEPT_POLICY);
        GEnumValue *enumValue = g_enum_get_value_by_nick(enumClass, cookiesPolicy);
        if (enumValue)
            webkit_cookie_manager_set_accept_policy(cookieManager, enumValue->value);
        g_type_class_unref(enumClass);
    }

    if (cookiesFile && !webkit_web_context_is_ephemeral(webContext)) {
        WebKitCookieManager *cookieManager = webkit_web_context_get_cookie_manager(webContext);
        WebKitCookiePersistentStorage storageType = g_str_has_suffix(cookiesFile, ".txt") ? WEBKIT_COOKIE_PERSISTENT_STORAGE_TEXT : WEBKIT_COOKIE_PERSISTENT_STORAGE_SQLITE;
        webkit_cookie_manager_set_persistent_storage(cookieManager, cookiesFile, storageType);
    }

    // Enable the favicon database, by specifying the default directory.
    webkit_web_context_set_favicon_database_directory(webContext, NULL);

    webkit_web_context_register_uri_scheme(webContext, BROWSER_ABOUT_SCHEME, (WebKitURISchemeRequestCallback)aboutURISchemeRequestCallback, webContext, NULL);

    WebKitUserContentManager *userContentManager = webkit_user_content_manager_new();
    webkit_user_content_manager_register_script_message_handler(userContentManager, "aboutData");
    g_signal_connect(userContentManager, "script-message-received::aboutData", G_CALLBACK(aboutDataScriptMessageReceivedCallback), webContext);

#if WEBKIT_CHECK_VERSION(2, 30, 0)
    WebKitWebsitePolicies *defaultWebsitePolicies = g_object_get_data(G_OBJECT(webkitSettings),
                "default-website-policies");
#endif

    // hvml schema
    webkit_web_context_register_uri_scheme(webContext, BROWSER_HVML_SCHEME, (WebKitURISchemeRequestCallback)hvmlURISchemeRequestCallback, webContext, NULL);

    if (contentFilter) {
        GFile *contentFilterFile = g_file_new_for_commandline_arg(contentFilter);

        FilterSaveData saveData = { NULL, NULL, NULL };
        gchar *filtersPath = g_build_filename(g_get_user_cache_dir(), g_get_prgname(), "filters", NULL);
        WebKitUserContentFilterStore *store = webkit_user_content_filter_store_new(filtersPath);
        g_free(filtersPath);

        webkit_user_content_filter_store_save_from_file(store, "GUIProFilter", contentFilterFile, NULL, (GAsyncReadyCallback)filterSavedCallback, &saveData);
        saveData.mainLoop = g_main_loop_new(NULL, FALSE);
        g_main_loop_run(saveData.mainLoop);
        g_object_unref(store);

        if (saveData.filter)
            webkit_user_content_manager_add_filter(userContentManager, saveData.filter);
        else
            g_printerr("Cannot save filter '%s': %s\n", contentFilter, saveData.error->message);

        g_clear_pointer(&saveData.error, g_error_free);
        g_clear_pointer(&saveData.filter, webkit_user_content_filter_unref);
        g_main_loop_unref(saveData.mainLoop);
        g_object_unref(contentFilterFile);
    }

    webkit_web_context_set_automation_allowed(webContext, automationMode);
    g_signal_connect(webContext, "automation-started", G_CALLBACK(automationStartedCallback), application);

    BrowserWindow *mainWindow = BROWSER_WINDOW(browser_window_new(NULL, webContext));
    gtk_application_add_window(GTK_APPLICATION(application), GTK_WINDOW(mainWindow));
    if (darkMode)
        g_object_set(gtk_widget_get_settings(GTK_WIDGET(mainWindow)), "gtk-application-prefer-dark-theme", TRUE, NULL);
    if (fullScreen)
        gtk_window_fullscreen(GTK_WINDOW(mainWindow));

    if (backgroundColor)
        browser_window_set_background_color(mainWindow, backgroundColor);

    GtkWidget *firstTab = NULL;
    if (uriArguments) {
        int i;

        for (i = 0; uriArguments[i]; i++) {
#if WEBKIT_CHECK_VERSION(2,30,0)
            WebKitWebView *webView = createBrowserTab(mainWindow, webkitSettings, userContentManager, defaultWebsitePolicies);
#else
            WebKitWebView *webView = createBrowserTab(mainWindow, webkitSettings, userContentManager);
#endif

            if (!i) {
                firstTab = GTK_WIDGET(webView);
                if (exitAfterLoad)
                    exitAfterWebViewLoadFinishes(webView, application);
            }
            gchar *url = argumentToURL(uriArguments[i]);
            webkit_web_view_load_uri(webView, url);
            g_free(url);
        }
    }
    else {
#if WEBKIT_CHECK_VERSION(2,30,0)
        WebKitWebView *webView = createBrowserTab(mainWindow, webkitSettings, userContentManager, defaultWebsitePolicies);
#else
        WebKitWebView *webView = createBrowserTab(mainWindow, webkitSettings, userContentManager);
#endif
        firstTab = GTK_WIDGET(webView);

        if (!editorMode) {
            if (sessionFile)
                browser_window_load_session(mainWindow, sessionFile);
            else if (!automationMode) {
                webkit_web_view_load_uri(webView, BROWSER_DEFAULT_URL);
                if (exitAfterLoad)
                    exitAfterWebViewLoadFinishes(webView, application);
            }
        }
    }

    g_object_unref(webContext);
    g_object_unref(userContentManager);

    gtk_widget_grab_focus(firstTab);
    gtk_widget_show(GTK_WIDGET(mainWindow));
}

int main(int argc, char *argv[])
{
#if ENABLE_DEVELOPER_MODE
    g_setenv("WEBKIT_INJECTED_BUNDLE_PATH", WEBKIT_INJECTED_BUNDLE_PATH, FALSE);
#endif

#if GTK_CHECK_VERSION(3, 98, 0)
    gtk_init();
#else
    gtk_init(&argc, &argv);
#endif

    GOptionContext *context = g_option_context_new(NULL);
    g_option_context_add_main_entries(context, commandLineOptions, 0);
#if !GTK_CHECK_VERSION(3, 98, 0)
    g_option_context_add_group(context, gtk_get_option_group(TRUE));
#endif

    WebKitSettings *webkitSettings = webkit_settings_new();
    webkit_settings_set_enable_developer_extras(webkitSettings, TRUE);
    webkit_settings_set_enable_webgl(webkitSettings, TRUE);
    webkit_settings_set_enable_media_stream(webkitSettings, TRUE);
    if (!addSettingsGroupToContext(context, webkitSettings)) {
        g_clear_object(&webkitSettings);
        return 1;
    }

    GError *error = 0;
    if (!g_option_context_parse(context, &argc, &argv, &error)) {
        g_printerr("Cannot parse arguments: %s\n", error->message);
        g_error_free(error);
        g_option_context_free(context);
        g_clear_object(&webkitSettings);

        return 1;
    }
    g_option_context_free(context);

    if (printVersion) {
        g_print("WebKitGTK %u.%u.%u",
            webkit_get_major_version(),
            webkit_get_minor_version(),
            webkit_get_micro_version());
        if (g_strcmp0(BUILD_REVISION, "tarball"))
            g_print(" (%s)", BUILD_REVISION);
        g_print("\n");
        g_clear_object(&webkitSettings);

        return 0;
    }

    GtkApplication *application = gtk_application_new(APP_NAME, G_APPLICATION_NON_UNIQUE);
    g_signal_connect(application, "startup", G_CALLBACK(startup), webkitSettings);
    g_signal_connect(application, "shutdown", G_CALLBACK(shutdown), webkitSettings);
    g_signal_connect(application, "activate", G_CALLBACK(activate), webkitSettings);
    g_application_run(G_APPLICATION(application), 0, NULL);
    g_object_unref(application);

    return exitAfterLoad && webProcessCrashed ? 1 : 0;
}
