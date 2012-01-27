/*
 * Copyright (C) 2011-2012 Sebastien Helleu <flashcode@flashtux.org>
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

#ifndef __WEECHAT_GUILE_H
#define __WEECHAT_GUILE_H 1

#define weechat_plugin weechat_guile_plugin
#define GUILE_PLUGIN_NAME "guile"

#define GUILE_CURRENT_SCRIPT_NAME ((guile_current_script) ? guile_current_script->name : "-")

extern struct t_weechat_plugin *weechat_guile_plugin;

extern int guile_quiet;
extern struct t_plugin_script *guile_scripts;
extern struct t_plugin_script *last_guile_script;
extern struct t_plugin_script *guile_current_script;
extern struct t_plugin_script *guile_registered_script;
extern const char *guile_current_script_filename;
extern SCM guile_port;

extern SCM weechat_guile_hashtable_to_alist (struct t_hashtable *hashtable);
extern struct t_hashtable *weechat_guile_alist_to_hashtable (SCM dict,
                                                             int hashtable_size);
extern void *weechat_guile_exec (struct t_plugin_script *script,
                                  int ret_type, const char *function,
                                  char *format, void **argv);
extern int weechat_guile_port_fill_input (SCM port);
extern void weechat_guile_port_write (SCM port, const void *data, size_t size);

#endif /* __WEECHAT_GUILE_H */
