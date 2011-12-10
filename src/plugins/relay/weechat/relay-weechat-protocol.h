/*
 * Copyright (C) 2003-2011 Sebastien Helleu <flashcode@flashtux.org>
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

#ifndef __WEECHAT_RELAY_WEECHAT_PROTOCOL_H
#define __WEECHAT_RELAY_WEECHAT_PROTOCOL_H 1

#define RELAY_WEECHAT_PROTOCOL_CALLBACK(__command)                      \
    int                                                                 \
    relay_weechat_protocol_cb_##__command (                             \
        struct t_relay_client *client,                                  \
        const char *id,                                                 \
        const char *command,                                            \
        int argc,                                                       \
        char **argv,                                                    \
        char **argv_eol)

#define RELAY_WEECHAT_PROTOCOL_MIN_ARGS(__min_args)                     \
    (void) id;                                                          \
    (void) command;                                                     \
    (void) argv;                                                        \
    (void) argv_eol;                                                    \
    if ((weechat_relay_plugin->debug >= 1) && (argc < __min_args))      \
    {                                                                   \
        weechat_printf (NULL,                                           \
                        _("%s%s: too few arguments received from "      \
                          "client %d for command \"%s\" "               \
                          "(received: %d arguments, expected: at "      \
                          "least %d)"),                                 \
                        weechat_prefix ("error"), RELAY_PLUGIN_NAME,    \
                        client->id, command, argc, __min_args);         \
        return WEECHAT_RC_ERROR;                                        \
    }

typedef int (t_relay_weechat_cmd_func)(struct t_relay_client *client,
                                       const char *id, const char *command,
                                       int argc, char **argv, char **argv_eol);

struct t_relay_weechat_protocol_cb
{
    char *name;                        /* relay command                     */
    t_relay_weechat_cmd_func *cmd_function; /* callback                     */
};

extern void relay_weechat_protocol_recv (struct t_relay_client *client,
                                         char *data);

#endif /* __WEECHAT_RELAY_WEECHAT_PROTOCOL_H */
