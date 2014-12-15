/*
 * weechat-guile.c - guile (scheme) plugin for WeeChat
 *
 * Copyright (C) 2011-2014 Sébastien Helleu <flashcode@flashtux.org>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#undef _

#include <libguile.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libgen.h>

#include "../weechat-plugin.h"
#include "../plugin-script.h"
#include "weechat-guile.h"
#include "weechat-guile-api.h"


WEECHAT_PLUGIN_NAME(GUILE_PLUGIN_NAME);
WEECHAT_PLUGIN_DESCRIPTION(N_("Support of scheme scripts (with Guile)"));
WEECHAT_PLUGIN_AUTHOR("Sébastien Helleu <flashcode@flashtux.org>");
WEECHAT_PLUGIN_VERSION(WEECHAT_VERSION);
WEECHAT_PLUGIN_LICENSE(WEECHAT_LICENSE);

struct t_weechat_plugin *weechat_guile_plugin = NULL;

int guile_quiet;
struct t_plugin_script *guile_scripts = NULL;
struct t_plugin_script *last_guile_script = NULL;
struct t_plugin_script *guile_current_script = NULL;
struct t_plugin_script *guile_registered_script = NULL;
const char *guile_current_script_filename = NULL;
SCM guile_module_weechat;
SCM guile_port;
char *guile_stdout = NULL;

struct t_guile_function
{
    SCM proc;                          /* proc to call                      */
    SCM *argv;                         /* arguments for proc                */
    size_t nargs;                      /* length of arguments               */
};

/*
 * string used to execute action "install":
 * when signal "guile_script_install" is received, name of string
 * is added to this string, to be installed later by a timer (when nothing is
 * running in script)
 */
char *guile_action_install_list = NULL;

/*
 * string used to execute action "remove":
 * when signal "guile_script_remove" is received, name of string
 * is added to this string, to be removed later by a timer (when nothing is
 * running in script)
 */
char *guile_action_remove_list = NULL;

/*
 * string used to execute action "autoload":
 * when signal "guile_script_autoload" is received, name of string
 * is added to this string, to autoload or disable autoload later by a timer
 * (when nothing is running in script)
 */
char *guile_action_autoload_list = NULL;


/*
 * Flushes stdout.
 */

void
weechat_guile_stdout_flush ()
{
    if (guile_stdout)
    {
        weechat_printf (NULL,
                        weechat_gettext ("%s: stdout/stderr: %s%s"),
                        GUILE_PLUGIN_NAME, guile_stdout, "");
        free (guile_stdout);
        guile_stdout = NULL;
    }
}

/*
 * Executes scheme procedure with internal catch and returns value.
 */

SCM
weechat_guile_catch (void *procedure, void *data)
{
    SCM value;

    value = scm_internal_catch (SCM_BOOL_T,
                                (scm_t_catch_body)procedure,
                                data,
                                (scm_t_catch_handler) scm_handle_by_message_noexit,
                                NULL);
    return value;
}

/*
 * Encapsulates call to scm_call_n (to give arguments).
 */

SCM
weechat_guile_scm_call_n (void *proc)
{
    struct t_guile_function *guile_function;

    guile_function = (struct t_guile_function *)proc;

    return scm_call_n (guile_function->proc,
                       guile_function->argv, guile_function->nargs);
}

/*
 * Executes scheme function (with optional args) and returns value.
 */

SCM
weechat_guile_exec_function (const char *function, SCM *argv, size_t nargs)
{
    SCM func, func2, value;
    struct t_guile_function guile_function;

    func = weechat_guile_catch (scm_c_lookup, (void *)function);
    func2 = weechat_guile_catch (scm_variable_ref, func);

    if (argv)
    {
        guile_function.proc = func2;
        guile_function.argv = argv;
        guile_function.nargs = nargs;
        value = weechat_guile_catch (weechat_guile_scm_call_n, &guile_function);
    }
    else
    {
        value = weechat_guile_catch (scm_call_0, func2);
    }

    return value;
}

/*
 * Callback called for each key/value in a hashtable.
 */

void
weechat_guile_hashtable_map_cb (void *data,
                                struct t_hashtable *hashtable,
                                const char *key,
                                const char *value)
{
    SCM *alist, pair, list;

    /* make C compiler happy */
    (void) hashtable;

    alist = (SCM *)data;

    pair = scm_cons (scm_from_locale_string (key),
                     scm_from_locale_string (value));
    list = scm_list_1 (pair);

    *alist = scm_append (scm_list_2 (*alist, list));
}

/*
 * Gets guile alist with a WeeChat hashtable.
 */

SCM
weechat_guile_hashtable_to_alist (struct t_hashtable *hashtable)
{
    SCM alist;

    alist = scm_list_n (SCM_UNDEFINED);

    weechat_hashtable_map_string (hashtable,
                                  &weechat_guile_hashtable_map_cb,
                                  &alist);

    return alist;
}

/*
 * Gets WeeChat hashtable with guile alist.
 *
 * Note: hashtable must be free after use.
 */

struct t_hashtable *
weechat_guile_alist_to_hashtable (SCM alist, int size, const char *type_keys,
                                  const char *type_values)
{
    struct t_hashtable *hashtable;
    int length, i;
    SCM pair;
    char *str, *str2;

    hashtable = weechat_hashtable_new (size,
                                       type_keys,
                                       type_values,
                                       NULL,
                                       NULL);
    if (!hashtable)
        return NULL;

    length = scm_to_int (scm_length (alist));
    for (i = 0; i < length; i++)
    {
        pair = scm_list_ref (alist, scm_from_int (i));
        if (strcmp (type_values, WEECHAT_HASHTABLE_STRING) == 0)
        {
            str = scm_to_locale_string (scm_list_ref (pair, scm_from_int (0)));
            str2 = scm_to_locale_string (scm_list_ref (pair, scm_from_int (1)));
            weechat_hashtable_set (hashtable, str, str2);
            if (str)
                free (str);
            if (str2)
                free (str2);
        }
        else if (strcmp (type_values, WEECHAT_HASHTABLE_POINTER) == 0)
        {
            str = scm_to_locale_string (scm_list_ref (pair, scm_from_int (0)));
            str2 = scm_to_locale_string (scm_list_ref (pair, scm_from_int (1)));
            weechat_hashtable_set (hashtable, str,
                                   plugin_script_str2ptr (weechat_guile_plugin,
                                                          NULL, NULL, str2));
            if (str)
                free (str);
            if (str2)
                free (str2);
        }
    }

    return hashtable;
}

/*
 * Executes a guile function.
 */

void *
weechat_guile_exec (struct t_plugin_script *script,
                    int ret_type, const char *function,
                    char *format, void **argv)
{
    struct t_plugin_script *old_guile_current_script;
    SCM rc, old_current_module;
    void *argv2[17], *ret_value;
    int i, argc, *ret_int;

    old_guile_current_script = guile_current_script;
    old_current_module = NULL;
    if (script->interpreter)
    {
        old_current_module = scm_current_module ();
        scm_set_current_module ((SCM)(script->interpreter));
    }
    guile_current_script = script;

    if (argv && argv[0])
    {
        argc = strlen (format);
        for (i = 0; i < argc; i++)
        {
            switch (format[i])
            {
                case 's': /* string */
                    argv2[i] = scm_from_locale_string ((char *)argv[i]);
                    break;
                case 'i': /* integer */
                    argv2[i] = scm_from_int (*((int *)argv[i]));
                    break;
                case 'h': /* hash */
                    argv2[i] = weechat_guile_hashtable_to_alist (argv[i]);
                    break;
            }
        }
        for (i = argc; i < 17; i++)
        {
            argv2[i] = SCM_UNDEFINED;
        }
        rc = weechat_guile_exec_function (function, (SCM *)argv2, argc);
    }
    else
    {
        rc = weechat_guile_exec_function (function, NULL, 0);
    }

    ret_value = NULL;

    if ((ret_type == WEECHAT_SCRIPT_EXEC_STRING) && (scm_is_string (rc)))
    {
        ret_value = scm_to_locale_string (rc);
    }
    else if ((ret_type == WEECHAT_SCRIPT_EXEC_INT) && (scm_is_integer (rc)))
    {
        ret_int = malloc (sizeof (*ret_int));
        if (ret_int)
            *ret_int = scm_to_int (rc);
        ret_value = ret_int;
    }
    else if (ret_type == WEECHAT_SCRIPT_EXEC_HASHTABLE)
    {
        ret_value = weechat_guile_alist_to_hashtable (rc,
                                                      WEECHAT_SCRIPT_HASHTABLE_DEFAULT_SIZE,
                                                      WEECHAT_HASHTABLE_STRING,
                                                      WEECHAT_HASHTABLE_STRING);
    }
    else
    {
        weechat_printf (NULL,
                        weechat_gettext ("%s%s: function \"%s\" must return "
                                         "a valid value"),
                        weechat_prefix ("error"), GUILE_PLUGIN_NAME, function);
    }

    if (ret_value == NULL)
    {
        weechat_printf (NULL,
                        weechat_gettext ("%s%s: error in function \"%s\""),
                        weechat_prefix ("error"), GUILE_PLUGIN_NAME, function);
    }

    if (old_current_module)
        scm_set_current_module (old_current_module);

    guile_current_script = old_guile_current_script;

    return ret_value;
}

/*
 * Initializes guile module for script.
 */

void
weechat_guile_module_init_script (void *data)
{
    SCM rc;

    weechat_guile_catch (scm_c_eval_string, "(use-modules (weechat))");
    rc = weechat_guile_catch (scm_c_primitive_load, data);

    /* error loading script? */
    if (rc == SCM_BOOL_F)
    {
        /* if script was registered, remove it from list */
        if (guile_current_script)
        {
            plugin_script_remove (weechat_guile_plugin,
                                  &guile_scripts, &last_guile_script,
                                  guile_current_script);
        }
        guile_current_script = NULL;
        guile_registered_script = NULL;
    }
}

/*
 * Loads a guile script.
 *
 * Returns:
 *   1: OK
 *   0: error
 */

int
weechat_guile_load (const char *filename)
{
    char *filename2, *ptr_base_name, *base_name;
    SCM module;

    if ((weechat_guile_plugin->debug >= 2) || !guile_quiet)
    {
        weechat_printf (NULL,
                        weechat_gettext ("%s: loading script \"%s\""),
                        GUILE_PLUGIN_NAME, filename);
    }

    guile_current_script = NULL;
    guile_registered_script = NULL;
    guile_current_script_filename = filename;

    filename2 = strdup (filename);
    if (!filename2)
        return 0;

    ptr_base_name = basename (filename2);
    base_name = strdup (ptr_base_name);
    module = scm_c_define_module (base_name,
                                  &weechat_guile_module_init_script, filename2);
    free (filename2);

    if (!guile_registered_script)
    {
        weechat_printf (NULL,
                        weechat_gettext ("%s%s: function \"register\" not "
                                         "found (or failed) in file \"%s\""),
                        weechat_prefix ("error"), GUILE_PLUGIN_NAME, filename);
        return 0;
    }

    weechat_guile_catch (scm_gc_protect_object, (void *)module);

    guile_current_script = guile_registered_script;

    /*
     * set input/close callbacks for buffers created by this script
     * (to restore callbacks after upgrade)
     */
    plugin_script_set_buffer_callbacks (weechat_guile_plugin,
                                        guile_scripts,
                                        guile_current_script,
                                        &weechat_guile_api_buffer_input_data_cb,
                                        &weechat_guile_api_buffer_close_cb);

    (void) weechat_hook_signal_send ("guile_script_loaded",
                                     WEECHAT_HOOK_SIGNAL_STRING,
                                     guile_current_script->filename);

    return 1;
}

/*
 * Callback for script_auto_load() function.
 */

void
weechat_guile_load_cb (void *data, const char *filename)
{
    /* make C compiler happy */
    (void) data;

    weechat_guile_load (filename);
}

/*
 * Unloads a guile script.
 */

void
weechat_guile_unload (struct t_plugin_script *script)
{
    int *rc;
    void *interpreter;
    char *filename;

    if ((weechat_guile_plugin->debug >= 2) || !guile_quiet)
    {
        weechat_printf (NULL,
                        weechat_gettext ("%s: unloading script \"%s\""),
                        GUILE_PLUGIN_NAME, script->name);
    }

    if (script->shutdown_func && script->shutdown_func[0])
    {
        rc = (int *)weechat_guile_exec (script, WEECHAT_SCRIPT_EXEC_INT,
                                        script->shutdown_func, NULL, NULL);
        if (rc)
            free (rc);
    }

    filename = strdup (script->filename);
    interpreter = script->interpreter;

    if (guile_current_script == script)
        guile_current_script = (guile_current_script->prev_script) ?
            guile_current_script->prev_script : guile_current_script->next_script;

    plugin_script_remove (weechat_guile_plugin, &guile_scripts, &last_guile_script,
                          script);

    if (interpreter)
        weechat_guile_catch (scm_gc_unprotect_object, interpreter);

    if (guile_current_script)
        scm_set_current_module ((SCM)(guile_current_script->interpreter));

    (void) weechat_hook_signal_send ("guile_script_unloaded",
                                     WEECHAT_HOOK_SIGNAL_STRING, filename);
    if (filename)
        free (filename);
}

/*
 * Unloads a guile script by name.
 */

void
weechat_guile_unload_name (const char *name)
{
    struct t_plugin_script *ptr_script;

    ptr_script = plugin_script_search (weechat_guile_plugin, guile_scripts, name);
    if (ptr_script)
    {
        weechat_guile_unload (ptr_script);
        if (!guile_quiet)
        {
            weechat_printf (NULL,
                            weechat_gettext ("%s: script \"%s\" unloaded"),
                            GUILE_PLUGIN_NAME, name);
        }
    }
    else
    {
        weechat_printf (NULL,
                        weechat_gettext ("%s%s: script \"%s\" not loaded"),
                        weechat_prefix ("error"), GUILE_PLUGIN_NAME, name);
    }
}

/*
 * Unloads all guile scripts.
 */

void
weechat_guile_unload_all ()
{
    while (guile_scripts)
    {
        weechat_guile_unload (guile_scripts);
    }
}

/*
 * Reloads a guile script by name.
 */

void
weechat_guile_reload_name (const char *name)
{
    struct t_plugin_script *ptr_script;
    char *filename;

    ptr_script = plugin_script_search (weechat_guile_plugin, guile_scripts, name);
    if (ptr_script)
    {
        filename = strdup (ptr_script->filename);
        if (filename)
        {
            weechat_guile_unload (ptr_script);
            if (!guile_quiet)
            {
                weechat_printf (NULL,
                                weechat_gettext ("%s: script \"%s\" unloaded"),
                                GUILE_PLUGIN_NAME, name);
            }
            weechat_guile_load (filename);
            free (filename);
        }
    }
    else
    {
        weechat_printf (NULL,
                        weechat_gettext ("%s%s: script \"%s\" not loaded"),
                        weechat_prefix ("error"), GUILE_PLUGIN_NAME, name);
    }
}

/*
 * Callback for command "/guile".
 */

int
weechat_guile_command_cb (void *data, struct t_gui_buffer *buffer,
                          int argc, char **argv, char **argv_eol)
{
    char *ptr_name, *path_script;
    SCM value;

    /* make C compiler happy */
    (void) data;
    (void) buffer;

    if (argc == 1)
    {
        plugin_script_display_list (weechat_guile_plugin, guile_scripts,
                                    NULL, 0);
    }
    else if (argc == 2)
    {
        if (weechat_strcasecmp (argv[1], "list") == 0)
        {
            plugin_script_display_list (weechat_guile_plugin, guile_scripts,
                                        NULL, 0);
        }
        else if (weechat_strcasecmp (argv[1], "listfull") == 0)
        {
            plugin_script_display_list (weechat_guile_plugin, guile_scripts,
                                        NULL, 1);
        }
        else if (weechat_strcasecmp (argv[1], "autoload") == 0)
        {
            plugin_script_auto_load (weechat_guile_plugin, &weechat_guile_load_cb);
        }
        else if (weechat_strcasecmp (argv[1], "reload") == 0)
        {
            weechat_guile_unload_all ();
            plugin_script_auto_load (weechat_guile_plugin, &weechat_guile_load_cb);
        }
        else if (weechat_strcasecmp (argv[1], "unload") == 0)
        {
            weechat_guile_unload_all ();
        }
        else
            return WEECHAT_RC_ERROR;
    }
    else
    {
        if (weechat_strcasecmp (argv[1], "list") == 0)
        {
            plugin_script_display_list (weechat_guile_plugin, guile_scripts,
                                        argv_eol[2], 0);
        }
        else if (weechat_strcasecmp (argv[1], "listfull") == 0)
        {
            plugin_script_display_list (weechat_guile_plugin, guile_scripts,
                                        argv_eol[2], 1);
        }
        else if ((weechat_strcasecmp (argv[1], "load") == 0)
                 || (weechat_strcasecmp (argv[1], "reload") == 0)
                 || (weechat_strcasecmp (argv[1], "unload") == 0))
        {
            ptr_name = argv_eol[2];
            if (strncmp (ptr_name, "-q ", 3) == 0)
            {
                guile_quiet = 1;
                ptr_name += 3;
                while (ptr_name[0] == ' ')
                {
                    ptr_name++;
                }
            }
            if (weechat_strcasecmp (argv[1], "load") == 0)
            {
                /* load guile script */
                path_script = plugin_script_search_path (weechat_guile_plugin,
                                                         ptr_name);
                weechat_guile_load ((path_script) ? path_script : ptr_name);
                if (path_script)
                    free (path_script);
            }
            else if (weechat_strcasecmp (argv[1], "reload") == 0)
            {
                /* reload one guile script */
                weechat_guile_reload_name (ptr_name);
            }
            else if (weechat_strcasecmp (argv[1], "unload") == 0)
            {
                /* unload guile script */
                weechat_guile_unload_name (ptr_name);
            }
            guile_quiet = 0;
        }
        else if (weechat_strcasecmp (argv[1], "eval") == 0)
        {
            /* eval guile code */
            value = weechat_guile_catch (scm_c_eval_string, argv_eol[2]);
            if (!SCM_EQ_P (value, SCM_UNDEFINED)
                && !SCM_EQ_P (value, SCM_UNSPECIFIED))
            {
                scm_display (value, guile_port);
            }
            weechat_guile_stdout_flush ();
        }
        else
            return WEECHAT_RC_ERROR;
    }

    return WEECHAT_RC_OK;
}

/*
 * Adds guile scripts to completion list.
 */

int
weechat_guile_completion_cb (void *data, const char *completion_item,
                             struct t_gui_buffer *buffer,
                             struct t_gui_completion *completion)
{
    /* make C compiler happy */
    (void) data;
    (void) completion_item;
    (void) buffer;

    plugin_script_completion (weechat_guile_plugin, completion, guile_scripts);

    return WEECHAT_RC_OK;
}

/*
 * Returns hdata for guile scripts.
 */

struct t_hdata *
weechat_guile_hdata_cb (void *data, const char *hdata_name)
{
    /* make C compiler happy */
    (void) data;

    return plugin_script_hdata_script (weechat_plugin,
                                       &guile_scripts, &last_guile_script,
                                       hdata_name);
}

/*
 * Returns infolist with guile scripts.
 */

struct t_infolist *
weechat_guile_infolist_cb (void *data, const char *infolist_name,
                           void *pointer, const char *arguments)
{
    /* make C compiler happy */
    (void) data;

    if (!infolist_name || !infolist_name[0])
        return NULL;

    if (weechat_strcasecmp (infolist_name, "guile_script") == 0)
    {
        return plugin_script_infolist_list_scripts (weechat_guile_plugin,
                                                    guile_scripts, pointer,
                                                    arguments);
    }

    return NULL;
}

/*
 * Dumps guile plugin data in WeeChat log file.
 */

int
weechat_guile_signal_debug_dump_cb (void *data, const char *signal,
                                    const char *type_data, void *signal_data)
{
    /* make C compiler happy */
    (void) data;
    (void) signal;
    (void) type_data;

    if (!signal_data
        || (weechat_strcasecmp ((char *)signal_data, GUILE_PLUGIN_NAME) == 0))
    {
        plugin_script_print_log (weechat_guile_plugin, guile_scripts);
    }

    return WEECHAT_RC_OK;
}

/*
 * Display infos about external libraries used.
 */

int
weechat_guile_signal_debug_libs_cb (void *data, const char *signal,
                                    const char *type_data, void *signal_data)
{
    /* make C compiler happy */
    (void) data;
    (void) signal;
    (void) type_data;
    (void) signal_data;

#if defined(SCM_MAJOR_VERSION) && defined(SCM_MINOR_VERSION) && defined(SCM_MICRO_VERSION)
    weechat_printf (NULL, "  %s: %d.%d.%d",
                    GUILE_PLUGIN_NAME,
                    SCM_MAJOR_VERSION,
                    SCM_MINOR_VERSION,
                    SCM_MICRO_VERSION);
#else
    weechat_printf (NULL, "  %s: (?)", GUILE_PLUGIN_NAME);
#endif

    return WEECHAT_RC_OK;
}

/*
 * Callback called when a buffer is closed.
 */

int
weechat_guile_signal_buffer_closed_cb (void *data, const char *signal,
                                       const char *type_data,
                                       void *signal_data)
{
    /* make C compiler happy */
    (void) data;
    (void) signal;
    (void) type_data;

    if (signal_data)
        plugin_script_remove_buffer_callbacks (guile_scripts, signal_data);

    return WEECHAT_RC_OK;
}

/*
 * Timer for executing actions.
 */

int
weechat_guile_timer_action_cb (void *data, int remaining_calls)
{
    /* make C compiler happy */
    (void) remaining_calls;

    if (data)
    {
        if (data == &guile_action_install_list)
        {
            plugin_script_action_install (weechat_guile_plugin,
                                          guile_scripts,
                                          &weechat_guile_unload,
                                          &weechat_guile_load,
                                          &guile_quiet,
                                          &guile_action_install_list);
        }
        else if (data == &guile_action_remove_list)
        {
            plugin_script_action_remove (weechat_guile_plugin,
                                         guile_scripts,
                                         &weechat_guile_unload,
                                         &guile_quiet,
                                         &guile_action_remove_list);
        }
        else if (data == &guile_action_autoload_list)
        {
            plugin_script_action_autoload (weechat_guile_plugin,
                                           &guile_quiet,
                                           &guile_action_autoload_list);
        }
    }

    return WEECHAT_RC_OK;
}

/*
 * Callback called when a script action is asked (install/remove a script).
 */

int
weechat_guile_signal_script_action_cb (void *data, const char *signal,
                                       const char *type_data,
                                       void *signal_data)
{
    /* make C compiler happy */
    (void) data;

    if (strcmp (type_data, WEECHAT_HOOK_SIGNAL_STRING) == 0)
    {
        if (strcmp (signal, "guile_script_install") == 0)
        {
            plugin_script_action_add (&guile_action_install_list,
                                      (const char *)signal_data);
            weechat_hook_timer (1, 0, 1,
                                &weechat_guile_timer_action_cb,
                                &guile_action_install_list);
        }
        else if (strcmp (signal, "guile_script_remove") == 0)
        {
            plugin_script_action_add (&guile_action_remove_list,
                                      (const char *)signal_data);
            weechat_hook_timer (1, 0, 1,
                                &weechat_guile_timer_action_cb,
                                &guile_action_remove_list);
        }
        else if (strcmp (signal, "guile_script_autoload") == 0)
        {
            plugin_script_action_add (&guile_action_autoload_list,
                                      (const char *)signal_data);
            weechat_hook_timer (1, 0, 1,
                                &weechat_guile_timer_action_cb,
                                &guile_action_autoload_list);
        }
    }

    return WEECHAT_RC_OK;
}

/*
 * Fills input.
 */

int
weechat_guile_port_fill_input (SCM port)
{
    /* make C compiler happy */
    (void) port;

    return ' ';
}

/*
 * Write.
 */

void
weechat_guile_port_write (SCM port, const void *data, size_t size)
{
    char *new_stdout;
    int length_stdout;

    /* make C compiler happy */
    (void) port;

    /* concatenate str to guile_stdout */
    if (guile_stdout)
    {
        length_stdout = strlen (guile_stdout);
        new_stdout = realloc (guile_stdout, length_stdout + size + 1);
        if (!new_stdout)
        {
            free (guile_stdout);
            return;
        }
        guile_stdout = new_stdout;
        memcpy (guile_stdout + length_stdout, data, size);
        guile_stdout[length_stdout + size] = '\0';
    }
    else
    {
        guile_stdout = malloc (size + 1);
        if (guile_stdout)
        {
            memcpy (guile_stdout, data, size);
            guile_stdout[size] = '\0';
        }
    }

    /* flush stdout if at least "\n" is in string */
    if (guile_stdout && (strchr (guile_stdout, '\n')))
        weechat_guile_stdout_flush ();
}

/*
 * Initializes guile plugin.
 */

int
weechat_plugin_init (struct t_weechat_plugin *plugin, int argc, char *argv[])
{
    struct t_plugin_script_init init;

    weechat_guile_plugin = plugin;

    guile_stdout = NULL;

#ifdef HAVE_GUILE_GMP_MEMORY_FUNCTIONS
    /*
     * prevent guile to use its own gmp allocator, because it can conflict
     * with other plugins using GnuTLS like relay, which can crash WeeChat
     * on unload (or exit)
     */
    scm_install_gmp_memory_functions = 0;
#endif

    scm_init_guile ();

    guile_module_weechat = scm_c_define_module ("weechat",
                                                &weechat_guile_api_module_init,
                                                NULL);
    scm_c_use_module ("weechat");
    weechat_guile_catch (scm_gc_protect_object, (void *)guile_module_weechat);

    init.callback_command = &weechat_guile_command_cb;
    init.callback_completion = &weechat_guile_completion_cb;
    init.callback_hdata = &weechat_guile_hdata_cb;
    init.callback_infolist = &weechat_guile_infolist_cb;
    init.callback_signal_debug_dump = &weechat_guile_signal_debug_dump_cb;
    init.callback_signal_debug_libs = &weechat_guile_signal_debug_libs_cb;
    init.callback_signal_buffer_closed = &weechat_guile_signal_buffer_closed_cb;
    init.callback_signal_script_action = &weechat_guile_signal_script_action_cb;
    init.callback_load_file = &weechat_guile_load_cb;

    guile_quiet = 1;
    plugin_script_init (weechat_guile_plugin, argc, argv, &init);
    guile_quiet = 0;

    plugin_script_display_short_list (weechat_guile_plugin,
                                      guile_scripts);

    /* init OK */
    return WEECHAT_RC_OK;
}

/*
 * Ends guile plugin.
 */

int
weechat_plugin_end (struct t_weechat_plugin *plugin)
{
    /* unload all scripts */
    guile_quiet = 1;
    plugin_script_end (plugin, &guile_scripts, &weechat_guile_unload_all);
    guile_quiet = 0;

    /* unprotect module */
    weechat_guile_catch (scm_gc_unprotect_object, (void *)guile_module_weechat);

    /* free some data */
    if (guile_action_install_list)
        free (guile_action_install_list);
    if (guile_action_remove_list)
        free (guile_action_remove_list);
    if (guile_action_autoload_list)
        free (guile_action_autoload_list);

    return WEECHAT_RC_OK;
}
