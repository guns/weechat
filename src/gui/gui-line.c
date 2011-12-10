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
 * gui-line.c: line functions (used by all GUI)
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <time.h>

#include "../core/weechat.h"
#include "../core/wee-config.h"
#include "../core/wee-hashtable.h"
#include "../core/wee-hdata.h"
#include "../core/wee-hook.h"
#include "../core/wee-infolist.h"
#include "../core/wee-log.h"
#include "../core/wee-string.h"
#include "../plugins/plugin.h"
#include "gui-line.h"
#include "gui-buffer.h"
#include "gui-chat.h"
#include "gui-color.h"
#include "gui-filter.h"
#include "gui-hotlist.h"
#include "gui-window.h"


/*
 * gui_lines_alloc: alloc structure "t_gui_lines" and initialize it
 */

struct t_gui_lines *
gui_lines_alloc ()
{
    struct t_gui_lines *new_lines;

    new_lines = malloc (sizeof (*new_lines));
    if (new_lines)
    {
        new_lines->first_line = NULL;
        new_lines->last_line = NULL;
        new_lines->last_read_line = NULL;
        new_lines->lines_count = 0;
        new_lines->first_line_not_read = 0;
        new_lines->lines_hidden = 0;
        new_lines->buffer_max_length = 0;
        new_lines->prefix_max_length = CONFIG_INTEGER(config_look_prefix_align_min);
    }

    return new_lines;
}

/*
 * gui_lines_free: free a "t_gui_lines" structure
 */

void
gui_lines_free (struct t_gui_lines *lines)
{
    free (lines);
}

/*
 * gui_line_get_align: get alignment for a line
 */

int
gui_line_get_align (struct t_gui_buffer *buffer, struct t_gui_line *line,
                    int with_suffix, int first_line)
{
    int length_time, length_buffer, length_suffix;

    /* return immediately if alignment for end of lines is "time" */
    if (!first_line
        && (CONFIG_INTEGER(config_look_align_end_of_lines) == CONFIG_LOOK_ALIGN_END_OF_LINES_TIME))
    {
        return 0;
    }

    /* length of time */
    if (buffer->time_for_each_line)
    {
        length_time = (gui_chat_time_length == 0) ? 0 : gui_chat_time_length + 1;
    }
    else
        length_time = 0;

    /* return immediately if alignment for end of lines is "buffer" */
    if (!first_line
        && (CONFIG_INTEGER(config_look_align_end_of_lines) == CONFIG_LOOK_ALIGN_END_OF_LINES_BUFFER))
    {
        return length_time;
    }

    /* length of buffer name (when many buffers are merged) */
    if (buffer->mixed_lines)
    {
        if ((CONFIG_INTEGER(config_look_prefix_buffer_align) == CONFIG_LOOK_PREFIX_BUFFER_ALIGN_NONE)
            && (CONFIG_INTEGER(config_look_prefix_align) == CONFIG_LOOK_PREFIX_ALIGN_NONE))
            length_buffer = gui_chat_strlen_screen (gui_buffer_get_short_name (buffer)) + 1;
        else
        {
            if (CONFIG_INTEGER(config_look_prefix_buffer_align) == CONFIG_LOOK_PREFIX_BUFFER_ALIGN_NONE)
                length_buffer = buffer->mixed_lines->buffer_max_length + 1;
            else
                length_buffer = ((CONFIG_INTEGER(config_look_prefix_buffer_align_max) > 0)
                                 && (buffer->mixed_lines->buffer_max_length > CONFIG_INTEGER(config_look_prefix_buffer_align_max))) ?
                    CONFIG_INTEGER(config_look_prefix_buffer_align_max) + 1 : buffer->mixed_lines->buffer_max_length + 1;
        }
    }
    else
        length_buffer = 0;

    /* return immediately if alignment for end of lines is "prefix" */
    if (!first_line
        && (CONFIG_INTEGER(config_look_align_end_of_lines) == CONFIG_LOOK_ALIGN_END_OF_LINES_PREFIX))
    {
        return length_time + length_buffer;
    }

    if (CONFIG_INTEGER(config_look_prefix_align) == CONFIG_LOOK_PREFIX_ALIGN_NONE)
    {
        return length_time + length_buffer + line->data->prefix_length
            + ((line->data->prefix_length > 0) ? 1 : 0);
    }

    length_suffix = 0;
    if (with_suffix)
    {
        if (CONFIG_STRING(config_look_prefix_suffix)
            && CONFIG_STRING(config_look_prefix_suffix)[0])
            length_suffix = gui_chat_strlen_screen (CONFIG_STRING(config_look_prefix_suffix)) + 1;
    }

    return length_time + ((buffer->lines->prefix_max_length > 0) ? 1 : 0)
        + length_buffer
        + (((CONFIG_INTEGER(config_look_prefix_align_max) > 0)
            && (buffer->lines->prefix_max_length > CONFIG_INTEGER(config_look_prefix_align_max))) ?
           CONFIG_INTEGER(config_look_prefix_align_max) : buffer->lines->prefix_max_length)
        + length_suffix;
}

/*
 * gui_line_is_displayed: return 1 if line is displayed (no filter on line,
 *                        or filters disabled), 0 if line is hidden
 */

int
gui_line_is_displayed (struct t_gui_line *line)
{
    /* line is hidden if filters are enabled and flag "displayed" is not set */
    if (gui_filters_enabled && !line->data->displayed)
        return 0;

    /* in all other cases, line is displayed */
    return 1;
}

/*
 * gui_line_get_first_displayed: get first line displayed of a buffer
 */

struct t_gui_line *
gui_line_get_first_displayed (struct t_gui_buffer *buffer)
{
    struct t_gui_line *ptr_line;

    ptr_line = buffer->lines->first_line;
    while (ptr_line && !gui_line_is_displayed (ptr_line))
    {
        ptr_line = ptr_line->next_line;
    }

    return ptr_line;
}

/*
 * gui_line_get_last_displayed: get last line displayed of a buffer
 */

struct t_gui_line *
gui_line_get_last_displayed (struct t_gui_buffer *buffer)
{
    struct t_gui_line *ptr_line;

    ptr_line = buffer->lines->last_line;
    while (ptr_line && !gui_line_is_displayed (ptr_line))
    {
        ptr_line = ptr_line->prev_line;
    }

    return ptr_line;
}

/*
 * gui_line_get_prev_displayed: get previous line displayed
 */

struct t_gui_line *
gui_line_get_prev_displayed (struct t_gui_line *line)
{
    if (line)
    {
        line = line->prev_line;
        while (line && !gui_line_is_displayed (line))
        {
            line = line->prev_line;
        }
    }
    return line;
}

/*
 * gui_line_get_next_displayed: get next line displayed
 */

struct t_gui_line *
gui_line_get_next_displayed (struct t_gui_line *line)
{
    if (line)
    {
        line = line->next_line;
        while (line && !gui_line_is_displayed (line))
        {
            line = line->next_line;
        }
    }
    return line;
}

/*
 * gui_line_search_text: search for text in a line
 */

int
gui_line_search_text (struct t_gui_line *line, const char *text,
                      int case_sensitive)
{
    char *prefix, *message;
    int rc;

    if (!line || !line->data->message || !text || !text[0])
        return 0;

    rc = 0;

    if (line->data->prefix)
    {
        prefix = gui_color_decode (line->data->prefix, NULL);
        if (prefix)
        {
            if ((case_sensitive && (strstr (prefix, text)))
                || (!case_sensitive && (string_strcasestr (prefix, text))))
                rc = 1;
            free (prefix);
        }
    }

    if (!rc)
    {
        message = gui_color_decode (line->data->message, NULL);
        if (message)
        {
            if ((case_sensitive && (strstr (message, text)))
                || (!case_sensitive && (string_strcasestr (message, text))))
                rc = 1;
            free (message);
        }
    }

    return rc;
}

/*
 * gui_line_match_regex: return 1 if message matches regex
 *                       0 if it doesn't match
 */

int
gui_line_match_regex (struct t_gui_line *line, regex_t *regex_prefix,
                      regex_t *regex_message)
{
    char *prefix, *message;
    int match_prefix, match_message;

    if (!line || (!regex_prefix && !regex_message))
        return 0;

    prefix = NULL;
    message = NULL;

    match_prefix = 1;
    match_message = 1;

    if (line->data->prefix)
    {
        prefix = gui_color_decode (line->data->prefix, NULL);
        if (!prefix
            || (regex_prefix && (regexec (regex_prefix, prefix, 0, NULL, 0) != 0)))
            match_prefix = 0;
    }
    else
    {
        if (regex_prefix)
            match_prefix = 0;
    }

    if (line->data->message)
    {
        message = gui_color_decode (line->data->message, NULL);
        if (!message
            || (regex_message && (regexec (regex_message, message, 0, NULL, 0) != 0)))
            match_message = 0;
    }
    else
    {
        if (regex_message)
            match_message = 0;
    }

    if (prefix)
        free (prefix);
    if (message)
        free (message);

    return (match_prefix && match_message);
}

/*
 * gui_line_match_tags: return 1 if line matches tags
 *                      0 if it doesn't match any tag in array
 */

int
gui_line_match_tags (struct t_gui_line *line, int tags_count,
                     char **tags_array)
{
    int i, j;

    if (!line)
        return 0;

    if (line->data->tags_count == 0)
        return 0;

    for (i = 0; i < tags_count; i++)
    {
        for (j = 0; j < line->data->tags_count; j++)
        {
            /* check tag */
            if (string_match (line->data->tags_array[j],
                              tags_array[i],
                              0))
                return 1;
        }
    }

    return 0;
}

/*
 * gui_line_get_nick_tag: get nick in tags: return "xxx" if tag "nick_xxx"
 *                        is found
 */

const char *
gui_line_get_nick_tag (struct t_gui_line *line)
{
    int i;

    for (i = 0; i < line->data->tags_count; i++)
    {
        if (strncmp (line->data->tags_array[i], "nick_", 5) == 0)
            return line->data->tags_array[i] + 5;
    }
    return NULL;
}

/*
 * gui_line_has_highlight: return 1 if given message contains highlight (with
 *                         a string in global highlight or buffer highlight)
 */

int
gui_line_has_highlight (struct t_gui_line *line)
{
    int rc, i, j, no_highlight;
    char *msg_no_color, *highlight_words;

    /*
     * highlights are disabled on this buffer? (special value "-" means that
     * buffer does not want any highlight)
     */
    if (line->data->buffer->highlight_words
        && (strcmp (line->data->buffer->highlight_words, "-") == 0))
        return 0;

    /*
     * check if highlight is forced by a tag (with option highlight_tags) or
     * disabled for line
     */
    no_highlight = 0;
    for (i = 0; i < line->data->tags_count; i++)
    {
        if (config_highlight_tags)
        {
            for (j = 0; j < config_num_highlight_tags; j++)
            {
                if (string_strcasecmp (line->data->tags_array[i],
                                       config_highlight_tags[j]) == 0)
                    return 1;
            }
        }
        if (strcmp (line->data->tags_array[i], GUI_CHAT_TAG_NO_HIGHLIGHT) == 0)
            no_highlight = 1;
    }
    if (no_highlight)
        return 0;

    /*
     * check that line matches highlight tags, if any (if no tag is specified,
     * then any tag is allowed)
     */
    if (line->data->buffer->highlight_tags_count > 0)
    {
        if (!gui_line_match_tags (line,
                                  line->data->buffer->highlight_tags_count,
                                  line->data->buffer->highlight_tags_array))
            return 0;
    }

    /* remove color codes from line message */
    msg_no_color = gui_color_decode (line->data->message, NULL);
    if (!msg_no_color)
        return 0;

    /*
     * there is highlight on line if one of buffer highlight words matches line
     * or one of global highlight words matches line
     */
    highlight_words = gui_buffer_string_replace_local_var (line->data->buffer,
                                                           line->data->buffer->highlight_words);
    rc = string_has_highlight (msg_no_color,
                               (highlight_words) ?
                               highlight_words : line->data->buffer->highlight_words);
    if (highlight_words)
        free (highlight_words);

    if (!rc)
    {
        highlight_words = gui_buffer_string_replace_local_var (line->data->buffer,
                                                               CONFIG_STRING(config_look_highlight));
        rc = string_has_highlight (msg_no_color,
                                   (highlight_words) ?
                                   highlight_words : CONFIG_STRING(config_look_highlight));
        if (highlight_words)
            free (highlight_words);
    }

    if (!rc && config_highlight_regex)
    {
        rc = string_has_highlight_regex_compiled (msg_no_color,
                                                  config_highlight_regex);
    }

    if (!rc && line->data->buffer->highlight_regex_compiled)
    {
        rc = string_has_highlight_regex_compiled (msg_no_color,
                                                  line->data->buffer->highlight_regex_compiled);
    }

    free (msg_no_color);

    return rc;
}

/*
 * gui_line_compute_buffer_max_length: compute "buffer_max_length" for a
 *                                     "t_gui_lines" structure
 */

void
gui_line_compute_buffer_max_length (struct t_gui_buffer *buffer,
                                    struct t_gui_lines *lines)
{
    struct t_gui_buffer *ptr_buffer;
    int length;
    const char *short_name;

    lines->buffer_max_length = 0;
    for (ptr_buffer = gui_buffers; ptr_buffer;
         ptr_buffer = ptr_buffer->next_buffer)
    {
        short_name = gui_buffer_get_short_name (ptr_buffer);
        if (ptr_buffer->number == buffer->number)
        {
            length = gui_chat_strlen_screen (short_name);
            if (length > lines->buffer_max_length)
                lines->buffer_max_length = length;
        }
    }
}

/*
 * gui_line_compute_prefix_max_length: compute "prefix_max_length" for a
 *                                     "t_gui_lines" structure
 */

void
gui_line_compute_prefix_max_length (struct t_gui_lines *lines)
{
    struct t_gui_line *ptr_line;

    lines->prefix_max_length = CONFIG_INTEGER(config_look_prefix_align_min);
    for (ptr_line = lines->first_line; ptr_line;
         ptr_line = ptr_line->next_line)
    {
        if (ptr_line->data->prefix_length > lines->prefix_max_length)
            lines->prefix_max_length = ptr_line->data->prefix_length;
    }
}

/*
 * gui_line_add_to_list: add a line to a "t_gui_lines" structure
 */

void
gui_line_add_to_list (struct t_gui_lines *lines,
                      struct t_gui_line *line)
{
    if (!lines->first_line)
        lines->first_line = line;
    else
        (lines->last_line)->next_line = line;
    line->prev_line = lines->last_line;
    line->next_line = NULL;
    lines->last_line = line;

    if (line->data->prefix_length > lines->prefix_max_length)
        lines->prefix_max_length = line->data->prefix_length;

    lines->lines_count++;
}

/*
 * gui_line_remove_from_list: remove a line from a "t_gui_lines" structure
 */

void
gui_line_remove_from_list (struct t_gui_buffer *buffer,
                           struct t_gui_lines *lines,
                           struct t_gui_line *line,
                           int free_data)
{
    struct t_gui_window *ptr_win;
    struct t_gui_window_scroll *ptr_scroll;
    int i, update_prefix_max_length;

    for (ptr_win = gui_windows; ptr_win; ptr_win = ptr_win->next_window)
    {
        /* reset scroll for any window scroll starting with this line */
        for (ptr_scroll = ptr_win->scroll; ptr_scroll;
             ptr_scroll = ptr_scroll->next_scroll)
        {
            if (ptr_scroll->start_line == line)
            {
                ptr_scroll->start_line = ptr_scroll->start_line->next_line;
                ptr_scroll->start_line_pos = 0;
                gui_buffer_ask_chat_refresh (buffer, 2);
            }
        }
        /* remove line from coords */
        if (ptr_win->coords)
        {
            for (i = 0; i < ptr_win->coords_size; i++)
            {
                if (ptr_win->coords[i].line == line)
                    gui_window_coords_init_line (ptr_win, i);
            }
        }
    }

    update_prefix_max_length =
        (line->data->prefix_length == lines->prefix_max_length);

    /* move read marker if it was on line we are removing */
    if (lines->last_read_line == line)
    {
        lines->last_read_line = lines->last_read_line->prev_line;
        lines->first_line_not_read = (lines->last_read_line) ? 0 : 1;
        gui_buffer_ask_chat_refresh (buffer, 1);
    }

    /* free data */
    if (free_data)
    {
        if (line->data->str_time)
            free (line->data->str_time);
        if (line->data->tags_array)
            string_free_split (line->data->tags_array);
        if (line->data->prefix)
            free (line->data->prefix);
        if (line->data->message)
            free (line->data->message);
        free (line->data);
    }

    /* remove line from list */
    if (line->prev_line)
        (line->prev_line)->next_line = line->next_line;
    if (line->next_line)
        (line->next_line)->prev_line = line->prev_line;
    if (lines->first_line == line)
        lines->first_line = line->next_line;
    if (lines->last_line == line)
        lines->last_line = line->prev_line;

    lines->lines_count--;

    free (line);

    /* compute "prefix_max_length" if needed */
    if (update_prefix_max_length)
        gui_line_compute_prefix_max_length (lines);
}

/*
 * gui_line_mixed_add: add line to mixed lines for a buffer
 */

void
gui_line_mixed_add (struct t_gui_lines *lines,
                    struct t_gui_line_data *line_data)
{
    struct t_gui_line *new_line;

    new_line = malloc (sizeof (*new_line));
    if (new_line)
    {
        new_line->data = line_data;
        gui_line_add_to_list (lines, new_line);
    }
}

/*
 * gui_line_mixed_free_buffer: free all mixed lines matching a buffer
 */

void
gui_line_mixed_free_buffer (struct t_gui_buffer *buffer)
{
    struct t_gui_line *ptr_line, *ptr_next_line;

    if (buffer->mixed_lines)
    {
        ptr_line = buffer->mixed_lines->first_line;
        while (ptr_line)
        {
            ptr_next_line = ptr_line->next_line;

            if (ptr_line->data->buffer == buffer)
            {
                gui_line_remove_from_list (buffer,
                                           buffer->mixed_lines,
                                           ptr_line,
                                           0);
            }

            ptr_line = ptr_next_line;
        }
    }
}

/*
 * gui_line_mixed_free_all: free all mixed lines in a buffer
 */

void
gui_line_mixed_free_all (struct t_gui_buffer *buffer)
{
    if (buffer->mixed_lines)
    {
        while (buffer->mixed_lines->first_line)
        {
            gui_line_remove_from_list (buffer,
                                       buffer->mixed_lines,
                                       buffer->mixed_lines->first_line,
                                       0);
        }
    }
}

/*
 * gui_line_free: delete a line from a buffer
 */

void
gui_line_free (struct t_gui_buffer *buffer, struct t_gui_line *line)
{
    struct t_gui_line *ptr_line;

    /* first remove mixed line if it exists */
    if (buffer->mixed_lines)
    {
        for (ptr_line = buffer->mixed_lines->first_line; ptr_line;
             ptr_line = ptr_line->next_line)
        {
            if (ptr_line->data == line->data)
            {
                gui_line_remove_from_list (buffer,
                                           buffer->mixed_lines,
                                           ptr_line,
                                           0);
                break;
            }
        }
    }

    /* remove line from lines list */
    gui_line_remove_from_list (buffer, buffer->own_lines, line, 1);
}

/*
 * gui_line_free_all: delete all formatted lines from a buffer
 */

void
gui_line_free_all (struct t_gui_buffer *buffer)
{
    while (buffer->own_lines->first_line)
    {
        gui_line_free (buffer, buffer->own_lines->first_line);
    }
}

/*
 * gui_line_get_notify_level: get notify level for a line
 *                            return -1 if "notify_none" is found (no notify
 *                            for line)
 */

int
gui_line_get_notify_level (struct t_gui_line *line)
{
    int i;

    for (i = 0; i < line->data->tags_count; i++)
    {
        if (string_strcasecmp (line->data->tags_array[i], "notify_none") == 0)
            return -1;
        if (string_strcasecmp (line->data->tags_array[i], "notify_highlight") == 0)
            return GUI_HOTLIST_HIGHLIGHT;
        if (string_strcasecmp (line->data->tags_array[i], "notify_private") == 0)
            return GUI_HOTLIST_PRIVATE;
        if (string_strcasecmp (line->data->tags_array[i], "notify_message") == 0)
            return GUI_HOTLIST_MESSAGE;
    }
    return GUI_HOTLIST_LOW;
}

/*
 * gui_line_add: add a new line for a buffer
 */

struct t_gui_line *
gui_line_add (struct t_gui_buffer *buffer, time_t date,
              time_t date_printed, const char *tags,
              const char *prefix, const char *message)
{
    struct t_gui_line *new_line;
    struct t_gui_line_data *new_line_data;
    struct t_gui_window *ptr_win;
    char *message_for_signal;
    const char *nick;
    int notify_level, *max_notify_level, lines_removed;
    time_t current_time;

    /*
     * remove line(s) if necessary, according to history options:
     *   max_lines:   if > 0, keep only N lines in buffer
     *   max_minutes: if > 0, keep only lines from last N minutes
     */
    lines_removed = 0;
    current_time = time (NULL);
    while (buffer->own_lines->first_line
           && (((CONFIG_INTEGER(config_history_max_buffer_lines_number) > 0)
                && (buffer->own_lines->lines_count + 1 >
                    CONFIG_INTEGER(config_history_max_buffer_lines_number)))
               || ((CONFIG_INTEGER(config_history_max_buffer_lines_minutes) > 0)
                   && (current_time - buffer->own_lines->first_line->data->date_printed >
                       CONFIG_INTEGER(config_history_max_buffer_lines_minutes) * 60))))
    {
        gui_line_free (buffer, buffer->own_lines->first_line);
        lines_removed++;
    }

    /* create new line */
    new_line = malloc (sizeof (*new_line));
    if (!new_line)
    {
        log_printf (_("Not enough memory for new line"));
        return NULL;
    }

    /* create data for line */
    new_line_data = malloc (sizeof (*(new_line->data)));
    if (!new_line_data)
    {
        free (new_line);
        log_printf (_("Not enough memory for new line"));
        return NULL;
    }
    new_line->data = new_line_data;

    /* fill data in new line */
    new_line->data->buffer = buffer;
    new_line->data->y = -1;
    new_line->data->date = date;
    new_line->data->date_printed = date_printed;
    new_line->data->str_time = (date == 0) ?
        NULL : gui_chat_get_time_string (date);
    if (tags)
    {
        new_line->data->tags_array = string_split (tags, ",", 0, 0,
                                                   &new_line->data->tags_count);
    }
    else
    {
        new_line->data->tags_count = 0;
        new_line->data->tags_array = NULL;
    }
    new_line->data->refresh_needed = 0;
    new_line->data->prefix = (prefix) ?
        strdup (prefix) : ((date != 0) ? strdup ("") : NULL);
    new_line->data->prefix_length = (prefix) ?
        gui_chat_strlen_screen (prefix) : 0;
    new_line->data->message = (message) ? strdup (message) : strdup ("");

    /* get notify level and max notify level for nick in buffer */
    notify_level = gui_line_get_notify_level (new_line);
    nick = gui_line_get_nick_tag (new_line);
    max_notify_level = NULL;
    if (nick)
        max_notify_level = hashtable_get (buffer->hotlist_max_level_nicks, nick);
    if (max_notify_level
        && (*max_notify_level < notify_level))
        notify_level = *max_notify_level;

    if (notify_level == GUI_HOTLIST_HIGHLIGHT)
        new_line->data->highlight = 1;
    else if (max_notify_level && (*max_notify_level < GUI_HOTLIST_HIGHLIGHT))
        new_line->data->highlight = 0;
    else
        new_line->data->highlight = gui_line_has_highlight (new_line);

    /* add line to lines list */
    gui_line_add_to_list (buffer->own_lines, new_line);

    /* check if line is filtered or not */
    new_line->data->displayed = gui_filter_check_line (new_line);
    if (new_line->data->displayed)
    {
        if (new_line->data->highlight)
        {
            (void) gui_hotlist_add (buffer, GUI_HOTLIST_HIGHLIGHT, NULL);
            if (!weechat_upgrading)
            {
                message_for_signal = gui_chat_build_string_prefix_message (new_line);
                if (message_for_signal)
                {
                    hook_signal_send ("weechat_highlight",
                                      WEECHAT_HOOK_SIGNAL_STRING,
                                      message_for_signal);
                    free (message_for_signal);
                }
            }
        }
        else
        {
            if (!weechat_upgrading && (notify_level == GUI_HOTLIST_PRIVATE))
            {
                message_for_signal = gui_chat_build_string_prefix_message (new_line);
                if (message_for_signal)
                {
                    hook_signal_send ("weechat_pv",
                                      WEECHAT_HOOK_SIGNAL_STRING,
                                      message_for_signal);
                    free (message_for_signal);
                }
            }
            if (notify_level >= GUI_HOTLIST_MIN)
                (void) gui_hotlist_add (buffer, notify_level, NULL);
        }
    }
    else
    {
        if (!buffer->own_lines->lines_hidden)
        {
            buffer->own_lines->lines_hidden = 1;
            if (buffer->mixed_lines)
                buffer->mixed_lines->lines_hidden = 1;
            hook_signal_send ("buffer_lines_hidden",
                              WEECHAT_HOOK_SIGNAL_POINTER, buffer);
        }
    }

    /* add mixed line, if buffer is attched to at least one other buffer */
    if (buffer->mixed_lines)
    {
        gui_line_mixed_add (buffer->mixed_lines, new_line->data);
    }

    /*
     * if some lines were removed, force a full refresh if at least one window
     * is displaying buffer and that number of lines in buffer is lower than
     * window height
     */
    if (lines_removed > 0)
    {
        for (ptr_win = gui_windows; ptr_win; ptr_win = ptr_win->next_window)
        {
            if ((ptr_win->buffer == buffer)
                && (buffer->own_lines->lines_count < ptr_win->win_chat_height))
            {
                gui_buffer_ask_chat_refresh (buffer, 2);
                break;
            }
        }
    }

    return new_line;
}

/*
 * gui_line_add_y: add or update a line for a buffer with free content
 */

void
gui_line_add_y (struct t_gui_buffer *buffer, int y, const char *message)
{
    struct t_gui_line *ptr_line, *new_line;
    struct t_gui_line_data *new_line_data;

    /* search if line exists for "y" */
    for (ptr_line = buffer->own_lines->first_line; ptr_line;
         ptr_line = ptr_line->next_line)
    {
        if (ptr_line->data->y >= y)
            break;
    }

    if (!ptr_line || (ptr_line->data->y > y))
    {
        new_line = malloc (sizeof (*new_line));
        if (!new_line)
        {
            log_printf (_("Not enough memory for new line"));
            return;
        }

        new_line_data = malloc (sizeof (*(new_line->data)));
        if (!new_line_data)
        {
            free (new_line);
            log_printf (_("Not enough memory for new line"));
            return;
        }
        new_line->data = new_line_data;

        buffer->own_lines->lines_count++;

        /* fill data in new line */
        new_line->data->buffer = buffer;
        new_line->data->y = y;
        new_line->data->date = 0;
        new_line->data->date_printed = 0;
        new_line->data->str_time = NULL;
        new_line->data->tags_count = 0;
        new_line->data->tags_array = NULL;
        new_line->data->refresh_needed = 1;
        new_line->data->prefix = NULL;
        new_line->data->prefix_length = 0;
        new_line->data->message = NULL;
        new_line->data->highlight = 0;

        /* add line to lines list */
        if (ptr_line)
        {
            /* add before line found */
            new_line->prev_line = ptr_line->prev_line;
            new_line->next_line = ptr_line;
            if (ptr_line->prev_line)
                (ptr_line->prev_line)->next_line = new_line;
            else
                buffer->own_lines->first_line = new_line;
            ptr_line->prev_line = new_line;
        }
        else
        {
            /* add at end of list */
            new_line->prev_line = buffer->own_lines->last_line;
            if (buffer->own_lines->first_line)
                buffer->own_lines->last_line->next_line = new_line;
            else
                buffer->own_lines->first_line = new_line;
            buffer->own_lines->last_line = new_line;
            new_line->next_line = NULL;
        }

        ptr_line = new_line;
    }

    /* set message for line */
    if (ptr_line->data->message)
        free (ptr_line->data->message);
    ptr_line->data->message = (message) ? strdup (message) : strdup ("");

    /* check if line is filtered or not */
    ptr_line->data->displayed = gui_filter_check_line (ptr_line);
    if (!ptr_line->data->displayed)
    {
        if (!buffer->own_lines->lines_hidden)
        {
            buffer->own_lines->lines_hidden = 1;
            hook_signal_send ("buffer_lines_hidden",
                              WEECHAT_HOOK_SIGNAL_POINTER, buffer);
        }
    }

    ptr_line->data->refresh_needed = 1;
}

/*
 * gui_line_clear: clear prefix and message on a line
 *                 (used on buffers with free content only)
 */

void
gui_line_clear (struct t_gui_line *line)
{
    if (line->data->prefix)
        free (line->data->prefix);
    line->data->prefix = strdup ("");

    if (line->data->message)
        free (line->data->message);
    line->data->message = strdup ("");
}

/*
 * gui_line_mix_buffers: mix lines of a buffer (or group of buffers) with a new
 *                       buffer
 */

void
gui_line_mix_buffers (struct t_gui_buffer *buffer)
{
    struct t_gui_buffer *ptr_buffer, *ptr_buffer_found;
    struct t_gui_lines *new_lines;
    struct t_gui_line *ptr_line1, *ptr_line2;

    /* search first other buffer with same number */
    ptr_buffer_found = NULL;
    for (ptr_buffer = gui_buffers; ptr_buffer;
         ptr_buffer = ptr_buffer->next_buffer)
    {
        if ((ptr_buffer != buffer) && (ptr_buffer->number == buffer->number))
        {
            ptr_buffer_found = ptr_buffer;
            break;
        }
    }
    if (!ptr_buffer_found)
        return;

    /* mix all lines (sorting by date) to a new structure "new_lines" */
    new_lines = gui_lines_alloc ();
    if (!new_lines)
        return;
    ptr_line1 = ptr_buffer_found->lines->first_line;
    ptr_line2 = buffer->lines->first_line;
    while (ptr_line1 || ptr_line2)
    {
        if (!ptr_line1)
        {
            gui_line_mixed_add (new_lines, ptr_line2->data);
            ptr_line2 = ptr_line2->next_line;
        }
        else
        {
            if (!ptr_line2)
            {
                gui_line_mixed_add (new_lines, ptr_line1->data);
                ptr_line1 = ptr_line1->next_line;
            }
            else
            {
                /* look for older line by comparing time */
                if (ptr_line1->data->date <= ptr_line2->data->date)
                {
                    while (ptr_line1
                           && (ptr_line1->data->date <= ptr_line2->data->date))
                    {
                        gui_line_mixed_add (new_lines, ptr_line1->data);
                        ptr_line1 = ptr_line1->next_line;
                    }
                }
                else
                {
                    while (ptr_line2
                           && (ptr_line1->data->date > ptr_line2->data->date))
                    {
                        gui_line_mixed_add (new_lines, ptr_line2->data);
                        ptr_line2 = ptr_line2->next_line;
                    }
                }
            }
        }
    }

    /* compute "prefix_max_length" for mixed lines */
    gui_line_compute_prefix_max_length (new_lines);

    /* compute "buffer_max_length" for mixed lines */
    gui_line_compute_buffer_max_length (buffer, new_lines);

    /* free old mixed lines */
    if (ptr_buffer_found->mixed_lines)
    {
        gui_line_mixed_free_all (ptr_buffer_found);
        free (ptr_buffer_found->mixed_lines);
    }

    /* use new structure with mixed lines in all buffers with correct number */
    for (ptr_buffer = gui_buffers; ptr_buffer;
         ptr_buffer = ptr_buffer->next_buffer)
    {
        if (ptr_buffer->number == buffer->number)
        {
            ptr_buffer->mixed_lines = new_lines;
            ptr_buffer->lines = ptr_buffer->mixed_lines;
        }
    }
}

/*
 * gui_line_hdata_lines_cb: return hdata for lines
 */

struct t_hdata *
gui_line_hdata_lines_cb (void *data, const char *hdata_name)
{
    struct t_hdata *hdata;

    /* make C compiler happy */
    (void) data;

    hdata = hdata_new (NULL, hdata_name, NULL, NULL);
    if (hdata)
    {
        HDATA_VAR(struct t_gui_lines, first_line, POINTER, "line");
        HDATA_VAR(struct t_gui_lines, last_line, POINTER, "line");
        HDATA_VAR(struct t_gui_lines, last_read_line, POINTER, "line");
        HDATA_VAR(struct t_gui_lines, lines_count, INTEGER, NULL);
        HDATA_VAR(struct t_gui_lines, first_line_not_read, INTEGER, NULL);
        HDATA_VAR(struct t_gui_lines, lines_hidden, INTEGER, NULL);
        HDATA_VAR(struct t_gui_lines, buffer_max_length, INTEGER, NULL);
        HDATA_VAR(struct t_gui_lines, prefix_max_length, INTEGER, NULL);
    }
    return hdata;
}

/*
 * gui_line_hdata_line_cb: return hdata for line
 */

struct t_hdata *
gui_line_hdata_line_cb (void *data, const char *hdata_name)
{
    struct t_hdata *hdata;

    /* make C compiler happy */
    (void) data;

    hdata = hdata_new (NULL, hdata_name, "prev_line", "next_line");
    if (hdata)
    {
        HDATA_VAR(struct t_gui_line, data, POINTER, "line_data");
        HDATA_VAR(struct t_gui_line, prev_line, POINTER, hdata_name);
        HDATA_VAR(struct t_gui_line, next_line, POINTER, hdata_name);
    }
    return hdata;
}

/*
 * gui_line_hdata_line_data_cb: return hdata for line data
 */

struct t_hdata *
gui_line_hdata_line_data_cb (void *data, const char *hdata_name)
{
    struct t_hdata *hdata;

    /* make C compiler happy */
    (void) data;

    hdata = hdata_new (NULL, hdata_name, NULL, NULL);
    if (hdata)
    {
        HDATA_VAR(struct t_gui_line_data, buffer, POINTER, "buffer");
        HDATA_VAR(struct t_gui_line_data, y, INTEGER, NULL);
        HDATA_VAR(struct t_gui_line_data, date, TIME, NULL);
        HDATA_VAR(struct t_gui_line_data, date_printed, TIME, NULL);
        HDATA_VAR(struct t_gui_line_data, str_time, STRING, NULL);
        HDATA_VAR(struct t_gui_line_data, tags_count, INTEGER, NULL);
        HDATA_VAR(struct t_gui_line_data, tags_array, POINTER, NULL);
        HDATA_VAR(struct t_gui_line_data, displayed, CHAR, NULL);
        HDATA_VAR(struct t_gui_line_data, highlight, CHAR, NULL);
        HDATA_VAR(struct t_gui_line_data, refresh_needed, CHAR, NULL);
        HDATA_VAR(struct t_gui_line_data, prefix, STRING, NULL);
        HDATA_VAR(struct t_gui_line_data, prefix_length, INTEGER, NULL);
        HDATA_VAR(struct t_gui_line_data, message, STRING, NULL);
    }
    return hdata;
}

/*
 * gui_buffer_line_add_to_infolist: add a buffer line in an infolist
 *                                  return 1 if ok, 0 if error
 */

int
gui_line_add_to_infolist (struct t_infolist *infolist,
                          struct t_gui_lines *lines,
                          struct t_gui_line *line)
{
    struct t_infolist_item *ptr_item;
    int i, length;
    char option_name[64], *tags;

    if (!infolist || !line)
        return 0;

    ptr_item = infolist_new_item (infolist);
    if (!ptr_item)
        return 0;

    if (!infolist_new_var_integer (ptr_item, "y", line->data->y))
        return 0;
    if (!infolist_new_var_time (ptr_item, "date", line->data->date))
        return 0;
    if (!infolist_new_var_time (ptr_item, "date_printed", line->data->date_printed))
        return 0;
    if (!infolist_new_var_string (ptr_item, "str_time", line->data->str_time))
        return 0;

    /* write tags */
    if (!infolist_new_var_integer (ptr_item, "tags_count", line->data->tags_count))
        return 0;
    length = 0;
    for (i = 0; i < line->data->tags_count; i++)
    {
        snprintf (option_name, sizeof (option_name), "tag_%05d", i + 1);
        if (!infolist_new_var_string (ptr_item, option_name,
                                      line->data->tags_array[i]))
            return 0;
        length += strlen (line->data->tags_array[i]) + 1;
    }
    tags = malloc (length + 1);
    if (!tags)
        return 0;
    tags[0] = '\0';
    for (i = 0; i < line->data->tags_count; i++)
    {
        strcat (tags, line->data->tags_array[i]);
        if (i < line->data->tags_count - 1)
            strcat (tags, ",");
    }
    if (!infolist_new_var_string (ptr_item, "tags", tags))
    {
        free (tags);
        return 0;
    }
    free (tags);

    if (!infolist_new_var_integer (ptr_item, "displayed", line->data->displayed))
        return 0;
    if (!infolist_new_var_integer (ptr_item, "highlight", line->data->highlight))
        return 0;
    if (!infolist_new_var_string (ptr_item, "prefix", line->data->prefix))
        return 0;
    if (!infolist_new_var_string (ptr_item, "message", line->data->message))
        return 0;
    if (!infolist_new_var_integer (ptr_item, "last_read_line",
                                   (lines->last_read_line == line) ? 1 : 0))
        return 0;

    return 1;
}

/*
 * gui_lines_print_log: print lines structure infos in log (usually for crash dump)
 */

void
gui_lines_print_log (struct t_gui_lines *lines)
{
    if (lines)
    {
        log_printf ("    first_line. . . . . . : 0x%lx", lines->first_line);
        log_printf ("    last_line . . . . . . : 0x%lx", lines->last_line);
        log_printf ("    last_read_line. . . . : 0x%lx", lines->last_read_line);
        log_printf ("    lines_count . . . . . : %d",    lines->lines_count);
        log_printf ("    first_line_not_read . : %d",    lines->first_line_not_read);
        log_printf ("    lines_hidden. . . . . : %d",    lines->lines_hidden);
        log_printf ("    buffer_max_length . . : %d",    lines->buffer_max_length);
        log_printf ("    prefix_max_length . . : %d",    lines->prefix_max_length);
    }
}
