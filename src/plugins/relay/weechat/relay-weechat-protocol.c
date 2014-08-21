/*
 * relay-weechat-protocol.c - WeeChat protocol for relay to client
 *
 * Copyright (C) 2003-2014 Sébastien Helleu <flashcode@flashtux.org>
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../../weechat-plugin.h"
#include "../relay.h"
#include "relay-weechat.h"
#include "relay-weechat-protocol.h"
#include "relay-weechat-msg.h"
#include "relay-weechat-nicklist.h"
#include "../relay-buffer.h"
#include "../relay-client.h"
#include "../relay-config.h"
#include "../relay-raw.h"


/*
 * Checks if the buffer pointer is a relay buffer (relay raw/list).
 *
 * Returns:
 *   1: buffer is a relay buffer (raw/list)
 *   0: buffer is NOT a relay buffer
 */

int
relay_weechat_is_relay_buffer (struct t_gui_buffer *buffer)
{
    return ((relay_raw_buffer && (buffer == relay_raw_buffer))
            || (relay_buffer && (buffer == relay_buffer))) ? 1 : 0;
}

/*
 * Gets buffer pointer with argument from a command.
 *
 * The argument "arg" can be a pointer ("0x12345678") or a full name
 * ("irc.freenode.#weechat").
 *
 * Returns pointer to buffer found, NULL if not found.
 */

struct t_gui_buffer *
relay_weechat_protocol_get_buffer (const char *arg)
{
    struct t_gui_buffer *ptr_buffer;
    long unsigned int value;
    int rc;
    struct t_hdata *ptr_hdata;

    ptr_buffer = NULL;

    if (strncmp (arg, "0x", 2) == 0)
    {
        rc = sscanf (arg, "%lx", &value);
        if ((rc != EOF) && (rc != 0))
            ptr_buffer = (struct t_gui_buffer *)value;
        if (ptr_buffer)
        {
            ptr_hdata = weechat_hdata_get ("buffer");
            if (!weechat_hdata_check_pointer (ptr_hdata,
                                              weechat_hdata_get_list (ptr_hdata, "gui_buffers"),
                                              ptr_buffer))
            {
                /* invalid pointer! */
                ptr_buffer = NULL;
            }
        }
    }
    else
        ptr_buffer = weechat_buffer_search ("==", arg);

    return ptr_buffer;
}

/*
 * Gets integer value of a synchronization flag.
 */

int
relay_weechat_protocol_sync_flag (const char *flag)
{
    if (strcmp (flag, "buffer") == 0)
        return RELAY_WEECHAT_PROTOCOL_SYNC_BUFFER;
    if (strcmp (flag, "nicklist") == 0)
        return RELAY_WEECHAT_PROTOCOL_SYNC_NICKLIST;
    if (strcmp (flag, "buffers") == 0)
        return RELAY_WEECHAT_PROTOCOL_SYNC_BUFFERS;
    if (strcmp (flag, "upgrade") == 0)
        return RELAY_WEECHAT_PROTOCOL_SYNC_UPGRADE;

    /* unknown flag */
    return 0;
}

/*
 * Checks if buffer is synchronized with at least one of the flags given.
 *
 * First searches buffer with full_name in hashtable "buffers_sync" (if buffer
 * is not NULL).
 * If buffer is NULL or not found, searches "*" (which means "all buffers").
 *
 * The "flags" argument can be a combination (logical OR) of:
 *   RELAY_WEECHAT_PROTOCOL_SYNC_BUFFER
 *   RELAY_WEECHAT_PROTOCOL_SYNC_NICKLIST
 *   RELAY_WEECHAT_PROTOCOL_SYNC_BUFFERS
 *   RELAY_WEECHAT_PROTOCOL_SYNC_UPGRADE
 *
 * Returns:
 *   1: buffer is synchronized with at least one flag given
 *   0: buffer is NOT synchronized with any of the flags given
 */

int
relay_weechat_protocol_is_sync (struct t_relay_client *ptr_client,
                                struct t_gui_buffer *buffer, int flags)
{
    int *ptr_flags;

    /* search buffer using its full name */
    if (buffer)
    {
        ptr_flags = weechat_hashtable_get (RELAY_WEECHAT_DATA(ptr_client, buffers_sync),
                                           weechat_buffer_get_string (buffer, "full_name"));
        if (ptr_flags)
            return ((*ptr_flags) & flags) ? 1 : 0;
    }

    /* search special name "*" as fallback */
    ptr_flags = weechat_hashtable_get (RELAY_WEECHAT_DATA(ptr_client, buffers_sync),
                                       "*");
    if (ptr_flags)
        return ((*ptr_flags) & flags) ? 1 : 0;

    /*
     * buffer not found at all in hashtable (neither name, neither "*")
     * => it is NOT synchronized
     */
    return 0;
}

/*
 * Callback for command "init" (from client).
 *
 * Message looks like:
 *   init password=mypass
 *   init password=mypass,compression=zlib
 *   init password=mypass,compression=off
 */

RELAY_WEECHAT_PROTOCOL_CALLBACK(init)
{
    char **options, *pos, *password;
    int num_options, i, compression;

    RELAY_WEECHAT_PROTOCOL_MIN_ARGS(1);

    options = weechat_string_split (argv_eol[0], ",", 0, 0, &num_options);
    if (options)
    {
        for (i = 0; i < num_options; i++)
        {
            pos = strchr (options[i], '=');
            if (pos)
            {
                pos[0] = '\0';
                pos++;
                if (strcmp (options[i], "password") == 0)
                {
                    password = weechat_string_eval_expression (weechat_config_string (relay_config_network_password),
                                                               NULL, NULL, NULL);
                    if (password)
                    {
                        if (strcmp (password, pos) == 0)
                        {
                            RELAY_WEECHAT_DATA(client, password_ok) = 1;
                            weechat_hook_signal_send ("relay_client_auth_ok",
                                                      WEECHAT_HOOK_SIGNAL_POINTER,
                                                      client);
                        }
                        free (password);
                    }
                }
                else if (strcmp (options[i], "compression") == 0)
                {
                    compression = relay_weechat_compression_search (pos);
                    if (compression >= 0)
                        RELAY_WEECHAT_DATA(client, compression) = compression;
                }
            }
        }
        weechat_string_free_split (options);
    }

    return WEECHAT_RC_OK;
}

/*
 * Callback for command "hdata" (from client).
 *
 * Message looks like:
 *   hdata buffer:gui_buffers(*) number,name,type,nicklist,title
 *   hdata buffer:gui_buffers(*)/own_lines/first_line(*)/data date,displayed,prefix,message
 */

RELAY_WEECHAT_PROTOCOL_CALLBACK(hdata)
{
    struct t_relay_weechat_msg *msg;

    RELAY_WEECHAT_PROTOCOL_MIN_ARGS(1);

    msg = relay_weechat_msg_new (id);
    if (msg)
    {
        if (relay_weechat_msg_add_hdata (msg, argv[0],
                                         (argc > 1) ? argv_eol[1] : NULL))
        {
            relay_weechat_msg_send (client, msg);
        }
        relay_weechat_msg_free (msg);
    }

    return WEECHAT_RC_OK;
}

/*
 * Callback for command "info" (from client).
 *
 * Message looks like:
 *   info version
 */

RELAY_WEECHAT_PROTOCOL_CALLBACK(info)
{
    struct t_relay_weechat_msg *msg;
    const char *info;

    RELAY_WEECHAT_PROTOCOL_MIN_ARGS(1);

    msg = relay_weechat_msg_new (id);
    if (msg)
    {
        info = weechat_info_get (argv[0],
                                 (argc > 1) ? argv_eol[1] : NULL);
        relay_weechat_msg_add_type (msg, RELAY_WEECHAT_MSG_OBJ_INFO);
        relay_weechat_msg_add_string (msg, argv[0]);
        relay_weechat_msg_add_string (msg, info);
        relay_weechat_msg_send (client, msg);
        relay_weechat_msg_free (msg);
    }

    return WEECHAT_RC_OK;
}

/*
 * Callback for command "infolist" (from client).
 *
 * Message looks like:
 *   infolist buffer
 */

RELAY_WEECHAT_PROTOCOL_CALLBACK(infolist)
{
    struct t_relay_weechat_msg *msg;
    long unsigned int value;
    char *args;
    int rc;

    RELAY_WEECHAT_PROTOCOL_MIN_ARGS(1);

    msg = relay_weechat_msg_new (id);
    if (msg)
    {
        value = 0;
        args = NULL;
        if (argc > 1)
        {
            rc = sscanf (argv[1], "%lx", &value);
            if ((rc == EOF) || (rc == 0))
                value = 0;
            if (argc > 2)
                args = argv_eol[2];
        }
        relay_weechat_msg_add_infolist (msg, argv[0], (void *)value, args);
        relay_weechat_msg_send (client, msg);
        relay_weechat_msg_free (msg);
    }

    return WEECHAT_RC_OK;
}

/*
 * Callback for command "nicklist" (from client).
 *
 * Message looks like:
 *   nicklist irc.freenode.#weechat
 *   nicklist 0x12345678
 */

RELAY_WEECHAT_PROTOCOL_CALLBACK(nicklist)
{
    struct t_relay_weechat_msg *msg;
    struct t_gui_buffer *ptr_buffer;

    RELAY_WEECHAT_PROTOCOL_MIN_ARGS(0);

    ptr_buffer = NULL;

    if (argc > 0)
    {
        ptr_buffer = relay_weechat_protocol_get_buffer (argv[0]);
        if (!ptr_buffer)
        {
            if (weechat_relay_plugin->debug >= 1)
            {
                weechat_printf (NULL,
                                _("%s: invalid buffer pointer in message: "
                                  "\"%s %s\""),
                                RELAY_PLUGIN_NAME,
                                command,
                                argv_eol[0]);
            }
            return WEECHAT_RC_OK;
        }
    }

    msg = relay_weechat_msg_new (id);
    if (msg)
    {
        relay_weechat_msg_add_nicklist (msg, ptr_buffer, NULL);
        relay_weechat_msg_send (client, msg);
        relay_weechat_msg_free (msg);
    }

    return WEECHAT_RC_OK;
}

/*
 * Timer callback for input command.
 */

int
relay_weechat_protocol_input_timer_cb (void *data,
                                       int remaining_calls)
{
    char **timer_args;
    int i;
    struct t_gui_buffer *ptr_buffer;

    /* make C compiler happy */
    (void) remaining_calls;

    timer_args = (char **)data;

    if (!timer_args)
        return WEECHAT_RC_ERROR;

    if (timer_args[0] && timer_args[1])
    {
        ptr_buffer = weechat_buffer_search ("==", timer_args[0]);
        if (ptr_buffer)
            weechat_command (ptr_buffer, timer_args[1]);
    }

    for (i = 0; i < 2; i++)
    {
        if (timer_args[i])
            free (timer_args[i]);
    }
    free (timer_args);

    return WEECHAT_RC_OK;
}

/*
 * Callback for command "input" (from client).
 *
 * Message looks like:
 *   input core.weechat /help filter
 *   input irc.freenode.#weechat hello guys!
 *   input 0x12345678 hello guys!
 */

RELAY_WEECHAT_PROTOCOL_CALLBACK(input)
{
    struct t_gui_buffer *ptr_buffer;
    char *pos, **timer_args;

    RELAY_WEECHAT_PROTOCOL_MIN_ARGS(2);

    ptr_buffer = relay_weechat_protocol_get_buffer (argv[0]);
    if (!ptr_buffer)
    {
        if (weechat_relay_plugin->debug >= 1)
        {
            weechat_printf (NULL,
                            _("%s: invalid buffer pointer in message: "
                              "\"%s %s\""),
                            RELAY_PLUGIN_NAME,
                            command,
                            argv_eol[0]);
        }
        return WEECHAT_RC_OK;
    }

    pos = strchr (argv_eol[0], ' ');
    if (pos)
    {
        /*
         * use a timer to execute the command after we go back in the
         * WeeChat main loop (some commands like /upgrade executed now can
         * cause a crash)
         */
        timer_args = malloc (2 * sizeof (*timer_args));
        if (timer_args)
        {
            timer_args[0] = strdup (weechat_buffer_get_string (ptr_buffer,
                                                               "full_name"));
            timer_args[1] = strdup (pos + 1);
            weechat_hook_timer (1, 0, 1,
                                &relay_weechat_protocol_input_timer_cb,
                                timer_args);
        }
    }

    return WEECHAT_RC_OK;
}

/*
 * Callback for signals "buffer_*".
 */

int
relay_weechat_protocol_signal_buffer_cb (void *data, const char *signal,
                                         const char *type_data,
                                         void *signal_data)
{
    struct t_relay_client *ptr_client;
    struct t_gui_line *ptr_line;
    struct t_hdata *ptr_hdata_line, *ptr_hdata_line_data;
    struct t_gui_line_data *ptr_line_data;
    struct t_gui_buffer *ptr_buffer;
    struct t_relay_weechat_msg *msg;
    char cmd_hdata[64], str_signal[128];

    /* make C compiler happy */
    (void) type_data;

    ptr_client = (struct t_relay_client *)data;
    if (!ptr_client || !relay_client_valid (ptr_client))
        return WEECHAT_RC_OK;

    snprintf (str_signal, sizeof (str_signal), "_%s", signal);

    if (strcmp (signal, "buffer_opened") == 0)
    {
        ptr_buffer = (struct t_gui_buffer *)signal_data;
        if (!ptr_buffer)
            return WEECHAT_RC_OK;

        /* send signal only if sync with flag "buffers" or "buffer" */
        if (relay_weechat_protocol_is_sync (ptr_client, ptr_buffer,
                                            RELAY_WEECHAT_PROTOCOL_SYNC_BUFFERS |
                                            RELAY_WEECHAT_PROTOCOL_SYNC_BUFFER))
        {
            msg = relay_weechat_msg_new (str_signal);
            if (msg)
            {
                snprintf (cmd_hdata, sizeof (cmd_hdata),
                          "buffer:0x%lx", (long unsigned int)ptr_buffer);
                relay_weechat_msg_add_hdata (msg, cmd_hdata,
                                             "number,full_name,short_name,"
                                             "nicklist,title,local_variables,"
                                             "prev_buffer,next_buffer");
                relay_weechat_msg_send (ptr_client, msg);
                relay_weechat_msg_free (msg);
            }
        }
    }
    else if (strcmp (signal, "buffer_type_changed") == 0)
    {
        ptr_buffer = (struct t_gui_buffer *)signal_data;
        if (!ptr_buffer)
            return WEECHAT_RC_OK;

        /* send signal only if sync with flag "buffer" */
        if (relay_weechat_protocol_is_sync (ptr_client, ptr_buffer,
                                            RELAY_WEECHAT_PROTOCOL_SYNC_BUFFER))
        {
            msg = relay_weechat_msg_new (str_signal);
            if (msg)
            {
                snprintf (cmd_hdata, sizeof (cmd_hdata),
                          "buffer:0x%lx", (long unsigned int)ptr_buffer);
                relay_weechat_msg_add_hdata (msg, cmd_hdata,
                                             "number,full_name,type");
                relay_weechat_msg_send (ptr_client, msg);
                relay_weechat_msg_free (msg);
            }
        }
    }
    else if (strcmp (signal, "buffer_moved") == 0)
    {
        ptr_buffer = (struct t_gui_buffer *)signal_data;
        if (!ptr_buffer)
            return WEECHAT_RC_OK;

        /* send signal only if sync with flag "buffers" or "buffer" */
        if (relay_weechat_protocol_is_sync (ptr_client, ptr_buffer,
                                            RELAY_WEECHAT_PROTOCOL_SYNC_BUFFERS |
                                            RELAY_WEECHAT_PROTOCOL_SYNC_BUFFER))
        {
            msg = relay_weechat_msg_new (str_signal);
            if (msg)
            {
                snprintf (cmd_hdata, sizeof (cmd_hdata),
                          "buffer:0x%lx", (long unsigned int)ptr_buffer);
                relay_weechat_msg_add_hdata (msg, cmd_hdata,
                                             "number,full_name,"
                                             "prev_buffer,next_buffer");
                relay_weechat_msg_send (ptr_client, msg);
                relay_weechat_msg_free (msg);
            }
        }
    }
    else if ((strcmp (signal, "buffer_merged") == 0)
             || (strcmp (signal, "buffer_unmerged") == 0))
    {
        ptr_buffer = (struct t_gui_buffer *)signal_data;
        if (!ptr_buffer)
            return WEECHAT_RC_OK;

        /* send signal only if sync with flag "buffers" or "buffer" */
        if (relay_weechat_protocol_is_sync (ptr_client, ptr_buffer,
                                            RELAY_WEECHAT_PROTOCOL_SYNC_BUFFERS |
                                            RELAY_WEECHAT_PROTOCOL_SYNC_BUFFER))
        {
            msg = relay_weechat_msg_new (str_signal);
            if (msg)
            {
                snprintf (cmd_hdata, sizeof (cmd_hdata),
                          "buffer:0x%lx", (long unsigned int)ptr_buffer);
                relay_weechat_msg_add_hdata (msg, cmd_hdata,
                                             "number,full_name,"
                                             "prev_buffer,next_buffer");
                relay_weechat_msg_send (ptr_client, msg);
                relay_weechat_msg_free (msg);
            }
        }
    }
    else if ((strcmp (signal, "buffer_hidden") == 0)
             || (strcmp (signal, "buffer_unhidden") == 0))
    {
        ptr_buffer = (struct t_gui_buffer *)signal_data;
        if (!ptr_buffer)
            return WEECHAT_RC_OK;

        /* send signal only if sync with flag "buffers" or "buffer" */
        if (relay_weechat_protocol_is_sync (ptr_client, ptr_buffer,
                                            RELAY_WEECHAT_PROTOCOL_SYNC_BUFFERS |
                                            RELAY_WEECHAT_PROTOCOL_SYNC_BUFFER))
        {
            msg = relay_weechat_msg_new (str_signal);
            if (msg)
            {
                snprintf (cmd_hdata, sizeof (cmd_hdata),
                          "buffer:0x%lx", (long unsigned int)ptr_buffer);
                relay_weechat_msg_add_hdata (msg, cmd_hdata,
                                             "number,full_name,"
                                             "prev_buffer,next_buffer");
                relay_weechat_msg_send (ptr_client, msg);
                relay_weechat_msg_free (msg);
            }
        }
    }
    else if (strcmp (signal, "buffer_renamed") == 0)
    {
        ptr_buffer = (struct t_gui_buffer *)signal_data;
        if (!ptr_buffer)
            return WEECHAT_RC_OK;

        /* send signal only if sync with flag "buffers" or "buffer" */
        if (relay_weechat_protocol_is_sync (ptr_client, ptr_buffer,
                                            RELAY_WEECHAT_PROTOCOL_SYNC_BUFFERS |
                                            RELAY_WEECHAT_PROTOCOL_SYNC_BUFFER))
        {
            msg = relay_weechat_msg_new (str_signal);
            if (msg)
            {
                snprintf (cmd_hdata, sizeof (cmd_hdata),
                          "buffer:0x%lx", (long unsigned int)ptr_buffer);
                relay_weechat_msg_add_hdata (msg, cmd_hdata,
                                             "number,full_name,short_name,"
                                             "local_variables");
                relay_weechat_msg_send (ptr_client, msg);
                relay_weechat_msg_free (msg);
            }
        }
    }
    else if (strcmp (signal, "buffer_title_changed") == 0)
    {
        ptr_buffer = (struct t_gui_buffer *)signal_data;
        if (!ptr_buffer)
            return WEECHAT_RC_OK;

        /* send signal only if sync with flag "buffers" or "buffer" */
        if (relay_weechat_protocol_is_sync (ptr_client, ptr_buffer,
                                            RELAY_WEECHAT_PROTOCOL_SYNC_BUFFERS |
                                            RELAY_WEECHAT_PROTOCOL_SYNC_BUFFER))
        {
            msg = relay_weechat_msg_new (str_signal);
            if (msg)
            {
                snprintf (cmd_hdata, sizeof (cmd_hdata),
                          "buffer:0x%lx", (long unsigned int)ptr_buffer);
                relay_weechat_msg_add_hdata (msg, cmd_hdata,
                                             "number,full_name,title");
                relay_weechat_msg_send (ptr_client, msg);
                relay_weechat_msg_free (msg);
            }
        }
    }
    else if (strcmp (signal, "buffer_cleared") == 0)
    {
        ptr_buffer = (struct t_gui_buffer *)signal_data;
        if (!ptr_buffer || relay_weechat_is_relay_buffer (ptr_buffer))
            return WEECHAT_RC_OK;

        /* send signal only if sync with flag "buffer" */
        if (relay_weechat_protocol_is_sync (ptr_client, ptr_buffer,
                                            RELAY_WEECHAT_PROTOCOL_SYNC_BUFFER))
        {
            msg = relay_weechat_msg_new (str_signal);
            if (msg)
            {
                snprintf (cmd_hdata, sizeof (cmd_hdata),
                          "buffer:0x%lx", (long unsigned int)ptr_buffer);
                relay_weechat_msg_add_hdata (msg, cmd_hdata,
                                             "number,full_name");
                relay_weechat_msg_send (ptr_client, msg);
                relay_weechat_msg_free (msg);
            }
        }
    }
    else if (strncmp (signal, "buffer_localvar_", 16) == 0)
    {
        ptr_buffer = (struct t_gui_buffer *)signal_data;
        if (!ptr_buffer)
            return WEECHAT_RC_OK;

        /* send signal only if sync with flag "buffer" */
        if (relay_weechat_protocol_is_sync (ptr_client, ptr_buffer,
                                            RELAY_WEECHAT_PROTOCOL_SYNC_BUFFER))
        {
            msg = relay_weechat_msg_new (str_signal);
            if (msg)
            {
                snprintf (cmd_hdata, sizeof (cmd_hdata),
                          "buffer:0x%lx", (long unsigned int)ptr_buffer);
                relay_weechat_msg_add_hdata (msg, cmd_hdata,
                                             "number,full_name,local_variables");
                relay_weechat_msg_send (ptr_client, msg);
                relay_weechat_msg_free (msg);
            }
        }
    }
    else if (strcmp (signal, "buffer_line_added") == 0)
    {
        ptr_line = (struct t_gui_line *)signal_data;
        if (!ptr_line)
            return WEECHAT_RC_OK;

        ptr_hdata_line = weechat_hdata_get ("line");
        if (!ptr_hdata_line)
            return WEECHAT_RC_OK;

        ptr_hdata_line_data = weechat_hdata_get ("line_data");
        if (!ptr_hdata_line_data)
            return WEECHAT_RC_OK;

        ptr_line_data = weechat_hdata_pointer (ptr_hdata_line, ptr_line, "data");
        if (!ptr_line_data)
            return WEECHAT_RC_OK;

        ptr_buffer = weechat_hdata_pointer (ptr_hdata_line_data, ptr_line_data,
                                            "buffer");
        if (!ptr_buffer || relay_weechat_is_relay_buffer (ptr_buffer))
            return WEECHAT_RC_OK;

        /* send signal only if sync with flag "buffer" */
        if (relay_weechat_protocol_is_sync (ptr_client, ptr_buffer,
                                            RELAY_WEECHAT_PROTOCOL_SYNC_BUFFER))
        {
            msg = relay_weechat_msg_new (str_signal);
            if (msg)
            {
                snprintf (cmd_hdata, sizeof (cmd_hdata),
                          "line_data:0x%lx",
                          (long unsigned int)ptr_line_data);
                relay_weechat_msg_add_hdata (msg, cmd_hdata,
                                             "buffer,date,date_printed,"
                                             "displayed,highlight,tags_array,"
                                             "prefix,message");
                relay_weechat_msg_send (ptr_client, msg);
                relay_weechat_msg_free (msg);
            }
        }
    }
    else if (strcmp (signal, "buffer_closing") == 0)
    {
        ptr_buffer = (struct t_gui_buffer *)signal_data;
        if (!ptr_buffer)
            return WEECHAT_RC_OK;

        /* send signal only if sync with flag "buffers" or "buffer" */
        if (relay_weechat_protocol_is_sync (ptr_client, ptr_buffer,
                                            RELAY_WEECHAT_PROTOCOL_SYNC_BUFFERS |
                                            RELAY_WEECHAT_PROTOCOL_SYNC_BUFFER))
        {
            msg = relay_weechat_msg_new (str_signal);
            if (msg)
            {
                snprintf (cmd_hdata, sizeof (cmd_hdata),
                          "buffer:0x%lx", (long unsigned int)ptr_buffer);
                weechat_hashtable_remove (RELAY_WEECHAT_DATA(ptr_client, buffers_nicklist),
                                          ptr_buffer);
                relay_weechat_msg_add_hdata (msg, cmd_hdata,
                                             "number,full_name");
                relay_weechat_msg_send (ptr_client, msg);
                relay_weechat_msg_free (msg);
            }
        }
    }

    return WEECHAT_RC_OK;
}

/*
 * Callback for entries in hashtable "buffers_nicklist" of client (sends
 * nicklist for each buffer in this hashtable).
 */

void
relay_weechat_protocol_nicklist_map_cb (void *data,
                                        struct t_hashtable *hashtable,
                                        const void *key,
                                        const void *value)
{
    struct t_relay_client *ptr_client;
    struct t_gui_buffer *ptr_buffer;
    struct t_relay_weechat_nicklist *ptr_nicklist;
    struct t_hdata *ptr_hdata;
    struct t_relay_weechat_msg *msg;

    /* make C compiler happy */
    (void) hashtable;

    ptr_client = (struct t_relay_client *)data;
    ptr_buffer = (struct t_gui_buffer *)key;
    ptr_nicklist = (struct t_relay_weechat_nicklist *)value;

    ptr_hdata = weechat_hdata_get ("buffer");
    if (ptr_hdata)
    {
        if (weechat_hdata_check_pointer (ptr_hdata,
                                         weechat_hdata_get_list (ptr_hdata, "gui_buffers"),
                                         ptr_buffer))
        {
            /*
             * if no diff at all, or if diffs are bigger than nicklist:
             * send whole nicklist
             */
            if (ptr_nicklist
                && ((ptr_nicklist->items_count == 0)
                    || (ptr_nicklist->items_count >= weechat_buffer_get_integer (ptr_buffer, "nicklist_count") + 1)))
            {
                ptr_nicklist = NULL;
            }

            /* send nicklist diffs or full nicklist */
            msg = relay_weechat_msg_new ((ptr_nicklist) ? "_nicklist_diff" : "_nicklist");
            if (msg)
            {
                relay_weechat_msg_add_nicklist (msg, ptr_buffer, ptr_nicklist);
                relay_weechat_msg_send (ptr_client, msg);
                relay_weechat_msg_free (msg);
            }
        }
    }
}

/*
 * Callback for nicklist timer.
 */

int
relay_weechat_protocol_timer_nicklist_cb (void *data, int remaining_calls)
{
    struct t_relay_client *ptr_client;

    /* make C compiler happy */
    (void) remaining_calls;

    ptr_client = (struct t_relay_client *)data;
    if (!ptr_client || !relay_client_valid (ptr_client))
        return WEECHAT_RC_OK;

    weechat_hashtable_map (RELAY_WEECHAT_DATA(ptr_client, buffers_nicklist),
                           &relay_weechat_protocol_nicklist_map_cb,
                           ptr_client);

    weechat_hashtable_remove_all (RELAY_WEECHAT_DATA(ptr_client, buffers_nicklist));

    RELAY_WEECHAT_DATA(ptr_client, hook_timer_nicklist) = NULL;

    return WEECHAT_RC_OK;
}

/*
 * Callback for hsignals "nicklist_*".
 */

int
relay_weechat_protocol_hsignal_nicklist_cb (void *data, const char *signal,
                                            struct t_hashtable *hashtable)
{
    struct t_relay_client *ptr_client;
    struct t_gui_nick_group *parent_group, *group;
    struct t_gui_nick *nick;
    struct t_gui_buffer *ptr_buffer;
    struct t_relay_weechat_nicklist *ptr_nicklist;
    char diff;

    ptr_client = (struct t_relay_client *)data;
    if (!ptr_client || !relay_client_valid (ptr_client))
        return WEECHAT_RC_OK;

    /* check if buffer is synchronized with flag "nicklist" */
    ptr_buffer = weechat_hashtable_get (hashtable, "buffer");
    if (!relay_weechat_protocol_is_sync (ptr_client, ptr_buffer,
                                         RELAY_WEECHAT_PROTOCOL_SYNC_NICKLIST))
        return WEECHAT_RC_OK;

    parent_group = weechat_hashtable_get (hashtable, "parent_group");
    group = weechat_hashtable_get (hashtable, "group");
    nick = weechat_hashtable_get (hashtable, "nick");

    /* if there is no parent group (for example "root" group), ignore the signal */
    if (!parent_group)
        return WEECHAT_RC_OK;

    ptr_nicklist = weechat_hashtable_get (RELAY_WEECHAT_DATA(ptr_client,
                                                             buffers_nicklist),
                                          ptr_buffer);
    if (!ptr_nicklist)
    {
        ptr_nicklist = relay_weechat_nicklist_new ();
        if (!ptr_nicklist)
            return WEECHAT_RC_OK;
        ptr_nicklist->nicklist_count = weechat_buffer_get_integer (ptr_buffer,
                                                                   "nicklist_count");
        weechat_hashtable_set (RELAY_WEECHAT_DATA(ptr_client, buffers_nicklist),
                               ptr_buffer,
                               ptr_nicklist);
    }

    /* set diff type */
    diff = RELAY_WEECHAT_NICKLIST_DIFF_UNKNOWN;
    if ((strcmp (signal, "nicklist_group_added") == 0)
        || (strcmp (signal, "nicklist_nick_added") == 0))
    {
        diff = RELAY_WEECHAT_NICKLIST_DIFF_ADDED;
    }
    else if ((strcmp (signal, "nicklist_group_removing") == 0)
        || (strcmp (signal, "nicklist_nick_removing") == 0))
    {
        diff = RELAY_WEECHAT_NICKLIST_DIFF_REMOVED;
    }
    else if ((strcmp (signal, "nicklist_group_changed") == 0)
        || (strcmp (signal, "nicklist_nick_changed") == 0))
    {
        diff = RELAY_WEECHAT_NICKLIST_DIFF_CHANGED;
    }

    if (diff != RELAY_WEECHAT_NICKLIST_DIFF_UNKNOWN)
    {
        /*
         * add items if nicklist was not empty or very small (otherwise we will
         * send full nicklist)
         */
        if (ptr_nicklist->nicklist_count > 1)
        {
            /* add nicklist item for parent group and group/nick */
            relay_weechat_nicklist_add_item (ptr_nicklist,
                                             RELAY_WEECHAT_NICKLIST_DIFF_PARENT,
                                             parent_group, NULL);
            relay_weechat_nicklist_add_item (ptr_nicklist, diff, group, nick);
        }

        /* add timer to send nicklist */
        if (RELAY_WEECHAT_DATA(ptr_client, hook_timer_nicklist))
        {
            weechat_unhook (RELAY_WEECHAT_DATA(ptr_client, hook_timer_nicklist));
            RELAY_WEECHAT_DATA(ptr_client, hook_timer_nicklist) = NULL;
        }
        relay_weechat_hook_timer_nicklist (ptr_client);
    }

    return WEECHAT_RC_OK;
}

/*
 * Callback for signals "upgrade*".
 */

int
relay_weechat_protocol_signal_upgrade_cb (void *data, const char *signal,
                                          const char *type_data,
                                          void *signal_data)
{
    struct t_relay_client *ptr_client;
    struct t_relay_weechat_msg *msg;
    char str_signal[128];

    /* make C compiler happy */
    (void) type_data;
    (void) signal_data;

    ptr_client = (struct t_relay_client *)data;
    if (!ptr_client || !relay_client_valid (ptr_client))
        return WEECHAT_RC_OK;

    snprintf (str_signal, sizeof (str_signal), "_%s", signal);

    if ((strcmp (signal, "upgrade") == 0)
        || (strcmp (signal, "upgrade_ended") == 0))
    {
        /* send signal only if client is synchronized with flag "upgrade" */
        if (relay_weechat_protocol_is_sync (ptr_client, NULL,
                                            RELAY_WEECHAT_PROTOCOL_SYNC_UPGRADE))
        {
            msg = relay_weechat_msg_new (str_signal);
            if (msg)
            {
                relay_weechat_msg_send (ptr_client, msg);
                relay_weechat_msg_free (msg);
            }
        }
    }

    return WEECHAT_RC_OK;
}

/*
 * Callback for command "sync" (from client).
 *
 * Message looks like:
 *   sync
 *   sync * buffer
 *   sync irc.freenode.#weechat buffer,nicklist
 */

RELAY_WEECHAT_PROTOCOL_CALLBACK(sync)
{
    char **buffers, **flags, *full_name;
    int num_buffers, num_flags, i, add_flags, mask, *ptr_old_flags, new_flags;
    int rc;
    long unsigned int value;

    RELAY_WEECHAT_PROTOCOL_MIN_ARGS(0);

    buffers = weechat_string_split ((argc > 0) ? argv[0] : "*", ",", 0, 0,
                                    &num_buffers);
    if (buffers)
    {
        add_flags = RELAY_WEECHAT_PROTOCOL_SYNC_ALL;
        if (argc > 1)
        {
            add_flags = 0;
            flags = weechat_string_split (argv[1], ",", 0, 0, &num_flags);
            if (flags)
            {
                for (i = 0; i < num_flags; i++)
                {
                    add_flags |= relay_weechat_protocol_sync_flag (flags[i]);
                }
                weechat_string_free_split (flags);
            }
        }
        if (add_flags)
        {
            for (i = 0; i < num_buffers; i++)
            {
                full_name = NULL;
                mask = RELAY_WEECHAT_PROTOCOL_SYNC_FOR_BUFFER;
                if (strncmp (buffers[i], "0x", 2) == 0)
                {
                    rc = sscanf (buffers[i], "%lx", &value);
                    if ((rc != EOF) && (rc != 0))
                    {
                        full_name = strdup (weechat_buffer_get_string ((struct t_gui_buffer *)value,
                                                                       "full_name"));
                    }
                }
                else
                {
                    full_name = strdup (buffers[i]);
                    if (strcmp (buffers[i], "*") == 0)
                        mask = RELAY_WEECHAT_PROTOCOL_SYNC_ALL;
                }

                if (full_name)
                {
                    ptr_old_flags = weechat_hashtable_get (RELAY_WEECHAT_DATA(client, buffers_sync),
                                                           full_name);
                    new_flags = ((ptr_old_flags) ? *ptr_old_flags : 0);
                    new_flags |= (add_flags & mask);
                    if (new_flags)
                    {
                        weechat_hashtable_set (RELAY_WEECHAT_DATA(client, buffers_sync),
                                               full_name,
                                               &new_flags);
                    }
                    free (full_name);
                }
            }
        }
        weechat_string_free_split (buffers);
    }

    return WEECHAT_RC_OK;
}

/*
 * Callback for command "desync" (from client).
 *
 * Message looks like:
 *   desync
 *   desync * nicklist
 *   desync irc.freenode.#weechat buffer,nicklist
 */

RELAY_WEECHAT_PROTOCOL_CALLBACK(desync)
{
    char **buffers, **flags, *full_name;
    int num_buffers, num_flags, i, sub_flags, mask, *ptr_old_flags, new_flags;
    int rc;
    long unsigned int value;

    RELAY_WEECHAT_PROTOCOL_MIN_ARGS(0);

    buffers = weechat_string_split ((argc > 0) ? argv[0] : "*", ",", 0, 0,
                                    &num_buffers);
    if (buffers)
    {
        sub_flags = RELAY_WEECHAT_PROTOCOL_SYNC_ALL;
        if (argc > 1)
        {
            sub_flags = 0;
            flags = weechat_string_split (argv[1], ",", 0, 0, &num_flags);
            if (flags)
            {
                for (i = 0; i < num_flags; i++)
                {
                    sub_flags |= relay_weechat_protocol_sync_flag (flags[i]);
                }
                weechat_string_free_split (flags);
            }
        }
        if (sub_flags)
        {
            for (i = 0; i < num_buffers; i++)
            {
                full_name = NULL;
                mask = RELAY_WEECHAT_PROTOCOL_SYNC_FOR_BUFFER;
                if (strncmp (buffers[i], "0x", 2) == 0)
                {
                    rc = sscanf (buffers[i], "%lx", &value);
                    if ((rc != EOF) && (rc != 0))
                    {
                        full_name = strdup (weechat_buffer_get_string ((struct t_gui_buffer *)value,
                                                                       "full_name"));
                    }
                }
                else
                {
                    full_name = strdup (buffers[i]);
                    if (strcmp (buffers[i], "*") == 0)
                        mask = RELAY_WEECHAT_PROTOCOL_SYNC_ALL;
                }

                if (full_name)
                {
                    ptr_old_flags = weechat_hashtable_get (RELAY_WEECHAT_DATA(client, buffers_sync),
                                                           full_name);
                    new_flags = ((ptr_old_flags) ? *ptr_old_flags : 0);
                    new_flags &= ~(sub_flags & mask);
                    if (new_flags)
                    {
                        weechat_hashtable_set (RELAY_WEECHAT_DATA(client, buffers_sync),
                                               full_name,
                                               &new_flags);
                    }
                    else
                    {
                        weechat_hashtable_remove (RELAY_WEECHAT_DATA(client, buffers_sync),
                                                  full_name);
                    }
                    free (full_name);
                }
            }
        }
        weechat_string_free_split (buffers);
    }

    return WEECHAT_RC_OK;
}

/*
 * Callback for command "test" (from client).
 *
 * Message looks like:
 *   test
 */

RELAY_WEECHAT_PROTOCOL_CALLBACK(test)
{
    struct t_relay_weechat_msg *msg;

    RELAY_WEECHAT_PROTOCOL_MIN_ARGS(0);

    msg = relay_weechat_msg_new (id);
    if (msg)
    {
        /* char */
        relay_weechat_msg_add_type (msg, RELAY_WEECHAT_MSG_OBJ_CHAR);
        relay_weechat_msg_add_char (msg, 'A');

        /* integer */
        relay_weechat_msg_add_type (msg, RELAY_WEECHAT_MSG_OBJ_INT);
        relay_weechat_msg_add_int (msg, 123456);

        /* integer (negative) */
        relay_weechat_msg_add_type (msg, RELAY_WEECHAT_MSG_OBJ_INT);
        relay_weechat_msg_add_int (msg, -123456);

        /* long */
        relay_weechat_msg_add_type (msg, RELAY_WEECHAT_MSG_OBJ_LONG);
        relay_weechat_msg_add_long (msg, 1234567890L);

        /* long (negative) */
        relay_weechat_msg_add_type (msg, RELAY_WEECHAT_MSG_OBJ_LONG);
        relay_weechat_msg_add_long (msg, -1234567890L);

        /* string */
        relay_weechat_msg_add_type (msg, RELAY_WEECHAT_MSG_OBJ_STRING);
        relay_weechat_msg_add_string (msg, "a string");

        /* empty string */
        relay_weechat_msg_add_type (msg, RELAY_WEECHAT_MSG_OBJ_STRING);
        relay_weechat_msg_add_string (msg, "");

        /* NULL string */
        relay_weechat_msg_add_type (msg, RELAY_WEECHAT_MSG_OBJ_STRING);
        relay_weechat_msg_add_string (msg, NULL);

        /* buffer */
        relay_weechat_msg_add_type (msg, RELAY_WEECHAT_MSG_OBJ_BUFFER);
        relay_weechat_msg_add_buffer (msg, "buffer", 6);

        /* NULL buffer */
        relay_weechat_msg_add_type (msg, RELAY_WEECHAT_MSG_OBJ_BUFFER);
        relay_weechat_msg_add_buffer (msg, NULL, 0);

        /* pointer */
        relay_weechat_msg_add_type (msg, RELAY_WEECHAT_MSG_OBJ_POINTER);
        relay_weechat_msg_add_pointer (msg, (void *)0x1234abcd);

        /* NULL pointer */
        relay_weechat_msg_add_type (msg, RELAY_WEECHAT_MSG_OBJ_POINTER);
        relay_weechat_msg_add_pointer (msg, NULL);

        /* time */
        relay_weechat_msg_add_type (msg, RELAY_WEECHAT_MSG_OBJ_TIME);
        relay_weechat_msg_add_time (msg, 1321993456);

        /* array of strings: { "abc", "de" } */
        relay_weechat_msg_add_type (msg, RELAY_WEECHAT_MSG_OBJ_ARRAY);
        relay_weechat_msg_add_type (msg, RELAY_WEECHAT_MSG_OBJ_STRING);
        relay_weechat_msg_add_int (msg, 2);
        relay_weechat_msg_add_string (msg, "abc");
        relay_weechat_msg_add_string (msg, "de");

        /* array of integers: { 123, 456, 789 } */
        relay_weechat_msg_add_type (msg, RELAY_WEECHAT_MSG_OBJ_ARRAY);
        relay_weechat_msg_add_type (msg, RELAY_WEECHAT_MSG_OBJ_INT);
        relay_weechat_msg_add_int (msg, 3);
        relay_weechat_msg_add_int (msg, 123);
        relay_weechat_msg_add_int (msg, 456);
        relay_weechat_msg_add_int (msg, 789);

        /* send message */
        relay_weechat_msg_send (client, msg);
        relay_weechat_msg_free (msg);
    }

    return WEECHAT_RC_OK;
}

/*
 * Callback for command "ping" (from client).
 *
 * Message looks like:
 *   ping
 *   ping 1370802127000
 */

RELAY_WEECHAT_PROTOCOL_CALLBACK(ping)
{
    struct t_relay_weechat_msg *msg;

    RELAY_WEECHAT_PROTOCOL_MIN_ARGS(0);

    msg = relay_weechat_msg_new ("_pong");
    if (msg)
    {
        relay_weechat_msg_add_type (msg, RELAY_WEECHAT_MSG_OBJ_STRING);
        relay_weechat_msg_add_string (msg, (argc > 0) ? argv_eol[0] : "");

        /* send message */
        relay_weechat_msg_send (client, msg);
        relay_weechat_msg_free (msg);
    }

    return WEECHAT_RC_OK;
}

/*
 * Callback for command "quit" (from client).
 *
 * Message looks like:
 *   test
 */

RELAY_WEECHAT_PROTOCOL_CALLBACK(quit)
{
    RELAY_WEECHAT_PROTOCOL_MIN_ARGS(0);

    relay_client_set_status (client, RELAY_STATUS_DISCONNECTED);

    return WEECHAT_RC_OK;
}

/*
 * Reads a command from a client.
 */

void
relay_weechat_protocol_recv (struct t_relay_client *client, const char *data)
{
    char *pos, *id, *command, **argv, **argv_eol;
    int i, argc, return_code;
    struct t_relay_weechat_protocol_cb protocol_cb[] =
        { { "init", &relay_weechat_protocol_cb_init },
          { "hdata", &relay_weechat_protocol_cb_hdata },
          { "info", &relay_weechat_protocol_cb_info },
          { "infolist", &relay_weechat_protocol_cb_infolist },
          { "nicklist", &relay_weechat_protocol_cb_nicklist },
          { "input", &relay_weechat_protocol_cb_input },
          { "sync", &relay_weechat_protocol_cb_sync },
          { "desync", &relay_weechat_protocol_cb_desync },
          { "test", &relay_weechat_protocol_cb_test },
          { "ping", &relay_weechat_protocol_cb_ping },
          { "quit", &relay_weechat_protocol_cb_quit },
          { NULL, NULL }
        };

    if (!data || !data[0] || RELAY_CLIENT_HAS_ENDED(client))
        return;

    /* display debug message */
    if (weechat_relay_plugin->debug >= 2)
    {
        weechat_printf (NULL, "%s: recv from client %s%s%s: \"%s\"",
                        RELAY_PLUGIN_NAME,
                        RELAY_COLOR_CHAT_CLIENT,
                        client->desc,
                        RELAY_COLOR_CHAT,
                        data);
    }

    /* extract id */
    id = NULL;
    if (data[0] == '(')
    {
        pos = strchr (data, ')');
        if (pos)
        {
            id = weechat_strndup (data + 1, pos - data - 1);
            data = pos + 1;
            while (data[0] == ' ')
            {
                data++;
            }
        }
    }

    /* search end of data */
    pos = strchr (data, ' ');
    if (pos)
        command = weechat_strndup (data, pos - data);
    else
        command = strdup (data);

    if (!command)
    {
        if (id)
            free (id);
        return;
    }

    argc = 0;
    argv = NULL;
    argv_eol = NULL;

    if (pos)
    {
        while (pos[0] == ' ')
        {
            pos++;
        }
        argv = weechat_string_split (pos, " ", 0, 0, &argc);
        argv_eol = weechat_string_split (pos, " ", 1, 0, NULL);
    }

    for (i = 0; protocol_cb[i].name; i++)
    {
        if (strcmp (protocol_cb[i].name, command) == 0)
        {
            if ((strcmp (protocol_cb[i].name, "init") != 0)
                && (!RELAY_WEECHAT_DATA(client, password_ok)))
            {
                /*
                 * command is not "init" and password is not set?
                 * then close connection!
                 */
                relay_client_set_status (client,
                                         RELAY_STATUS_DISCONNECTED);
            }
            else
            {
                return_code = (int) (protocol_cb[i].cmd_function) (client,
                                                                   id,
                                                                   protocol_cb[i].name,
                                                                   argc,
                                                                   argv,
                                                                   argv_eol);
                if ((weechat_relay_plugin->debug >= 1)
                    && (return_code == WEECHAT_RC_ERROR))
                {
                    weechat_printf (NULL,
                                    _("%s%s: failed to execute command \"%s\" "
                                      "for client %s%s%s"),
                                    weechat_prefix ("error"),
                                    RELAY_PLUGIN_NAME,
                                    command,
                                    RELAY_COLOR_CHAT_CLIENT,
                                    client->desc,
                                    RELAY_COLOR_CHAT);
                }
            }
            break;
        }
    }

    if (id)
        free (id);
    free (command);
    if (argv)
        weechat_string_free_split (argv);
    if (argv_eol)
        weechat_string_free_split (argv_eol);
}
