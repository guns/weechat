/*
 * Copyright (C) 2008-2010 Dmitry Kobylin <fnfal@academ.tsc.ru>
 * Copyright (C) 2008-2014 Sébastien Helleu <flashcode@flashtux.org>
 *
 * This file is part of WeeChat, the extensible chat client.
 *
 * WeeChat is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * WeeChat is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with WeeChat.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __WEECHAT_TCL_API_H
#define __WEECHAT_TCL_API_H 1

extern int weechat_tcl_api_buffer_input_data_cb (void *data,
                                                 struct t_gui_buffer *buffer,
                                                 const char *input_data);
extern int weechat_tcl_api_buffer_close_cb (void *data,
                                            struct t_gui_buffer *buffer);
extern void weechat_tcl_api_init (Tcl_Interp *interp);

#endif /* __WEECHAT_TCL_API_H */
