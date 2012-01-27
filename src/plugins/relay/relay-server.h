/*
 * Copyright (C) 2003-2012 Sebastien Helleu <flashcode@flashtux.org>
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

#ifndef __WEECHAT_RELAY_SERVER_H
#define __WEECHAT_RELAY_SERVER_H 1

struct t_relay_server
{
    enum t_relay_protocol protocol;    /* protocol (irc,..)                 */
    char *protocol_args;               /* arguments used for protocol       */
                                       /* example: server for irc protocol  */
    int port;                          /* listening on this port            */
    int sock;                          /* socket for connection             */
    struct t_hook *hook_fd;            /* hook for socket                   */
    time_t start_time;                 /* start time                        */
    struct t_relay_server *prev_server;/* link to previous server           */
    struct t_relay_server *next_server;/* link to next server               */
};

extern struct t_relay_server *relay_servers;
extern struct t_relay_server *last_relay_server;

extern void relay_server_get_protocol_args (const char *protocol_and_string,
                                            char **protocol,
                                            char **protocol_args);
extern struct t_relay_server *relay_server_search (const char *protocol_and_args);
extern struct t_relay_server *relay_server_search_port (int port);
extern void relay_server_close_socket (struct t_relay_server *server);
extern int relay_server_create_socket (struct t_relay_server *server);
extern struct t_relay_server *relay_server_new (enum t_relay_protocol protocol,
                                                const char *protocol_args,
                                                int port);
extern void relay_server_update_port (struct t_relay_server *server, int port);
extern void relay_server_free (struct t_relay_server *server);
extern void relay_server_free_all ();
extern void relay_server_print_log ();

#endif /* __WEECHAT_RELAY_SERVER_H */
