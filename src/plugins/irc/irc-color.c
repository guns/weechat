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
 * irc-color.c: IRC color decoding/encoding in messages
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "../weechat-plugin.h"
#include "irc.h"
#include "irc-color.h"
#include "irc-config.h"


char *irc_color_to_weechat[IRC_NUM_COLORS] =
{ /*  0 */ "white",
  /*  1 */ "black",
  /*  2 */ "blue",
  /*  3 */ "green",
  /*  4 */ "lightred",
  /*  5 */ "red",
  /*  6 */ "magenta",
  /*  7 */ "brown",
  /*  8 */ "yellow",
  /*  9 */ "lightgreen",
  /* 10 */ "cyan",
  /* 11 */ "lightcyan",
  /* 12 */ "lightblue",
  /* 13 */ "lightmagenta",
  /* 14 */ "gray",
  /* 15 */ "white"
};


/*
 * irc_color_decode: replace IRC colors by WeeChat colors
 *                   if keep_colors == 0: remove any color/style in message
 *                   otherwise: keep colors
 *                   Note: after use, string returned has to be free()
 */

char *
irc_color_decode (const char *string, int keep_colors)
{
    unsigned char *out, *ptr_string;
    int out_length, length, out_pos;
    char str_fg[3], str_bg[3], str_color[128], str_key[128];
    const char *remapped_color;
    int fg, bg, bold, reverse, italic, underline, rc;

    out_length = (strlen (string) * 2) + 1;
    out = malloc (out_length);
    if (!out)
        return NULL;

    bold = 0;
    reverse = 0;
    italic = 0;
    underline = 0;

    ptr_string = (unsigned char *)string;
    out[0] = '\0';
    while (ptr_string && ptr_string[0])
    {
        switch (ptr_string[0])
        {
            case IRC_COLOR_BOLD_CHAR:
                if (keep_colors)
                    strcat ((char *)out,
                            weechat_color((bold) ? "-bold" : "bold"));
                bold ^= 1;
                ptr_string++;
                break;
            case IRC_COLOR_RESET_CHAR:
                if (keep_colors)
                    strcat ((char *)out, weechat_color("reset"));
                bold = 0;
                reverse = 0;
                italic = 0;
                underline = 0;
                ptr_string++;
                break;
            case IRC_COLOR_FIXED_CHAR:
                ptr_string++;
                break;
            case IRC_COLOR_REVERSE_CHAR:
            case IRC_COLOR_REVERSE2_CHAR:
                if (keep_colors)
                    strcat ((char *)out,
                            weechat_color((reverse) ? "-reverse" : "reverse"));
                reverse ^= 1;
                ptr_string++;
                break;
            case IRC_COLOR_ITALIC_CHAR:
                if (keep_colors)
                    strcat ((char *)out,
                            weechat_color((italic) ? "-italic" : "italic"));
                italic ^= 1;
                ptr_string++;
                break;
            case IRC_COLOR_UNDERLINE_CHAR:
                if (keep_colors)
                    strcat ((char *)out,
                            weechat_color((underline) ? "-underline" : "underline"));
                underline ^= 1;
                ptr_string++;
                break;
            case IRC_COLOR_COLOR_CHAR:
                ptr_string++;
                str_fg[0] = '\0';
                str_bg[0] = '\0';
                if (isdigit (ptr_string[0]))
                {
                    str_fg[0] = ptr_string[0];
                    str_fg[1] = '\0';
                    ptr_string++;
                    if (isdigit (ptr_string[0]))
                    {
                        str_fg[1] = ptr_string[0];
                        str_fg[2] = '\0';
                        ptr_string++;
                    }
                }
                if ((ptr_string[0] == ',') && (isdigit (ptr_string[1])))
                {
                    ptr_string++;
                    str_bg[0] = ptr_string[0];
                    str_bg[1] = '\0';
                    ptr_string++;
                    if (isdigit (ptr_string[0]))
                    {
                        str_bg[1] = ptr_string[0];
                        str_bg[2] = '\0';
                        ptr_string++;
                    }
                }
                if (keep_colors)
                {
                    if (str_fg[0] || str_bg[0])
                    {
                        fg = -1;
                        bg = -1;
                        if (str_fg[0])
                        {
                            rc = sscanf (str_fg, "%d", &fg);
                            if ((rc != EOF) && (rc >= 1))
                            {
                                fg %= IRC_NUM_COLORS;
                            }
                        }
                        if (str_bg[0])
                        {
                            rc = sscanf (str_bg, "%d", &bg);
                            if ((rc != EOF) && (rc >= 1))
                            {
                                bg %= IRC_NUM_COLORS;
                            }
                        }
                        /* search "fg,bg" in hashtable of remapped colors */
                        snprintf (str_key, sizeof (str_key), "%d,%d", fg, bg);
                        remapped_color = weechat_hashtable_get (
                            irc_config_hashtable_color_mirc_remap,
                            str_key);
                        if (remapped_color)
                        {
                            snprintf (str_color, sizeof (str_color),
                                      "|%s", remapped_color);
                        }
                        else
                        {
                            snprintf (str_color, sizeof (str_color),
                                      "|%s%s%s",
                                      (fg >= 0) ? irc_color_to_weechat[fg] : "",
                                      (bg >= 0) ? "," : "",
                                      (bg >= 0) ? irc_color_to_weechat[bg] : "");
                        }
                        strcat ((char *)out, weechat_color(str_color));
                    }
                    else
                        strcat ((char *)out, weechat_color("resetcolor"));
                }
                break;
            default:
                length = weechat_utf8_char_size ((char *)ptr_string);
                if (length == 0)
                    length = 1;
                out_pos = strlen ((char *)out);
                memcpy (out + out_pos, ptr_string, length);
                out[out_pos + length] = '\0';
                ptr_string += length;
                break;
        }
    }

    return (char *)out;
}

/*
 * irc_color_decode_for_user_entry: parses a message (coming from IRC server),
 *                                  and replaces colors/bold/.. by ^C, ^B, ..
 *                                  Note: after use, string returned has to be free()
 */

char *
irc_color_decode_for_user_entry (const char *string)
{
    unsigned char *out, *ptr_string;
    int out_length, out_pos, length;

    out_length = (strlen (string) * 2) + 1;
    out = malloc (out_length);
    if (!out)
        return NULL;

    ptr_string = (unsigned char *)string;
    out_pos = 0;
    while (ptr_string && ptr_string[0] && (out_pos < out_length - 1))
    {
        switch (ptr_string[0])
        {
            case IRC_COLOR_BOLD_CHAR:
                out[out_pos++] = 0x02;
                ptr_string++;
                break;
            case IRC_COLOR_FIXED_CHAR:
                ptr_string++;
                break;
            case IRC_COLOR_RESET_CHAR:
                out[out_pos++] = 0x0F;
                ptr_string++;
                break;
            case IRC_COLOR_REVERSE_CHAR:
            case IRC_COLOR_REVERSE2_CHAR:
                out[out_pos++] = 0x12;
                ptr_string++;
                break;
            case IRC_COLOR_ITALIC_CHAR:
                out[out_pos++] = 0x1D;
                ptr_string++;
                break;
            case IRC_COLOR_UNDERLINE_CHAR:
                out[out_pos++] = 0x15;
                ptr_string++;
                break;
            case IRC_COLOR_COLOR_CHAR:
                out[out_pos++] = 0x03;
                ptr_string++;
                break;
            default:
                length = weechat_utf8_char_size ((char *)ptr_string);
                if (length == 0)
                    length = 1;
                memcpy (out + out_pos, ptr_string, length);
                out_pos += length;
                ptr_string += length;
        }
    }

    out[out_pos] = '\0';

    return (char *)out;
}

/*
 * irc_color_encode: parses a message (entered by user), and
 *                   encode special chars (^Cb, ^Cc, ..) in IRC colors
 *                   if keep_colors == 0: remove any color/style in message
 *                   otherwise: keep colors
 *                   Note: after use, string returned has to be free()
 */

char *
irc_color_encode (const char *string, int keep_colors)
{
    unsigned char *out, *ptr_string;
    int out_length, out_pos, length;

    out_length = (strlen (string) * 2) + 1;
    out = malloc (out_length);
    if (!out)
        return NULL;

    ptr_string = (unsigned char *)string;
    out_pos = 0;
    while (ptr_string && ptr_string[0] && (out_pos < out_length - 1))
    {
        switch (ptr_string[0])
        {
            case 0x02: /* ^B */
                if (keep_colors)
                    out[out_pos++] = IRC_COLOR_BOLD_CHAR;
                ptr_string++;
                break;
            case 0x03: /* ^C */
                if (keep_colors)
                    out[out_pos++] = IRC_COLOR_COLOR_CHAR;
                ptr_string++;
                if (isdigit (ptr_string[0]))
                {
                    if (keep_colors)
                        out[out_pos++] = ptr_string[0];
                    ptr_string++;
                    if (isdigit (ptr_string[0]))
                    {
                        if (keep_colors)
                            out[out_pos++] = ptr_string[0];
                        ptr_string++;
                    }
                }
                if (ptr_string[0] == ',')
                {
                    if (keep_colors)
                        out[out_pos++] = ',';
                    ptr_string++;
                    if (isdigit (ptr_string[0]))
                    {
                        if (keep_colors)
                            out[out_pos++] = ptr_string[0];
                        ptr_string++;
                        if (isdigit (ptr_string[0]))
                        {
                            if (keep_colors)
                                out[out_pos++] = ptr_string[0];
                            ptr_string++;
                        }
                    }
                }
                break;
            case 0x0F: /* ^O */
                if (keep_colors)
                    out[out_pos++] = IRC_COLOR_RESET_CHAR;
                ptr_string++;
                break;
            case 0x12: /* ^R */
                if (keep_colors)
                    out[out_pos++] = IRC_COLOR_REVERSE_CHAR;
                ptr_string++;
                break;
            case 0x15: /* ^U */
                if (keep_colors)
                    out[out_pos++] = IRC_COLOR_UNDERLINE_CHAR;
                ptr_string++;
                break;
            default:
                length = weechat_utf8_char_size ((char *)ptr_string);
                if (length == 0)
                    length = 1;
                memcpy (out + out_pos, ptr_string, length);
                out_pos += length;
                ptr_string += length;
        }
    }

    out[out_pos] = '\0';

    return (char *)out;
}

/*
 * irc_color_modifier_cb: callback for modifiers "irc_color_decode" and
 *                        "irc_color_encode"
 *                        This modifier can be used by other plugins to
 *                        decode/encode IRC colors in messages
 */

char *
irc_color_modifier_cb (void *data, const char *modifier,
                       const char *modifier_data, const char *string)
{
    int keep_colors;

    /* make C compiler happy */
    (void) data;

    keep_colors = (modifier_data && (strcmp (modifier_data, "1") == 0)) ? 1 : 0;

    if (strcmp (modifier, "irc_color_decode") == 0)
        return irc_color_decode (string, keep_colors);

    if (strcmp (modifier, "irc_color_encode") == 0)
        return irc_color_encode (string, keep_colors);

    /* unknown modifier */
    return NULL;
}

/*
 * irc_color_for_tags: return color name for tags (replace "," by ":")
 *                     Note: result must be freed after use
 */

char *
irc_color_for_tags (const char *color)
{
    if (!color)
        return NULL;

    return weechat_string_replace (color, ",", ":");
}
