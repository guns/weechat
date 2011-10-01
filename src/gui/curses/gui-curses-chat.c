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
 * gui-curses-chat.c: chat display functions for Curses GUI
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "../../core/weechat.h"
#include "../../core/wee-config.h"
#include "../../core/wee-hook.h"
#include "../../core/wee-string.h"
#include "../../core/wee-utf8.h"
#include "../../plugins/plugin.h"
#include "../gui-buffer.h"
#include "../gui-chat.h"
#include "../gui-color.h"
#include "../gui-hotlist.h"
#include "../gui-line.h"
#include "../gui-main.h"
#include "../gui-window.h"
#include "gui-curses.h"


/*
 * gui_chat_get_real_width: return real width: width - 1 if nicklist is at right,
 *                          for good copy/paste (without nicklist separator)
 */

int
gui_chat_get_real_width (struct t_gui_window *window)
{
    if (window->win_chat_x + window->win_chat_width < gui_window_get_width ())
        return window->win_chat_width - 1;
    else
        return window->win_chat_width;
}

/*
 * gui_chat_marker_for_line: return 1 if marker must be displayed after this
 *                           line, or 0 if it must not
 */

int
gui_chat_marker_for_line (struct t_gui_buffer *buffer, struct t_gui_line *line)
{
    struct t_gui_line *last_read_line;
    
    /* marker is disabled in config? */
    if (CONFIG_INTEGER(config_look_read_marker) != CONFIG_LOOK_READ_MARKER_LINE)
        return 0;
    
    /* marker is not set for buffer? */
    if (!buffer->lines->last_read_line)
        return 0;
    
    last_read_line = buffer->lines->last_read_line;
    if (!last_read_line->data->displayed)
        last_read_line = gui_line_get_prev_displayed (last_read_line);
    
    if (!last_read_line)
        return 0;
    
    while (line)
    {
        if (last_read_line == line)
        {
            if (CONFIG_BOOLEAN(config_look_read_marker_always_show))
                return 1;
            return (gui_line_get_next_displayed (line) != NULL) ? 1 : 0;
        }
        
        if (line->data->displayed)
            break;
        
        line = line->next_line;
    }
    return 0;
}

/*
 * gui_chat_reset_style: reset style using color depending on window (active or
 *                       not) and line (buffer selected in merged buffer or not)
 */

void
gui_chat_reset_style (struct t_gui_window *window, struct t_gui_line *line,
                      int color_inactive_window, int color_inactive_buffer,
                      int color_default)
{
    if ((window != gui_current_window) &&
        CONFIG_BOOLEAN(config_look_color_inactive_window))
    {
        gui_window_reset_style (GUI_WINDOW_OBJECTS(window)->win_chat,
                                color_inactive_window);
        return;
    }
    if (line && !(line->data->buffer->active)
        && CONFIG_BOOLEAN(config_look_color_inactive_buffer))
    {
        gui_window_reset_style (GUI_WINDOW_OBJECTS(window)->win_chat,
                                color_inactive_buffer);
        return;
    }
    gui_window_reset_style (GUI_WINDOW_OBJECTS(window)->win_chat,
                            color_default);
}

/*
 * gui_chat_display_new_line: display a new line
 */

void
gui_chat_display_new_line (struct t_gui_window *window,
                           int num_lines, int count,
                           int *lines_displayed, int simulate)
{
    if ((count == 0) || (*lines_displayed >= num_lines - count))
    {
        if ((!simulate)
            && (window->win_chat_cursor_x <= gui_chat_get_real_width (window) - 1))
        {
            wmove (GUI_WINDOW_OBJECTS(window)->win_chat,
                   window->win_chat_cursor_y,
                   window->win_chat_cursor_x);
            wclrtoeol (GUI_WINDOW_OBJECTS(window)->win_chat);
        }
        window->win_chat_cursor_y++;
    }
    window->win_chat_cursor_x = 0;
    (*lines_displayed)++;
}

/*
 * gui_chat_display_horizontal_line: display an horizontal line (marker for
 *                                   data not read)
 */

void
gui_chat_display_horizontal_line (struct t_gui_window *window, int simulate)
{
    int x, size_on_screen;
    char *read_marker_string, *default_string = "- ";
    
    if (!simulate)
    {
        gui_window_coords_init_line (window, window->win_chat_cursor_y);
        if (CONFIG_INTEGER(config_look_read_marker) == CONFIG_LOOK_READ_MARKER_LINE)
        {
            read_marker_string = CONFIG_STRING(config_look_read_marker_string);
            if (!read_marker_string || !read_marker_string[0])
                read_marker_string = default_string;
            size_on_screen = utf8_strlen_screen (read_marker_string);
            gui_window_set_weechat_color (GUI_WINDOW_OBJECTS(window)->win_chat,
                                          GUI_COLOR_CHAT_READ_MARKER);
            wmove (GUI_WINDOW_OBJECTS(window)->win_chat,
                   window->win_chat_cursor_y, window->win_chat_cursor_x);
            wclrtoeol (GUI_WINDOW_OBJECTS(window)->win_chat);
            x = 0;
            while (x < window->win_chat_width - 1)
            {
                mvwprintw (GUI_WINDOW_OBJECTS(window)->win_chat,
                           window->win_chat_cursor_y, x,
                           "%s", read_marker_string);
                x += size_on_screen;
            }
        }
        window->win_chat_cursor_x = window->win_chat_width;
    }
}

/*
 * gui_chat_string_next_char: returns next char of a word (for display)
 *                            special chars like colors, bold, .. are skipped
 *                            and optionaly applied
 */

char *
gui_chat_string_next_char (struct t_gui_window *window, struct t_gui_line *line,
                           const unsigned char *string, int apply_style,
                           int apply_style_inactive)
{
    if (apply_style && apply_style_inactive)
    {
        if ((window != gui_current_window)
            && CONFIG_BOOLEAN(config_look_color_inactive_window))
        {
            apply_style = 0;
        }
        else if (line && !(line->data->buffer->active))
        {
            if (CONFIG_BOOLEAN(config_look_color_inactive_buffer))
                apply_style = 0;
        }
    }
    
    while (string[0])
    {
        switch (string[0])
        {
            case GUI_COLOR_COLOR_CHAR:
                string++;
                switch (string[0])
                {
                    case GUI_COLOR_FG_CHAR: /* fg color */
                        string++;
                        gui_window_string_apply_color_fg ((unsigned char **)&string,
                                                          (apply_style) ? GUI_WINDOW_OBJECTS(window)->win_chat : NULL);
                        break;
                    case GUI_COLOR_BG_CHAR: /* bg color */
                        string++;
                        gui_window_string_apply_color_bg ((unsigned char **)&string,
                                                          (apply_style) ? GUI_WINDOW_OBJECTS(window)->win_chat : NULL);
                        break;
                    case GUI_COLOR_FG_BG_CHAR: /* fg + bg color */
                        string++;
                        gui_window_string_apply_color_fg_bg ((unsigned char **)&string,
                                                             (apply_style) ? GUI_WINDOW_OBJECTS(window)->win_chat : NULL);
                        break;
                    case GUI_COLOR_EXTENDED_CHAR: /* pair number */
                        string++;
                        gui_window_string_apply_color_pair ((unsigned char **)&string,
                                                            (apply_style) ? GUI_WINDOW_OBJECTS(window)->win_chat : NULL);
                        break;
                    case GUI_COLOR_BAR_CHAR: /* bar color */
                        string++;
                        switch (string[0])
                        {
                            case GUI_COLOR_BAR_FG_CHAR:
                            case GUI_COLOR_BAR_DELIM_CHAR:
                            case GUI_COLOR_BAR_BG_CHAR:
                            case GUI_COLOR_BAR_START_INPUT_CHAR:
                            case GUI_COLOR_BAR_START_INPUT_HIDDEN_CHAR:
                            case GUI_COLOR_BAR_MOVE_CURSOR_CHAR:
                            case GUI_COLOR_BAR_START_ITEM:
                            case GUI_COLOR_BAR_START_LINE_ITEM:
                                string++;
                                break;
                        }
                        break;
                    default:
                        gui_window_string_apply_color_weechat ((unsigned char **)&string,
                                                               (apply_style) ? GUI_WINDOW_OBJECTS(window)->win_chat : NULL);
                        break;
                }
                break;
            case GUI_COLOR_SET_ATTR_CHAR:
                string++;
                gui_window_string_apply_color_set_attr ((unsigned char **)&string,
                                                        (apply_style) ? GUI_WINDOW_OBJECTS(window)->win_chat : NULL);
                break;
            case GUI_COLOR_REMOVE_ATTR_CHAR:
                string++;
                gui_window_string_apply_color_remove_attr ((unsigned char **)&string,
                                                           (apply_style) ? GUI_WINDOW_OBJECTS(window)->win_chat : NULL);
                break;
            case GUI_COLOR_RESET_CHAR:
                string++;
                if (apply_style)
                {
                    if (apply_style_inactive)
                    {
                        gui_chat_reset_style (window, line,
                                              GUI_COLOR_CHAT_INACTIVE_WINDOW,
                                              GUI_COLOR_CHAT_INACTIVE_BUFFER,
                                              GUI_COLOR_CHAT);
                    }
                    else
                    {
                        gui_window_reset_style (GUI_WINDOW_OBJECTS(window)->win_chat,
                                                GUI_COLOR_CHAT);
                    }
                }
                break;
            default:
                return (char *)string;
                break;
        }
    }
    
    /* nothing found except color/attrib codes, so return NULL */
    return NULL;
}

/*
 * gui_chat_display_word_raw: display word on chat buffer, letter by letter
 *                            special chars like color, bold, .. are interpreted
 *                            return number of chars displayed on screen
 */

int
gui_chat_display_word_raw (struct t_gui_window *window, struct t_gui_line *line,
                           const char *string,
                           int max_chars_on_screen, int display,
                           int apply_style_inactive)
{
    char *next_char, *output, utf_char[16];
    int x, chars_displayed, display_char, size_on_screen;
    
    if (display)
    {
        wmove (GUI_WINDOW_OBJECTS(window)->win_chat,
               window->win_chat_cursor_y,
               window->win_chat_cursor_x);
    }
    
    chars_displayed = 0;
    x = window->win_chat_cursor_x;
    
    while (string && string[0])
    {
        string = gui_chat_string_next_char (window, line,
                                            (unsigned char *)string, 1,
                                            apply_style_inactive);
        if (!string)
            return chars_displayed;
        
        next_char = utf8_next_char (string);
        if (display && next_char)
        {
            memcpy (utf_char, string, next_char - string);
            utf_char[next_char - string] = '\0';
            if (!gui_chat_utf_char_valid (utf_char))
                snprintf (utf_char, sizeof (utf_char), " ");
            
            display_char = (window->buffer->type != GUI_BUFFER_TYPE_FREE)
                || (x >= window->scroll->start_col);
            
            size_on_screen = utf8_strlen_screen (utf_char);
            if ((max_chars_on_screen > 0)
                && (chars_displayed + size_on_screen > max_chars_on_screen))
            {
                return chars_displayed;
            }
            if (display_char && (size_on_screen > 0))
            {
                output = string_iconv_from_internal (NULL, utf_char);
                wprintw (GUI_WINDOW_OBJECTS(window)->win_chat,
                         "%s", (output) ? output : utf_char);
                if (output)
                    free (output);
                chars_displayed += size_on_screen;
            }
            x += size_on_screen;
        }
        
        string = next_char;
    }
    
    return chars_displayed;
}

/*
 * gui_chat_display_word: display a word on chat buffer
 */

void
gui_chat_display_word (struct t_gui_window *window,
                       struct t_gui_line *line,
                       const char *word, const char *word_end,
                       int prefix, int num_lines, int count,
                       int *lines_displayed, int simulate,
                       int apply_style_inactive)
{
    char *data, *ptr_data, *end_line, saved_char, str_space[] = " ";
    int pos_saved_char, chars_to_display, num_displayed;
    int length_align;
    attr_t attrs;
    attr_t *ptr_attrs;
    short pair;
    short *ptr_pair;
    
    if (!word ||
        ((!simulate) && (window->win_chat_cursor_y >= window->win_chat_height)))
        return;
    
    if (!simulate && (window->win_chat_cursor_y < window->coords_size))
        window->coords[window->win_chat_cursor_y].line = line;
    
    data = strdup (word);
    if (!data)
        return;
    
    end_line = data + strlen (data);
    
    if (word_end && word_end[0])
        data[word_end - word] = '\0';
    else
        word_end = NULL;
    
    ptr_data = data;
    while (ptr_data && ptr_data[0])
    {
        /* insert spaces for aligning text under time/nick */
        length_align = gui_line_get_align (window->buffer, line, 0, 0);
        if ((window->win_chat_cursor_x == 0)
            && (*lines_displayed > 0)
            /* FIXME: modify arbitraty value for non aligning messages on time/nick? */
            && (length_align < (window->win_chat_width - 5)))
        {
            if (!simulate)
            {
                wmove (GUI_WINDOW_OBJECTS(window)->win_chat,
                       window->win_chat_cursor_y,
                       window->win_chat_cursor_x);
                wclrtoeol (GUI_WINDOW_OBJECTS(window)->win_chat);
            }
            window->win_chat_cursor_x += length_align;
            if ((CONFIG_INTEGER(config_look_align_end_of_lines) == CONFIG_LOOK_ALIGN_END_OF_LINES_MESSAGE)
                && (CONFIG_INTEGER(config_look_prefix_align) != CONFIG_LOOK_PREFIX_ALIGN_NONE)
                && CONFIG_STRING(config_look_prefix_suffix)
                && CONFIG_STRING(config_look_prefix_suffix)[0])
            {
                if (!simulate)
                {
                    ptr_attrs = &attrs;
                    ptr_pair = &pair;
                    wattr_get (GUI_WINDOW_OBJECTS(window)->win_chat,
                               ptr_attrs, ptr_pair, NULL);
                    gui_window_set_weechat_color (GUI_WINDOW_OBJECTS(window)->win_chat,
                                                  GUI_COLOR_CHAT_PREFIX_SUFFIX);
                    gui_chat_display_word_raw (window, line,
                                               CONFIG_STRING(config_look_prefix_suffix),
                                               0, 1, apply_style_inactive);
                }
                window->win_chat_cursor_x += gui_chat_strlen_screen (CONFIG_STRING(config_look_prefix_suffix));
                if (!simulate)
                {
                    gui_chat_display_word_raw (window, line, str_space, 0, 1,
                                               apply_style_inactive);
                }
                window->win_chat_cursor_x += gui_chat_strlen_screen (str_space);
                if (!simulate)
                    wattr_set (GUI_WINDOW_OBJECTS(window)->win_chat, attrs, pair, NULL);
            }
            if (window->win_chat_cursor_y < window->coords_size)
                window->coords[window->win_chat_cursor_y].data = (char *)word + (ptr_data - data);
        }
        
        chars_to_display = gui_chat_strlen_screen (ptr_data);

        /* too long for current line */
        if (window->win_chat_cursor_x + chars_to_display > gui_chat_get_real_width (window))
        {
            num_displayed = gui_chat_get_real_width (window) - window->win_chat_cursor_x;
            pos_saved_char = gui_chat_string_real_pos (ptr_data, num_displayed);
            if (!simulate)
            {
                saved_char = ptr_data[pos_saved_char];
                ptr_data[pos_saved_char] = '\0';
                if ((count == 0) || (*lines_displayed >= num_lines - count))
                {
                    gui_chat_display_word_raw (window, line, ptr_data, 0, 1,
                                               apply_style_inactive);
                }
                else
                {
                    gui_chat_display_word_raw (window, line, ptr_data, 0, 0,
                                               apply_style_inactive);
                }
                ptr_data[pos_saved_char] = saved_char;
            }
            ptr_data += pos_saved_char;
        }
        else
        {
            num_displayed = chars_to_display;
            if (!simulate)
            {
                if ((count == 0) || (*lines_displayed >= num_lines - count))
                {
                    gui_chat_display_word_raw (window, line, ptr_data, 0, 1,
                                               apply_style_inactive);
                }
                else
                {
                    gui_chat_display_word_raw (window, line, ptr_data, 0, 0,
                                               apply_style_inactive);
                }
            }
            ptr_data += strlen (ptr_data);
        }
        
        window->win_chat_cursor_x += num_displayed;
        
        /* display new line? */
        if ((!prefix && (ptr_data >= end_line)) ||
            (((simulate) ||
              (window->win_chat_cursor_y <= window->win_chat_height - 1)) &&
             (window->win_chat_cursor_x > (gui_chat_get_real_width (window) - 1))))
            gui_chat_display_new_line (window, num_lines, count,
                                       lines_displayed, simulate);
        
        if ((!prefix && (ptr_data >= end_line)) ||
            ((!simulate) && (window->win_chat_cursor_y >= window->win_chat_height)))
            ptr_data = NULL;
    }
    
    free (data);
}

/*
 * gui_chat_display_time_to_prefix: display time, buffer name (for merged
 *                                  buffers) and prefix for a line
 */

void
gui_chat_display_time_to_prefix (struct t_gui_window *window,
                                 struct t_gui_line *line,
                                 int num_lines, int count,
                                 int *lines_displayed,
                                 int simulate)
{
    char str_space[] = " ", str_plus[] = "+";
    char *prefix_no_color, *prefix_highlighted;
    const char *short_name;
    int i, length, length_allowed, num_spaces;
    struct t_gui_lines *mixed_lines;
    
    if (!simulate)
    {
        if (window->win_chat_cursor_y < window->coords_size)
            window->coords[window->win_chat_cursor_y].line = line;
        gui_chat_reset_style (window, line,
                              GUI_COLOR_CHAT_INACTIVE_WINDOW,
                              GUI_COLOR_CHAT_INACTIVE_BUFFER,
                              GUI_COLOR_CHAT);
    }
    
    /* display time */
    if (window->buffer->time_for_each_line
        && (line->data->str_time && line->data->str_time[0]))
    {
        if (window->win_chat_cursor_y < window->coords_size)
            window->coords[window->win_chat_cursor_y].time_x1 = window->win_chat_cursor_x;
        gui_chat_display_word (window, line, line->data->str_time,
                               NULL, 1, num_lines, count, lines_displayed,
                               simulate,
                               CONFIG_BOOLEAN(config_look_color_inactive_time));
        if (window->win_chat_cursor_y < window->coords_size)
            window->coords[window->win_chat_cursor_y].time_x2 = window->win_chat_cursor_x - 1;
        
        if (!simulate)
        {
            gui_chat_reset_style (window, line,
                                  GUI_COLOR_CHAT_INACTIVE_WINDOW,
                                  GUI_COLOR_CHAT_INACTIVE_BUFFER,
                                  GUI_COLOR_CHAT);
        }
        gui_chat_display_word (window, line, str_space,
                               NULL, 1, num_lines, count, lines_displayed,
                               simulate,
                               CONFIG_BOOLEAN(config_look_color_inactive_time));
    }
    
    /* display buffer name (if many buffers are merged) */
    mixed_lines = line->data->buffer->mixed_lines;
    if (mixed_lines)
    {
        if ((CONFIG_INTEGER(config_look_prefix_buffer_align_max) > 0)
            && (CONFIG_INTEGER(config_look_prefix_buffer_align) != CONFIG_LOOK_PREFIX_BUFFER_ALIGN_NONE))
        {
            length_allowed =
                (mixed_lines->buffer_max_length <= CONFIG_INTEGER(config_look_prefix_buffer_align_max)) ?
                mixed_lines->buffer_max_length : CONFIG_INTEGER(config_look_prefix_buffer_align_max);
        }
        else
            length_allowed = mixed_lines->buffer_max_length;

        short_name = gui_buffer_get_short_name (line->data->buffer);
        length = gui_chat_strlen_screen (short_name);
        num_spaces = length_allowed - length;
        
        if (CONFIG_INTEGER(config_look_prefix_buffer_align) == CONFIG_LOOK_PREFIX_BUFFER_ALIGN_RIGHT)
        {
            if (!simulate)
            {
                gui_chat_reset_style (window, line,
                                      GUI_COLOR_CHAT_INACTIVE_WINDOW,
                                      GUI_COLOR_CHAT_INACTIVE_BUFFER,
                                      GUI_COLOR_CHAT);
            }
            for (i = 0; i < num_spaces; i++)
            {
                gui_chat_display_word (window, line, str_space,
                                       NULL, 1, num_lines, count,
                                       lines_displayed, simulate,
                                       CONFIG_BOOLEAN(config_look_color_inactive_prefix_buffer));
            }
        }
        
        if (!simulate)
        {
            gui_chat_reset_style (window, line,
                                  GUI_COLOR_CHAT_INACTIVE_WINDOW,
                                  (CONFIG_BOOLEAN(config_look_color_inactive_buffer)
                                   && CONFIG_BOOLEAN(config_look_color_inactive_prefix_buffer)
                                   && (!line->data->buffer->active)) ?
                                  GUI_COLOR_CHAT_PREFIX_BUFFER_INACTIVE_BUFFER :
                                  GUI_COLOR_CHAT_PREFIX_BUFFER,
                                  GUI_COLOR_CHAT_PREFIX_BUFFER);
        }
        
        if (window->win_chat_cursor_y < window->coords_size)
            window->coords[window->win_chat_cursor_y].buffer_x1 = window->win_chat_cursor_x;
        
        /* not enough space to display full buffer name? => truncate it! */
        if ((CONFIG_INTEGER(config_look_prefix_buffer_align) != CONFIG_LOOK_PREFIX_BUFFER_ALIGN_NONE)
            && (num_spaces < 0))
        {
            gui_chat_display_word (window, line,
                                   short_name,
                                   short_name +
                                   gui_chat_string_real_pos (short_name,
                                                             length_allowed),
                                   1, num_lines, count, lines_displayed,
                                   simulate,
                                   CONFIG_BOOLEAN(config_look_color_inactive_prefix_buffer));
        }
        else
        {
            gui_chat_display_word (window, line,
                                   short_name, NULL, 1, num_lines, count,
                                   lines_displayed, simulate,
                                   CONFIG_BOOLEAN(config_look_color_inactive_prefix_buffer));
        }
        
        if (window->win_chat_cursor_y < window->coords_size)
            window->coords[window->win_chat_cursor_y].buffer_x2 = window->win_chat_cursor_x - 1;
        
        if ((CONFIG_INTEGER(config_look_prefix_buffer_align) != CONFIG_LOOK_PREFIX_BUFFER_ALIGN_NONE)
            && (num_spaces < 0))
        {
            if (!simulate)
            {
                gui_window_set_weechat_color (GUI_WINDOW_OBJECTS(window)->win_chat,
                                              GUI_COLOR_CHAT_PREFIX_MORE);
            }
            gui_chat_display_word (window, line,
                                   (CONFIG_BOOLEAN(config_look_prefix_buffer_align_more)) ?
                                   str_plus : str_space,
                                   NULL, 1, num_lines, count, lines_displayed,
                                   simulate,
                                   CONFIG_BOOLEAN(config_look_color_inactive_prefix_buffer));
        }
        else
        {
            if (!simulate)
            {
                gui_chat_reset_style (window, line,
                                      GUI_COLOR_CHAT_INACTIVE_WINDOW,
                                      GUI_COLOR_CHAT_INACTIVE_BUFFER,
                                      GUI_COLOR_CHAT);
            }
            if ((CONFIG_INTEGER(config_look_prefix_buffer_align) == CONFIG_LOOK_PREFIX_BUFFER_ALIGN_LEFT)
                || ((CONFIG_INTEGER(config_look_prefix_buffer_align) == CONFIG_LOOK_PREFIX_BUFFER_ALIGN_NONE)
                && (CONFIG_INTEGER(config_look_prefix_align) != CONFIG_LOOK_PREFIX_ALIGN_NONE)))
            {
                for (i = 0; i < num_spaces; i++)
                {
                    gui_chat_display_word (window, line, str_space,
                                           NULL, 1, num_lines, count, lines_displayed,
                                           simulate,
                                           CONFIG_BOOLEAN(config_look_color_inactive_prefix_buffer));
                }
            }
            if (mixed_lines->buffer_max_length > 0)
            {
                gui_chat_display_word (window, line, str_space,
                                       NULL, 1, num_lines, count, lines_displayed,
                                       simulate,
                                       CONFIG_BOOLEAN(config_look_color_inactive_prefix_buffer));
            }
        }
    }
    
    /* display prefix */
    if (line->data->prefix
        && (line->data->prefix[0]
            || (CONFIG_INTEGER(config_look_prefix_align) != CONFIG_LOOK_PREFIX_ALIGN_NONE)))
    {
        if (!simulate)
        {
            gui_chat_reset_style (window, line,
                                  GUI_COLOR_CHAT_INACTIVE_WINDOW,
                                  GUI_COLOR_CHAT_INACTIVE_BUFFER,
                                  GUI_COLOR_CHAT);
        }
        
        if (CONFIG_INTEGER(config_look_prefix_align_max) > 0)
        {
            length_allowed =
                (window->buffer->lines->prefix_max_length <= CONFIG_INTEGER(config_look_prefix_align_max)) ?
                window->buffer->lines->prefix_max_length : CONFIG_INTEGER(config_look_prefix_align_max);
        }
        else
            length_allowed = window->buffer->lines->prefix_max_length;
        
        num_spaces = length_allowed - line->data->prefix_length;
        
        if (CONFIG_INTEGER(config_look_prefix_align) == CONFIG_LOOK_PREFIX_ALIGN_RIGHT)
        {
            for (i = 0; i < num_spaces; i++)
            {
                gui_chat_display_word (window, line, str_space,
                                       NULL, 1, num_lines, count,
                                       lines_displayed, simulate,
                                       CONFIG_BOOLEAN(config_look_color_inactive_prefix));
            }
        }
        
        prefix_highlighted = NULL;
        if (line->data->highlight)
        {
            prefix_no_color = gui_color_decode (line->data->prefix, NULL);
            if (prefix_no_color)
            {
                length = strlen (prefix_no_color) + 32;
                prefix_highlighted = malloc (length);
                if (prefix_highlighted)
                {
                    snprintf (prefix_highlighted, length, "%s%s",
                              GUI_COLOR(GUI_COLOR_CHAT_HIGHLIGHT),
                              prefix_no_color);
                }
                free (prefix_no_color);
            }
            if (!simulate)
            {
                gui_chat_reset_style (window, line,
                                      GUI_COLOR_CHAT_INACTIVE_WINDOW,
                                      (CONFIG_BOOLEAN(config_look_color_inactive_buffer)
                                       && CONFIG_BOOLEAN(config_look_color_inactive_prefix)
                                       && (!line->data->buffer->active)) ?
                                      GUI_COLOR_CHAT_INACTIVE_BUFFER :
                                      GUI_COLOR_CHAT_HIGHLIGHT,
                                      GUI_COLOR_CHAT_HIGHLIGHT);
            }
        }
        else
        {
            if (!simulate)
            {
                gui_chat_reset_style (window, line,
                                      GUI_COLOR_CHAT_INACTIVE_WINDOW,
                                      (CONFIG_BOOLEAN(config_look_color_inactive_buffer)
                                       && CONFIG_BOOLEAN(config_look_color_inactive_prefix)
                                       && (!line->data->buffer->active)) ?
                                      GUI_COLOR_CHAT_INACTIVE_BUFFER :
                                      GUI_COLOR_CHAT,
                                      GUI_COLOR_CHAT);
            }
        }
        
        if (window->win_chat_cursor_y < window->coords_size)
            window->coords[window->win_chat_cursor_y].prefix_x1 = window->win_chat_cursor_x;
        
        /* not enough space to display full prefix? => truncate it! */
        if ((CONFIG_INTEGER(config_look_prefix_align) != CONFIG_LOOK_PREFIX_ALIGN_NONE)
            && (num_spaces < 0))
        {
            gui_chat_display_word (window, line,
                                   (prefix_highlighted) ? prefix_highlighted : line->data->prefix,
                                   (prefix_highlighted) ?
                                   prefix_highlighted + gui_chat_string_real_pos (prefix_highlighted,
                                                                                  length_allowed) :
                                   line->data->prefix + gui_chat_string_real_pos (line->data->prefix,
                                                                                  length_allowed),
                                   1, num_lines, count, lines_displayed,
                                   simulate,
                                   CONFIG_BOOLEAN(config_look_color_inactive_prefix));
        }
        else
        {
            gui_chat_display_word (window, line,
                                   (prefix_highlighted) ? prefix_highlighted : line->data->prefix,
                                   NULL, 1, num_lines, count, lines_displayed,
                                   simulate,
                                   CONFIG_BOOLEAN(config_look_color_inactive_prefix));
        }
        
        if (window->win_chat_cursor_y < window->coords_size)
            window->coords[window->win_chat_cursor_y].prefix_x2 = window->win_chat_cursor_x - 1;
        
        if (prefix_highlighted)
            free (prefix_highlighted);
        
        if (!simulate)
        {
            gui_chat_reset_style (window, line,
                                  GUI_COLOR_CHAT_INACTIVE_WINDOW,
                                  GUI_COLOR_CHAT_INACTIVE_BUFFER,
                                  GUI_COLOR_CHAT);
        }
        
        if (CONFIG_INTEGER(config_look_prefix_align) == CONFIG_LOOK_PREFIX_ALIGN_LEFT)
        {
            for (i = 0; i < num_spaces; i++)
            {
                gui_chat_display_word (window, line, str_space,
                                       NULL, 1, num_lines, count, lines_displayed,
                                       simulate,
                                       CONFIG_BOOLEAN(config_look_color_inactive_prefix));
            }
        }
        if ((CONFIG_INTEGER(config_look_prefix_align) != CONFIG_LOOK_PREFIX_ALIGN_NONE)
            && (num_spaces < 0))
        {
            if (!simulate)
            {
                gui_window_set_weechat_color (GUI_WINDOW_OBJECTS(window)->win_chat,
                                              GUI_COLOR_CHAT_PREFIX_MORE);
            }
            gui_chat_display_word (window, line,
                                   (CONFIG_BOOLEAN(config_look_prefix_align_more)) ?
                                   str_plus : str_space,
                                   NULL, 1, num_lines, count, lines_displayed,
                                   simulate,
                                   CONFIG_BOOLEAN(config_look_color_inactive_prefix));
        }
        else
        {
            if (window->buffer->lines->prefix_max_length > 0)
            {
                gui_chat_display_word (window, line, str_space,
                                       NULL, 1, num_lines, count, lines_displayed,
                                       simulate,
                                       CONFIG_BOOLEAN(config_look_color_inactive_prefix));
            }
        }
        if ((CONFIG_INTEGER(config_look_prefix_align) != CONFIG_LOOK_PREFIX_ALIGN_NONE)
            && (CONFIG_STRING(config_look_prefix_suffix)
                && CONFIG_STRING(config_look_prefix_suffix)[0]))
        {
            if (!simulate)
            {
                gui_window_set_weechat_color (GUI_WINDOW_OBJECTS(window)->win_chat,
                                              GUI_COLOR_CHAT_PREFIX_SUFFIX);
            }
            gui_chat_display_word (window, line,
                                   CONFIG_STRING(config_look_prefix_suffix),
                                   NULL, 1, num_lines, count,
                                   lines_displayed, simulate, 0);
            gui_chat_display_word (window, line, str_space,
                                   NULL, 1, num_lines, count,
                                   lines_displayed, simulate, 0);
        }
    }
}

/*
 * gui_chat_display_line: display a line in the chat window
 *                        if count == 0, display whole line
 *                        if count > 0, display 'count' lines
 *                          (beginning from the end)
 *                        if simulate == 1, nothing is displayed
 *                          (for counting how many lines would have been
 *                          displayed)
 *                        return number of lines displayed (or simulated)
 */

int
gui_chat_display_line (struct t_gui_window *window, struct t_gui_line *line,
                       int count, int simulate)
{
    int num_lines, x, y, lines_displayed, line_align;
    int read_marker_x, read_marker_y, marker_line;
    int word_start_offset, word_end_offset;
    int word_length_with_spaces, word_length;
    char *ptr_data, *ptr_end_offset, *next_char;
    char *ptr_style, *message_with_tags;
    
    if (!line)
        return 0;
    
    if (simulate)
    {
        x = window->win_chat_cursor_x;
        y = window->win_chat_cursor_y;
        window->win_chat_cursor_x = 0;
        window->win_chat_cursor_y = 0;
        num_lines = 0;
    }
    else
    {
        if (window->win_chat_cursor_y > window->win_chat_height - 1)
            return 0;
        x = window->win_chat_cursor_x;
        y = window->win_chat_cursor_y;
        num_lines = gui_chat_display_line (window, line, 0, 1);
        window->win_chat_cursor_x = x;
        window->win_chat_cursor_y = y;
    }
    
    /* calculate marker position (maybe not used for this line!) */
    if (window->buffer->time_for_each_line && line->data->str_time)
        read_marker_x = x + gui_chat_strlen_screen (line->data->str_time);
    else
        read_marker_x = x;
    read_marker_y = y;
    
    lines_displayed = 0;
    
    marker_line = gui_chat_marker_for_line (window->buffer, line);
    
    /* display time and prefix */
    gui_chat_display_time_to_prefix (window, line, num_lines, count,
                                     &lines_displayed, simulate);
    if (!simulate && !gui_chat_display_tags)
    {
        if (window->win_chat_cursor_y < window->coords_size)
            window->coords[window->win_chat_cursor_y].data = line->data->message;
        window->coords_x_message = window->win_chat_cursor_x;
    }
    
    /* reset color & style for a new line */
    if (!simulate)
    {
        if (CONFIG_BOOLEAN(config_look_color_inactive_message))
        {
            gui_chat_reset_style (window, line,
                                  GUI_COLOR_CHAT_INACTIVE_WINDOW,
                                  GUI_COLOR_CHAT_INACTIVE_BUFFER,
                                  GUI_COLOR_CHAT);
        }
        else
        {
            gui_chat_reset_style (window, line,
                                  GUI_COLOR_CHAT,
                                  GUI_COLOR_CHAT,
                                  GUI_COLOR_CHAT);
        }
    }
    
    if (!line->data->message || !line->data->message[0])
    {
        gui_chat_display_new_line (window, num_lines, count,
                                   &lines_displayed, simulate);
    }
    else
    {
        message_with_tags = (gui_chat_display_tags) ?
            gui_chat_build_string_message_tags (line) : NULL;
        ptr_data = (message_with_tags) ?
            message_with_tags : line->data->message;
        while (ptr_data && ptr_data[0])
        {
            gui_chat_get_word_info (window,
                                    ptr_data,
                                    &word_start_offset,
                                    &word_end_offset,
                                    &word_length_with_spaces, &word_length);
            
            ptr_end_offset = ptr_data + word_end_offset;
            
            if (word_length > 0)
            {
                /* spaces + word too long for current line but ok for next line */
                line_align = gui_line_get_align (window->buffer, line, 1,
                                                 (lines_displayed == 0) ? 1 : 0);
                if ((window->win_chat_cursor_x + word_length_with_spaces > gui_chat_get_real_width (window))
                    && (word_length <= gui_chat_get_real_width (window) - line_align))
                {
                    gui_chat_display_new_line (window, num_lines, count,
                                               &lines_displayed, simulate);
                    /* apply styles before jumping to start of word */
                    if (!simulate && (word_start_offset > 0))
                    {
                        ptr_style = ptr_data;
                        while (ptr_style < ptr_data + word_start_offset)
                        {
                            /* loop until no style/char available */
                            ptr_style = gui_chat_string_next_char (window, line,
                                                                   (unsigned char *)ptr_style,
                                                                   1,
                                                                   CONFIG_BOOLEAN(config_look_color_inactive_message));
                            if (!ptr_style)
                                break;
                            ptr_style = utf8_next_char (ptr_style);
                        }
                    }
                    /* jump to start of word */
                    ptr_data += word_start_offset;
                }
                
                /* display word */
                gui_chat_display_word (window, line, ptr_data,
                                       ptr_end_offset + 1,
                                       0, num_lines, count, &lines_displayed,
                                       simulate,
                                       CONFIG_BOOLEAN(config_look_color_inactive_message));
                
                if ((!simulate) && (window->win_chat_cursor_y >= window->win_chat_height))
                    ptr_data = NULL;
                else
                {
                    /* move pointer after end of word */
                    ptr_data = ptr_end_offset + 1;
                    if (*(ptr_data - 1) == '\0')
                        ptr_data = NULL;
                    
                    if (window->win_chat_cursor_x == 0)
                    {
                        while (ptr_data && (ptr_data[0] == ' '))
                        {
                            next_char = utf8_next_char (ptr_data);
                            if (!next_char)
                                break;
                            ptr_data = gui_chat_string_next_char (window, line,
                                                                  (unsigned char *)next_char,
                                                                  1,
                                                                  CONFIG_BOOLEAN(config_look_color_inactive_message));
                        }
                    }
                }
            }
            else
            {
                gui_chat_display_new_line (window, num_lines, count,
                                           &lines_displayed, simulate);
                ptr_data = NULL;
            }
        }
        if (message_with_tags)
            free (message_with_tags);
    }
    
    if (marker_line)
    {
        gui_chat_display_horizontal_line (window, simulate);
        gui_chat_display_new_line (window, num_lines, count,
                                   &lines_displayed, simulate);
    }
    
    if (simulate)
    {
        window->win_chat_cursor_x = x;
        window->win_chat_cursor_y = y;
    }
    else
    {
        /* display marker if line is matching user search */
        if (window->buffer->text_search != GUI_TEXT_SEARCH_DISABLED)
        {
            if (gui_line_search_text (line, window->buffer->input_buffer,
                                      window->buffer->text_search_exact))
            {
                gui_window_set_weechat_color (GUI_WINDOW_OBJECTS(window)->win_chat,
                                              GUI_COLOR_CHAT_TEXT_FOUND);
                mvwprintw (GUI_WINDOW_OBJECTS(window)->win_chat,
                           read_marker_y, read_marker_x,
                           "*");
            }
        }
        else
        {
            /* display read marker if needed */
            if ((CONFIG_INTEGER(config_look_read_marker) == CONFIG_LOOK_READ_MARKER_CHAR)
                && window->buffer->lines->last_read_line
                && (window->buffer->lines->last_read_line == gui_line_get_prev_displayed (line)))
            {
                gui_window_set_weechat_color (GUI_WINDOW_OBJECTS(window)->win_chat,
                                              GUI_COLOR_CHAT_READ_MARKER);
                mvwprintw (GUI_WINDOW_OBJECTS(window)->win_chat,
                           read_marker_y, read_marker_x,
                           "*");
            }
        }
    }
    
    return lines_displayed;
}

/*
 * gui_chat_display_line_y: display a line in the chat window (for a buffer
 *                          with free content)
 */

void
gui_chat_display_line_y (struct t_gui_window *window, struct t_gui_line *line,
                         int y)
{
    /* reset color & style for a new line */
    gui_chat_reset_style (window, line,
                          GUI_COLOR_CHAT_INACTIVE_WINDOW,
                          GUI_COLOR_CHAT_INACTIVE_BUFFER,
                          GUI_COLOR_CHAT);
    
    window->win_chat_cursor_x = 0;
    window->win_chat_cursor_y = y;
    
    wmove (GUI_WINDOW_OBJECTS(window)->win_chat,
           window->win_chat_cursor_y,
           window->win_chat_cursor_x);
    wclrtoeol (GUI_WINDOW_OBJECTS(window)->win_chat);
    
    if (gui_chat_display_word_raw (window, line, line->data->message,
                                   window->win_chat_width, 1,
                                   CONFIG_BOOLEAN(config_look_color_inactive_message)) < window->win_chat_width)
    {
        gui_window_clrtoeol (GUI_WINDOW_OBJECTS(window)->win_chat);
    }
}

/*
 * gui_chat_calculate_line_diff: returns pointer to line & offset for a
 *                               difference with given line
 */

void
gui_chat_calculate_line_diff (struct t_gui_window *window,
                              struct t_gui_line **line, int *line_pos,
                              int difference)
{
    int backward, current_size;
    
    if (!line || !line_pos)
        return;
    
    backward = (difference < 0);
    
    if (!(*line))
    {
        /* if looking backward, start at last line of buffer */
        if (backward)
        {
            *line = gui_line_get_last_displayed (window->buffer);
            if (!(*line))
                return;
            current_size = gui_chat_display_line (window, *line, 0, 1);
            if (current_size == 0)
                current_size = 1;
            *line_pos = current_size - 1;
        }
        /* if looking forward, start at first line of buffer */
        else
        {
            *line = gui_line_get_first_displayed (window->buffer);
            if (!(*line))
                return;
            *line_pos = 0;
            current_size = gui_chat_display_line (window, *line, 0, 1);
        }
    }
    else
        current_size = gui_chat_display_line (window, *line, 0, 1);
    
    while ((*line) && (difference != 0))
    {
        /* looking backward */
        if (backward)
        {
            if (*line_pos > 0)
                (*line_pos)--;
            else
            {
                *line = gui_line_get_prev_displayed (*line);
                if (*line)
                {
                    current_size = gui_chat_display_line (window, *line, 0, 1);
                    if (current_size == 0)
                        current_size = 1;
                    *line_pos = current_size - 1;
                }
            }
            difference++;
        }
        /* looking forward */
        else
        {
            if (*line_pos < current_size - 1)
                (*line_pos)++;
            else
            {
                *line = gui_line_get_next_displayed (*line);
                if (*line)
                {
                    current_size = gui_chat_display_line (window, *line, 0, 1);
                    if (current_size == 0)
                        current_size = 1;
                    *line_pos = 0;
                }
            }
            difference--;
        }
    }
    
    /* first or last line reached */
    if (!(*line))
    {
        if (backward)
        {
            /* first line reached */
            *line = gui_line_get_first_displayed (window->buffer);
            *line_pos = 0;
        }
        else
        {
            /* last line reached => consider we'll display all until the end */
            *line_pos = 0;
        }
    }
}

/*
 * gui_chat_draw: draw chat window for a buffer
 */

void
gui_chat_draw (struct t_gui_buffer *buffer, int erase)
{
    struct t_gui_window *ptr_win;
    struct t_gui_line *ptr_line;
    char format_empty[32];
    int i, line_pos, count, old_scrolling, old_lines_after;
    int y_start, y_end, y;
    
    if (!gui_ok)
        return;
    
    for (ptr_win = gui_windows; ptr_win; ptr_win = ptr_win->next_window)
    {
        if (ptr_win->buffer->number == buffer->number)
        {
            gui_window_coords_alloc (ptr_win);
            
            gui_chat_reset_style (ptr_win, NULL,
                                  GUI_COLOR_CHAT_INACTIVE_WINDOW,
                                  GUI_COLOR_CHAT_INACTIVE_BUFFER,
                                  GUI_COLOR_CHAT);
            
            if (erase)
            {
                snprintf (format_empty, 32, "%%-%ds", ptr_win->win_chat_width);
                for (i = 0; i < ptr_win->win_chat_height; i++)
                {
                    mvwprintw (GUI_WINDOW_OBJECTS(ptr_win)->win_chat, i, 0,
                               format_empty, " ");
                }
            }
            
            ptr_win->win_chat_cursor_x = 0;
            ptr_win->win_chat_cursor_y = 0;
            
            switch (ptr_win->buffer->type)
            {
                case GUI_BUFFER_TYPE_FORMATTED:
                    /* display at position of scrolling */
                    if (ptr_win->scroll->start_line)
                    {
                        ptr_line = ptr_win->scroll->start_line;
                        line_pos = ptr_win->scroll->start_line_pos;
                    }
                    else
                    {
                        /* look for first line to display, starting from last line */
                        ptr_line = NULL;
                        line_pos = 0;
                        gui_chat_calculate_line_diff (ptr_win, &ptr_line, &line_pos,
                                                      (-1) * (ptr_win->win_chat_height - 1));
                    }
                    
                    count = 0;
                    
                    if (line_pos > 0)
                    {
                        /* display end of first line at top of screen */
                        count = gui_chat_display_line (ptr_win, ptr_line,
                                                       gui_chat_display_line (ptr_win,
                                                                              ptr_line,
                                                                              0, 1) -
                                                       line_pos, 0);
                        ptr_line = gui_line_get_next_displayed (ptr_line);
                        ptr_win->scroll->first_line_displayed = 0;
                    }
                    else
                        ptr_win->scroll->first_line_displayed =
                            (ptr_line == gui_line_get_first_displayed (ptr_win->buffer));
                    
                    /* display lines */
                    while (ptr_line && (ptr_win->win_chat_cursor_y <= ptr_win->win_chat_height - 1))
                    {
                        count = gui_chat_display_line (ptr_win, ptr_line, 0, 0);
                        ptr_line = gui_line_get_next_displayed (ptr_line);
                    }
                    
                    old_scrolling = ptr_win->scroll->scrolling;
                    old_lines_after = ptr_win->scroll->lines_after;
                    
                    ptr_win->scroll->scrolling = (ptr_win->win_chat_cursor_y > ptr_win->win_chat_height - 1);
                    
                    /* check if last line of buffer is entirely displayed and scrolling */
                    /* if so, disable scroll indicator */
                    if (!ptr_line && ptr_win->scroll->scrolling)
                    {
                        if ((count == gui_chat_display_line (ptr_win, gui_line_get_last_displayed (ptr_win->buffer), 0, 1))
                            || (count == ptr_win->win_chat_height))
                            ptr_win->scroll->scrolling = 0;
                    }
                    
                    if (!ptr_win->scroll->scrolling
                        && (ptr_win->scroll->start_line == gui_line_get_first_displayed (ptr_win->buffer)))
                    {
                        ptr_win->scroll->start_line = NULL;
                        ptr_win->scroll->start_line_pos = 0;
                    }
                    
                    ptr_win->scroll->lines_after = 0;
                    if (ptr_win->scroll->scrolling && ptr_line)
                    {
                        /* count number of lines after last line displayed */
                        while (ptr_line)
                        {
                            ptr_line = gui_line_get_next_displayed (ptr_line);
                            if (ptr_line)
                                ptr_win->scroll->lines_after++;
                        }
                        ptr_win->scroll->lines_after++;
                    }
                    
                    if ((ptr_win->scroll->scrolling != old_scrolling)
                        || (ptr_win->scroll->lines_after != old_lines_after))
                    {
                        hook_signal_send ("window_scrolled",
                                          WEECHAT_HOOK_SIGNAL_POINTER, ptr_win);
                    }
                    
                    if (!ptr_win->scroll->scrolling
                        && ptr_win->scroll->reset_allowed)
                    {
                        ptr_win->scroll->start_line = NULL;
                        ptr_win->scroll->start_line_pos = 0;
                    }
                    
                    /* cursor is below end line of chat window? */
                    if (ptr_win->win_chat_cursor_y > ptr_win->win_chat_height - 1)
                    {
                        ptr_win->win_chat_cursor_x = 0;
                        ptr_win->win_chat_cursor_y = ptr_win->win_chat_height - 1;
                    }
                    
                    ptr_win->scroll->reset_allowed = 0;
                    
                    break;
                case GUI_BUFFER_TYPE_FREE:
                    /* display at position of scrolling */
                    ptr_line = (ptr_win->scroll->start_line) ?
                        ptr_win->scroll->start_line : buffer->lines->first_line;
                    if (ptr_line)
                    {
                        if (!ptr_line->data->displayed)
                            ptr_line = gui_line_get_next_displayed (ptr_line);
                        if (ptr_line)
                        {
                            y_start = (ptr_win->scroll->start_line) ? ptr_line->data->y : 0;
                            y_end = y_start + ptr_win->win_chat_height - 1;
                            while (ptr_line && (ptr_line->data->y <= y_end))
                            {
                                y = ptr_line->data->y - y_start;
                                if (y < ptr_win->coords_size)
                                {
                                    ptr_win->coords[y].line = ptr_line;
                                    ptr_win->coords[y].data = ptr_line->data->message;
                                }
                                if (ptr_line->data->refresh_needed || erase)
                                {
                                    gui_chat_display_line_y (ptr_win, ptr_line,
                                                             y);
                                }
                                ptr_line = gui_line_get_next_displayed (ptr_line);
                            }
                        }
                    }
                    break;
                case GUI_BUFFER_NUM_TYPES:
                    break;
            }
            wnoutrefresh (GUI_WINDOW_OBJECTS(ptr_win)->win_chat);
        }
    }
    
    refresh ();
    
    if (buffer->type == GUI_BUFFER_TYPE_FREE)
    {
        for (ptr_line = buffer->lines->first_line; ptr_line;
             ptr_line = ptr_line->next_line)
        {
            ptr_line->data->refresh_needed = 0;
        }
    }
    
    buffer->chat_refresh_needed = 0;
}

/*
 * gui_chat_draw_line: add a line to chat window for a buffer
 */

void
gui_chat_draw_line (struct t_gui_buffer *buffer, struct t_gui_line *line)
{
    /*
     * This function does nothing in Curses GUI,
     * line will be displayed by gui_buffer_draw_chat()
     */
    (void) buffer;
    (void) line;
}
