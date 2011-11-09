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
 * irc-completion.c: completion for IRC commands
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../weechat-plugin.h"
#include "irc.h"
#include "irc-buffer.h"
#include "irc-channel.h"
#include "irc-color.h"
#include "irc-completion.h"
#include "irc-config.h"
#include "irc-ignore.h"
#include "irc-nick.h"
#include "irc-notify.h"
#include "irc-server.h"


/*
 * irc_completion_server_cb: callback for completion with current server
 */

int
irc_completion_server_cb (void *data, const char *completion_item,
                          struct t_gui_buffer *buffer,
                          struct t_gui_completion *completion)
{
    IRC_BUFFER_GET_SERVER(buffer);

    /* make C compiler happy */
    (void) data;
    (void) completion_item;

    if (ptr_server)
    {
        weechat_hook_completion_list_add (completion, ptr_server->name,
                                          0, WEECHAT_LIST_POS_SORT);
    }

    return WEECHAT_RC_OK;
}

/*
 * irc_completion_server_nick_cb: callback for completion with self nick
 *                                of current server
 */

int
irc_completion_server_nick_cb (void *data, const char *completion_item,
                               struct t_gui_buffer *buffer,
                               struct t_gui_completion *completion)
{
    IRC_BUFFER_GET_SERVER(buffer);

    /* make C compiler happy */
    (void) data;
    (void) completion_item;

    if (ptr_server && ptr_server->nick)
    {
        weechat_hook_completion_list_add (completion, ptr_server->nick,
                                          1, WEECHAT_LIST_POS_SORT);
    }

    return WEECHAT_RC_OK;
}

/*
 * irc_completion_server_channels_cb: callback for completion with channels
 *                                    of current server
 */

int
irc_completion_server_channels_cb (void *data, const char *completion_item,
                                   struct t_gui_buffer *buffer,
                                   struct t_gui_completion *completion)
{
    struct t_irc_channel *ptr_channel;

    IRC_BUFFER_GET_SERVER(buffer);

    /* make C compiler happy */
    (void) data;
    (void) completion_item;
    (void) buffer;

    if (ptr_server)
    {
        for (ptr_channel = ptr_server->channels; ptr_channel;
             ptr_channel = ptr_channel->next_channel)
        {
            if (ptr_channel->type == IRC_CHANNEL_TYPE_CHANNEL)
            {
                weechat_hook_completion_list_add (completion, ptr_channel->name,
                                                  0, WEECHAT_LIST_POS_SORT);
            }
        }
    }

    return WEECHAT_RC_OK;
}

/*
 * irc_completion_server_privates_cb: callback for completion with privates
 *                                    of current server
 */

int
irc_completion_server_privates_cb (void *data, const char *completion_item,
                                   struct t_gui_buffer *buffer,
                                   struct t_gui_completion *completion)
{
    struct t_irc_channel *ptr_channel;

    IRC_BUFFER_GET_SERVER(buffer);

    /* make C compiler happy */
    (void) data;
    (void) completion_item;
    (void) buffer;

    if (ptr_server)
    {
        for (ptr_channel = ptr_server->channels; ptr_channel;
             ptr_channel = ptr_channel->next_channel)
        {
            if (ptr_channel->type == IRC_CHANNEL_TYPE_PRIVATE)
            {
                weechat_hook_completion_list_add (completion, ptr_channel->name,
                                                  0, WEECHAT_LIST_POS_SORT);
            }
        }
    }

    return WEECHAT_RC_OK;
}

/*
 * irc_completion_server_nicks_cb: callback for completion with nicks
 *                                 of current server
 */

int
irc_completion_server_nicks_cb (void *data, const char *completion_item,
                                struct t_gui_buffer *buffer,
                                struct t_gui_completion *completion)
{
    struct t_irc_channel *ptr_channel2;
    struct t_irc_nick *ptr_nick;

    IRC_BUFFER_GET_SERVER_CHANNEL(buffer);

    /* make C compiler happy */
    (void) data;
    (void) completion_item;

    if (ptr_server)
    {
        for (ptr_channel2 = ptr_server->channels; ptr_channel2;
             ptr_channel2 = ptr_channel2->next_channel)
        {
            if (ptr_channel2->type == IRC_CHANNEL_TYPE_CHANNEL)
            {
                for (ptr_nick = ptr_channel2->nicks; ptr_nick;
                     ptr_nick = ptr_nick->next_nick)
                {
                    weechat_hook_completion_list_add (completion, ptr_nick->name,
                                                      1, WEECHAT_LIST_POS_SORT);
                }
            }
        }

        /* add self nick at the end */
        weechat_hook_completion_list_add (completion, ptr_server->nick,
                                          1, WEECHAT_LIST_POS_END);
    }

    return WEECHAT_RC_OK;
}

/*
 * irc_completion_servers_cb: callback for completion with servers
 */

int
irc_completion_servers_cb (void *data, const char *completion_item,
                           struct t_gui_buffer *buffer,
                           struct t_gui_completion *completion)
{
    struct t_irc_server *ptr_server;

    /* make C compiler happy */
    (void) data;
    (void) completion_item;
    (void) buffer;

    for (ptr_server = irc_servers; ptr_server;
         ptr_server = ptr_server->next_server)
    {
        weechat_hook_completion_list_add (completion, ptr_server->name,
                                          0, WEECHAT_LIST_POS_SORT);
    }

    return WEECHAT_RC_OK;
}

/*
 * irc_completion_channel_cb: callback for completion with current channel
 */

int
irc_completion_channel_cb (void *data, const char *completion_item,
                           struct t_gui_buffer *buffer,
                           struct t_gui_completion *completion)
{
    IRC_BUFFER_GET_SERVER_CHANNEL(buffer);

    /* make C compiler happy */
    (void) data;
    (void) completion_item;

    if (ptr_channel)
    {
        weechat_hook_completion_list_add (completion, ptr_channel->name,
                                          0, WEECHAT_LIST_POS_SORT);
    }

    return WEECHAT_RC_OK;
}

/*
 * irc_completion_channel_nicks_add_speakers: add recent speakers to completion
 *                                            list
 */

void
irc_completion_channel_nicks_add_speakers (struct t_gui_completion *completion,
                                           struct t_irc_channel *channel,
                                           int highlight)
{
    int list_size, i;
    const char *nick;

    if (channel->nicks_speaking[highlight])
    {
        list_size = weechat_list_size (channel->nicks_speaking[highlight]);
        for (i = 0; i < list_size; i++)
        {
            nick = weechat_list_string (weechat_list_get (channel->nicks_speaking[highlight], i));
            if (nick && irc_nick_search (channel, nick))
            {
                weechat_hook_completion_list_add (completion,
                                                  nick,
                                                  1,
                                                  WEECHAT_LIST_POS_BEGINNING);
            }
        }
    }
}

/*
 * irc_completion_channel_nicks_cb: callback for completion with nicks
 *                                  of current channel
 */

int
irc_completion_channel_nicks_cb (void *data, const char *completion_item,
                                 struct t_gui_buffer *buffer,
                                 struct t_gui_completion *completion)
{
    struct t_irc_nick *ptr_nick;

    IRC_BUFFER_GET_SERVER_CHANNEL(buffer);

    /* make C compiler happy */
    (void) data;
    (void) completion_item;

    if (ptr_channel)
    {
        switch (ptr_channel->type)
        {
            case IRC_CHANNEL_TYPE_CHANNEL:
                for (ptr_nick = ptr_channel->nicks; ptr_nick;
                     ptr_nick = ptr_nick->next_nick)
                {
                    weechat_hook_completion_list_add (completion,
                                                      ptr_nick->name,
                                                      1,
                                                      WEECHAT_LIST_POS_SORT);
                }
                /* add recent speakers on channel */
                if (weechat_config_integer (irc_config_look_nick_completion_smart) == IRC_CONFIG_NICK_COMPLETION_SMART_SPEAKERS)
                {
                    irc_completion_channel_nicks_add_speakers (completion, ptr_channel, 0);
                }
                /* add nicks whose make highlights on me recently on this channel */
                if (weechat_config_integer (irc_config_look_nick_completion_smart) == IRC_CONFIG_NICK_COMPLETION_SMART_SPEAKERS_HIGHLIGHTS)
                {
                    irc_completion_channel_nicks_add_speakers (completion, ptr_channel, 1);
                }
                /* add self nick at the end */
                weechat_hook_completion_list_add (completion,
                                                  ptr_server->nick,
                                                  1,
                                                  WEECHAT_LIST_POS_END);
                break;
            case IRC_CHANNEL_TYPE_PRIVATE:
                /* remote nick */
                weechat_hook_completion_list_add (completion,
                                                  ptr_channel->name,
                                                  1,
                                                  WEECHAT_LIST_POS_SORT);
                /* add self nick at the end */
                weechat_hook_completion_list_add (completion,
                                                  ptr_server->nick,
                                                  1,
                                                  WEECHAT_LIST_POS_END);
                break;
        }
        ptr_channel->nick_completion_reset = 0;
    }

    return WEECHAT_RC_OK;
}

/*
 * irc_completion_channel_nicks_hosts_cb: callback for completion with nicks
 *                                        and hosts of current channel
 */

int
irc_completion_channel_nicks_hosts_cb (void *data, const char *completion_item,
                                       struct t_gui_buffer *buffer,
                                       struct t_gui_completion *completion)
{
    struct t_irc_nick *ptr_nick;
    char *buf;
    int length;

    IRC_BUFFER_GET_SERVER_CHANNEL(buffer);

    /* make C compiler happy */
    (void) data;
    (void) completion_item;

    if (ptr_channel)
    {
        switch (ptr_channel->type)
        {
            case IRC_CHANNEL_TYPE_CHANNEL:
                for (ptr_nick = ptr_channel->nicks; ptr_nick;
                     ptr_nick = ptr_nick->next_nick)
                {
                    weechat_hook_completion_list_add (completion,
                                                      ptr_nick->name,
                                                      1,
                                                      WEECHAT_LIST_POS_SORT);
                    if (ptr_nick->host)
                    {
                        length = strlen (ptr_nick->name) + 1 +
                            strlen (ptr_nick->host) + 1;
                        buf = malloc (length);
                        if (buf)
                        {
                            snprintf (buf, length, "%s!%s",
                                      ptr_nick->name, ptr_nick->host);
                            weechat_hook_completion_list_add (completion,
                                                              buf,
                                                              0,
                                                              WEECHAT_LIST_POS_SORT);
                            free (buf);
                        }
                    }
                }
                break;
            case IRC_CHANNEL_TYPE_PRIVATE:
                weechat_hook_completion_list_add (completion,
                                                  ptr_channel->name,
                                                  1,
                                                  WEECHAT_LIST_POS_SORT);
                break;
        }
    }

    return WEECHAT_RC_OK;
}

/*
 * irc_completion_channel_topic_cb: callback for completion with topic of
 *                                  current channel
 */

int
irc_completion_channel_topic_cb (void *data, const char *completion_item,
                                 struct t_gui_buffer *buffer,
                                 struct t_gui_completion *completion)
{
    char *topic, *topic_color;
    int length;

    IRC_BUFFER_GET_SERVER_CHANNEL(buffer);

    /* make C compiler happy */
    (void) data;
    (void) completion_item;

    if (ptr_channel && ptr_channel->topic && ptr_channel->topic[0])
    {
        if (weechat_strncasecmp (ptr_channel->topic, ptr_channel->name,
                                 strlen (ptr_channel->name)) == 0)
        {
            /*
             * if topic starts with channel name, add another channel name
             * before topic, so that completion will be:
             *   /topic #test #test is a test channel
             * instead of
             *   /topic #test is a test channel
             */
            length = strlen (ptr_channel->name) + strlen (ptr_channel->topic) + 16;
            topic = malloc (length + 1);
            if (topic)
            {
                snprintf (topic, length, "%s %s",
                          ptr_channel->name, ptr_channel->topic);
            }
        }
        else
            topic = strdup (ptr_channel->topic);

        topic_color = irc_color_decode_for_user_entry ((topic) ? topic : ptr_channel->topic);
        weechat_hook_completion_list_add (completion,
                                          (topic_color) ? topic_color : ((topic) ? topic : ptr_channel->topic),
                                          0, WEECHAT_LIST_POS_SORT);
        if (topic_color)
            free (topic_color);
        if (topic)
            free (topic);
    }

    return WEECHAT_RC_OK;
}

/*
 * irc_completion_channels_cb: callback for completion with channels
 *                             of all servers
 */

int
irc_completion_channels_cb (void *data, const char *completion_item,
                            struct t_gui_buffer *buffer,
                            struct t_gui_completion *completion)
{
    struct t_irc_server *ptr_server;
    struct t_irc_channel *ptr_channel;

    /* make C compiler happy */
    (void) data;
    (void) completion_item;
    (void) buffer;

    for (ptr_server = irc_servers; ptr_server;
         ptr_server = ptr_server->next_server)
    {
        for (ptr_channel = ptr_server->channels; ptr_channel;
             ptr_channel = ptr_channel->next_channel)
        {
            if (ptr_channel->type == IRC_CHANNEL_TYPE_CHANNEL)
            {
                weechat_hook_completion_list_add (completion, ptr_channel->name,
                                                  0, WEECHAT_LIST_POS_SORT);
            }
        }
    }

    return WEECHAT_RC_OK;
}

/*
 * irc_completion_privates_cb: callback for completion with privates
 *                             of all servers
 */

int
irc_completion_privates_cb (void *data, const char *completion_item,
                            struct t_gui_buffer *buffer,
                            struct t_gui_completion *completion)
{
    struct t_irc_server *ptr_server;
    struct t_irc_channel *ptr_channel;

    /* make C compiler happy */
    (void) data;
    (void) completion_item;
    (void) buffer;

    for (ptr_server = irc_servers; ptr_server;
         ptr_server = ptr_server->next_server)
    {
        for (ptr_channel = ptr_server->channels; ptr_channel;
             ptr_channel = ptr_channel->next_channel)
        {
            if (ptr_channel->type == IRC_CHANNEL_TYPE_PRIVATE)
            {
                weechat_hook_completion_list_add (completion, ptr_channel->name,
                                                  0, WEECHAT_LIST_POS_SORT);
            }
        }
    }

    return WEECHAT_RC_OK;
}

/*
 * irc_completion_msg_part_cb: callback for completion with default part message
 */

int
irc_completion_msg_part_cb (void *data, const char *completion_item,
                            struct t_gui_buffer *buffer,
                            struct t_gui_completion *completion)
{
    const char *msg_part;

    IRC_BUFFER_GET_SERVER(buffer);

    /* make C compiler happy */
    (void) data;
    (void) completion_item;

    if (ptr_server)
    {
        msg_part = IRC_SERVER_OPTION_STRING(ptr_server,
                                            IRC_SERVER_OPTION_DEFAULT_MSG_PART);
        if (msg_part && msg_part[0])
        {
            weechat_hook_completion_list_add (completion, msg_part,
                                              0, WEECHAT_LIST_POS_SORT);
        }
    }

    return WEECHAT_RC_OK;
}

/*
 * irc_completion_ignores_numbers_cb: callback for completion with ignores
 *                                    numbers
 */

int
irc_completion_ignores_numbers_cb (void *data, const char *completion_item,
                                   struct t_gui_buffer *buffer,
                                   struct t_gui_completion *completion)
{
    struct t_irc_ignore *ptr_ignore;
    char str_number[32];

    /* make C compiler happy */
    (void) data;
    (void) completion_item;
    (void) buffer;

    for (ptr_ignore = irc_ignore_list; ptr_ignore;
         ptr_ignore = ptr_ignore->next_ignore)
    {
        snprintf (str_number, sizeof (str_number), "%d", ptr_ignore->number);
        weechat_hook_completion_list_add (completion, str_number,
                                          0, WEECHAT_LIST_POS_END);
    }

    return WEECHAT_RC_OK;
}

/*
 * irc_completion_notify_nicks_cb: callback for completion with nicks in notify
 *                                 list
 */

int
irc_completion_notify_nicks_cb (void *data, const char *completion_item,
                                struct t_gui_buffer *buffer,
                                struct t_gui_completion *completion)
{
    struct t_irc_notify *ptr_notify;

    IRC_BUFFER_GET_SERVER(buffer);

    /* make C compiler happy */
    (void) data;
    (void) completion_item;

    if (ptr_server)
    {
        for (ptr_notify = ptr_server->notify_list; ptr_notify;
             ptr_notify = ptr_notify->next_notify)
        {
            weechat_hook_completion_list_add (completion, ptr_notify->nick,
                                              0, WEECHAT_LIST_POS_SORT);
        }
    }
    else
    {
        for (ptr_server = irc_servers; ptr_server;
             ptr_server = ptr_server->next_server)
        {
            for (ptr_notify = ptr_server->notify_list; ptr_notify;
                 ptr_notify = ptr_notify->next_notify)
            {
                weechat_hook_completion_list_add (completion, ptr_notify->nick,
                                                  0, WEECHAT_LIST_POS_SORT);
            }
        }
    }

    return WEECHAT_RC_OK;
}

/*
 * irc_completion_init: init completion for IRC plugin
 */

void
irc_completion_init ()
{
    weechat_hook_completion ("irc_server",
                             N_("current IRC server"),
                             &irc_completion_server_cb, NULL);
    weechat_hook_completion ("irc_server_nick",
                             N_("nick on current IRC server"),
                             &irc_completion_server_nick_cb, NULL);
    weechat_hook_completion ("irc_server_channels",
                             N_("channels on current IRC server"),
                             &irc_completion_server_channels_cb, NULL);
    weechat_hook_completion ("irc_server_privates",
                             N_("privates on current IRC server"),
                             &irc_completion_server_privates_cb, NULL);
    weechat_hook_completion ("irc_server_nicks",
                             N_("nicks on all channels of current IRC server"),
                             &irc_completion_server_nicks_cb, NULL);
    weechat_hook_completion ("irc_servers",
                             N_("IRC servers (internal names)"),
                             &irc_completion_servers_cb, NULL);
    weechat_hook_completion ("irc_channel",
                             N_("current IRC channel"),
                             &irc_completion_channel_cb, NULL);
    weechat_hook_completion ("nick",
                             N_("nicks of current IRC channel"),
                             &irc_completion_channel_nicks_cb, NULL);
    weechat_hook_completion ("irc_channel_nicks_hosts",
                             N_("nicks and hostnames of current IRC channel"),
                             &irc_completion_channel_nicks_hosts_cb, NULL);
    weechat_hook_completion ("irc_channel_topic",
                             N_("topic of current IRC channel"),
                             &irc_completion_channel_topic_cb, NULL);
    weechat_hook_completion ("irc_channels",
                             N_("channels on all IRC servers"),
                             &irc_completion_channels_cb, NULL);
    weechat_hook_completion ("irc_privates",
                             N_("privates on all IRC servers"),
                             &irc_completion_privates_cb, NULL);
    weechat_hook_completion ("irc_msg_part",
                             N_("default part message for IRC channel"),
                             &irc_completion_msg_part_cb, NULL);
    weechat_hook_completion ("irc_ignores_numbers",
                             N_("numbers for defined ignores"),
                             &irc_completion_ignores_numbers_cb, NULL);
    weechat_hook_completion ("irc_notify_nicks",
                             N_("nicks in notify list"),
                             &irc_completion_notify_nicks_cb, NULL);
}
