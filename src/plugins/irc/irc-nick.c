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
 * irc-nick.c: nick management for IRC plugin
 */

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "../weechat-plugin.h"
#include "irc.h"
#include "irc-nick.h"
#include "irc-color.h"
#include "irc-config.h"
#include "irc-mode.h"
#include "irc-server.h"
#include "irc-channel.h"


/*
 * irc_nick_valid: check if a nick pointer exists for a channel
 *                 return 1 if nick exists
 *                        0 if nick is not found
 */

int
irc_nick_valid (struct t_irc_channel *channel, struct t_irc_nick *nick)
{
    struct t_irc_nick *ptr_nick;

    if (!channel)
        return 0;

    for (ptr_nick = channel->nicks; ptr_nick; ptr_nick = ptr_nick->next_nick)
    {
        if (ptr_nick == nick)
            return 1;
    }

    /* nick not found */
    return 0;
}

/*
 * irc_nick_is_nick: check if string is a valid nick string (RFC 1459)
 *                   return 1 if string valid nick
 *                          0 if not a valid nick
 */

int
irc_nick_is_nick (const char *string)
{
    const char *ptr;

    if (!string || !string[0])
        return 0;

    /* first char must not be a number or hyphen */
    ptr = string;
    if (strchr ("0123456789-", *ptr))
        return 0;

    while (ptr && ptr[0])
    {
        if (!strchr (IRC_NICK_VALID_CHARS, *ptr))
            return 0;
        ptr++;
    }

    return 1;
}

/*
 * irc_nick_strdup_for_color: duplicate a nick and stop at first char in list
 *                            (using option irc.look.nick_color_stop_chars)
 */

char *
irc_nick_strdup_for_color (const char *nickname)
{
    int char_size, other_char_seen;
    char *result, *pos, utf_char[16];

    result = malloc (strlen (nickname) + 1);
    pos = result;
    other_char_seen = 0;
    while (nickname[0])
    {
        char_size = weechat_utf8_char_size (nickname);
        memcpy (utf_char, nickname, char_size);
        utf_char[char_size] = '\0';

        if (strstr (weechat_config_string (irc_config_look_nick_color_stop_chars),
                    utf_char))
        {
            if (other_char_seen)
            {
                pos[0] = '\0';
                return result;
            }
        }
        else
        {
            other_char_seen = 1;
        }
        memcpy (pos, utf_char, char_size);
        pos += char_size;

        nickname += char_size;
    }
    pos[0] = '\0';
    return result;
}

/*
 * irc_nick_hash_color: hash a nickname to find color
 */

int
irc_nick_hash_color (const char *nickname)
{
    int color;
    const char *ptr_nick;

    if (!irc_config_nick_colors)
        irc_config_set_nick_colors ();

    if (irc_config_num_nick_colors == 0)
        return 0;

    color = 0;
    ptr_nick = nickname;
    while (ptr_nick && ptr_nick[0])
    {
        color += weechat_utf8_char_int (ptr_nick);
        ptr_nick = weechat_utf8_next_char (ptr_nick);
    }

    return (color % irc_config_num_nick_colors);
}

/*
 * irc_nick_find_color: find a color code for a nick
 *                      (according to nick letters)
 */

const char *
irc_nick_find_color (const char *nickname)
{
    int color;
    char *nickname2;
    const char *forced_color, *str_color;

    if (!irc_config_nick_colors)
        irc_config_set_nick_colors ();

    if (irc_config_num_nick_colors == 0)
        return weechat_color ("default");

    nickname2 = irc_nick_strdup_for_color (nickname);

    /* look if color is forced */
    forced_color = weechat_hashtable_get (irc_config_hashtable_nick_color_force,
                                          (nickname2) ? nickname2 : nickname);
    if (forced_color)
    {
        forced_color = weechat_color (forced_color);
        if (forced_color && forced_color[0])
        {
            if (nickname2)
                free (nickname2);
            return forced_color;
        }
    }

    /* hash nickname to get color */
    color = irc_nick_hash_color ((nickname2) ? nickname2 : nickname);
    if (nickname2)
        free (nickname2);

    /* return color */
    str_color = weechat_color (irc_config_nick_colors[color]);
    return (str_color[0]) ? str_color : weechat_color("default");
}

/*
 * irc_nick_find_color_name: find a color name for a nick
 *                           (according to nick letters)
 */

const char *
irc_nick_find_color_name (const char *nickname)
{
    int color;
    char *nickname2;
    const char *forced_color;
    static char *default_color = "default";

    if (!irc_config_nick_colors)
        irc_config_set_nick_colors ();

    if (irc_config_num_nick_colors == 0)
        return default_color;

    nickname2 = irc_nick_strdup_for_color (nickname);

    /* look if color is forced */
    forced_color = weechat_hashtable_get (irc_config_hashtable_nick_color_force,
                                          (nickname2) ? nickname2 : nickname);
    if (forced_color)
    {
        if (nickname2)
            free (nickname2);
        return forced_color;
    }

    /* hash nickname to get color */
    color = irc_nick_hash_color ((nickname2) ? nickname2 : nickname);
    if (nickname2)
        free (nickname2);

    /* return color name */
    return irc_config_nick_colors[color];
}

/*
 * irc_nick_set_current_prefix: set current prefix, using higher prefix set in
 *                              prefixes
 */

void
irc_nick_set_current_prefix (struct t_irc_nick *nick)
{
    char *ptr_prefixes;

    nick->prefix[0] = ' ';
    for (ptr_prefixes = nick->prefixes; ptr_prefixes[0]; ptr_prefixes++)
    {
        if (ptr_prefixes[0] != ' ')
        {
            nick->prefix[0] = ptr_prefixes[0];
            break;
        }
    }
}

/*
 * irc_nick_set_prefix: set or unset a prefix in prefixes
 *                      if set == 1, prefix is set (prefix is used)
 *                                0, prefix is unset (space is used)
 */

void
irc_nick_set_prefix (struct t_irc_server *server, struct t_irc_nick *nick,
                     int set, char prefix)
{
    int index;

    index = irc_server_get_prefix_char_index (server, prefix);
    if (index >= 0)
    {
        nick->prefixes[index] = (set) ? prefix : ' ';
        irc_nick_set_current_prefix (nick);
    }
}

/*
 * irc_nick_set_prefixes: set prefixes for nick
 */

void
irc_nick_set_prefixes (struct t_irc_server *server, struct t_irc_nick *nick,
                       const char *prefixes)
{
    const char *ptr_prefixes;

    /* reset all prefixes in nick */
    memset (nick->prefixes, ' ', strlen (nick->prefixes));

    /* add prefixes in nick */
    if (prefixes)
    {
        for (ptr_prefixes = prefixes; ptr_prefixes[0]; ptr_prefixes++)
        {
            irc_nick_set_prefix (server, nick, 1, ptr_prefixes[0]);
        }
    }

    /* set current prefix */
    irc_nick_set_current_prefix (nick);
}

/*
 * irc_nick_is_op: return 1 is nick is "op" (or better than "op", for example
 *                 channel admin or channel owner)
 */

int
irc_nick_is_op (struct t_irc_server *server, struct t_irc_nick *nick)
{
    int index;

    if (nick->prefix[0] == ' ')
        return 0;

    index = irc_server_get_prefix_char_index (server, nick->prefix[0]);
    if (index < 0)
        return 0;

    return (index <= irc_server_get_prefix_mode_index (server, 'o')) ? 1 : 0;
}

/*
 * irc_nick_has_prefix_mode: return 1 if nick prefixes contains prefix
 *                           for a mode given
 *                           for example if mode is 'o', we'll search for '@'
 *                           in nick prefixes
 */

int
irc_nick_has_prefix_mode (struct t_irc_server *server, struct t_irc_nick *nick,
                          char prefix_mode)
{
    char prefix_char;

    prefix_char = irc_server_get_prefix_char_for_mode (server, prefix_mode);
    if (prefix_char == ' ')
        return 0;

    return (strchr (nick->prefixes, prefix_char)) ? 1 : 0;
}

/*
 * irc_nick_get_nicklist_group: get nicklist group for a nick
 */

struct t_gui_nick_group *
irc_nick_get_nicklist_group (struct t_irc_server *server,
                             struct t_gui_buffer *buffer,
                             struct t_irc_nick *nick)
{
    int index;
    char str_group[2];
    const char *prefix_modes;
    struct t_gui_nick_group *ptr_group;

    if (!server || !buffer || !nick)
        return NULL;

    ptr_group = NULL;
    index = irc_server_get_prefix_char_index (server, nick->prefix[0]);
    if (index < 0)
    {
        ptr_group = weechat_nicklist_search_group (buffer, NULL,
                                                   IRC_NICK_GROUP_OTHER_NAME);
    }
    else
    {
        prefix_modes = irc_server_get_prefix_modes (server);
        str_group[0] = prefix_modes[index];
        str_group[1] = '\0';
        ptr_group = weechat_nicklist_search_group (buffer, NULL, str_group);
    }

    return ptr_group;
}

/*
 * irc_nick_get_prefix_color_name: return name of prefix color for a nick
 */

const char *
irc_nick_get_prefix_color_name (struct t_irc_server *server,
                                struct t_irc_nick *nick)
{
    static char *default_color = "";
    const char *prefix_modes, *color;
    char mode[2];
    int i, index;

    if (irc_config_hashtable_nick_prefixes)
    {
        index = irc_server_get_prefix_char_index (server, nick->prefix[0]);
        if (index >= 0)
        {
            mode[0] = ' ';
            mode[1] = '\0';
            prefix_modes = irc_server_get_prefix_modes (server);
            for (i = index; prefix_modes[i]; i++)
            {
                mode[0] = prefix_modes[i];
                color = weechat_hashtable_get (irc_config_hashtable_nick_prefixes,
                                               mode);
                if (color)
                    return color;
            }
            /*
             * no color found with mode (and following modes)?
             * => fallback to "*"
             */
            mode[0] = '*';
            color = weechat_hashtable_get (irc_config_hashtable_nick_prefixes,
                                           mode);
            if (color)
                return color;
        }
    }

    /* no color by default */
    return default_color;
}

/*
 * irc_nick_get_color_for_nicklist: get nick color for nicklist
 */

const char *
irc_nick_get_color_for_nicklist (struct t_irc_server *server,
                                 struct t_irc_nick *nick)
{
    static char *nick_color_bar_fg = "bar_fg";
    static char *nick_color_self = "weechat.color.chat_nick_self";
    static char *nick_color_away = "weechat.color.nicklist_away";

    if (nick->away)
        return nick_color_away;

    if (weechat_config_boolean(irc_config_look_color_nicks_in_nicklist))
    {
        if (weechat_strcasecmp (nick->name, server->nick) == 0)
            return nick_color_self;
        else
            return irc_nick_find_color_name (nick->name);
    }

    return nick_color_bar_fg;
}

/*
 * irc_nick_nicklist_add: add nick to buffer nicklist
 */

void
irc_nick_nicklist_add (struct t_irc_server *server,
                       struct t_irc_channel *channel,
                       struct t_irc_nick *nick)
{
    struct t_gui_nick_group *ptr_group;

    ptr_group = irc_nick_get_nicklist_group (server, channel->buffer, nick);
    weechat_nicklist_add_nick (channel->buffer, ptr_group,
                               nick->name,
                               irc_nick_get_color_for_nicklist (server, nick),
                               nick->prefix,
                               irc_nick_get_prefix_color_name (server, nick),
                               1);
}

/*
 * irc_nick_nicklist_remove: remove nick from buffer nicklist
 */

void
irc_nick_nicklist_remove (struct t_irc_server *server,
                          struct t_irc_channel *channel,
                          struct t_irc_nick *nick)
{
    struct t_gui_nick_group *ptr_group;

    ptr_group = irc_nick_get_nicklist_group (server, channel->buffer, nick);
    weechat_nicklist_remove_nick (channel->buffer,
                                  weechat_nicklist_search_nick (channel->buffer,
                                                                ptr_group,
                                                                nick->name));
}

/*
 * irc_nick_nicklist_set: set a property for nick in buffer nicklist
 */

void
irc_nick_nicklist_set (struct t_irc_channel *channel,
                       struct t_irc_nick *nick,
                       const char *property, const char *value)
{
    struct t_gui_nick *ptr_nick;

    ptr_nick = weechat_nicklist_search_nick (channel->buffer, NULL, nick->name);
    if (ptr_nick)
    {
        weechat_nicklist_nick_set (channel->buffer, ptr_nick, property, value);
    }
}

/*
 * irc_nick_nicklist_set_prefix_color_all: set nick prefix colors in nicklist
 *                                         for all servers/channels
 */

void
irc_nick_nicklist_set_prefix_color_all ()
{
    struct t_irc_server *ptr_server;
    struct t_irc_channel *ptr_channel;
    struct t_irc_nick *ptr_nick;

    for (ptr_server = irc_servers; ptr_server;
         ptr_server = ptr_server->next_server)
    {
        for (ptr_channel = ptr_server->channels; ptr_channel;
             ptr_channel = ptr_channel->next_channel)
        {
            for (ptr_nick = ptr_channel->nicks; ptr_nick;
                 ptr_nick = ptr_nick->next_nick)
            {
                irc_nick_nicklist_set (ptr_channel, ptr_nick, "prefix_color",
                                       irc_nick_get_prefix_color_name (ptr_server,
                                                                       ptr_nick));
            }
        }
    }
}

/*
 * irc_nick_nicklist_set_color_all: set nick colors in nicklist for all
 *                                  servers/channels
 */

void
irc_nick_nicklist_set_color_all ()
{
    struct t_irc_server *ptr_server;
    struct t_irc_channel *ptr_channel;
    struct t_irc_nick *ptr_nick;

    for (ptr_server = irc_servers; ptr_server;
         ptr_server = ptr_server->next_server)
    {
        for (ptr_channel = ptr_server->channels; ptr_channel;
             ptr_channel = ptr_channel->next_channel)
        {
            for (ptr_nick = ptr_channel->nicks; ptr_nick;
                 ptr_nick = ptr_nick->next_nick)
            {
                irc_nick_nicklist_set (ptr_channel, ptr_nick, "color",
                                       irc_nick_get_color_for_nicklist (ptr_server,
                                                                        ptr_nick));
            }
        }
    }
}

/*
 * irc_nick_new: allocate a new nick for a channel and add it to the nick list
 */

struct t_irc_nick *
irc_nick_new (struct t_irc_server *server, struct t_irc_channel *channel,
              const char *nickname, const char *prefixes, int away)
{
    struct t_irc_nick *new_nick, *ptr_nick;
    int length;

    /* nick already exists on this channel? */
    ptr_nick = irc_nick_search (channel, nickname);
    if (ptr_nick)
    {
        /* remove old nick from nicklist */
        irc_nick_nicklist_remove (server, channel, ptr_nick);

        /* update nick */
        irc_nick_set_prefixes (server, ptr_nick, prefixes);
        ptr_nick->away = away;

        /* add new nick in nicklist */
        irc_nick_nicklist_add (server, channel, ptr_nick);

        return ptr_nick;
    }

    /* alloc memory for new nick */
    if ((new_nick = malloc (sizeof (*new_nick))) == NULL)
        return NULL;

    /* initialize new nick */
    new_nick->name = strdup (nickname);
    new_nick->host = NULL;
    length = strlen (irc_server_get_prefix_chars (server));
    new_nick->prefixes = malloc (length + 1);
    if (new_nick->prefixes)
    {
        memset (new_nick->prefixes, ' ', length);
        new_nick->prefixes[length] = '\0';
    }
    new_nick->prefix[0] = ' ';
    new_nick->prefix[1] = '\0';
    irc_nick_set_prefixes (server, new_nick, prefixes);
    new_nick->away = away;
    if (weechat_strcasecmp (new_nick->name, server->nick) == 0)
        new_nick->color = strdup (IRC_COLOR_CHAT_NICK_SELF);
    else
        new_nick->color = strdup (irc_nick_find_color (new_nick->name));

    /* add nick to end of list */
    new_nick->prev_nick = channel->last_nick;
    if (channel->nicks)
        channel->last_nick->next_nick = new_nick;
    else
        channel->nicks = new_nick;
    channel->last_nick = new_nick;
    new_nick->next_nick = NULL;

    channel->nicks_count++;

    channel->nick_completion_reset = 1;

    /* add nick to buffer nicklist */
    irc_nick_nicklist_add (server, channel, new_nick);

    /* all is ok, return address of new nick */
    return new_nick;
}

/*
 * irc_nick_change: change nickname
 */

void
irc_nick_change (struct t_irc_server *server, struct t_irc_channel *channel,
                 struct t_irc_nick *nick, const char *new_nick)
{
    int nick_is_me;

    /* remove nick from nicklist */
    irc_nick_nicklist_remove (server, channel, nick);

    /* update nicks speaking */
    nick_is_me = (strcmp (nick->name, server->nick) == 0) ? 1 : 0;
    if (!nick_is_me)
        irc_channel_nick_speaking_rename (channel, nick->name, new_nick);

    /* change nickname */
    if (nick->name)
        free (nick->name);
    nick->name = strdup (new_nick);
    if (nick->color)
        free (nick->color);
    if (nick_is_me)
        nick->color = strdup (IRC_COLOR_CHAT_NICK_SELF);
    else
        nick->color = strdup (irc_nick_find_color (nick->name));

    /* add nick in nicklist */
    irc_nick_nicklist_add (server, channel, nick);
}

/*
 * irc_nick_set_mode: set a mode for a nick
 */

void
irc_nick_set_mode (struct t_irc_server *server, struct t_irc_channel *channel,
                   struct t_irc_nick *nick, int set, char mode)
{
    int index;
    const char *prefix_chars;

    index = irc_server_get_prefix_mode_index (server, mode);
    if (index < 0)
        return;

    /* remove nick from nicklist */
    irc_nick_nicklist_remove (server, channel, nick);

    /* set flag */
    prefix_chars = irc_server_get_prefix_chars (server);
    irc_nick_set_prefix (server, nick, set, prefix_chars[index]);

    /* add nick in nicklist */
    irc_nick_nicklist_add (server, channel, nick);

    if (strcmp (nick->name, server->nick) == 0)
        weechat_bar_item_update ("input_prompt");
}

/*
 * irc_nick_free: free a nick and remove it from nicks list
 */

void
irc_nick_free (struct t_irc_server *server, struct t_irc_channel *channel,
               struct t_irc_nick *nick)
{
    struct t_irc_nick *new_nicks;

    if (!channel || !nick)
        return;

    /* remove nick from nicklist */
    irc_nick_nicklist_remove (server, channel, nick);

    /* remove nick */
    if (channel->last_nick == nick)
        channel->last_nick = nick->prev_nick;
    if (nick->prev_nick)
    {
        (nick->prev_nick)->next_nick = nick->next_nick;
        new_nicks = channel->nicks;
    }
    else
        new_nicks = nick->next_nick;

    if (nick->next_nick)
        (nick->next_nick)->prev_nick = nick->prev_nick;

    channel->nicks_count--;

    /* free data */
    if (nick->name)
        free (nick->name);
    if (nick->host)
        free (nick->host);
    if (nick->prefixes)
        free (nick->prefixes);
    if (nick->color)
        free (nick->color);

    free (nick);

    channel->nicks = new_nicks;
    channel->nick_completion_reset = 1;
}

/*
 * irc_nick_free_all: free all allocated nicks for a channel
 */

void
irc_nick_free_all (struct t_irc_server *server, struct t_irc_channel *channel)
{
    if (!channel)
        return;

    /* remove all nicks for the channel */
    while (channel->nicks)
    {
        irc_nick_free (server, channel, channel->nicks);
    }

    /* sould be zero, but prevent any bug :D */
    channel->nicks_count = 0;
}

/*
 * irc_nick_search: returns pointer on a nick
 */

struct t_irc_nick *
irc_nick_search (struct t_irc_channel *channel, const char *nickname)
{
    struct t_irc_nick *ptr_nick;

    if (!channel || !nickname)
        return NULL;

    for (ptr_nick = channel->nicks; ptr_nick;
         ptr_nick = ptr_nick->next_nick)
    {
        if (weechat_strcasecmp (ptr_nick->name, nickname) == 0)
            return ptr_nick;
    }

    /* nick not found */
    return NULL;
}

/*
 * irc_nick_count: returns number of nicks (total, op, halfop, voice) on a channel
 */

void
irc_nick_count (struct t_irc_server *server, struct t_irc_channel *channel,
                int *total, int *count_op, int *count_halfop, int *count_voice,
                int *count_normal)
{
    struct t_irc_nick *ptr_nick;

    (*total) = 0;
    (*count_op) = 0;
    (*count_halfop) = 0;
    (*count_voice) = 0;
    (*count_normal) = 0;
    for (ptr_nick = channel->nicks; ptr_nick;
         ptr_nick = ptr_nick->next_nick)
    {
        (*total)++;
        if (irc_nick_is_op (server, ptr_nick))
            (*count_op)++;
        else
        {
            if (irc_nick_has_prefix_mode (server, ptr_nick, 'h'))
                (*count_halfop)++;
            else
            {
                if (irc_nick_has_prefix_mode (server, ptr_nick, 'v'))
                    (*count_voice)++;
                else
                    (*count_normal)++;
            }
        }
    }
}

/*
 * irc_nick_set_away: set/unset away status for a channel
 */

void
irc_nick_set_away (struct t_irc_server *server, struct t_irc_channel *channel,
                   struct t_irc_nick *nick, int is_away)
{
    if (!is_away
        || ((IRC_SERVER_OPTION_INTEGER(server, IRC_SERVER_OPTION_AWAY_CHECK) > 0)
            && ((IRC_SERVER_OPTION_INTEGER(server, IRC_SERVER_OPTION_AWAY_CHECK_MAX_NICKS) == 0)
                || (channel->nicks_count <= IRC_SERVER_OPTION_INTEGER(server, IRC_SERVER_OPTION_AWAY_CHECK_MAX_NICKS)))))
    {
        if ((is_away && !nick->away) || (!is_away && nick->away))
        {
            nick->away = is_away;
            irc_nick_nicklist_set (channel, nick, "color",
                                   irc_nick_get_color_for_nicklist (server, nick));
        }
    }
}

/*
 * irc_nick_as_prefix: return string with nick to display as prefix on buffer
 *                     (string will end by a tab)
 */

char *
irc_nick_as_prefix (struct t_irc_server *server, struct t_irc_nick *nick,
                    const char *nickname, const char *force_color)
{
    static char result[256];
    char prefix[2];
    const char *str_prefix_color;

    prefix[0] = (nick) ? nick->prefix[0] : '\0';
    prefix[1] = '\0';
    if (weechat_config_boolean (weechat_config_get ("weechat.look.nickmode")))
    {
        if (nick)
        {
            if ((prefix[0] == ' ')
                && !weechat_config_boolean (weechat_config_get ("weechat.look.nickmode_empty")))
                prefix[0] = '\0';
            str_prefix_color = weechat_color (irc_nick_get_prefix_color_name (server, nick));
        }
        else
        {
            prefix[0] = (weechat_config_boolean (weechat_config_get ("weechat.look.nickmode_empty"))) ?
                ' ' : '\0';
            str_prefix_color = IRC_COLOR_RESET;
        }
    }
    else
    {
        prefix[0] = '\0';
        str_prefix_color = IRC_COLOR_RESET;
    }

    snprintf (result, sizeof (result), "%s%s%s%s%s%s%s%s\t",
              (weechat_config_string (irc_config_look_nick_prefix)
               && weechat_config_string (irc_config_look_nick_prefix)[0]) ?
              IRC_COLOR_NICK_PREFIX : "",
              (weechat_config_string (irc_config_look_nick_prefix)
               && weechat_config_string (irc_config_look_nick_prefix)[0]) ?
              weechat_config_string (irc_config_look_nick_prefix) : "",
              str_prefix_color,
              prefix,
              (force_color) ? force_color : ((nick) ? nick->color : ((nickname) ? irc_nick_find_color (nickname) : IRC_COLOR_CHAT_NICK)),
              (nick) ? nick->name : nickname,
              (weechat_config_string (irc_config_look_nick_suffix)
               && weechat_config_string (irc_config_look_nick_suffix)[0]) ?
              IRC_COLOR_NICK_SUFFIX : "",
              (weechat_config_string (irc_config_look_nick_suffix)
               && weechat_config_string (irc_config_look_nick_suffix)[0]) ?
              weechat_config_string (irc_config_look_nick_suffix) : "");

    return result;
}

/*
 * irc_nick_color_for_pv: return string with color of nick for private
 */

const char *
irc_nick_color_for_pv (struct t_irc_channel *channel, const char *nickname)
{
    if (weechat_config_boolean (irc_config_look_color_pv_nick_like_channel))
    {
        if (!channel->pv_remote_nick_color)
            channel->pv_remote_nick_color = strdup (irc_nick_find_color (nickname));
        if (channel->pv_remote_nick_color)
            return channel->pv_remote_nick_color;
    }

    return IRC_COLOR_CHAT_NICK_OTHER;
}

/*
 * irc_nick_hdata_nick_cb: return hdata for nick
 */

struct t_hdata *
irc_nick_hdata_nick_cb (void *data, const char *hdata_name)
{
    struct t_hdata *hdata;

    /* make C compiler happy */
    (void) data;

    hdata = weechat_hdata_new (hdata_name, "prev_nick", "next_nick");
    if (hdata)
    {
        WEECHAT_HDATA_VAR(struct t_irc_nick, name, STRING, NULL);
        WEECHAT_HDATA_VAR(struct t_irc_nick, host, STRING, NULL);
        WEECHAT_HDATA_VAR(struct t_irc_nick, prefixes, STRING, NULL);
        WEECHAT_HDATA_VAR(struct t_irc_nick, prefix, STRING, NULL);
        WEECHAT_HDATA_VAR(struct t_irc_nick, away, INTEGER, NULL);
        WEECHAT_HDATA_VAR(struct t_irc_nick, color, STRING, NULL);
        WEECHAT_HDATA_VAR(struct t_irc_nick, prev_nick, POINTER, hdata_name);
        WEECHAT_HDATA_VAR(struct t_irc_nick, next_nick, POINTER, hdata_name);
    }
    return hdata;
}

/*
 * irc_nick_add_to_infolist: add a nick in an infolist
 *                           return 1 if ok, 0 if error
 */

int
irc_nick_add_to_infolist (struct t_infolist *infolist,
                          struct t_irc_nick *nick)
{
    struct t_infolist_item *ptr_item;

    if (!infolist || !nick)
        return 0;

    ptr_item = weechat_infolist_new_item (infolist);
    if (!ptr_item)
        return 0;

    if (!weechat_infolist_new_var_string (ptr_item, "name", nick->name))
        return 0;
    if (!weechat_infolist_new_var_string (ptr_item, "host", nick->host))
        return 0;
    if (!weechat_infolist_new_var_string (ptr_item, "prefixes", nick->prefixes))
        return 0;
    if (!weechat_infolist_new_var_string (ptr_item, "prefix", nick->prefix))
        return 0;
    if (!weechat_infolist_new_var_integer (ptr_item, "away", nick->away))
        return 0;
    if (!weechat_infolist_new_var_string (ptr_item, "color", nick->color))
        return 0;

    return 1;
}

/*
 * irc_nick_print_log: print nick infos in log (usually for crash dump)
 */

void
irc_nick_print_log (struct t_irc_nick *nick)
{
    weechat_log_printf ("");
    weechat_log_printf ("    => nick %s (addr:0x%lx):",    nick->name, nick);
    weechat_log_printf ("         host . . . . . : '%s'",  nick->host);
    weechat_log_printf ("         prefixes . . . : '%s'",  nick->prefixes);
    weechat_log_printf ("         prefix . . . . : '%s'",  nick->prefix);
    weechat_log_printf ("         away . . . . . : %d",    nick->away);
    weechat_log_printf ("         color. . . . . : '%s'",  nick->color);
    weechat_log_printf ("         prev_nick. . . : 0x%lx", nick->prev_nick);
    weechat_log_printf ("         next_nick. . . : 0x%lx", nick->next_nick);
}
