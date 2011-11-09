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

/*
 * relay-client-weechat.c: WeeChat protocol for relay to client
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>

#include "../weechat-plugin.h"
#include "relay.h"
#include "relay-client-weechat.h"
#include "relay-client.h"


/*
 * relay_client_weechat_sendf: send formatted data to client
 */

int
relay_client_weechat_sendf (struct t_relay_client *client,
                            const char *format, ...)
{
    char str_length[8];
    int length_vbuffer, num_sent, total_sent;

    if (!client)
        return 0;

    weechat_va_format (format);
    if (!vbuffer)
        return 0;
    length_vbuffer = strlen (vbuffer);

    total_sent = 0;

    snprintf (str_length, sizeof (str_length), "%07d", length_vbuffer);

    num_sent = send (client->sock, str_length, 7, 0);
    client->bytes_sent += 7;
    total_sent += num_sent;
    if (num_sent >= 0)
    {
        num_sent = send (client->sock, vbuffer, length_vbuffer, 0);
        client->bytes_sent += length_vbuffer;
        total_sent += num_sent;
    }

    if (num_sent < 0)
    {
        weechat_printf (NULL,
                        _("%s%s: error sending data to client %s"),
                        weechat_prefix ("error"), RELAY_PLUGIN_NAME,
                        strerror (errno));
    }

    return total_sent;
}

/*
 * relay_client_weechat_send_infolist: send infolist to client
 */

void
relay_client_weechat_send_infolist (struct t_relay_client *client,
                                    const char *name,
                                    struct t_infolist *infolist)
{
    const char *fields;
    char **argv;
    int i, argc, size;

    relay_client_weechat_sendf (client, "name %s", name);

    while (weechat_infolist_next (infolist))
    {
        fields = weechat_infolist_fields (infolist);
        if (fields)
        {
            argv = weechat_string_split (fields, ",", 0, 0, &argc);
            if (argv && (argc > 0))
            {
                for (i = 0; i < argc; i++)
                {
                    switch (argv[i][0])
                    {
                        case 'i':
                            relay_client_weechat_sendf (client, "%s %c %d",
                                                        argv[i] + 2, argv[i][0],
                                                        weechat_infolist_integer (infolist,
                                                                                  argv[i] + 2));
                            break;
                        case 's':
                            relay_client_weechat_sendf (client, "%s %c %s",
                                                        argv[i] + 2, argv[i][0],
                                                        weechat_infolist_string (infolist,
                                                                                 argv[i] + 2));
                            break;
                        case 'p':
                            relay_client_weechat_sendf (client, "%s %c %lx",
                                                        argv[i] + 2, argv[i][0],
                                                        (long unsigned int)weechat_infolist_pointer (infolist,
                                                                                                     argv[i] + 2));
                            break;
                        case 'b':
                            relay_client_weechat_sendf (client, "%s %c %lx",
                                                        argv[i] + 2, argv[i][0],
                                                        (long unsigned int)weechat_infolist_buffer (infolist,
                                                                                                    argv[i] + 2,
                                                                                                      &size));
                            break;
                        case 't':
                            relay_client_weechat_sendf (client, "%s %c %ld",
                                                        argv[i] + 2, argv[i][0],
                                                        weechat_infolist_time (infolist, argv[i] + 2));
                            break;
                    }
                }
            }
            if (argv)
                weechat_string_free_split (argv);
        }
    }
}

/*
 * relay_client_weechat_recv_one_msg: read one message from client
 */

void
relay_client_weechat_recv_one_msg (struct t_relay_client *client, char *data)
{
    char *pos;
    struct t_infolist *infolist;

    pos = strchr (data, '\r');
    if (pos)
        pos[0] = '\0';

    if (weechat_relay_plugin->debug)
    {
        weechat_printf (NULL, "relay: weechat: \"%s\"", data);
    }

    if (weechat_strcasecmp (data, "quit") == 0)
        relay_client_set_status (client, RELAY_STATUS_DISCONNECTED);
    else
    {
        infolist = weechat_infolist_get (data, NULL, NULL);
        if (infolist)
        {
            relay_client_weechat_send_infolist (client, data, infolist);
            weechat_infolist_free (infolist);
        }
    }
}

/*
 * relay_client_weechat_recv: read data from client
 */

void
relay_client_weechat_recv (struct t_relay_client *client, const char *data)
{
    char **items;
    int items_count, i;

    items = weechat_string_split (data, "\n", 0, 0, &items_count);
    for (i = 0; i < items_count; i++)
    {
        relay_client_weechat_recv_one_msg (client, items[i]);
    }
    if (items)
        weechat_string_free_split (items);
}

/*
 * relay_client_weechat_alloc: init relay data specific to weechat protocol
 */

void
relay_client_weechat_alloc (struct t_relay_client *client)
{
    struct t_relay_client_weechat_data *weechat_data;

    client->protocol_data = malloc (sizeof (*weechat_data));
    if (client->protocol_data)
    {
        /* ... */
    }
}

/*
 * relay_client_weechat_alloc_with_infolist: init relay data specific to
 *                                           weechat protocol with an infolist
 */

void
relay_client_weechat_alloc_with_infolist (struct t_relay_client *client,
                                          struct t_infolist *infolist)
{
    struct t_relay_client_weechat_data *weechat_data;

    /* make C compiler happy */
    (void) infolist;

    client->protocol_data = malloc (sizeof (*weechat_data));
    if (client->protocol_data)
    {
        /* ... */
    }
}

/*
 * relay_client_weechat_free: free relay data specific to weechat protocol
 */

void
relay_client_weechat_free (struct t_relay_client *client)
{
    if (client->protocol_data)
        free (client->protocol_data);
}

/*
 * relay_client_weechat_add_to_infolist: add client weechat data in an
 *                                       infolist item
 *                                       return 1 if ok, 0 if error
 */

int
relay_client_weechat_add_to_infolist (struct t_infolist_item *item,
                                      struct t_relay_client *client)
{
    if (!item || !client)
        return 0;

    return 1;
}

/*
 * relay_client_weechat_print_log: print weechat client infos in log (usually
 *                                 for crash dump)
 */

void
relay_client_weechat_print_log (struct t_relay_client *client)
{
    if (client->protocol_data)
    {
    }
}
