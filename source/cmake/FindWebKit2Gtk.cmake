# Copyright (C) 2022 Beijing FMSoft Technologies Co., Ltd.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER AND ITS CONTRIBUTORS ``AS
# IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR ITS
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#[=======================================================================[.rst:
FindWebKit2Gtk
--------------

Find WebKit headers and libraries.

Imported Targets
^^^^^^^^^^^^^^^^

``WebKit::WebKit``
  The WebKit library, if found.

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables in your project:

``WebKit_FOUND``
  true if (the requested version of) WebKit is available.
``WebKit_VERSION``
  the version of WebKit.
``WebKit_LIBRARIES``
  the libraries to link against to use WebKit.
``WebKit_INCLUDE_DIRS``
  where to find the WebKit headers.
``WebKit_COMPILE_OPTIONS``
  this should be passed to target_compile_options(), if the
  target is not used for linking

#]=======================================================================]

find_package(PkgConfig QUIET)

pkg_check_modules(PC_WEBKIT2GTK QUIET ${WEBKIT2GTK_PC_NAME})
set(WebKit2Gtk_COMPILE_OPTIONS ${PC_WEBKIT2GTK_CFLAGS_OTHER})
set(WebKit2Gtk_VERSION ${PC_WEBKIT2GTK_VERSION})

find_path(WebKit2Gtk_INCLUDE_DIR
    NAMES webkit2/webkit2.h
    HINTS ${PC_WEBKIT2GTK_INCLUDEDIR}/${WEBKIT2GTK_PC_NAME} ${PC_WEBKIT2GTK_INCLUDE_DIR}/${WEBKIT2GTK_PC_NAME}
)

find_library(WebKit2Gtk_LIBRARY
    NAMES ${WebKit2Gtk_NAMES} ${WEBKIT2GTK_PC_NAME}
    HINTS ${PC_WEBKIT2GTK_LIBDIR} ${PC_WEBKIT2GTK_LIBRARY_DIRS}
)

find_library(JavaScriptCoreGtk_LIBRARY
    NAMES ${JAVASCRIPTCOREGTK_PC_NAME}
    HINTS ${PC_WEBKIT2GTK_LIBDIR} ${PC_WEBKIT2GTK_LIBRARY_DIRS}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(WebKit2Gtk
    FOUND_VAR WebKit2Gtk_FOUND
    REQUIRED_VARS WebKit2Gtk_INCLUDE_DIR WebKit2Gtk_LIBRARY JavaScriptCoreGtk_LIBRARY
    VERSION_VAR WebKit2Gtk_VERSION
)

pkg_get_variable(WEBKIT2GTK_BUILD_REVISION ${WEBKIT2GTK_PC_NAME} revision)

if (WebKit2Gtk_FOUND AND NOT TARGET WebKit::WebKit)
    add_library(WebKit::WebKit UNKNOWN IMPORTED GLOBAL)
    set_target_properties(WebKit::WebKit PROPERTIES
        IMPORTED_LOCATION "${WebKit2Gtk_LIBRARY}"
        INTERFACE_COMPILE_OPTIONS "${WebKit2Gtk_COMPILE_OPTIONS}"
        INTERFACE_INCLUDE_DIRECTORIES "${WebKit2Gtk_INCLUDE_DIR}"
    )

    add_library(WebKit::JSC UNKNOWN IMPORTED GLOBAL)
    set_target_properties(WebKit::JSC PROPERTIES
        IMPORTED_LOCATION "${JavaScriptCoreGtk_LIBRARY}"
        INTERFACE_COMPILE_OPTIONS "${WebKit2Gtk_COMPILE_OPTIONS}"
        INTERFACE_INCLUDE_DIRECTORIES "${WebKit2Gtk_INCLUDE_DIR}"
    )
endif ()

if (WebKit2Gtk_FOUND)
    set(WebKit_LIBRARIES ${WebKit2Gtk_LIBRARY} ${JavaScriptCoreGtk_LIBRARY})
    set(WebKit_INCLUDE_DIRS ${WebKit2Gtk_INCLUDE_DIR})
    mark_as_advanced(WebKit_INCLUDE_DIRS WebKit_LIBRARIES)
endif ()


