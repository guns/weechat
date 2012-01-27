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

/*
 * relay.c: network communication between WeeChat and remote client
 */

#include <stdlib.h>

#include "../weechat-plugin.h"
#include "relay.h"
#include "relay-buffer.h"
#include "relay-client.h"
#include "relay-command.h"
#include "relay-completion.h"
#include "relay-config.h"
#include "relay-info.h"
#include "relay-raw.h"
#include "relay-server.h"
#include "relay-upgrade.h"


WEECHAT_PLUGIN_NAME(RELAY_PLUGIN_NAME);
WEECHAT_PLUGIN_DESCRIPTION("Network communication between WeeChat and "
                           "remote application");
WEECHAT_PLUGIN_AUTHOR("Sebastien Helleu <flashcode@flashtux.org>");
WEECHAT_PLUGIN_VERSION(WEECHAT_VERSION);
WEECHAT_PLUGIN_LICENSE(WEECHAT_LICENSE);

struct t_weechat_plugin *weechat_relay_plugin = NULL;

int relay_signal_upgrade_received = 0; /* signal "upgrade" received ?       */

char *relay_protocol_string[] =        /* strings for protocols             */
{ "weechat", "irc" };


/*
 * relay_protocol_search: search a protocol by name
 */

int
relay_protocol_search (const char *name)
{
    int i;

    for (i = 0; i < RELAY_NUM_PROTOCOLS; i++)
    {
        if (weechat_strcasecmp (relay_protocol_string[i], name) == 0)
            return i;
    }

    /* protocol not found */
    return -1;
}

/*
 * relay_signal_upgrade_cb: callback for "upgrade" signal
 */

int
relay_signal_upgrade_cb (void *data, const char *signal, const char *type_data,
                         void *signal_data)
{
    struct t_relay_server *ptr_server;

    /* make C compiler happy */
    (void) data;
    (void) signal;
    (void) type_data;
    (void) signal_data;

    relay_signal_upgrade_received = 1;

    for (ptr_server = relay_servers; ptr_server;
         ptr_server = ptr_server->next_server)
    {
        relay_server_close_socket (ptr_server);
    }

    return WEECHAT_RC_OK;
}

/*
 * relay_debug_dump_cb: callback for "debug_dump" signal
 */

int
relay_debug_dump_cb (void *data, const char *signal, const char *type_data,
                     void *signal_data)
{
    /* make C compiler happy */
    (void) data;
    (void) signal;
    (void) type_data;

    if (!signal_data
        || (weechat_strcasecmp ((char *)signal_data, RELAY_PLUGIN_NAME) == 0))
    {
        weechat_log_printf ("");
        weechat_log_printf ("***** \"%s\" plugin dump *****",
                            weechat_plugin->name);

        relay_server_print_log ();
        relay_client_print_log ();

        weechat_log_printf ("");
        weechat_log_printf ("***** End of \"%s\" plugin dump *****",
                            weechat_plugin->name);
    }

    return WEECHAT_RC_OK;
}

/*
 * weechat_plugin_init: initialize relay plugin
 */

int
weechat_plugin_init (struct t_weechat_plugin *plugin, int argc, char *argv[])
{
    int i, upgrading;

    /* make C compiler happy */
    (void) argc;
    (void) argv;

    weechat_plugin = plugin;

    if (!relay_config_init ())
        return WEECHAT_RC_ERROR;

    if (relay_config_read () < 0)
        return WEECHAT_RC_ERROR;

    relay_command_init ();

    /* hook completions */
    relay_completion_init ();

    weechat_hook_signal ("upgrade", &relay_signal_upgrade_cb, NULL);
    weechat_hook_signal ("debug_dump", &relay_debug_dump_cb, NULL);

    relay_info_init ();

    /* look at arguments */
    upgrading = 0;
    for (i = 0; i < argc; i++)
    {
        if (weechat_strcasecmp (argv[i], "--upgrade") == 0)
        {
            upgrading = 1;
        }
    }

    if (upgrading)
        relay_upgrade_load ();

    return WEECHAT_RC_OK;
}

/*
 * weechat_plugin_end: end relay plugin
 */

int
weechat_plugin_end (struct t_weechat_plugin *plugin)
{
    /* make C compiler happy */
    (void) plugin;

    relay_config_write ();

    if (relay_signal_upgrade_received)
        relay_upgrade_save ();
    else
    {
        relay_raw_message_free_all ();

        relay_server_free_all ();

        relay_client_disconnect_all ();

        if (relay_buffer)
            weechat_buffer_close (relay_buffer);

        relay_client_free_all ();
    }

    return WEECHAT_RC_OK;
}
