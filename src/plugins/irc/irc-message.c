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
 * irc-message.c: functions for IRC messages
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../weechat-plugin.h"
#include "irc.h"
#include "irc-server.h"
#include "irc-channel.h"


/*
 * irc_message_parse: parse IRC message and return pointer to host, command,
 *                    channel, target nick and arguments (if any)
 */

void
irc_message_parse (const char *message, char **nick, char **host,
                   char **command, char **channel, char **arguments)
{
    const char *pos, *pos2, *pos3, *pos4, *pos5;

    if (nick)
        *nick = NULL;
    if (host)
        *host = NULL;
    if (command)
        *command = NULL;
    if (channel)
        *channel = NULL;
    if (arguments)
        *arguments = NULL;

    if (!message)
        return;

    /*
     * we will use this message as example:
     *   :FlashCode!n=FlashCod@host.com PRIVMSG #channel :hello!
     */
    if (message[0] == ':')
    {
        pos2 = strchr (message, '!');
        pos = strchr (message, ' ');
        if (pos2 && (!pos || pos > pos2))
        {
            if (nick)
                *nick = weechat_strndup (message + 1, pos2 - (message + 1));
        }
        else if (pos)
        {
            if (nick)
                *nick = weechat_strndup (message + 1, pos - (message + 1));
        }
        if (pos)
        {
            if (host)
                *host = weechat_strndup (message + 1, pos - (message + 1));
            pos++;
        }
        else
            pos = message;
    }
    else
        pos = message;

    /* pos is pointer on PRIVMSG #channel :hello! */
    if (pos && pos[0])
    {
        while (pos[0] == ' ')
        {
            pos++;
        }
        pos2 = strchr (pos, ' ');
        if (pos2)
        {
            /* pos2 is pointer on #channel :hello! */
            if (command)
                *command = weechat_strndup (pos, pos2 - pos);
            pos2++;
            while (pos2[0] == ' ')
            {
                pos2++;
            }
            if (arguments)
                *arguments = strdup (pos2);
            if (pos2[0] != ':')
            {
                if (irc_channel_is_channel (pos2))
                {
                    pos3 = strchr (pos2, ' ');
                    if (channel)
                    {
                        if (pos3)
                            *channel = weechat_strndup (pos2, pos3 - pos2);
                        else
                            *channel = strdup (pos2);
                    }
                }
                else
                {
                    pos3 = strchr (pos2, ' ');
                    if (nick && !*nick)
                    {
                        if (pos3)
                            *nick = weechat_strndup (pos2, pos3 - pos2);
                        else
                            *nick = strdup (pos2);
                    }
                    if (pos3)
                    {
                        pos4 = pos3;
                        pos3++;
                        while (pos3[0] == ' ')
                        {
                            pos3++;
                        }
                        if (irc_channel_is_channel (pos3))
                        {
                            pos5 = strchr (pos3, ' ');
                            if (channel)
                            {
                                if (pos5)
                                    *channel = weechat_strndup (pos3, pos5 - pos3);
                                else
                                    *channel = strdup (pos3);
                            }
                        }
                        else if (channel && !*channel)
                        {
                            *channel = weechat_strndup (pos2, pos4 - pos2);
                        }
                    }
                }
            }
        }
        else
        {
            if (command)
                *command = strdup (pos);
        }
    }
}

/*
 * irc_message_parse_to_hashtable: parse IRC message and return hashtable with
 *                                 keys: nick, host, command, channel, arguments
 *                                 Note: hashtable has to be free()
 *                                       after use
 */

struct t_hashtable *
irc_message_parse_to_hashtable (const char *message)
{
    char *nick, *host, *command, *channel, *arguments;
    char empty_str[1] = { '\0' };
    struct t_hashtable *hashtable;

    irc_message_parse (message, &nick, &host, &command, &channel, &arguments);

    hashtable = weechat_hashtable_new (8,
                                       WEECHAT_HASHTABLE_STRING,
                                       WEECHAT_HASHTABLE_STRING,
                                       NULL,
                                       NULL);
    if (!hashtable)
        return NULL;

    weechat_hashtable_set (hashtable, "nick", (nick) ? nick : empty_str);
    weechat_hashtable_set (hashtable, "host", (host) ? host : empty_str);
    weechat_hashtable_set (hashtable, "command", (command) ? command : empty_str);
    weechat_hashtable_set (hashtable, "channel", (channel) ? channel : empty_str);
    weechat_hashtable_set (hashtable, "arguments", (arguments) ? arguments : empty_str);

    if (nick)
        free (nick);
    if (host)
        free (host);
    if (command)
        free (command);
    if (channel)
        free (channel);
    if (arguments)
        free (arguments);

    return hashtable;
}

/*
 * irc_message_get_nick_from_host: get nick from host in an IRC message
 */

const char *
irc_message_get_nick_from_host (const char *host)
{
    static char nick[128];
    char host2[128], *pos_space, *pos;
    const char *ptr_host;

    if (!host)
        return NULL;

    nick[0] = '\0';
    if (host)
    {
        ptr_host = host;
        pos_space = strchr (host, ' ');
        if (pos_space)
        {
            if (pos_space - host < (int)sizeof (host2))
            {
                strncpy (host2, host, pos_space - host);
                host2[pos_space - host] = '\0';
            }
            else
                snprintf (host2, sizeof (host2), "%s", host);
            ptr_host = host2;
        }

        if (ptr_host[0] == ':')
            ptr_host++;

        pos = strchr (ptr_host, '!');
        if (pos && (pos - ptr_host < (int)sizeof (nick)))
        {
            strncpy (nick, ptr_host, pos - ptr_host);
            nick[pos - ptr_host] = '\0';
        }
        else
        {
            snprintf (nick, sizeof (nick), "%s", ptr_host);
        }
    }

    return nick;
}

/*
 * irc_message_get_address_from_host: get address from host in an IRC message
 */

const char *
irc_message_get_address_from_host (const char *host)
{
    static char address[256];
    char host2[256], *pos_space, *pos;
    const char *ptr_host;

    address[0] = '\0';
    if (host)
    {
        ptr_host = host;
        pos_space = strchr (host, ' ');
        if (pos_space)
        {
            if (pos_space - host < (int)sizeof (host2))
            {
                strncpy (host2, host, pos_space - host);
                host2[pos_space - host] = '\0';
            }
            else
                snprintf (host2, sizeof (host2), "%s", host);
            ptr_host = host2;
        }

        if (ptr_host[0] == ':')
            ptr_host++;
        pos = strchr (ptr_host, '!');
        if (pos)
            snprintf (address, sizeof (address), "%s", pos + 1);
        else
            snprintf (address, sizeof (address), "%s", ptr_host);
    }

    return address;
}

/*
 * irc_message_replace_vars: replace special IRC vars ($nick, $channel,
 *                           $server) in a string
 *                           Note: result has to be free() after use
 */

char *
irc_message_replace_vars (struct t_irc_server *server,
                          struct t_irc_channel *channel, const char *string)
{
    char *var_nick, *var_channel, *var_server;
    char empty_string[1] = { '\0' };
    char *res, *temp;

    var_nick = (server && server->nick) ? server->nick : empty_string;
    var_channel = (channel) ? channel->name : empty_string;
    var_server = (server) ? server->name : empty_string;

    /* replace nick */
    temp = weechat_string_replace (string, "$nick", var_nick);
    if (!temp)
        return NULL;
    res = temp;

    /* replace channel */
    temp = weechat_string_replace (res, "$channel", var_channel);
    free (res);
    if (!temp)
        return NULL;
    res = temp;

    /* replace server */
    temp = weechat_string_replace (res, "$server", var_server);
    free (res);
    if (!temp)
        return NULL;
    res = temp;

    /* return result */
    return res;
}

/*
 * irc_message_split_add: add a message + arguments in hashtable
 */

void
irc_message_split_add (struct t_hashtable *hashtable, int number,
                       const char *message, const char *arguments)
{
    char key[32], value[32];

    if (message)
    {
        snprintf (key, sizeof (key), "msg%d", number);
        weechat_hashtable_set (hashtable, key, message);
        if (weechat_irc_plugin->debug >= 2)
        {
            weechat_printf (NULL,
                            "irc_message_split_add >> %s='%s' (%d bytes)",
                            key, message,
                            strlen (message));
        }
    }
    if (arguments)
    {
        snprintf (key, sizeof (key), "args%d", number);
        weechat_hashtable_set (hashtable, key, arguments);
        if (weechat_irc_plugin->debug >= 2)
        {
            weechat_printf (NULL,
                            "irc_message_split_add >> %s='%s'",
                            key, arguments);
        }
    }
    snprintf (value, sizeof (value), "%d", number);
    weechat_hashtable_set (hashtable, "count", value);
}

/*
 * irc_message_split_string: split "arguments" using delimiter and max length
 *                           messages added to hashtable are:
 *                             host + command + target + XXX + suffix
 *                           (where XXX is part of "arguments")
 *                           return 1 if split ok, 0 if error
 */

int
irc_message_split_string (struct t_hashtable *hashtable,
                          const char *host,
                          const char *command,
                          const char *target,
                          const char *prefix,
                          const char *arguments,
                          const char *suffix,
                          const char delimiter,
                          int max_length_host)
{
    const char *pos, *pos_max, *pos_next, *pos_last_delim;
    char message[1024], *dup_arguments;
    int max_length, number;

    /*
     * Examples of arguments for this function:
     *
     *   message..: :nick!user@host.com PRIVMSG #channel :Hello world!
     *   arguments:
     *     host     : ":nick!user@host.com"
     *     command  : "PRIVMSG"
     *     target   : "#channel"
     *     prefix   : ":"
     *     arguments: "Hello world!"
     *     suffix   : ""
     *
     *   message..: :nick!user@host.com PRIVMSG #channel :\01ACTION is eating\01
     *   arguments:
     *     host     : ":nick!user@host.com"
     *     command  : "PRIVMSG"
     *     target   : "#channel"
     *     prefix   : ":\01ACTION "
     *     arguments: "is eating"
     *     suffix   : "\01"
     */

    max_length = 510;
    if (max_length_host >= 0)
        max_length -= max_length_host;
    else
        max_length -= (host) ? strlen (host) + 1 : 0;
    max_length -= strlen (command) + 1;
    if (target)
        max_length -= strlen (target);
    if (prefix)
        max_length -= strlen (prefix);
    if (suffix)
        max_length -= strlen (suffix);

    if (max_length < 2)
        return 0;

    /* debug message */
    if (weechat_irc_plugin->debug >= 2)
    {
        weechat_printf (NULL,
                        "irc_message_split_string: host='%s', command='%s', "
                        "target='%s', prefix='%s', arguments='%s', "
                        "suffix='%s', max_length=%d",
                        host, command, target, prefix, arguments, suffix,
                        max_length);
    }

    number = 1;

    if (!arguments || !arguments[0])
    {
        snprintf (message, sizeof (message), "%s%s%s %s%s%s%s",
                  (host) ? host : "",
                  (host) ? " " : "",
                  command,
                  (target) ? target : "",
                  (target && target[0]) ? " " : "",
                  (prefix) ? prefix : "",
                  (suffix) ? suffix : "");
        irc_message_split_add (hashtable, 1, message, "");
        return 1;
    }

    while (arguments && arguments[0])
    {
        pos = arguments;
        pos_max = pos + max_length;
        pos_last_delim = NULL;
        while (pos && pos[0])
        {
            if (pos[0] == delimiter)
                pos_last_delim = pos;
            pos_next = weechat_utf8_next_char (pos);
            if (pos_next > pos_max)
                break;
            pos = pos_next;
        }
        if (pos[0] && pos_last_delim)
            pos = pos_last_delim;
        dup_arguments = weechat_strndup (arguments, pos - arguments);
        if (dup_arguments)
        {
            snprintf (message, sizeof (message), "%s%s%s %s%s%s%s%s",
                      (host) ? host : "",
                      (host) ? " " : "",
                      command,
                      (target) ? target : "",
                      (target && target[0]) ? " " : "",
                      (prefix) ? prefix : "",
                      dup_arguments,
                      (suffix) ? suffix : "");
            irc_message_split_add (hashtable, number, message, dup_arguments);
            number++;
            free (dup_arguments);
        }
        arguments = (pos == pos_last_delim) ? pos + 1 : pos;
    }

    return 1;
}

/*
 * irc_message_split_join: split a JOIN message, taking care of keeping
 *                         channel keys with channel names
 *                         return 1 if split ok, 0 if error
 */

int
irc_message_split_join (struct t_hashtable *hashtable,
                        const char *host, const char *arguments)
{
    int number, channels_count, keys_count, length, length_no_channel;
    int length_to_add, index_channel;
    char **channels, **keys, *pos, *str;
    char msg_to_send[2048], keys_to_add[2048];

    number = 1;

    channels = NULL;
    channels_count = 0;
    keys = NULL;
    keys_count = 0;

    pos = strchr (arguments, ' ');
    if (pos)
    {
        str = weechat_strndup (arguments, pos - arguments);
        if (!str)
            return 0;
        channels = weechat_string_split (str, ",", 0, 0, &channels_count);
        free (str);
        while (pos[0] == ' ')
        {
            pos++;
        }
        if (pos[0])
            keys = weechat_string_split (pos, ",", 0, 0, &keys_count);
    }
    else
    {
        channels = weechat_string_split (arguments, ",", 0, 0, &channels_count);
    }

    snprintf (msg_to_send, sizeof (msg_to_send), "%s%sJOIN",
              (host) ? host : "",
              (host) ? " " : "");
    length = strlen (msg_to_send);
    length_no_channel = length;
    keys_to_add[0] = '\0';
    index_channel = 0;
    while (index_channel < channels_count)
    {
        length_to_add = 1 + strlen (channels[index_channel]);
        if (index_channel < keys_count)
            length_to_add += 1 + strlen (keys[index_channel]);
        if ((length + length_to_add < 510) || (length == length_no_channel))
        {
            if (length + length_to_add < (int)sizeof (msg_to_send))
            {
                strcat (msg_to_send, (length == length_no_channel) ? " " : ",");
                strcat (msg_to_send, channels[index_channel]);
            }
            if (index_channel < keys_count)
            {
                if (strlen (keys_to_add) + 1 +
                    strlen (keys[index_channel]) < (int)sizeof (keys_to_add))
                {
                    strcat (keys_to_add, (keys_to_add[0]) ? "," : " ");
                    strcat (keys_to_add, keys[index_channel]);
                }
            }
            length += length_to_add;
            index_channel++;
        }
        else
        {
            strcat (msg_to_send, keys_to_add);
            irc_message_split_add (hashtable, number,
                                   msg_to_send,
                                   msg_to_send + length_no_channel + 1);
            number++;
            snprintf (msg_to_send, sizeof (msg_to_send), "%s%sJOIN",
                      (host) ? host : "",
                      (host) ? " " : "");
            length = strlen (msg_to_send);
            keys_to_add[0] = '\0';
        }
    }

    if (length > length_no_channel)
    {
        strcat (msg_to_send, keys_to_add);
        irc_message_split_add (hashtable, number,
                               msg_to_send,
                               msg_to_send + length_no_channel + 1);
    }

    if (channels)
        weechat_string_free_split (channels);
    if (keys)
        weechat_string_free_split (keys);

    return 1;
}

/*
 * irc_message_split_privmsg_notice: split a PRIVMSG or NOTICE message, taking
 *                                   care of keeping the '\01' char used in
 *                                   CTCP messages
 *                                   return 1 if split ok, 0 if error
 */

int
irc_message_split_privmsg_notice (struct t_hashtable *hashtable,
                                  char *host, char *command, char *target,
                                  char *arguments, int max_length_host)
{
    char prefix[512], suffix[2], *pos, saved_char;
    int length, rc;

    /*
     * message sent looks like:
     *   PRIVMSG #channel :hello world!
     *
     * when IRC server sends message to other people, message looks like:
     *   :nick!user@host.com PRIVMSG #channel :hello world!
     */

    /* for CTCP, prefix will be ":\01xxxx " and suffix "\01" */
    prefix[0] = '\0';
    suffix[0] = '\0';
    length = strlen (arguments);
    if ((arguments[0] == '\01')
        && (arguments[length - 1] == '\01'))
    {
        pos = strchr (arguments, ' ');
        if (pos)
        {
            pos++;
            saved_char = pos[0];
            pos[0] = '\0';
            snprintf (prefix, sizeof (prefix), ":%s", arguments);
            pos[0] = saved_char;
            arguments[length - 1] = '\0';
            arguments = pos;
            suffix[0] = '\01';
            suffix[1] = '\0';
        }
    }
    if (!prefix[0])
        strcpy (prefix, ":");

    rc = irc_message_split_string (hashtable, host, command, target,
                                   prefix, arguments, suffix,
                                   ' ', max_length_host);

    return rc;
}

/*
 * irc_message_split_005: split a 005 message (isupport)
 *                        return 1 if split ok, 0 if error
 */

int
irc_message_split_005 (struct t_hashtable *hashtable,
                       char *host, char *command, char *target, char *arguments)
{
    char *pos, suffix[512];

    /*
     * 005 message looks like:
     *   :server 005 mynick MODES=4 CHANLIMIT=#:20 NICKLEN=16 USERLEN=10
     *     HOSTLEN=63 TOPICLEN=450 KICKLEN=450 CHANNELLEN=30 KEYLEN=23
     *     CHANTYPES=# PREFIX=(ov)@+ CASEMAPPING=ascii CAPAB IRCD=dancer
     *     :are available on this server
     */

    /* search suffix */
    suffix[0] = '\0';
    pos = strstr (arguments, " :");
    if (pos)
    {
        snprintf (suffix, sizeof (suffix), "%s", pos);
        pos[0] = '\0';
    }

    return irc_message_split_string (hashtable, host, command, target,
                                     NULL, arguments, suffix, ' ', -1);
}

/*
 * irc_message_split: split an IRC message about to be sent to IRC server
 *                    The maximum length of an IRC message is 510 bytes for
 *                    user data + final "\r\n", so full size is 512 bytes.
 *                    The split takes care about type of message to do a split
 *                    at best place in message.
 *                    The hashtable returned contains keys "msg1", "msg2", ...,
 *                    "msgN" with split of message (these messages do not
 *                    include the final "\r\n").
 *                    Hashtable contains "args1", "args2", ..., "argsN" with
 *                    split of arguments only (no host/command here).
 *                    Each message in hashtable has command and arguments, and
 *                    then is ready to be sent to IRC server.
 */

struct t_hashtable *
irc_message_split (struct t_irc_server *server, const char *message)
{
    struct t_hashtable *hashtable;
    char **argv, **argv_eol, *host, *command, *arguments, target[512];
    int split_ok, argc, index_args, max_length_nick, max_length_host;

    split_ok = 0;
    host = NULL;
    command = NULL;
    arguments = NULL;
    index_args = 0;
    argv = NULL;
    argv_eol = NULL;

    /* debug message */
    if (weechat_irc_plugin->debug >= 2)
        weechat_printf (NULL, "irc_message_split: message='%s'", message);

    hashtable = weechat_hashtable_new (8,
                                       WEECHAT_HASHTABLE_STRING,
                                       WEECHAT_HASHTABLE_STRING,
                                       NULL,
                                       NULL);
    if (!hashtable)
        return NULL;

    if (!message || !message[0])
        goto end;

    argv = weechat_string_split (message, " ", 0, 0, &argc);
    argv_eol = weechat_string_split (message, " ", 2, 0, NULL);

    if (argc < 2)
        goto end;

    if (argv[0][0] == ':')
    {
        if (argc < 3)
            goto end;
        host = argv[0];
        command = argv[1];
        arguments = argv_eol[2];
        index_args = 2;
    }
    else
    {
        command = argv[0];
        arguments = argv_eol[1];
        index_args = 1;
    }

    max_length_nick = (server && (server->nick_max_length > 0)) ?
        server->nick_max_length : 16;
    max_length_host = 1 + /* ":"  */
        max_length_nick + /* nick */
        1 +               /* "!"  */
        63 +              /* host */
        1;                /* " "  */

    if ((weechat_strcasecmp (command, "ison") == 0)
        || (weechat_strcasecmp (command, "wallops") == 0))
    {
        split_ok = irc_message_split_string (hashtable, host, command,
                                             NULL, ":",
                                             (argv_eol[index_args][0] == ':') ?
                                             argv_eol[index_args] + 1 : argv_eol[index_args],
                                             NULL, ' ', max_length_host);
    }
    else if (weechat_strcasecmp (command, "join") == 0)
    {
        /* split join (if it's more than 510 bytes) */
        if (strlen (message) > 510)
            split_ok = irc_message_split_join (hashtable, host, arguments);
    }
    else if ((weechat_strcasecmp (command, "privmsg") == 0)
             || (weechat_strcasecmp (command, "notice") == 0))
    {
        /* split privmsg/notice */
        if (index_args + 1 <= argc - 1)
        {
            split_ok = irc_message_split_privmsg_notice (hashtable, host,
                                                         command,
                                                         argv[index_args],
                                                         (argv_eol[index_args + 1][0] == ':') ?
                                                         argv_eol[index_args + 1] + 1 : argv_eol[index_args + 1],
                                                         max_length_host);
        }
    }
    else if (weechat_strcasecmp (command, "005") == 0)
    {
        /* split 005 (isupport) */
        if (index_args + 1 <= argc - 1)
        {
            split_ok = irc_message_split_005 (hashtable, host, command,
                                              argv[index_args],
                                              (argv_eol[index_args + 1][0] == ':') ?
                                              argv_eol[index_args + 1] + 1 : argv_eol[index_args + 1]);
        }
    }
    else if (weechat_strcasecmp (command, "353") == 0)
    {
        /*
         * split 353 (list of users on channel):
         *   :server 353 mynick = #channel :mynick nick1 @nick2 +nick3
         */
        if (index_args + 2 <= argc - 1)
        {
            if (irc_channel_is_channel (argv[index_args + 1]))
            {
                snprintf (target, sizeof (target), "%s %s",
                          argv[index_args], argv[index_args + 1]);
                split_ok = irc_message_split_string (hashtable, host, command,
                                                     target, ":",
                                                     (argv_eol[index_args + 2][0] == ':') ?
                                                     argv_eol[index_args + 2] + 1 : argv_eol[index_args + 2],
                                                     NULL, ' ', -1);
            }
            else
            {
                if (index_args + 3 <= argc - 1)
                {
                    snprintf (target, sizeof (target), "%s %s %s",
                              argv[index_args], argv[index_args + 1],
                              argv[index_args + 2]);
                    split_ok = irc_message_split_string (hashtable, host, command,
                                                         target, ":",
                                                         (argv_eol[index_args + 3][0] == ':') ?
                                                         argv_eol[index_args + 3] + 1 : argv_eol[index_args + 3],
                                                         NULL, ' ', -1);
                }
            }
        }
    }

end:
    if (!split_ok
        || (weechat_hashtable_get_integer (hashtable, "items_count") == 0))
    {
        irc_message_split_add (hashtable, 1, message, arguments);
    }

    if (argv)
        weechat_string_free_split (argv);
    if (argv_eol)
        weechat_string_free_split (argv_eol);

    return hashtable;
}
