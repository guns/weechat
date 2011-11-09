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
 * script-api.c: script API functions, used by script plugins (perl/python/..)
 */

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#include "../weechat-plugin.h"
#include "script.h"
#include "script-api.h"
#include "script-callback.h"


/*
 * script_api_charset_set: set charset for script
 */

void
script_api_charset_set (struct t_plugin_script *script,
                        const char *charset)
{
    if (script->charset)
        free (script->charset);

    script->charset = (charset) ? strdup (charset) : NULL;
}

/*
 * script_api_config_new: create a new configuration file
 *                        return new configuration file, NULL if error
 */

struct t_config_file *
script_api_config_new (struct t_weechat_plugin *weechat_plugin,
                       struct t_plugin_script *script,
                       const char *name,
                       int (*callback_reload)(void *data,
                                              struct t_config_file *config_file),
                       const char *function,
                       const char *data)
{
    struct t_script_callback *new_script_callback;
    struct t_config_file *new_config_file;

    if (function && function[0])
    {
        new_script_callback = script_callback_alloc ();
        if (!new_script_callback)
            return NULL;

        new_config_file = weechat_config_new (name, callback_reload,
                                              new_script_callback);
        if (!new_config_file)
        {
            script_callback_free_data (new_script_callback);
            free (new_script_callback);
            return NULL;
        }

        script_callback_init (new_script_callback, script, function, data);
        new_script_callback->config_file = new_config_file;

        script_callback_add (script, new_script_callback);
    }
    else
    {
        new_config_file = weechat_config_new (name, NULL, NULL);
    }

    return new_config_file;
}

/*
 * script_api_config_new_section: create a new section in configuration file
 *                                return new section, NULL if error
 */

struct t_config_section *
script_api_config_new_section (struct t_weechat_plugin *weechat_plugin,
                               struct t_plugin_script *script,
                               struct t_config_file *config_file,
                               const char *name,
                               int user_can_add_options,
                               int user_can_delete_options,
                               int (*callback_read)(void *data,
                                                    struct t_config_file *config_file,
                                                    struct t_config_section *section,
                                                    const char *option_name,
                                                    const char *value),
                               const char *function_read,
                               const char *data_read,
                               int (*callback_write)(void *data,
                                                     struct t_config_file *config_file,
                                                     const char *section_name),
                               const char *function_write,
                               const char *data_write,
                               int (*callback_write_default)(void *data,
                                                             struct t_config_file *config_file,
                                                             const char *section_name),
                               const char *function_write_default,
                               const char *data_write_default,
                               int (*callback_create_option)(void *data,
                                                             struct t_config_file *config_file,
                                                             struct t_config_section *section,
                                                             const char *option_name,
                                                             const char *value),
                               const char *function_create_option,
                               const char *data_create_option,
                               int (*callback_delete_option)(void *data,
                                                             struct t_config_file *config_file,
                                                             struct t_config_section *section,
                                                             struct t_config_option *option),
                               const char *function_delete_option,
                               const char *data_delete_option)
{
    struct t_script_callback *new_script_callback1, *new_script_callback2;
    struct t_script_callback *new_script_callback3, *new_script_callback4;
    struct t_script_callback *new_script_callback5;
    struct t_config_section *new_section;
    void *callback1, *callback2, *callback3, *callback4, *callback5;

    new_script_callback1 = NULL;
    new_script_callback2 = NULL;
    new_script_callback3 = NULL;
    new_script_callback4 = NULL;
    new_script_callback5 = NULL;
    callback1 = NULL;
    callback2 = NULL;
    callback3 = NULL;
    callback4 = NULL;
    callback5 = NULL;

    if (function_read && function_read[0])
    {
        new_script_callback1 = script_callback_alloc ();
        if (!new_script_callback1)
            return NULL;
        callback1 = callback_read;
    }

    if (function_write && function_write[0])
    {
        new_script_callback2 = script_callback_alloc ();
        if (!new_script_callback2)
        {
            if (new_script_callback1)
            {
                script_callback_free_data (new_script_callback1);
                free (new_script_callback1);
            }
            return NULL;
        }
        callback2 = callback_write;
    }

    if (function_write_default && function_write_default[0])
    {
        new_script_callback3 = script_callback_alloc ();
        if (!new_script_callback3)
        {
            if (new_script_callback1)
            {
                script_callback_free_data (new_script_callback1);
                free (new_script_callback1);
            }
            if (new_script_callback2)
            {
                script_callback_free_data (new_script_callback2);
                free (new_script_callback2);
            }
            return NULL;
        }
        callback3 = callback_write_default;
    }

    if (function_create_option && function_create_option[0])
    {
        new_script_callback4 = script_callback_alloc ();
        if (!new_script_callback4)
        {
            if (new_script_callback1)
            {
                script_callback_free_data (new_script_callback1);
                free (new_script_callback1);
            }
            if (new_script_callback2)
            {
                script_callback_free_data (new_script_callback2);
                free (new_script_callback2);
            }
            if (new_script_callback3)
            {
                script_callback_free_data (new_script_callback3);
                free (new_script_callback3);
            }
            return NULL;
        }
        callback4 = callback_create_option;
    }

    if (function_delete_option && function_delete_option[0])
    {
        new_script_callback5 = script_callback_alloc ();
        if (!new_script_callback5)
        {
            if (new_script_callback1)
            {
                script_callback_free_data (new_script_callback1);
                free (new_script_callback1);
            }
            if (new_script_callback2)
            {
                script_callback_free_data (new_script_callback2);
                free (new_script_callback2);
            }
            if (new_script_callback3)
            {
                script_callback_free_data (new_script_callback3);
                free (new_script_callback3);
            }
            if (new_script_callback4)
            {
                script_callback_free_data (new_script_callback4);
                free (new_script_callback4);
            }
            return NULL;
        }
        callback5 = callback_delete_option;
    }

    new_section = weechat_config_new_section (config_file,
                                              name,
                                              user_can_add_options,
                                              user_can_delete_options,
                                              callback1,
                                              new_script_callback1,
                                              callback2,
                                              new_script_callback2,
                                              callback3,
                                              new_script_callback3,
                                              callback4,
                                              new_script_callback4,
                                              callback5,
                                              new_script_callback5);
    if (!new_section)
    {
        if (new_script_callback1)
        {
            script_callback_free_data (new_script_callback1);
            free (new_script_callback1);
        }
        if (new_script_callback2)
        {
            script_callback_free_data (new_script_callback2);
            free (new_script_callback2);
        }
        if (new_script_callback3)
        {
            script_callback_free_data (new_script_callback3);
            free (new_script_callback3);
        }
        if (new_script_callback4)
        {
            script_callback_free_data (new_script_callback4);
            free (new_script_callback4);
        }
        if (new_script_callback5)
        {
            script_callback_free_data (new_script_callback5);
            free (new_script_callback5);
        }
        return NULL;
    }

    if (new_script_callback1)
    {
        script_callback_init (new_script_callback1, script,
                              function_read, data_read);
        new_script_callback1->config_file = config_file;
        new_script_callback1->config_section = new_section;
        script_callback_add (script, new_script_callback1);
    }

    if (new_script_callback2)
    {
        script_callback_init (new_script_callback2, script,
                              function_write, data_write);
        new_script_callback2->config_file = config_file;
        new_script_callback2->config_section = new_section;
        script_callback_add (script, new_script_callback2);
    }

    if (new_script_callback3)
    {
        script_callback_init (new_script_callback3, script,
                              function_write_default, data_write_default);
        new_script_callback3->config_file = config_file;
        new_script_callback3->config_section = new_section;
        script_callback_add (script, new_script_callback3);
    }

    if (new_script_callback4)
    {
        script_callback_init (new_script_callback4, script,
                              function_create_option, data_create_option);
        new_script_callback4->config_file = config_file;
        new_script_callback4->config_section = new_section;
        script_callback_add (script, new_script_callback4);
    }

    if (new_script_callback5)
    {
        script_callback_init (new_script_callback5, script,
                              function_delete_option, data_delete_option);
        new_script_callback5->config_file = config_file;
        new_script_callback5->config_section = new_section;
        script_callback_add (script, new_script_callback5);
    }

    return new_section;
}

/*
 * script_api_config_new_option: create a new option in section
 *                               return new option, NULL if error
 */

struct t_config_option *
script_api_config_new_option (struct t_weechat_plugin *weechat_plugin,
                              struct t_plugin_script *script,
                              struct t_config_file *config_file,
                              struct t_config_section *section,
                              const char *name, const char *type,
                              const char *description, const char *string_values,
                              int min, int max,
                              const char *default_value,
                              const char *value,
                              int null_value_allowed,
                              int (*callback_check_value)(void *data,
                                                          struct t_config_option *option,
                                                          const char *value),
                              const char *function_check_value,
                              const char *data_check_value,
                              void (*callback_change)(void *data,
                                                      struct t_config_option *option),
                              const char *function_change,
                              const char *data_change,
                              void (*callback_delete)(void *data,
                                                      struct t_config_option *option),
                              const char *function_delete,
                              const char *data_delete)
{
    struct t_script_callback *new_script_callback1, *new_script_callback2;
    struct t_script_callback *new_script_callback3;
    void *callback1, *callback2, *callback3;
    struct t_config_option *new_option;

    new_script_callback1 = NULL;
    new_script_callback2 = NULL;
    new_script_callback3 = NULL;
    callback1 = NULL;
    callback2 = NULL;
    callback3 = NULL;

    if (function_check_value && function_check_value[0])
    {
        new_script_callback1 = script_callback_alloc ();
        if (!new_script_callback1)
            return NULL;
        callback1 = callback_check_value;
    }

    if (function_change && function_change[0])
    {
        new_script_callback2 = script_callback_alloc ();
        if (!new_script_callback2)
        {
            if (new_script_callback1)
            {
                script_callback_free_data (new_script_callback1);
                free (new_script_callback1);
            }
            return NULL;
        }
        callback2 = callback_change;
    }

    if (function_delete && function_delete[0])
    {
        new_script_callback3 = script_callback_alloc ();
        if (!new_script_callback3)
        {
            if (new_script_callback1)
            {
                script_callback_free_data (new_script_callback1);
                free (new_script_callback1);
            }
            if (new_script_callback2)
            {
                script_callback_free_data (new_script_callback2);
                free (new_script_callback2);
            }
            return NULL;
        }
        callback3 = callback_delete;
    }

    new_option = weechat_config_new_option (config_file, section, name, type,
                                            description, string_values, min,
                                            max, default_value, value,
                                            null_value_allowed,
                                            callback1, new_script_callback1,
                                            callback2, new_script_callback2,
                                            callback3, new_script_callback3);
    if (!new_option)
    {
        if (new_script_callback1)
        {
            script_callback_free_data (new_script_callback1);
            free (new_script_callback1);
        }
        if (new_script_callback2)
        {
            script_callback_free_data (new_script_callback2);
            free (new_script_callback2);
        }
        if (new_script_callback3)
        {
            script_callback_free_data (new_script_callback3);
            free (new_script_callback3);
        }
        return NULL;
    }

    if (new_script_callback1)
    {
        script_callback_init (new_script_callback1, script,
                              function_check_value, data_check_value);
        new_script_callback1->config_file = config_file;
        new_script_callback1->config_section = section;
        new_script_callback1->config_option = new_option;
        script_callback_add (script, new_script_callback1);
    }

    if (new_script_callback2)
    {
        script_callback_init (new_script_callback2, script,
                              function_change, data_change);
        new_script_callback2->config_file = config_file;
        new_script_callback2->config_section = section;
        new_script_callback2->config_option = new_option;
        script_callback_add (script, new_script_callback2);
    }

    if (new_script_callback3)
    {
        script_callback_init (new_script_callback3, script,
                              function_delete, data_delete);
        new_script_callback3->config_file = config_file;
        new_script_callback3->config_section = section;
        new_script_callback3->config_option = new_option;
        script_callback_add (script, new_script_callback3);
    }

    return new_option;
}

/*
 * script_api_config_option_free: free an option in configuration file
 */

void
script_api_config_option_free (struct t_weechat_plugin *weechat_plugin,
                               struct t_plugin_script *script,
                               struct t_config_option *option)
{
    struct t_script_callback *ptr_script_callback, *next_callback;

    if (!weechat_plugin || !script || !option)
        return;

    weechat_config_option_free (option);

    ptr_script_callback = script->callbacks;
    while (ptr_script_callback)
    {
        next_callback = ptr_script_callback->next_callback;

        if (ptr_script_callback->config_option == option)
            script_callback_remove (script, ptr_script_callback);

        ptr_script_callback = next_callback;
    }
}

/*
 * script_api_config_section_free_options: free all options of a section in
 *                                         configuration file
 */

void
script_api_config_section_free_options (struct t_weechat_plugin *weechat_plugin,
                                        struct t_plugin_script *script,
                                        struct t_config_section *section)
{
    struct t_script_callback *ptr_script_callback, *next_callback;

    if (!weechat_plugin || !script || !section)
        return;

    weechat_config_section_free_options (section);

    ptr_script_callback = script->callbacks;
    while (ptr_script_callback)
    {
        next_callback = ptr_script_callback->next_callback;

        if ((ptr_script_callback->config_section == section)
            && ptr_script_callback->config_option)
            script_callback_remove (script, ptr_script_callback);

        ptr_script_callback = next_callback;
    }
}

/*
 * script_api_config_section_free: free a section in configuration file
 */

void
script_api_config_section_free (struct t_weechat_plugin *weechat_plugin,
                                struct t_plugin_script *script,
                                struct t_config_section *section)
{
    struct t_script_callback *ptr_script_callback, *next_callback;

    if (!weechat_plugin || !script || !section)
        return;

    weechat_config_section_free (section);

    ptr_script_callback = script->callbacks;
    while (ptr_script_callback)
    {
        next_callback = ptr_script_callback->next_callback;

        if (ptr_script_callback->config_section == section)
            script_callback_remove (script, ptr_script_callback);

        ptr_script_callback = next_callback;
    }
}

/*
 * script_api_config_free: free configuration file
 */

void
script_api_config_free (struct t_weechat_plugin *weechat_plugin,
                        struct t_plugin_script *script,
                        struct t_config_file *config_file)
{
    struct t_script_callback *ptr_script_callback, *next_callback;

    if (!weechat_plugin || !script || !config_file)
        return;

    weechat_config_free (config_file);

    ptr_script_callback = script->callbacks;
    while (ptr_script_callback)
    {
        next_callback = ptr_script_callback->next_callback;

        if (ptr_script_callback->config_file == config_file)
            script_callback_remove (script, ptr_script_callback);

        ptr_script_callback = next_callback;
    }
}

/*
 * script_api_printf: print a message
 */

void
script_api_printf (struct t_weechat_plugin *weechat_plugin,
                   struct t_plugin_script *script,
                   struct t_gui_buffer *buffer, const char *format, ...)
{
    char *buf2;

    weechat_va_format (format);
    if (!vbuffer)
        return;

    buf2 = (script && script->charset && script->charset[0]) ?
        weechat_iconv_to_internal (script->charset, vbuffer) : NULL;
    weechat_printf (buffer, "%s", (buf2) ? buf2 : vbuffer);
    if (buf2)
        free (buf2);

    free (vbuffer);
}

/*
 * script_api_printf_date_tags: print a message with optional date and tags
 */

void
script_api_printf_date_tags (struct t_weechat_plugin *weechat_plugin,
                             struct t_plugin_script *script,
                             struct t_gui_buffer *buffer,
                             time_t date, const char *tags,
                             const char *format, ...)
{
    char *buf2;

    weechat_va_format (format);
    if (!vbuffer)
        return;

    buf2 = (script->charset && script->charset[0]) ?
        weechat_iconv_to_internal (script->charset, vbuffer) : NULL;
    weechat_printf_date_tags (buffer, date, tags,
                              "%s", (buf2) ? buf2 : vbuffer);
    if (buf2)
        free (buf2);

    free (vbuffer);
}

/*
 * script_api_printf_y: print a message on a buffer with free content
 */

void
script_api_printf_y (struct t_weechat_plugin *weechat_plugin,
                     struct t_plugin_script *script,
                     struct t_gui_buffer *buffer, int y,
                     const char *format, ...)
{
    char *buf2;

    weechat_va_format (format);
    if (!vbuffer)
        return;

    buf2 = (script->charset && script->charset[0]) ?
        weechat_iconv_to_internal (script->charset, vbuffer) : NULL;
    weechat_printf_y (buffer, y, "%s", (buf2) ? buf2 : vbuffer);
    if (buf2)
        free (buf2);

    free (vbuffer);
}

/*
 * script_api_log_printf: add a message in WeeChat log file
 */

void
script_api_log_printf (struct t_weechat_plugin *weechat_plugin,
                       struct t_plugin_script *script,
                       const char *format, ...)
{
    char *buf2;

    weechat_va_format (format);
    if (!vbuffer)
        return;

    buf2 = (script->charset && script->charset[0]) ?
        weechat_iconv_to_internal (script->charset, vbuffer) : NULL;
    weechat_log_printf ("%s", (buf2) ? buf2 : vbuffer);
    if (buf2)
        free (buf2);

    free (vbuffer);
}

/*
 * script_api_hook_command: hook a command
 *                          return new hook, NULL if error
 */

struct t_hook *
script_api_hook_command (struct t_weechat_plugin *weechat_plugin,
                         struct t_plugin_script *script,
                         const char *command, const char *description,
                         const char *args, const char *args_description,
                         const char *completion,
                         int (*callback)(void *data,
                                         struct t_gui_buffer *buffer,
                                         int argc, char **argv,
                                         char **argv_eol),
                         const char *function,
                         const char *data)
{
    struct t_script_callback *new_script_callback;
    struct t_hook *new_hook;

    new_script_callback = script_callback_alloc ();
    if (!new_script_callback)
        return NULL;

    new_hook = weechat_hook_command (command, description, args,
                                     args_description, completion,
                                     callback, new_script_callback);
    if (!new_hook)
    {
        script_callback_free_data (new_script_callback);
        free (new_script_callback);
        return NULL;
    }

    script_callback_init (new_script_callback, script, function, data);
    new_script_callback->hook = new_hook;

    script_callback_add (script, new_script_callback);

    return new_hook;
}

/*
 * script_api_hook_command_run: hook a command_run
 *                              return new hook, NULL if error
 */

struct t_hook *
script_api_hook_command_run (struct t_weechat_plugin *weechat_plugin,
                             struct t_plugin_script *script,
                             const char *command,
                             int (*callback)(void *data,
                                             struct t_gui_buffer *buffer,
                                             const char *command),
                             const char *function,
                             const char *data)
{
    struct t_script_callback *new_script_callback;
    struct t_hook *new_hook;

    new_script_callback = script_callback_alloc ();
    if (!new_script_callback)
        return NULL;

    new_hook = weechat_hook_command_run (command,
                                         callback, new_script_callback);
    if (!new_hook)
    {
        script_callback_free_data (new_script_callback);
        free (new_script_callback);
        return NULL;
    }

    script_callback_init (new_script_callback, script, function, data);
    new_script_callback->hook = new_hook;

    script_callback_add (script, new_script_callback);

    return new_hook;
}

/*
 * script_api_hook_timer: hook a timer
 *                        return new hook, NULL if error
 */

struct t_hook *
script_api_hook_timer (struct t_weechat_plugin *weechat_plugin,
                       struct t_plugin_script *script,
                       int interval, int align_second, int max_calls,
                       int (*callback)(void *data,
                                       int remaining_calls),
                       const char *function,
                       const char *data)
{
    struct t_script_callback *new_script_callback;
    struct t_hook *new_hook;

    new_script_callback = script_callback_alloc ();
    if (!new_script_callback)
        return NULL;

    new_hook = weechat_hook_timer (interval, align_second, max_calls,
                                   callback, new_script_callback);
    if (!new_hook)
    {
        script_callback_free_data (new_script_callback);
        free (new_script_callback);
        return NULL;
    }

    script_callback_init (new_script_callback, script, function, data);
    new_script_callback->hook = new_hook;

    script_callback_add (script, new_script_callback);

    return new_hook;
}

/*
 * script_api_hook_fd: hook a fd
 *                     return new hook, NULL if error
 */

struct t_hook *
script_api_hook_fd (struct t_weechat_plugin *weechat_plugin,
                    struct t_plugin_script *script,
                    int fd, int flag_read, int flag_write,
                    int flag_exception,
                    int (*callback)(void *data, int fd),
                    const char *function,
                    const char *data)
{
    struct t_script_callback *new_script_callback;
    struct t_hook *new_hook;

    new_script_callback = script_callback_alloc ();
    if (!new_script_callback)
        return NULL;

    new_hook = weechat_hook_fd (fd, flag_read, flag_write, flag_exception,
                                callback, new_script_callback);
    if (!new_hook)
    {
        script_callback_free_data (new_script_callback);
        free (new_script_callback);
        return NULL;
    }

    script_callback_init (new_script_callback, script, function, data);
    new_script_callback->hook = new_hook;

    script_callback_add (script, new_script_callback);

    return new_hook;
}

/*
 * script_api_hook_connect: hook a connection
 *                          return new hook, NULL if error
 */

struct t_hook *
script_api_hook_process (struct t_weechat_plugin *weechat_plugin,
                         struct t_plugin_script *script,
                         const char *command,
                         int timeout,
                         int (*callback)(void *data,
                                         const char *command,
                                         int return_code,
                                         const char *out,
                                         const char *err),
                         const char *function,
                         const char *data)
{
    struct t_script_callback *new_script_callback;
    struct t_hook *new_hook;

    new_script_callback = script_callback_alloc ();
    if (!new_script_callback)
        return NULL;

    script_callback_init (new_script_callback, script, function, data);
    script_callback_add (script, new_script_callback);

    new_hook = weechat_hook_process (command, timeout, callback,
                                     new_script_callback);

    if (!new_hook)
    {
        script_callback_remove (script, new_script_callback);
        return NULL;
    }

    new_script_callback->hook = new_hook;

    return new_hook;
}

/*
 * script_api_hook_connect: hook a connection
 *                          return new hook, NULL if error
 */

struct t_hook *
script_api_hook_connect (struct t_weechat_plugin *weechat_plugin,
                         struct t_plugin_script *script,
                         const char *proxy, const char *address, int port,
                         int sock, int ipv6, void *gnutls_sess,
                         void *gnutls_cb, int gnutls_dhkey_size,
                         const char *gnutls_priorities,
                         const char *local_hostname,
                         int (*callback)(void *data, int status,
                                         int gnutls_rc,
                                         const char *error,
                                         const char *ip_address),
                         const char *function,
                         const char *data)
{
    struct t_script_callback *new_script_callback;
    struct t_hook *new_hook;

    new_script_callback = script_callback_alloc ();
    if (!new_script_callback)
        return NULL;

    new_hook = weechat_hook_connect (proxy, address, port, sock, ipv6,
                                     gnutls_sess, gnutls_cb, gnutls_dhkey_size,
                                     gnutls_priorities, local_hostname,
                                     callback, new_script_callback);
    if (!new_hook)
    {
        script_callback_free_data (new_script_callback);
        free (new_script_callback);
        return NULL;
    }

    script_callback_init (new_script_callback, script, function, data);
    new_script_callback->hook = new_hook;

    script_callback_add (script, new_script_callback);

    return new_hook;
}

/*
 * script_api_hook_print: hook a print
 *                        return new hook, NULL if error
 */

struct t_hook *
script_api_hook_print (struct t_weechat_plugin *weechat_plugin,
                       struct t_plugin_script *script,
                       struct t_gui_buffer *buffer,
                       const char *tags, const char *message, int strip_colors,
                       int (*callback)(void *data,
                                       struct t_gui_buffer *buffer,
                                       time_t date,
                                       int tags_count, const char **tags,
                                       int displayed, int highlight,
                                       const char *prefix,
                                       const char *message),
                       const char *function,
                       const char *data)
{
    struct t_script_callback *new_script_callback;
    struct t_hook *new_hook;

    new_script_callback = script_callback_alloc ();
    if (!new_script_callback)
        return NULL;

    new_hook = weechat_hook_print (buffer, tags, message, strip_colors,
                                   callback, new_script_callback);
    if (!new_hook)
    {
        script_callback_free_data (new_script_callback);
        free (new_script_callback);
        return NULL;
    }

    script_callback_init (new_script_callback, script, function, data);
    new_script_callback->hook = new_hook;

    script_callback_add (script, new_script_callback);

    return new_hook;
}

/*
 * script_api_hook_signal: hook a signal
 *                         return new hook, NULL if error
 */

struct t_hook *
script_api_hook_signal (struct t_weechat_plugin *weechat_plugin,
                        struct t_plugin_script *script,
                        const char *signal,
                        int (*callback)(void *data, const char *signal,
                                        const char *type_data,
                                        void *signal_data),
                        const char *function,
                        const char *data)
{
    struct t_script_callback *new_script_callback;
    struct t_hook *new_hook;

    new_script_callback = script_callback_alloc ();
    if (!new_script_callback)
        return NULL;

    new_hook = weechat_hook_signal (signal, callback, new_script_callback);
    if (!new_hook)
    {
        script_callback_free_data (new_script_callback);
        free (new_script_callback);
        return NULL;
    }

    script_callback_init (new_script_callback, script, function, data);
    new_script_callback->hook = new_hook;

    script_callback_add (script, new_script_callback);

    return new_hook;
}

/*
 * script_api_hook_hsignal: hook a hsignal
 *                          return new hook, NULL if error
 */

struct t_hook *
script_api_hook_hsignal (struct t_weechat_plugin *weechat_plugin,
                         struct t_plugin_script *script,
                         const char *signal,
                         int (*callback)(void *data, const char *signal,
                                         struct t_hashtable *hashtable),
                         const char *function,
                         const char *data)
{
    struct t_script_callback *new_script_callback;
    struct t_hook *new_hook;

    new_script_callback = script_callback_alloc ();
    if (!new_script_callback)
        return NULL;

    new_hook = weechat_hook_hsignal (signal, callback, new_script_callback);
    if (!new_hook)
    {
        script_callback_free_data (new_script_callback);
        free (new_script_callback);
        return NULL;
    }

    script_callback_init (new_script_callback, script, function, data);
    new_script_callback->hook = new_hook;

    script_callback_add (script, new_script_callback);

    return new_hook;
}

/*
 * script_api_hook_config: hook a config option
 *                         return new hook, NULL if error
 */

struct t_hook *
script_api_hook_config (struct t_weechat_plugin *weechat_plugin,
                        struct t_plugin_script *script,
                        const char *option,
                        int (*callback)(void *data, const char *option,
                                        const char *value),
                        const char *function,
                        const char *data)
{
    struct t_script_callback *new_script_callback;
    struct t_hook *new_hook;

    new_script_callback = script_callback_alloc ();
    if (!new_script_callback)
        return NULL;

    new_hook = weechat_hook_config (option, callback, new_script_callback);
    if (!new_hook)
    {
        script_callback_free_data (new_script_callback);
        free (new_script_callback);
        return NULL;
    }

    script_callback_init (new_script_callback, script, function, data);
    new_script_callback->hook = new_hook;

    script_callback_add (script, new_script_callback);

    return new_hook;
}

/*
 * script_api_hook_completion: hook a completion
 *                             return new hook, NULL if error
 */

struct t_hook *
script_api_hook_completion (struct t_weechat_plugin *weechat_plugin,
                            struct t_plugin_script *script,
                            const char *completion,
                            const char *description,
                            int (*callback)(void *data,
                                            const char *completion_item,
                                            struct t_gui_buffer *buffer,
                                            struct t_gui_completion *completion),
                            const char *function,
                            const char *data)
{
    struct t_script_callback *new_script_callback;
    struct t_hook *new_hook;

    new_script_callback = script_callback_alloc ();
    if (!new_script_callback)
        return NULL;

    new_hook = weechat_hook_completion (completion, description,
                                        callback, new_script_callback);
    if (!new_hook)
    {
        script_callback_free_data (new_script_callback);
        free (new_script_callback);
        return NULL;
    }

    script_callback_init (new_script_callback, script, function, data);
    new_script_callback->hook = new_hook;

    script_callback_add (script, new_script_callback);

    return new_hook;
}

/*
 * script_api_hook_modifier: hook a modifier
 *                           return new hook, NULL if error
 */

struct t_hook *
script_api_hook_modifier (struct t_weechat_plugin *weechat_plugin,
                          struct t_plugin_script *script,
                          const char *modifier,
                          char *(*callback)(void *data, const char *modifier,
                                            const char *modifier_data,
                                            const char *string),
                          const char *function,
                          const char *data)
{
    struct t_script_callback *new_script_callback;
    struct t_hook *new_hook;

    new_script_callback = script_callback_alloc ();
    if (!new_script_callback)
        return NULL;

    new_hook = weechat_hook_modifier (modifier, callback, new_script_callback);
    if (!new_hook)
    {
        script_callback_free_data (new_script_callback);
        free (new_script_callback);
        return NULL;
    }

    script_callback_init (new_script_callback, script, function, data);
    new_script_callback->hook = new_hook;

    script_callback_add (script, new_script_callback);

    return new_hook;
}

/*
 * script_api_hook_info: hook an info
 *                       return new hook, NULL if error
 */

struct t_hook *
script_api_hook_info (struct t_weechat_plugin *weechat_plugin,
                      struct t_plugin_script *script,
                      const char *info_name,
                      const char *description,
                      const char *args_description,
                      const char *(*callback)(void *data,
                                              const char *info_name,
                                              const char *arguments),
                      const char *function,
                      const char *data)
{
    struct t_script_callback *new_script_callback;
    struct t_hook *new_hook;

    new_script_callback = script_callback_alloc ();
    if (!new_script_callback)
        return NULL;

    new_hook = weechat_hook_info (info_name, description, args_description,
                                  callback, new_script_callback);
    if (!new_hook)
    {
        script_callback_free_data (new_script_callback);
        free (new_script_callback);
        return NULL;
    }

    script_callback_init (new_script_callback, script, function, data);
    new_script_callback->hook = new_hook;

    script_callback_add (script, new_script_callback);

    return new_hook;
}

/*
 * script_api_hook_info_hashtable: hook an info_hashtable
 *                                 return new hook, NULL if error
 */

struct t_hook *
script_api_hook_info_hashtable (struct t_weechat_plugin *weechat_plugin,
                                struct t_plugin_script *script,
                                const char *info_name,
                                const char *description,
                                const char *args_description,
                                const char *output_description,
                                struct t_hashtable *(*callback)(void *data,
                                                                const char *info_name,
                                                                struct t_hashtable *hashtable),
                                const char *function,
                                const char *data)
{
    struct t_script_callback *new_script_callback;
    struct t_hook *new_hook;

    new_script_callback = script_callback_alloc ();
    if (!new_script_callback)
        return NULL;

    new_hook = weechat_hook_info_hashtable (info_name, description,
                                            args_description,
                                            output_description,
                                            callback, new_script_callback);
    if (!new_hook)
    {
        script_callback_free_data (new_script_callback);
        free (new_script_callback);
        return NULL;
    }

    script_callback_init (new_script_callback, script, function, data);
    new_script_callback->hook = new_hook;

    script_callback_add (script, new_script_callback);

    return new_hook;
}

/*
 * script_api_hook_infolist: hook an infolist
 *                           return new hook, NULL if error
 */

struct t_hook *
script_api_hook_infolist (struct t_weechat_plugin *weechat_plugin,
                          struct t_plugin_script *script,
                          const char *infolist_name,
                          const char *description,
                          const char *pointer_description,
                          const char *args_description,
                          struct t_infolist *(*callback)(void *data,
                                                         const char *infolist_name,
                                                         void *pointer,
                                                         const char *arguments),
                          const char *function,
                          const char *data)
{
    struct t_script_callback *new_script_callback;
    struct t_hook *new_hook;

    new_script_callback = script_callback_alloc ();
    if (!new_script_callback)
        return NULL;

    new_hook = weechat_hook_infolist (infolist_name, description,
                                      pointer_description, args_description,
                                      callback, new_script_callback);
    if (!new_hook)
    {
        script_callback_free_data (new_script_callback);
        free (new_script_callback);
        return NULL;
    }

    script_callback_init (new_script_callback, script, function, data);
    new_script_callback->hook = new_hook;

    script_callback_add (script, new_script_callback);

    return new_hook;
}

/*
 * script_api_hook_focus: hook a focus
 *                        return new hook, NULL if error
 */

struct t_hook *
script_api_hook_focus (struct t_weechat_plugin *weechat_plugin,
                       struct t_plugin_script *script,
                       const char *area,
                       struct t_hashtable *(*callback)(void *data,
                                                       struct t_hashtable *info),
                       const char *function,
                       const char *data)
{
    struct t_script_callback *new_script_callback;
    struct t_hook *new_hook;

    new_script_callback = script_callback_alloc ();
    if (!new_script_callback)
        return NULL;

    new_hook = weechat_hook_focus (area, callback, new_script_callback);
    if (!new_hook)
    {
        script_callback_free_data (new_script_callback);
        free (new_script_callback);
        return NULL;
    }

    script_callback_init (new_script_callback, script, function, data);
    new_script_callback->hook = new_hook;

    script_callback_add (script, new_script_callback);

    return new_hook;
}

/*
 * script_api_unhook: unhook something
 */

void
script_api_unhook (struct t_weechat_plugin *weechat_plugin,
                   struct t_plugin_script *script,
                   struct t_hook *hook)
{
    struct t_script_callback *ptr_script_callback, *next_callback;

    if (!weechat_plugin || !script || !hook)
        return;

    weechat_unhook (hook);

    ptr_script_callback = script->callbacks;
    while (ptr_script_callback)
    {
        next_callback = ptr_script_callback->next_callback;

        if (ptr_script_callback->hook == hook)
            script_callback_remove (script, ptr_script_callback);

        ptr_script_callback = next_callback;
    }
}

/*
 * script_api_unhook_all: remove all hooks from a script
 */

void
script_api_unhook_all (struct t_plugin_script *script)
{
    struct t_script_callback *ptr_callback, *next_callback;

    ptr_callback = script->callbacks;
    while (ptr_callback)
    {
        next_callback = ptr_callback->next_callback;

        script_callback_remove (script, ptr_callback);

        ptr_callback = next_callback;
    }
}

/*
 * script_api_buffer_new: create a new buffer
 */

struct t_gui_buffer *
script_api_buffer_new (struct t_weechat_plugin *weechat_plugin,
                       struct t_plugin_script *script,
                       const char *name,
                       int (*input_callback)(void *data,
                                             struct t_gui_buffer *buffer,
                                             const char *input_data),
                       const char *function_input,
                       const char *data_input,
                       int (*close_callback)(void *data,
                                             struct t_gui_buffer *buffer),
                       const char *function_close,
                       const char *data_close)
{
    struct t_script_callback *new_script_callback_input;
    struct t_script_callback *new_script_callback_close;
    struct t_gui_buffer *new_buffer;

    if ((!function_input || !function_input[0])
        && (!function_close || !function_close[0]))
        return weechat_buffer_new (name, NULL, NULL, NULL, NULL);

    new_script_callback_input = NULL;
    new_script_callback_close = NULL;

    if (function_input && function_input[0])
    {
        new_script_callback_input = script_callback_alloc ();
        if (!new_script_callback_input)
            return NULL;
    }

    if (function_close && function_close[0])
    {
        new_script_callback_close = script_callback_alloc ();
        if (!new_script_callback_close)
        {
            if (new_script_callback_input)
            {
                script_callback_free_data (new_script_callback_input);
                free (new_script_callback_input);
            }
            return NULL;
        }
    }

    new_buffer = weechat_buffer_new (name,
                                     (new_script_callback_input) ?
                                     input_callback : NULL,
                                     (new_script_callback_input) ?
                                     new_script_callback_input : NULL,
                                     (new_script_callback_close) ?
                                     close_callback : NULL,
                                     (new_script_callback_close) ?
                                     new_script_callback_close : NULL);
    if (!new_buffer)
    {
        if (new_script_callback_input)
        {
            script_callback_free_data (new_script_callback_input);
            free (new_script_callback_input);
        }
        if (new_script_callback_close)
        {
            script_callback_free_data (new_script_callback_close);
            free (new_script_callback_close);
        }
        return NULL;
    }

    if (new_script_callback_input)
    {
        script_callback_init (new_script_callback_input,
                              script, function_input, data_input);
        new_script_callback_input->buffer = new_buffer;
        script_callback_add (script, new_script_callback_input);
    }

    if (new_script_callback_close)
    {
        script_callback_init (new_script_callback_close,
                              script, function_close, data_close);
        new_script_callback_close->buffer = new_buffer;
        script_callback_add (script, new_script_callback_close);
    }

    /* used when upgrading weechat, to set callbacks */
    weechat_buffer_set (new_buffer, "localvar_set_script_name",
                        script->name);
    weechat_buffer_set (new_buffer, "localvar_set_script_input_cb",
                        function_input);
    weechat_buffer_set (new_buffer, "localvar_set_script_input_cb_data",
                        data_input);
    weechat_buffer_set (new_buffer, "localvar_set_script_close_cb",
                        function_close);
    weechat_buffer_set (new_buffer, "localvar_set_script_close_cb_data",
                        data_close);

    return new_buffer;
}

/*
 * script_api_buffer_close: close a buffer
 */

void
script_api_buffer_close (struct t_weechat_plugin *weechat_plugin,
                         struct t_plugin_script *script,
                         struct t_gui_buffer *buffer)
{
    struct t_script_callback *ptr_script_callback, *next_callback;

    if (!weechat_plugin || !script || !buffer)
        return;

    weechat_buffer_close (buffer);

    ptr_script_callback = script->callbacks;
    while (ptr_script_callback)
    {
        next_callback = ptr_script_callback->next_callback;

        if (ptr_script_callback->buffer == buffer)
            script_callback_remove (script, ptr_script_callback);

        ptr_script_callback = next_callback;
    }
}

/*
 * script_api_bar_item_new: add a new bar item
 */

struct t_gui_bar_item *
script_api_bar_item_new (struct t_weechat_plugin *weechat_plugin,
                         struct t_plugin_script *script,
                         const char *name,
                         char *(*build_callback)(void *data,
                                                 struct t_gui_bar_item *item,
                                                 struct t_gui_window *window),
                         const char *function,
                         const char *data)
{
    struct t_script_callback *new_script_callback;
    struct t_gui_bar_item *new_item;

    new_script_callback = script_callback_alloc ();
    if (!new_script_callback)
        return NULL;

    script_callback_init (new_script_callback, script, function, data);

    new_item = weechat_bar_item_new (name,
                                     (function && function[0]) ?
                                     build_callback : NULL,
                                     (function && function[0]) ?
                                     new_script_callback : NULL);
    if (!new_item)
    {
        script_callback_free_data (new_script_callback);
        free (new_script_callback);
        return NULL;
    }

    new_script_callback->bar_item = new_item;
    script_callback_add (script, new_script_callback);

    return new_item;
}

/*
 * script_api_bar_item_remove: remove a bar item
 */

void
script_api_bar_item_remove (struct t_weechat_plugin *weechat_plugin,
                            struct t_plugin_script *script,
                            struct t_gui_bar_item *item)
{
    struct t_script_callback *ptr_script_callback, *next_callback;

    if (!weechat_plugin || !script || !item)
        return;

    weechat_bar_item_remove (item);

    ptr_script_callback = script->callbacks;
    while (ptr_script_callback)
    {
        next_callback = ptr_script_callback->next_callback;

        if (ptr_script_callback->bar_item == item)
            script_callback_remove (script, ptr_script_callback);

        ptr_script_callback = next_callback;
    }
}

/*
 * script_api_command: execute a command (simulate user entry)
 */

void
script_api_command (struct t_weechat_plugin *weechat_plugin,
                    struct t_plugin_script *script,
                    struct t_gui_buffer *buffer, const char *command)
{
    char *command2;

    command2 = (script->charset && script->charset[0]) ?
        weechat_iconv_to_internal (script->charset, command) : NULL;

    weechat_command (buffer, (command2) ? command2 : command);

    if (command2)
        free (command2);
}

/*
 * script_api_config_get_plugin: get a value of a script option
 *                               format in file is "plugin.script.option"
 */

const char *
script_api_config_get_plugin (struct t_weechat_plugin *weechat_plugin,
                              struct t_plugin_script *script,
                              const char *option)
{
    char *option_fullname;
    const char *return_value;

    option_fullname = malloc ((strlen (script->name) +
                               strlen (option) + 2));
    if (!option_fullname)
        return NULL;

    strcpy (option_fullname, script->name);
    strcat (option_fullname, ".");
    strcat (option_fullname, option);

    return_value = weechat_config_get_plugin (option_fullname);
    free (option_fullname);

    return return_value;
}

/*
 * script_api_config_is_set_plugin: check if a script option is set
 */

int
script_api_config_is_set_plugin (struct t_weechat_plugin *weechat_plugin,
                                 struct t_plugin_script *script,
                                 const char *option)
{
    char *option_fullname;
    int return_code;

    option_fullname = malloc ((strlen (script->name) +
                               strlen (option) + 2));
    if (!option_fullname)
        return 0;

    strcpy (option_fullname, script->name);
    strcat (option_fullname, ".");
    strcat (option_fullname, option);

    return_code = weechat_config_is_set_plugin (option_fullname);
    free (option_fullname);

    return return_code;
}

/*
 * script_api_config_set_plugin: set value of a script config option
 *                               format in file is "plugin.script.option"
 */

int
script_api_config_set_plugin (struct t_weechat_plugin *weechat_plugin,
                              struct t_plugin_script *script,
                              const char *option, const char *value)
{
    char *option_fullname;
    int return_code;

    option_fullname = malloc ((strlen (script->name) +
                               strlen (option) + 2));
    if (!option_fullname)
        return 0;

    strcpy (option_fullname, script->name);
    strcat (option_fullname, ".");
    strcat (option_fullname, option);

    return_code = weechat_config_set_plugin (option_fullname, value);
    free (option_fullname);

    return return_code;
}

/*
 * script_api_config_set_plugin: set value of a script config option
 *                               format in file is "plugin.script.option"
 */

void
script_api_config_set_desc_plugin (struct t_weechat_plugin *weechat_plugin,
                                   struct t_plugin_script *script,
                                   const char *option, const char *description)
{
    char *option_fullname;

    option_fullname = malloc ((strlen (script->name) +
                               strlen (option) + 2));
    if (!option_fullname)
        return;

    strcpy (option_fullname, script->name);
    strcat (option_fullname, ".");
    strcat (option_fullname, option);

    weechat_config_set_desc_plugin (option_fullname, description);
    free (option_fullname);
}

/*
 * script_api_config_unset_plugin: unset script config option
 *                                 format in file is "plugin.script.option"
 */

int
script_api_config_unset_plugin (struct t_weechat_plugin *weechat_plugin,
                                struct t_plugin_script *script,
                                const char *option)
{
    char *option_fullname;
    int return_code;

    option_fullname = malloc ((strlen (script->name) +
                               strlen (option) + 2));
    if (!option_fullname)
        return 0;

    strcpy (option_fullname, script->name);
    strcat (option_fullname, ".");
    strcat (option_fullname, option);

    return_code = weechat_config_unset_plugin (option_fullname);
    free (option_fullname);

    return return_code;
}

/*
 * script_api_upgrade_read: read upgrade file
 *                          return 1 if ok, 0 if error
 */

int
script_api_upgrade_read (struct t_weechat_plugin *weechat_plugin,
                         struct t_plugin_script *script,
                         struct t_upgrade_file *upgrade_file,
                         int (*callback_read)(void *data,
                                              struct t_upgrade_file *upgrade_file,
                                              int object_id,
                                              struct t_infolist *infolist),
                         const char *function,
                         const char *data)
{
    struct t_script_callback *new_script_callback;
    int rc;

    if (!function || !function[0])
        return 0;

    new_script_callback = script_callback_alloc ();
    if (!new_script_callback)
        return 0;

    script_callback_init (new_script_callback, script, function, data);
    new_script_callback->upgrade_file = upgrade_file;
    script_callback_add (script, new_script_callback);

    rc = weechat_upgrade_read (upgrade_file,
                               callback_read,
                               new_script_callback);

    script_callback_remove (script, new_script_callback);

    return rc;
}
