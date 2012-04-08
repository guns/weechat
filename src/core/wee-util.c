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
 * wee-util.c: some useful functions for WeeChat
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <signal.h>
#include <ctype.h>
#include <time.h>

#include "weechat.h"
#include "wee-util.h"
#include "wee-config.h"
#include "wee-string.h"
#include "wee-utf8.h"


/*
 * util_timeval_cmp: compare two timeval structures
 *                   return: -1 if tv1 < tv2
 *                            0 if tv1 == tv2
 *                            1 if tv1 > tv2
 */

int
util_timeval_cmp (struct timeval *tv1, struct timeval *tv2)
{
    if (tv1->tv_sec < tv2->tv_sec)
        return -1;
    if (tv1->tv_sec > tv2->tv_sec)
        return 1;
    if (tv1->tv_usec < tv2->tv_usec)
        return -1;
    if (tv1->tv_usec > tv2->tv_usec)
        return 1;
    return 0;
}

/*
 * util_timeval_diff: calculates difference between two times (return in
 *                    milliseconds)
 */

long
util_timeval_diff (struct timeval *tv1, struct timeval *tv2)
{
    long diff_sec, diff_usec;

    diff_sec = tv2->tv_sec - tv1->tv_sec;
    diff_usec = tv2->tv_usec - tv1->tv_usec;

    if (diff_usec < 0)
    {
        diff_usec += 1000000;
        diff_sec--;
    }
    return ((diff_usec / 1000) + (diff_sec * 1000));
}

/*
 * util_timeval_add: add interval (in milliseconds) to a timeval struct
 */

void
util_timeval_add (struct timeval *tv, long interval)
{
    long usec;

    tv->tv_sec += (interval / 1000);
    usec = tv->tv_usec + ((interval % 1000) * 1000);
    if (usec > 1000000)
    {
        tv->tv_usec = usec % 1000000;
        tv->tv_sec++;
    }
    else
        tv->tv_usec = usec;
}

/*
 * util_get_time_string: converts date to a string, using format of option
 *                       weechat.look.time_format (can be localized)
 */

char *
util_get_time_string (const time_t *date)
{
    struct tm *local_time;
    static char text_time[128];

    text_time[0] = '\0';
    local_time = localtime (date);
    if (local_time)
    {
        strftime (text_time, sizeof (text_time),
                  CONFIG_STRING(config_look_time_format), local_time);
    }

    return text_time;
}

/*
 * util_catch_signal: catch a signal
 */

void
util_catch_signal (int signum, void (*handler)(int))
{
    struct sigaction act;

    sigemptyset (&act.sa_mask);
    act.sa_flags = 0;
    act.sa_handler = handler;
    sigaction(signum, &act, NULL);
}

/*
 * util_mkdir_home: create a directory in WeeChat home
 *                  return 1 if ok, 0 if error
 */

int
util_mkdir_home (const char *directory, int mode)
{
    char *dir_name;
    int dir_length;

    if (!directory)
        return 0;

    /* build directory, adding WeeChat home */
    dir_length = strlen (weechat_home) + strlen (directory) + 2;
    dir_name = malloc (dir_length);
    if (!dir_name)
        return 0;

    snprintf (dir_name, dir_length, "%s/%s", weechat_home, directory);

    if (mkdir (dir_name, mode) < 0)
    {
        if (errno != EEXIST)
        {
            free (dir_name);
            return 0;
        }
    }

    free (dir_name);
    return 1;
}

/*
 * util_mkdir: create a directory
 *             return 1 if ok, 0 if error
 */

int
util_mkdir (const char *directory, int mode)
{
    if (!directory)
        return 0;

    if (mkdir (directory, mode) < 0)
    {
        if (errno != EEXIST)
            return 0;
    }

    return 1;
}

/*
 * util_mkdir_parents: create a directory and make parent directories as needed
 *                     return 1 if ok, 0 if error
 */

int
util_mkdir_parents (const char *directory, int mode)
{
    char *string, *ptr_string, *pos_sep;
    struct stat buf;
    int rc;

    if (!directory)
        return 0;

    string = strdup (directory);
    if (!string)
        return 0;

    ptr_string = string;
    while (ptr_string[0] == DIR_SEPARATOR_CHAR)
    {
        ptr_string++;
    }

    while (ptr_string && ptr_string[0])
    {
        pos_sep = strchr (ptr_string, DIR_SEPARATOR_CHAR);
        if (pos_sep)
            pos_sep[0] = '\0';

        rc = stat (string, &buf);
        if ((rc < 0) || !S_ISDIR(buf.st_mode))
        {
            /* try to create directory */
            if (!util_mkdir (string, mode))
            {
                free (string);
                return 0;
            }
        }

        if (pos_sep)
        {
            pos_sep[0] = DIR_SEPARATOR_CHAR;
            ptr_string = pos_sep + 1;
        }
        else
            ptr_string = NULL;
    }

    free (string);

    return 1;
}

/*
 * util_exec_on_files: find files in a directory and execute a
 *                     function on each file
 */

void
util_exec_on_files (const char *directory, int hidden_files, void *data,
                    void (*callback)(void *data, const char *filename))
{
    char complete_filename[1024];
    DIR *dir;
    struct dirent *entry;
    struct stat statbuf;

    if (!directory || !callback)
        return;

    dir = opendir (directory);
    if (dir)
    {
        while ((entry = readdir (dir)))
        {
            if (hidden_files || (entry->d_name[0] != '.'))
            {
                snprintf (complete_filename, sizeof (complete_filename) - 1,
                          "%s/%s", directory, entry->d_name);
                lstat (complete_filename, &statbuf);
                if (!S_ISDIR(statbuf.st_mode))
                {
                    (*callback) (data, complete_filename);
                }
            }
        }
        closedir (dir);
    }
}

/*
 *  util_search_full_lib_name: search the full name of a WeeChat library
 *                             file with name and extension
 *                            - look in WeeChat user's dir, then WeeChat
 *                              global lib dir
 *                            - sys_directory is the system directory under
 *                              WeeChat lib prefix, for example "plugins"
 *                            - result has to be free() after use (if not NULL)
 *                            - NULL is returned if lib is not found
 */

char *
util_search_full_lib_name_ext (const char *filename, const char *extension,
                               const char *plugins_dir)
{
    char *name_with_ext, *final_name;
    int length;
    struct stat st;

    length = strlen (filename) + strlen (extension) + 1;
    name_with_ext = malloc (length);
    if (!name_with_ext)
        return NULL;
    snprintf (name_with_ext, length,
              "%s%s",
              filename,
              (strchr (filename, '.')) ? "" : extension);

    /* try WeeChat user's dir */
    length = strlen (weechat_home) + strlen (name_with_ext) +
        strlen (plugins_dir) + 16;
    final_name = malloc (length);
    if (!final_name)
    {
        free (name_with_ext);
        return NULL;
    }
    snprintf (final_name, length,
              "%s%s%s%s%s",
              weechat_home,
              DIR_SEPARATOR,
              plugins_dir,
              DIR_SEPARATOR,
              name_with_ext);
    if ((stat (final_name, &st) == 0) && (st.st_size > 0))
    {
        free (name_with_ext);
        return final_name;
    }
    free (final_name);

    /* try WeeChat global lib dir */
    length = strlen (WEECHAT_LIBDIR) + strlen (name_with_ext) +
        strlen (plugins_dir) + 16;
    final_name = malloc (length);
    if (!final_name)
    {
        free (name_with_ext);
        return NULL;
    }
    snprintf (final_name, length,
              "%s%s%s%s%s",
              WEECHAT_LIBDIR,
              DIR_SEPARATOR,
              plugins_dir,
              DIR_SEPARATOR,
              name_with_ext);
    if ((stat (final_name, &st) == 0) && (st.st_size > 0))
    {
        free (name_with_ext);
        return final_name;
    }
    free (final_name);

    free (name_with_ext);

    return NULL;
}

/*
 * util_search_full_lib_name: search the full name of a WeeChat library
 *                            file with a part of name
 *                            - look in WeeChat user's dir, then WeeChat
 *                              global lib dir
 *                            - plugins_dir is the directory under WeeChat lib
 *                              prefix (for system dir) or under WeeChat home,
 *                              for example "plugins"
 *                            - result has to be free() after use (if not NULL)
 */

char *
util_search_full_lib_name (const char *filename, const char *plugins_dir)
{
    char *filename2, *full_name;
    int i;

    /* expand home in filename */
    filename2 = string_expand_home (filename);
    if (!filename2)
        return NULL;

    /* if full path, return it */
    if (strchr (filename2, '/') || strchr (filename2, '\\'))
        return filename2;

    if (config_plugin_extensions)
    {
        for (i = 0; i < config_num_plugin_extensions; i++)
        {
            full_name = util_search_full_lib_name_ext (filename2,
                                                       config_plugin_extensions[i],
                                                       plugins_dir);
            if (full_name)
            {
                free (filename2);
                return full_name;
            }
        }
    }
    else
    {
        full_name = util_search_full_lib_name_ext (filename2, "", plugins_dir);
        if (full_name)
        {
            free (filename2);
            return full_name;
        }
    }

    free (filename2);

    return strdup (filename);
}

/*
 * util_file_get_content: read the content of a file
 *                        return an allocated buffer with the file content
 *                        else NULL if an error occurred
 *                        (the buffer must be freed by the caller)
 */

char *
util_file_get_content (const char *filename)
{
    char *buffer, *buffer2;
    FILE *f;
    size_t count, fp;

    buffer = NULL;
    fp = 0;

    f = fopen (filename, "r");
    if (f)
    {
        while (!feof (f))
        {
            buffer2 = (char *) realloc (buffer, (fp + (1024 * sizeof (char))));
            if (!buffer2)
            {
                if (buffer)
                    free (buffer);
                return NULL;
            }
            buffer = buffer2;
            count = fread (&buffer[fp], sizeof(char), 1024, f);
            if (count <= 0)
            {
                free (buffer);
                return NULL;
            }
            fp += count;
        }
        buffer2 = (char *) realloc (buffer, fp + sizeof (char));
        if (!buffer2)
        {
            if (buffer)
                free (buffer);
            return NULL;
        }
        buffer = buffer2;
        buffer[fp] = '\0';
        fclose (f);
    }

    return buffer;
}

/*
 * util_version_number: get version number (integer) with version as string
 *                      (non-digit chars like "-dev" are ignored)
 *                      for example:
 *                        "0.3.2-dev" ==> 197120 (== 0x00030200)
 *                        "0.3.2-rc1" ==> 197120 (== 0x00030200)
 *                        "0.3.2"     ==> 197120 (== 0x00030200)
 *                        "0.3.1.1"   ==> 196865 (== 0x00030101)
 *                        "0.3.1"     ==> 196864 (== 0x00030100)
 *                        "0.3.0"     ==> 196608 (== 0x00030000)
 */

int
util_version_number (const char *version)
{
    char **items, buf[64], *ptr_item, *error;
    int num_items, i, version_int[4], index_buf;
    long number;

    items = string_split (version, ".", 0, 4, &num_items);
    for (i = 0; i < 4; i++)
    {
        version_int[i] = 0;
        if (items && (i < num_items))
        {
            ptr_item = items[i];
            index_buf = 0;
            while (ptr_item && ptr_item[0] && (index_buf < (int)sizeof (buf) - 1))
            {
                if (ptr_item[0] == '-')
                    break;
                if (isdigit ((unsigned char)ptr_item[0]))
                {
                    buf[index_buf] = ptr_item[0];
                    index_buf++;
                }
                ptr_item = utf8_next_char (ptr_item);
            }
            buf[index_buf] = '\0';
            if (buf[0])
            {
                error = NULL;
                number = strtol (buf, &error, 10);
                if (error && !error[0])
                {
                    if (number < 0)
                        number = 0;
                    else if (number > 0xFF)
                        number = 0xFF;
                    version_int[i] = number;
                }
            }
        }
    }
    if (items)
        string_free_split (items);

    return (version_int[0] << 24) | (version_int[1] << 16)
        | (version_int[2] << 8) | version_int[3];
}
