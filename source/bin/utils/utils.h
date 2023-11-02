/*
** utils.h -- The internal utility interfaces
**
** Copyright (C) 2023 FMSoft (http://www.fmsoft.cn)
**
** Author: XueShuming
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

#ifndef XGUI_PRO_BIN_UTILS_H
#define XGUI_PRO_BIN_UTILS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

time_t xgutils_get_monotoic_time_ms(void);

#ifdef __cplusplus
}
#endif

#endif  /* XGUI_PRO_BIN_UTILS_H */

