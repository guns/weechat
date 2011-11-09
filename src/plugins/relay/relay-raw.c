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
 * relay-raw.c: functions for Relay raw data messages
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "../weechat-plugin.h"
#include "relay.h"
#include "relay-raw.h"
#include "relay-buffer.h"
#include "relay-client.h"
#include "relay-config.h"


struct t_gui_buffer *relay_raw_buffer = NULL;

int relay_raw_messages_count = 0;
struct t_relay_raw_message *relay_raw_messages = NULL;
struct t_relay_raw_message *last_relay_raw_message = NULL;


/*
 * relay_raw_message_print: print a relay raw message
 */

void
relay_raw_message_print (struct t_relay_raw_message *raw_message)
{
    if (relay_raw_buffer && raw_message)
    {
        weechat_printf_date_tags (relay_raw_buffer,
                                  raw_message->date, NULL,
                                  "%s\t%s",
                                  raw_message->prefix,
                                  raw_message->message);
    }
}

/*
 * relay_raw_open: open Relay raw buffer
 */

void
relay_raw_open (int switch_to_buffer)
{
    struct t_relay_raw_message *ptr_raw_message;

    if (!relay_raw_buffer)
    {
        relay_raw_buffer = weechat_buffer_search (RELAY_PLUGIN_NAME,
                                                  RELAY_RAW_BUFFER_NAME);
        if (!relay_raw_buffer)
        {
            relay_raw_buffer = weechat_buffer_new (RELAY_RAW_BUFFER_NAME,
                                                   &relay_buffer_input_cb, NULL,
                                                   &relay_buffer_close_cb, NULL);

            /* failed to create buffer ? then return */
            if (!relay_raw_buffer)
                return;

            weechat_buffer_set (relay_raw_buffer,
                                "title", _("Relay raw messages"));

            if (!weechat_buffer_get_integer (relay_raw_buffer, "short_name_is_set"))
            {
                weechat_buffer_set (relay_raw_buffer, "short_name",
                                    RELAY_RAW_BUFFER_NAME);
            }
            weechat_buffer_set (relay_raw_buffer, "localvar_set_type", "debug");
            weechat_buffer_set (relay_raw_buffer, "localvar_set_server", RELAY_RAW_BUFFER_NAME);
            weechat_buffer_set (relay_raw_buffer, "localvar_set_channel", RELAY_RAW_BUFFER_NAME);
            weechat_buffer_set (relay_raw_buffer, "localvar_set_no_log", "1");

            /* disable all highlights on this buffer */
            weechat_buffer_set (relay_raw_buffer, "highlight_words", "-");

            /* print messages in list */
            for (ptr_raw_message = relay_raw_messages; ptr_raw_message;
                 ptr_raw_message = ptr_raw_message->next_message)
            {
                relay_raw_message_print (ptr_raw_message);
            }
        }
    }

    if (relay_raw_buffer && switch_to_buffer)
        weechat_buffer_set (relay_raw_buffer, "display", "1");
}

/*
 * relay_raw_message_free: free a raw message and remove it from list
 */

void
relay_raw_message_free (struct t_relay_raw_message *raw_message)
{
    struct t_relay_raw_message *new_raw_messages;

    /* remove message from raw messages list */
    if (last_relay_raw_message == raw_message)
        last_relay_raw_message = raw_message->prev_message;
    if (raw_message->prev_message)
    {
        (raw_message->prev_message)->next_message = raw_message->next_message;
        new_raw_messages = relay_raw_messages;
    }
    else
        new_raw_messages = raw_message->next_message;

    if (raw_message->next_message)
        (raw_message->next_message)->prev_message = raw_message->prev_message;

    /* free data */
    if (raw_message->prefix)
        free (raw_message->prefix);
    if (raw_message->message)
        free (raw_message->message);

    free (raw_message);

    relay_raw_messages = new_raw_messages;

    relay_raw_messages_count--;
}

/*
 * relay_raw_message_free_all: free all raw messages
 */

void
relay_raw_message_free_all ()
{
    while (relay_raw_messages)
    {
        relay_raw_message_free (relay_raw_messages);
    }
}

/*
 * relay_raw_message_remove_old: remove old raw messages if limit has been
 *                               reached
 */

void
relay_raw_message_remove_old ()
{
    int max_messages;

    max_messages = weechat_config_integer (relay_config_look_raw_messages);
    while (relay_raw_messages && (relay_raw_messages_count >= max_messages))
    {
        relay_raw_message_free (relay_raw_messages);
    }
}

/*
 * relay_raw_message_add_to_list: add new message to list
 */

struct t_relay_raw_message *
relay_raw_message_add_to_list (time_t date, const char *prefix,
                               const char *message)
{
    struct t_relay_raw_message *new_raw_message;

    if (!prefix || !message)
        return NULL;

    relay_raw_message_remove_old ();

    new_raw_message = malloc (sizeof (*new_raw_message));
    if (new_raw_message)
    {
        new_raw_message->date = date;
        new_raw_message->prefix = strdup (prefix);
        new_raw_message->message = strdup (message);

        /* add message to list */
        new_raw_message->prev_message = last_relay_raw_message;
        new_raw_message->next_message = NULL;
        if (relay_raw_messages)
            last_relay_raw_message->next_message = new_raw_message;
        else
            relay_raw_messages = new_raw_message;
        last_relay_raw_message = new_raw_message;

        relay_raw_messages_count++;
    }

    return new_raw_message;
}

/*
 * relay_raw_message_add: add new message to list
 */

struct t_relay_raw_message *
relay_raw_message_add (struct t_relay_client *client, int send,
                       const char *message)
{
    char *buf, *buf2, prefix[256];
    const unsigned char *ptr_buf;
    const char *hexa = "0123456789ABCDEF";
    int pos_buf, pos_buf2, char_size, i;
    struct t_relay_raw_message *new_raw_message;

    buf = weechat_iconv_to_internal (NULL, message);
    buf2 = malloc ((strlen (buf) * 3) + 1);
    if (buf2)
    {
        ptr_buf = (buf) ? (unsigned char *)buf : (unsigned char *)message;
        pos_buf = 0;
        pos_buf2 = 0;
        while (ptr_buf[pos_buf])
        {
            if (ptr_buf[pos_buf] < 32)
            {
                buf2[pos_buf2++] = '\\';
                buf2[pos_buf2++] = hexa[ptr_buf[pos_buf] / 16];
                buf2[pos_buf2++] = hexa[ptr_buf[pos_buf] % 16];
                pos_buf++;
            }
            else
            {
                char_size = weechat_utf8_char_size ((const char *)(ptr_buf + pos_buf));
                for (i = 0; i < char_size; i++)
                {
                    buf2[pos_buf2++] = ptr_buf[pos_buf++];
                }
            }
        }
        buf2[pos_buf2] = '\0';
    }

    if (client)
    {
        snprintf (prefix, sizeof (prefix), "%s[%s%d%s] %s%s %s%s",
                  weechat_color ("chat_delimiters"),
                  weechat_color ("chat"),
                  client->id,
                  weechat_color ("chat_delimiters"),
                  weechat_color ("chat_server"),
                  client->protocol_args,
                  (send) ?
                  weechat_color ("chat_prefix_quit") :
                  weechat_color ("chat_prefix_join"),
                  (send) ? RELAY_RAW_PREFIX_SEND : RELAY_RAW_PREFIX_RECV);
    }
    else
    {
        snprintf (prefix, sizeof (prefix), "%s%s",
                  (send) ?
                  weechat_color ("chat_prefix_quit") :
                  weechat_color ("chat_prefix_join"),
                  (send) ? RELAY_RAW_PREFIX_SEND : RELAY_RAW_PREFIX_RECV);
    }

    new_raw_message = relay_raw_message_add_to_list (time (NULL),
                                                     prefix,
                                                     (buf2) ? buf2 : ((buf) ? buf : message));

    if (buf)
        free (buf);
    if (buf2)
        free (buf2);

    return new_raw_message;
}

/*
 * relay_raw_print: print a message on Relay raw buffer
 */

void
relay_raw_print (struct t_relay_client *client, int send, const char *message)
{
    struct t_relay_raw_message *new_raw_message;

    if (!message)
        return;

    /* auto-open Relay raw buffer if debug for irc plugin is >= 1 */
    if (!relay_raw_buffer && (weechat_relay_plugin->debug >= 1))
        relay_raw_open (0);

    new_raw_message = relay_raw_message_add (client, send, message);
    if (new_raw_message)
    {
        if (relay_raw_buffer)
            relay_raw_message_print (new_raw_message);
        if (weechat_config_integer (relay_config_look_raw_messages) == 0)
            relay_raw_message_free (new_raw_message);
    }
}

/*
 * relay_raw_add_to_infolist: add a raw message in an infolist
 *                            return 1 if ok, 0 if error
 */

int
relay_raw_add_to_infolist (struct t_infolist *infolist,
                           struct t_relay_raw_message *raw_message)
{
    struct t_infolist_item *ptr_item;

    if (!infolist || !raw_message)
        return 0;

    ptr_item = weechat_infolist_new_item (infolist);
    if (!ptr_item)
        return 0;

    if (!weechat_infolist_new_var_time (ptr_item, "date", raw_message->date))
        return 0;
    if (!weechat_infolist_new_var_string (ptr_item, "prefix", raw_message->prefix))
        return 0;
    if (!weechat_infolist_new_var_string (ptr_item, "message", raw_message->message))
        return 0;

    return 1;
}
