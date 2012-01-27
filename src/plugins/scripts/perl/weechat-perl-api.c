/*
 * Copyright (C) 2003-2012 Sebastien Helleu <flashcode@flashtux.org>
 * Copyright (C) 2005-2008 Emmanuel Bouthenot <kolter@openics.org>
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
 * weechat-perl-api.c: perl API functions
 */

#undef _

#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>
#include <time.h>

#include "../../weechat-plugin.h"
#include "../script.h"
#include "../script-api.h"
#include "../script-callback.h"
#include "weechat-perl.h"


#define API_FUNC(__init, __name, __ret)                                 \
    char *perl_function_name = __name;                                  \
    (void) cv;                                                          \
    if (__init                                                          \
        && (!perl_current_script || !perl_current_script->name))        \
    {                                                                   \
        WEECHAT_SCRIPT_MSG_NOT_INIT(PERL_CURRENT_SCRIPT_NAME,           \
                                    perl_function_name);                \
        __ret;                                                          \
    }
#define API_WRONG_ARGS(__ret)                                           \
    {                                                                   \
        WEECHAT_SCRIPT_MSG_WRONG_ARGS(PERL_CURRENT_SCRIPT_NAME,         \
                                      perl_function_name);              \
        __ret;                                                          \
    }
#define API_RETURN_OK XSRETURN_YES
#define API_RETURN_ERROR XSRETURN_NO
#define API_RETURN_EMPTY XSRETURN_EMPTY
#define API_RETURN_STRING(__string)                                     \
    if (__string)                                                       \
    {                                                                   \
        XST_mPV (0, __string);                                          \
        XSRETURN (1);                                                   \
    }                                                                   \
    XST_mPV (0, "");                                                    \
    XSRETURN (1)
#define API_RETURN_STRING_FREE(__string)                                \
    if (__string)                                                       \
    {                                                                   \
        XST_mPV (0, __string);                                          \
        free (__string);                                                \
        XSRETURN (1);                                                   \
    }                                                                   \
    XST_mPV (0, "");                                                    \
    XSRETURN (1)
#define API_RETURN_INT(__int)                                           \
    XST_mIV (0, __int);                                                 \
    XSRETURN (1);
#define API_RETURN_LONG(__long)                                         \
    XST_mIV (0, __long);                                                \
    XSRETURN (1);
#define API_RETURN_OBJ(__obj)                                           \
    ST (0) = newRV_inc((SV *)__obj);                                    \
    if (SvREFCNT(ST(0))) sv_2mortal(ST(0));                             \
    XSRETURN (1);

#define API_DEF_FUNC(__name)                                           \
    newXS ("weechat::" #__name, XS_weechat_api_##__name, "weechat");


extern void boot_DynaLoader (pTHX_ CV* cv);


/*
 * weechat::register: startup function for all WeeChat Perl scripts
 */

XS (XS_weechat_api_register)
{
    char *name, *author, *version, *license, *description, *shutdown_func;
    char *charset;
    dXSARGS;

    /* make C compiler happy */
    (void) items;

    API_FUNC(0, "register", API_RETURN_ERROR);
    perl_current_script = NULL;
    perl_registered_script = NULL;

    if (items < 7)
        API_WRONG_ARGS(API_RETURN_ERROR);

    name = SvPV_nolen (ST (0));
    author = SvPV_nolen (ST (1));
    version = SvPV_nolen (ST (2));
    license = SvPV_nolen (ST (3));
    description = SvPV_nolen (ST (4));
    shutdown_func = SvPV_nolen (ST (5));
    charset = SvPV_nolen (ST (6));

    if (script_search (weechat_perl_plugin, perl_scripts, name))
    {
        /* error: another script already exists with this name! */
        weechat_printf (NULL,
                        weechat_gettext ("%s%s: unable to register script "
                                         "\"%s\" (another script already "
                                         "exists with this name)"),
                        weechat_prefix ("error"), PERL_PLUGIN_NAME, name);
        API_RETURN_ERROR;
    }

    /* register script */
    perl_current_script = script_add (weechat_perl_plugin,
                                      &perl_scripts, &last_perl_script,
                                      (perl_current_script_filename) ?
                                      perl_current_script_filename : "",
                                      name, author, version, license,
                                      description, shutdown_func, charset);
    if (perl_current_script)
    {
        perl_registered_script = perl_current_script;
        if ((weechat_perl_plugin->debug >= 1) || !perl_quiet)
        {
            weechat_printf (NULL,
                            weechat_gettext ("%s: registered script \"%s\", "
                                             "version %s (%s)"),
                            PERL_PLUGIN_NAME, name, version, description);
        }
    }
    else
    {
        API_RETURN_ERROR;
    }

    API_RETURN_OK;
}

/*
 * weechat::plugin_get_name: get name of plugin (return "core" for WeeChat core)
 */

XS (XS_weechat_api_plugin_get_name)
{
    const char *result;
    dXSARGS;

    API_FUNC(1, "plugin_get_name", API_RETURN_EMPTY);
    if (items < 1)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    result = weechat_plugin_get_name (script_str2ptr (SvPV_nolen (ST (0)))); /* plugin */

    API_RETURN_STRING(result);
}

/*
 * weechat::charser_set: set script charset
 */

XS (XS_weechat_api_charset_set)
{
    dXSARGS;

    API_FUNC(1, "charset_set", API_RETURN_ERROR);
    if (items < 1)
        API_WRONG_ARGS(API_RETURN_ERROR);

    script_api_charset_set (perl_current_script,
                            SvPV_nolen (ST (0))); /* charset */

    API_RETURN_OK;
}

/*
 * weechat::iconv_to_internal: convert string to internal WeeChat charset
 */

XS (XS_weechat_api_iconv_to_internal)
{
    char *result, *charset, *string;
    dXSARGS;

    API_FUNC(1, "iconv_to_internal", API_RETURN_EMPTY);
    if (items < 2)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    charset = SvPV_nolen (ST (0));
    string = SvPV_nolen (ST (1));

    result = weechat_iconv_to_internal (charset, string);

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat::iconv_from_internal: convert string from WeeChat inernal charset
 *                               to another one
 */

XS (XS_weechat_api_iconv_from_internal)
{
    char *result, *charset, *string;
    dXSARGS;

    API_FUNC(1, "iconv_from_internal", API_RETURN_EMPTY);
    if (items < 2)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    charset = SvPV_nolen (ST (0));
    string = SvPV_nolen (ST (1));

    result = weechat_iconv_from_internal (charset, string);

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat::gettext: get translated string
 */

XS (XS_weechat_api_gettext)
{
    const char *result;
    dXSARGS;

    API_FUNC(1, "gettext", API_RETURN_EMPTY);
    if (items < 1)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    result = weechat_gettext (SvPV_nolen (ST (0))); /* string */

    API_RETURN_STRING(result);
}

/*
 * weechat::ngettext: get translated string with plural form
 */

XS (XS_weechat_api_ngettext)
{
    char *single, *plural;
    const char *result;
    dXSARGS;

    API_FUNC(1, "ngettext", API_RETURN_EMPTY);
    if (items < 3)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    single = SvPV_nolen (ST (0));
    plural = SvPV_nolen (ST (1));

    result = weechat_ngettext (single, plural,
                               SvIV (ST (2))); /* count */

    API_RETURN_STRING(result);
}

/*
 * weechat::string_match: return 1 if string matches a mask
 *                        mask can begin or end with "*", no other "*"
 *                        are allowed inside mask
 */

XS (XS_weechat_api_string_match)
{
    int value;
    dXSARGS;

    API_FUNC(1, "string_match", API_RETURN_INT(0));
    if (items < 3)
        API_WRONG_ARGS(API_RETURN_INT(0));

    value = weechat_string_match (SvPV_nolen (ST (0)), /* string */
                                  SvPV_nolen (ST (1)), /* mask */
                                  SvIV (ST (2))); /* case_sensitive */

    API_RETURN_INT(value);
}

/*
 * weechat::string_has_highlight: return 1 if string contains a highlight
 *                                (using list of words to highlight)
 *                                return 0 if no highlight is found in string
 */

XS (XS_weechat_api_string_has_highlight)
{
    int value;
    dXSARGS;

    API_FUNC(1, "string_has_highlight", API_RETURN_INT(0));
    if (items < 2)
        API_WRONG_ARGS(API_RETURN_INT(0));

    value = weechat_string_has_highlight (SvPV_nolen (ST (0)), /* string */
                                          SvPV_nolen (ST (1))); /* highlight_words */

    API_RETURN_INT(value);
}

/*
 * weechat::string_has_highlight_regex: return 1 if string contains a highlight
 *                                      (using regular expression)
 *                                      return 0 if no highlight is found in
 *                                      string
 */

XS (XS_weechat_api_string_has_highlight_regex)
{
    int value;
    dXSARGS;

    API_FUNC(1, "string_has_highlight_regex", API_RETURN_INT(0));
    if (items < 2)
        API_WRONG_ARGS(API_RETURN_INT(0));

    value = weechat_string_has_highlight_regex (SvPV_nolen (ST (0)), /* string */
                                                SvPV_nolen (ST (1))); /* regex */

    API_RETURN_INT(value);
}

/*
 * weechat::string_mask_to_regex: convert a mask (string with only "*" as
 *                                wildcard) to a regex, paying attention to
 *                                special chars in a regex
 */

XS (XS_weechat_api_string_mask_to_regex)
{
    char *result;
    dXSARGS;

    API_FUNC(1, "string_mask_to_regex", API_RETURN_EMPTY);
    if (items < 1)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    result = weechat_string_mask_to_regex (SvPV_nolen (ST (0))); /* mask */

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat::string_remove_color: remove WeeChat color codes from string
 */

XS (XS_weechat_api_string_remove_color)
{
    char *result, *string, *replacement;
    dXSARGS;

    API_FUNC(1, "string_remove_color", API_RETURN_EMPTY);
    if (items < 2)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    string = SvPV_nolen (ST (0));
    replacement = SvPV_nolen (ST (1));

    result = weechat_string_remove_color (string, replacement);

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat::string_is_command_char: check if first char of string is a command
 *                                  char
 */

XS (XS_weechat_api_string_is_command_char)
{
    int value;
    dXSARGS;

    API_FUNC(1, "string_is_command_char", API_RETURN_INT(0));
    if (items < 1)
        API_WRONG_ARGS(API_RETURN_INT(0));

    value = weechat_string_is_command_char (SvPV_nolen (ST (0))); /* string */

    API_RETURN_INT(value);
}

/*
 * weechat::string_input_for_buffer: return string with input text for buffer
 *                                   or empty string if it's a command
 */

XS (XS_weechat_api_string_input_for_buffer)
{
    const char *result;
    dXSARGS;

    API_FUNC(1, "string_input_for_buffer", API_RETURN_EMPTY);
    if (items < 1)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    result = weechat_string_input_for_buffer (SvPV_nolen (ST (0))); /* string */

    API_RETURN_STRING(result);
}

/*
 * weechat::mkdir_home: create a directory in WeeChat home
 */

XS (XS_weechat_api_mkdir_home)
{
    dXSARGS;

    API_FUNC(1, "mkdir_home", API_RETURN_ERROR);
    if (items < 2)
        API_WRONG_ARGS(API_RETURN_ERROR);

    if (weechat_mkdir_home (SvPV_nolen (ST (0)), /* directory */
                            SvIV (ST (1)))) /* mode */
        API_RETURN_OK;

    API_RETURN_ERROR;
}

/*
 * weechat::mkdir: create a directory
 */

XS (XS_weechat_api_mkdir)
{
    dXSARGS;

    API_FUNC(1, "mkdir", API_RETURN_ERROR);
    if (items < 2)
        API_WRONG_ARGS(API_RETURN_ERROR);

    if (weechat_mkdir (SvPV_nolen (ST (0)), /* directory */
                       SvIV (ST (1)))) /* mode */
        API_RETURN_OK;

    API_RETURN_ERROR;
}

/*
 * weechat::mkdir_parents: create a directory and make parent directories as
 *                         needed
 */

XS (XS_weechat_api_mkdir_parents)
{
    dXSARGS;

    API_FUNC(1, "mkdir_parents", API_RETURN_ERROR);
    if (items < 2)
        API_WRONG_ARGS(API_RETURN_ERROR);

    if (weechat_mkdir_parents (SvPV_nolen (ST (0)), /* directory */
                               SvIV (ST (1)))) /* mode */
        API_RETURN_OK;

    API_RETURN_ERROR;
}

/*
 * weechat::list_new: create a new list
 */

XS (XS_weechat_api_list_new)
{
    char *result;
    dXSARGS;

    /* make C compiler happy */
    (void) items;
    (void) cv;

    API_FUNC(1, "list_new", API_RETURN_EMPTY);

    result = script_ptr2str (weechat_list_new ());

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat::list_add: add a string to list
 */

XS (XS_weechat_api_list_add)
{
    char *result, *weelist, *data, *where, *user_data;
    dXSARGS;

    API_FUNC(1, "list_add", API_RETURN_EMPTY);
    if (items < 4)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    weelist = SvPV_nolen (ST (0));
    data = SvPV_nolen (ST (1));
    where = SvPV_nolen (ST (2));
    user_data = SvPV_nolen (ST (3));

    result = script_ptr2str (weechat_list_add (script_str2ptr (weelist),
                                               data,
                                               where,
                                               script_str2ptr (user_data)));

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat::list_search: search a string in list
 */

XS (XS_weechat_api_list_search)
{
    char *result, *weelist, *data;
    dXSARGS;

    API_FUNC(1, "list_search", API_RETURN_EMPTY);
    if (items < 2)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    weelist = SvPV_nolen (ST (0));
    data = SvPV_nolen (ST (1));

    result = script_ptr2str (weechat_list_search (script_str2ptr (weelist),
                                                  data));

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat::list_search_pos: search position of a string in list
 */

XS (XS_weechat_api_list_search_pos)
{
    char *weelist, *data;
    int pos;
    dXSARGS;

    API_FUNC(1, "list_search_pos", API_RETURN_INT(-1));
    if (items < 2)
        API_WRONG_ARGS(API_RETURN_INT(-1));

    weelist = SvPV_nolen (ST (0));
    data = SvPV_nolen (ST (1));

    pos = weechat_list_search_pos (script_str2ptr (weelist), data);

    API_RETURN_INT(pos);
}

/*
 * weechat::list_casesearch: search a string in list (ignore case)
 */

XS (XS_weechat_api_list_casesearch)
{
    char *result, *weelist, *data;
    dXSARGS;

    API_FUNC(1, "list_casesearch", API_RETURN_EMPTY);
    if (items < 2)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    weelist = SvPV_nolen (ST (0));
    data = SvPV_nolen (ST (1));

    result = script_ptr2str (weechat_list_casesearch (script_str2ptr (weelist),
                                                      data));

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat::list_casesearch_pos: search position of a string in list
 *                               (ignore case)
 */

XS (XS_weechat_api_list_casesearch_pos)
{
    char *weelist, *data;
    int pos;
    dXSARGS;

    API_FUNC(1, "list_casesearch_pos", API_RETURN_INT(-1));
    if (items < 2)
        API_WRONG_ARGS(API_RETURN_INT(-1));

    weelist = SvPV_nolen (ST (0));
    data = SvPV_nolen (ST (1));

    pos = weechat_list_casesearch_pos (script_str2ptr (weelist), data);

    API_RETURN_INT(pos);
}

/*
 * weechat::list_get: get item by position
 */

XS (XS_weechat_api_list_get)
{
    char *result;
    dXSARGS;

    API_FUNC(1, "list_get", API_RETURN_EMPTY);
    if (items < 2)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    result = script_ptr2str (weechat_list_get (script_str2ptr (SvPV_nolen (ST (0))), /* weelist */
                                               SvIV (ST (1)))); /* position */

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat::list_set: set new value for item
 */

XS (XS_weechat_api_list_set)
{
    char *item, *new_value;
    dXSARGS;

    API_FUNC(1, "list_set", API_RETURN_ERROR);
    if (items < 2)
        API_WRONG_ARGS(API_RETURN_ERROR);

    item = SvPV_nolen (ST (0));
    new_value = SvPV_nolen (ST (1));

    weechat_list_set (script_str2ptr (item), new_value);

    API_RETURN_OK;
}

/*
 * weechat::list_next: get next item
 */

XS (XS_weechat_api_list_next)
{
    char *result;
    dXSARGS;

    API_FUNC(1, "list_next", API_RETURN_EMPTY);
    if (items < 1)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    result = script_ptr2str (weechat_list_next (script_str2ptr (SvPV_nolen (ST (0))))); /* item */

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat::list_prev: get previous item
 */

XS (XS_weechat_api_list_prev)
{
    char *result;
    dXSARGS;

    API_FUNC(1, "list_prev", API_RETURN_EMPTY);
    if (items < 1)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    result = script_ptr2str (weechat_list_prev (script_str2ptr (SvPV_nolen (ST (0))))); /* item */

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat::list_string: get string value of item
 */

XS (XS_weechat_api_list_string)
{
    const char *result;
    dXSARGS;

    API_FUNC(1, "list_string", API_RETURN_EMPTY);
    if (items < 1)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    result = weechat_list_string (script_str2ptr (SvPV_nolen (ST (0)))); /* item */

    API_RETURN_STRING(result);
}

/*
 * weechat::list_size: get number of elements in list
 */

XS (XS_weechat_api_list_size)
{
    int size;
    dXSARGS;

    API_FUNC(1, "list_size", API_RETURN_INT(0));
    if (items < 1)
        API_WRONG_ARGS(API_RETURN_INT(0));

    size = weechat_list_size (script_str2ptr (SvPV_nolen (ST (0)))); /* weelist */

    API_RETURN_INT(size);
}

/*
 * weechat::list_remove: remove item from list
 */

XS (XS_weechat_api_list_remove)
{
    char *weelist, *item;
    dXSARGS;

    API_FUNC(1, "list_remove", API_RETURN_ERROR);
    if (items < 2)
        API_WRONG_ARGS(API_RETURN_ERROR);

    weelist = SvPV_nolen (ST (0));
    item = SvPV_nolen (ST (1));

    weechat_list_remove (script_str2ptr (weelist), script_str2ptr (item));

    API_RETURN_OK;
}

/*
 * weechat::list_remove_all: remove all items from list
 */

XS (XS_weechat_api_list_remove_all)
{
    dXSARGS;

    API_FUNC(1, "list_remove_all", API_RETURN_ERROR);
    if (items < 1)
        API_WRONG_ARGS(API_RETURN_ERROR);

    weechat_list_remove_all (script_str2ptr (SvPV_nolen (ST (0)))); /* weelist */

    API_RETURN_OK;
}

/*
 * weechat::list_free: free list
 */

XS (XS_weechat_api_list_free)
{
    dXSARGS;

    API_FUNC(1, "list_free", API_RETURN_ERROR);
    if (items < 1)
        API_WRONG_ARGS(API_RETURN_ERROR);

    weechat_list_free (script_str2ptr (SvPV_nolen (ST (0)))); /* weelist */

    API_RETURN_OK;
}

/*
 * weechat_perl_api_config_reload_cb: callback for config reload
 */

int
weechat_perl_api_config_reload_cb (void *data,
                                   struct t_config_file *config_file)
{
    struct t_script_callback *script_callback;
    void *func_argv[2];
    char empty_arg[1] = { '\0' };
    int *rc, ret;

    script_callback = (struct t_script_callback *)data;

    if (script_callback && script_callback->function && script_callback->function[0])
    {
        func_argv[0] = (script_callback->data) ? script_callback->data : empty_arg;
        func_argv[1] = script_ptr2str (config_file);

        rc = (int *) weechat_perl_exec (script_callback->script,
                                        WEECHAT_SCRIPT_EXEC_INT,
                                        script_callback->function,
                                        "ss", func_argv);

        if (!rc)
            ret = WEECHAT_CONFIG_READ_FILE_NOT_FOUND;
        else
        {
            ret = *rc;
            free (rc);
        }
        if (func_argv[1])
            free (func_argv[1]);

        return ret;
    }

    return WEECHAT_CONFIG_READ_FILE_NOT_FOUND;
}

/*
 * weechat::config_new: create a new configuration file
 */

XS (XS_weechat_api_config_new)
{
    char *result, *name, *function, *data;
    dXSARGS;

    API_FUNC(1, "config_new", API_RETURN_EMPTY);
    if (items < 3)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    name = SvPV_nolen (ST (0));
    function = SvPV_nolen (ST (1));
    data = SvPV_nolen (ST (2));

    result = script_ptr2str (script_api_config_new (weechat_perl_plugin,
                                                    perl_current_script,
                                                    name,
                                                    &weechat_perl_api_config_reload_cb,
                                                    function,
                                                    data));

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat_perl_api_config_section_read_cb: callback for reading option in section
 */

int
weechat_perl_api_config_section_read_cb (void *data,
                                         struct t_config_file *config_file,
                                         struct t_config_section *section,
                                         const char *option_name,
                                         const char *value)
{
    struct t_script_callback *script_callback;
    void *func_argv[5];
    char empty_arg[1] = { '\0' };
    int *rc, ret;

    script_callback = (struct t_script_callback *)data;

    if (script_callback && script_callback->function && script_callback->function[0])
    {
        func_argv[0] = (script_callback->data) ? script_callback->data : empty_arg;
        func_argv[1] = script_ptr2str (config_file);
        func_argv[2] = script_ptr2str (section);
        func_argv[3] = (option_name) ? (char *)option_name : empty_arg;
        func_argv[4] = (value) ? (char *)value : empty_arg;

        rc = (int *) weechat_perl_exec (script_callback->script,
                                        WEECHAT_SCRIPT_EXEC_INT,
                                        script_callback->function,
                                        "sssss", func_argv);

        if (!rc)
            ret = WEECHAT_CONFIG_OPTION_SET_ERROR;
        else
        {
            ret = *rc;
            free (rc);
        }
        if (func_argv[1])
            free (func_argv[1]);
        if (func_argv[2])
            free (func_argv[2]);

        return ret;
    }

    return WEECHAT_CONFIG_OPTION_SET_ERROR;
}

/*
 * weechat_perl_api_config_section_write_cb: callback for writing section
 */

int
weechat_perl_api_config_section_write_cb (void *data,
                                          struct t_config_file *config_file,
                                          const char *section_name)
{
    struct t_script_callback *script_callback;
    void *func_argv[3];
    char empty_arg[1] = { '\0' };
    int *rc, ret;

    script_callback = (struct t_script_callback *)data;

    if (script_callback && script_callback->function && script_callback->function[0])
    {
        func_argv[0] = (script_callback->data) ? script_callback->data : empty_arg;
        func_argv[1] = script_ptr2str (config_file);
        func_argv[2] = (section_name) ? (char *)section_name : empty_arg;

        rc = (int *) weechat_perl_exec (script_callback->script,
                                        WEECHAT_SCRIPT_EXEC_INT,
                                        script_callback->function,
                                        "sss", func_argv);

        if (!rc)
            ret = WEECHAT_CONFIG_WRITE_ERROR;
        else
        {
            ret = *rc;
            free (rc);
        }
        if (func_argv[1])
            free (func_argv[1]);

        return ret;
    }

    return WEECHAT_CONFIG_WRITE_ERROR;
}

/*
 * weechat_perl_api_config_section_write_default_cb: callback for writing
 *                                                   default values for section
 */

int
weechat_perl_api_config_section_write_default_cb (void *data,
                                                  struct t_config_file *config_file,
                                                  const char *section_name)
{
    struct t_script_callback *script_callback;
    void *func_argv[3];
    char empty_arg[1] = { '\0' };
    int *rc, ret;

    script_callback = (struct t_script_callback *)data;

    if (script_callback && script_callback->function && script_callback->function[0])
    {
        func_argv[0] = (script_callback->data) ? script_callback->data : empty_arg;
        func_argv[1] = script_ptr2str (config_file);
        func_argv[2] = (section_name) ? (char *)section_name : empty_arg;

        rc = (int *) weechat_perl_exec (script_callback->script,
                                        WEECHAT_SCRIPT_EXEC_INT,
                                        script_callback->function,
                                        "sss", func_argv);

        if (!rc)
            ret = WEECHAT_CONFIG_WRITE_ERROR;
        else
        {
            ret = *rc;
            free (rc);
        }
        if (func_argv[1])
            free (func_argv[1]);

        return ret;
    }

    return WEECHAT_CONFIG_WRITE_ERROR;
}

/*
 * weechat_perl_api_config_section_create_option_cb: callback to create an option
 */

int
weechat_perl_api_config_section_create_option_cb (void *data,
                                                  struct t_config_file *config_file,
                                                  struct t_config_section *section,
                                                  const char *option_name,
                                                  const char *value)
{
    struct t_script_callback *script_callback;
    void *func_argv[5];
    char empty_arg[1] = { '\0' };
    int *rc, ret;

    script_callback = (struct t_script_callback *)data;

    if (script_callback && script_callback->function && script_callback->function[0])
    {
        func_argv[0] = (script_callback->data) ? script_callback->data : empty_arg;
        func_argv[1] = script_ptr2str (config_file);
        func_argv[2] = script_ptr2str (section);
        func_argv[3] = (option_name) ? (char *)option_name : empty_arg;
        func_argv[4] = (value) ? (char *)value : empty_arg;

        rc = (int *) weechat_perl_exec (script_callback->script,
                                        WEECHAT_SCRIPT_EXEC_INT,
                                        script_callback->function,
                                        "sssss", func_argv);

        if (!rc)
            ret = WEECHAT_CONFIG_OPTION_SET_ERROR;
        else
        {
            ret = *rc;
            free (rc);
        }
        if (func_argv[1])
            free (func_argv[1]);
        if (func_argv[2])
            free (func_argv[2]);

        return ret;
    }

    return WEECHAT_CONFIG_OPTION_SET_ERROR;
}

/*
 * weechat_perl_api_config_section_delete_option_cb: callback to delete an option
 */

int
weechat_perl_api_config_section_delete_option_cb (void *data,
                                                  struct t_config_file *config_file,
                                                  struct t_config_section *section,
                                                  struct t_config_option *option)
{
    struct t_script_callback *script_callback;
    void *func_argv[4];
    char empty_arg[1] = { '\0' };
    int *rc, ret;

    script_callback = (struct t_script_callback *)data;

    if (script_callback && script_callback->function && script_callback->function[0])
    {
        func_argv[0] = (script_callback->data) ? script_callback->data : empty_arg;
        func_argv[1] = script_ptr2str (config_file);
        func_argv[2] = script_ptr2str (section);
        func_argv[3] = script_ptr2str (option);

        rc = (int *) weechat_perl_exec (script_callback->script,
                                        WEECHAT_SCRIPT_EXEC_INT,
                                        script_callback->function,
                                        "ssss", func_argv);

        if (!rc)
            ret = WEECHAT_CONFIG_OPTION_UNSET_ERROR;
        else
        {
            ret = *rc;
            free (rc);
        }
        if (func_argv[1])
            free (func_argv[1]);
        if (func_argv[2])
            free (func_argv[2]);
        if (func_argv[3])
            free (func_argv[3]);

        return ret;
    }

    return WEECHAT_CONFIG_OPTION_UNSET_ERROR;
}

/*
 * weechat::config_new_section: create a new section in configuration file
 */

XS (XS_weechat_api_config_new_section)
{
    char *result, *cfg_file, *name, *function_read, *data_read;
    char *function_write, *data_write, *function_write_default;
    char *data_write_default, *function_create_option, *data_create_option;
    char *function_delete_option, *data_delete_option;

    dXSARGS;

    API_FUNC(1, "config_new_section", API_RETURN_EMPTY);
    if (items < 14)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    cfg_file = SvPV_nolen (ST (0));
    name = SvPV_nolen (ST (1));
    function_read = SvPV_nolen (ST (4));
    data_read = SvPV_nolen (ST (5));
    function_write = SvPV_nolen (ST (6));
    data_write = SvPV_nolen (ST (7));
    function_write_default = SvPV_nolen (ST (8));
    data_write_default = SvPV_nolen (ST (9));
    function_create_option = SvPV_nolen (ST (10));
    data_create_option = SvPV_nolen (ST (11));
    function_delete_option = SvPV_nolen (ST (12));
    data_delete_option = SvPV_nolen (ST (13));

    result = script_ptr2str (script_api_config_new_section (weechat_perl_plugin,
                                                            perl_current_script,
                                                            script_str2ptr (cfg_file),
                                                            name,
                                                            SvIV (ST (2)), /* user_can_add_options */
                                                            SvIV (ST (3)), /* user_can_delete_options */
                                                            &weechat_perl_api_config_section_read_cb,
                                                            function_read,
                                                            data_read,
                                                            &weechat_perl_api_config_section_write_cb,
                                                            function_write,
                                                            data_write,
                                                            &weechat_perl_api_config_section_write_default_cb,
                                                            function_write_default,
                                                            data_write_default,
                                                            &weechat_perl_api_config_section_create_option_cb,
                                                            function_create_option,
                                                            data_create_option,
                                                            &weechat_perl_api_config_section_delete_option_cb,
                                                            function_delete_option,
                                                            data_delete_option));

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat::config_search_section: search section in configuration file
 */

XS (XS_weechat_api_config_search_section)
{
    char *result, *config_file, *section_name;
    dXSARGS;

    API_FUNC(1, "config_search_section", API_RETURN_EMPTY);
    if (items < 2)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    config_file = SvPV_nolen (ST (0));
    section_name = SvPV_nolen (ST (1));

    result = script_ptr2str (weechat_config_search_section (script_str2ptr (config_file),
                                                            section_name));

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat_perl_api_config_option_check_value_cb: callback for checking new
 *                                                value for option
 */

int
weechat_perl_api_config_option_check_value_cb (void *data,
                                               struct t_config_option *option,
                                               const char *value)
{
    struct t_script_callback *script_callback;
    void *func_argv[3];
    char empty_arg[1] = { '\0' };
    int *rc, ret;

    script_callback = (struct t_script_callback *)data;

    if (script_callback && script_callback->function && script_callback->function[0])
    {
        func_argv[0] = (script_callback->data) ? script_callback->data : empty_arg;
        func_argv[1] = script_ptr2str (option);
        func_argv[2] = (value) ? (char *)value : empty_arg;

        rc = (int *) weechat_perl_exec (script_callback->script,
                                        WEECHAT_SCRIPT_EXEC_INT,
                                        script_callback->function,
                                        "sss", func_argv);

        if (!rc)
            ret = 0;
        else
        {
            ret = *rc;
            free (rc);
        }
        if (func_argv[1])
            free (func_argv[1]);

        return ret;
    }

    return 0;
}

/*
 * weechat_perl_api_config_option_change_cb: callback for option changed
 */

void
weechat_perl_api_config_option_change_cb (void *data,
                                          struct t_config_option *option)
{
    struct t_script_callback *script_callback;
    void *func_argv[2];
    char empty_arg[1] = { '\0' };
    int *rc;

    script_callback = (struct t_script_callback *)data;

    if (script_callback && script_callback->function && script_callback->function[0])
    {
        func_argv[0] = (script_callback->data) ? script_callback->data : empty_arg;
        func_argv[1] = script_ptr2str (option);

        rc = (int *) weechat_perl_exec (script_callback->script,
                                        WEECHAT_SCRIPT_EXEC_INT,
                                        script_callback->function,
                                        "ss", func_argv);

        if (func_argv[1])
            free (func_argv[1]);

        if (rc)
            free (rc);
    }
}

/*
 * weechat_perl_api_config_option_delete_cb: callback when option is deleted
 */

void
weechat_perl_api_config_option_delete_cb (void *data,
                                          struct t_config_option *option)
{
    struct t_script_callback *script_callback;
    void *func_argv[2];
    char empty_arg[1] = { '\0' };
    int *rc;

    script_callback = (struct t_script_callback *)data;

    if (script_callback && script_callback->function && script_callback->function[0])
    {
        func_argv[0] = (script_callback->data) ? script_callback->data : empty_arg;
        func_argv[1] = script_ptr2str (option);

        rc = (int *) weechat_perl_exec (script_callback->script,
                                        WEECHAT_SCRIPT_EXEC_INT,
                                        script_callback->function,
                                        "ss", func_argv);

        if (func_argv[1])
            free (func_argv[1]);

        if (rc)
            free (rc);
    }
}

/*
 * weechat::config_new_option: create a new option in section
 */

XS (XS_weechat_api_config_new_option)
{
    char *result, *config_file, *section, *name, *type;
    char *description, *string_values, *default_value, *value;
    char *function_check_value, *data_check_value, *function_change;
    char *data_change, *function_delete, *data_delete;
    dXSARGS;

    API_FUNC(1, "config_new_option", API_RETURN_EMPTY);
    if (items < 17)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    config_file = SvPV_nolen (ST (0));
    section = SvPV_nolen (ST (1));
    name = SvPV_nolen (ST (2));
    type = SvPV_nolen (ST (3));
    description = SvPV_nolen (ST (4));
    string_values = SvPV_nolen (ST (5));
    default_value = SvPV_nolen (ST (8));
    value = SvPV_nolen (ST (9));
    function_check_value = SvPV_nolen (ST (11));
    data_check_value = SvPV_nolen (ST (12));
    function_change = SvPV_nolen (ST (13));
    data_change = SvPV_nolen (ST (14));
    function_delete = SvPV_nolen (ST (15));
    data_delete = SvPV_nolen (ST (16));

    result = script_ptr2str (script_api_config_new_option (weechat_perl_plugin,
                                                           perl_current_script,
                                                           script_str2ptr (config_file),
                                                           script_str2ptr (section),
                                                           name,
                                                           type,
                                                           description,
                                                           string_values,
                                                           SvIV (ST (6)), /* min */
                                                           SvIV (ST (7)), /* max */
                                                           default_value,
                                                           value,
                                                           SvIV (ST (10)), /* null_value_allowed */
                                                           &weechat_perl_api_config_option_check_value_cb,
                                                           function_check_value,
                                                           data_check_value,
                                                           &weechat_perl_api_config_option_change_cb,
                                                           function_change,
                                                           data_change,
                                                           &weechat_perl_api_config_option_delete_cb,
                                                           function_delete,
                                                           data_delete));

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat::config_search_option: search option in configuration file or section
 */

XS (XS_weechat_api_config_search_option)
{
    char *result, *config_file, *section, *option_name;
    dXSARGS;

    API_FUNC(1, "config_search_option", API_RETURN_EMPTY);
    if (items < 3)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    config_file = SvPV_nolen (ST (0));
    section = SvPV_nolen (ST (1));
    option_name = SvPV_nolen (ST (2));

    result = script_ptr2str (weechat_config_search_option (script_str2ptr (config_file),
                                                           script_str2ptr (section),
                                                           option_name));

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat::config_string_to_boolean: return boolean value of a string
 */

XS (XS_weechat_api_config_string_to_boolean)
{
    int value;
    dXSARGS;

    API_FUNC(1, "config_string_to_boolean", API_RETURN_INT(0));
    if (items < 1)
        API_WRONG_ARGS(API_RETURN_INT(0));

    value = weechat_config_string_to_boolean (SvPV_nolen (ST (0))); /* text */

    API_RETURN_INT(value);
}

/*
 * weechat::config_option_reset: reset an option with default value
 */

XS (XS_weechat_api_config_option_reset)
{
    int rc;
    char *option;
    dXSARGS;

    API_FUNC(1, "config_option_reset", API_RETURN_INT(0));
    if (items < 2)
        API_WRONG_ARGS(API_RETURN_INT(0));

    option = SvPV_nolen (ST (0));

    rc = weechat_config_option_reset (script_str2ptr (option),
                                      SvIV (ST (1))); /* run_callback */

    API_RETURN_INT(rc);
}

/*
 * weechat::config_option_set: set new value for option
 */

XS (XS_weechat_api_config_option_set)
{
    int rc;
    char *option, *new_value;
    dXSARGS;

    API_FUNC(1, "config_option_set", API_RETURN_INT(WEECHAT_CONFIG_OPTION_SET_ERROR));
    if (items < 3)
        API_WRONG_ARGS(API_RETURN_INT(WEECHAT_CONFIG_OPTION_SET_ERROR));

    option = SvPV_nolen (ST (0));
    new_value = SvPV_nolen (ST (1));

    rc = weechat_config_option_set (script_str2ptr (option),
                                    new_value,
                                    SvIV (ST (2))); /* run_callback */

    API_RETURN_INT(rc);
}

/*
 * weechat::config_option_set_null: set null (undefined) value for option
 */

XS (XS_weechat_api_config_option_set_null)
{
    int rc;
    char *option;
    dXSARGS;

    API_FUNC(1, "config_option_set_null", API_RETURN_INT(WEECHAT_CONFIG_OPTION_SET_ERROR));
    if (items < 2)
        API_WRONG_ARGS(API_RETURN_INT(WEECHAT_CONFIG_OPTION_SET_ERROR));

    option = SvPV_nolen (ST (0));

    rc = weechat_config_option_set_null (script_str2ptr (option),
                                         SvIV (ST (1))); /* run_callback */

    API_RETURN_INT(rc);
}

/*
 * weechat::config_option_unset: unset an option
 */

XS (XS_weechat_api_config_option_unset)
{
    int rc;
    char *option;
    dXSARGS;

    API_FUNC(1, "config_option_unset", API_RETURN_INT(WEECHAT_CONFIG_OPTION_UNSET_ERROR));
    if (items < 1)
        API_WRONG_ARGS(API_RETURN_INT(WEECHAT_CONFIG_OPTION_UNSET_ERROR));

    option = SvPV_nolen (ST (0));

    rc = weechat_config_option_unset (script_str2ptr (option));

    API_RETURN_INT(rc);
}

/*
 * weechat::config_option_rename: rename an option
 */

XS (XS_weechat_api_config_option_rename)
{
    char *option, *new_name;
    dXSARGS;

    API_FUNC(1, "config_option_rename", API_RETURN_ERROR);
    if (items < 2)
        API_WRONG_ARGS(API_RETURN_ERROR);

    option = SvPV_nolen (ST (0));
    new_name = SvPV_nolen (ST (1));

    weechat_config_option_rename (script_str2ptr (option),
                                  new_name);

    API_RETURN_OK;
}

/*
 * weechat::config_option_is_null: return 1 if value of option is null
 */

XS (XS_weechat_api_config_option_is_null)
{
    int value;
    dXSARGS;

    API_FUNC(1, "config_option_is_null", API_RETURN_INT(1));
    if (items < 1)
        API_WRONG_ARGS(API_RETURN_INT(1));

    value = weechat_config_option_is_null (script_str2ptr (SvPV_nolen (ST (0)))); /* option */

    API_RETURN_INT(value);
}

/*
 * weechat::config_option_default_is_null: return 1 if default value of option
 *                                         is null
 */

XS (XS_weechat_api_config_option_default_is_null)
{
    int value;
    dXSARGS;

    API_FUNC(1, "config_option_default_is_null", API_RETURN_INT(1));
    if (items < 1)
        API_WRONG_ARGS(API_RETURN_INT(1));

    value = weechat_config_option_default_is_null (script_str2ptr (SvPV_nolen (ST (0)))); /* option */

    API_RETURN_INT(value);
}

/*
 * weechat::config_boolean: return boolean value of option
 */

XS (XS_weechat_api_config_boolean)
{
    int value;
    dXSARGS;

    API_FUNC(1, "config_boolean", API_RETURN_INT(0));
    if (items < 1)
        API_WRONG_ARGS(API_RETURN_INT(0));

    value = weechat_config_boolean (script_str2ptr (SvPV_nolen (ST (0)))); /* option */

    API_RETURN_INT(value);
}

/*
 * weechat::config_boolean_default: return default boolean value of option
 */

XS (XS_weechat_api_config_boolean_default)
{
    int value;
    dXSARGS;

    API_FUNC(1, "config_boolean_default", API_RETURN_INT(0));
    if (items < 1)
        API_WRONG_ARGS(API_RETURN_INT(0));

    value = weechat_config_boolean_default (script_str2ptr (SvPV_nolen (ST (0)))); /* option */

    API_RETURN_INT(value);
}

/*
 * weechat::config_integer: return integer value of option
 */

XS (XS_weechat_api_config_integer)
{
    int value;
    dXSARGS;

    API_FUNC(1, "config_integer", API_RETURN_INT(0));
    if (items < 1)
        API_WRONG_ARGS(API_RETURN_INT(0));

    value = weechat_config_integer (script_str2ptr (SvPV_nolen (ST (0)))); /* option */

    API_RETURN_INT(value);
}

/*
 * weechat::config_integer_default: return default integer value of option
 */

XS (XS_weechat_api_config_integer_default)
{
    int value;
    dXSARGS;

    API_FUNC(1, "config_integer_default", API_RETURN_INT(0));
    if (items < 1)
        API_WRONG_ARGS(API_RETURN_INT(0));

    value = weechat_config_integer_default (script_str2ptr (SvPV_nolen (ST (0)))); /* option */

    API_RETURN_INT(value);
}

/*
 * weechat::config_string: return string value of option
 */

XS (XS_weechat_api_config_string)
{
    const char *result;
    dXSARGS;

    API_FUNC(1, "config_string", API_RETURN_EMPTY);
    if (items < 1)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    result = weechat_config_string (script_str2ptr (SvPV_nolen (ST (0)))); /* option */

    API_RETURN_STRING(result);
}

/*
 * weechat::config_string_default: return default string value of option
 */

XS (XS_weechat_api_config_string_default)
{
    const char *result;
    dXSARGS;

    API_FUNC(1, "config_string_default", API_RETURN_EMPTY);
    if (items < 1)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    result = weechat_config_string_default (script_str2ptr (SvPV_nolen (ST (0)))); /* option */

    API_RETURN_STRING(result);
}

/*
 * weechat::config_color: return color value of option
 */

XS (XS_weechat_api_config_color)
{
    const char *result;
    dXSARGS;

    API_FUNC(1, "config_color", API_RETURN_INT(0));
    if (items < 1)
        API_WRONG_ARGS(API_RETURN_INT(0));

    result = weechat_config_color (script_str2ptr (SvPV_nolen (ST (0)))); /* option */

    API_RETURN_STRING(result);
}

/*
 * weechat::config_color_default: return default color value of option
 */

XS (XS_weechat_api_config_color_default)
{
    const char *result;
    dXSARGS;

    API_FUNC(1, "config_color_default", API_RETURN_INT(0));
    if (items < 1)
        API_WRONG_ARGS(API_RETURN_INT(0));

    result = weechat_config_color_default (script_str2ptr (SvPV_nolen (ST (0)))); /* option */

    API_RETURN_STRING(result);
}

/*
 * weechat::config_write_option: write an option in configuration file
 */

XS (XS_weechat_api_config_write_option)
{
    char *config_file, *option;
    dXSARGS;

    API_FUNC(1, "config_write_option", API_RETURN_ERROR);
    if (items < 2)
        API_WRONG_ARGS(API_RETURN_ERROR);

    config_file = SvPV_nolen (ST (0));
    option = SvPV_nolen (ST (1));

    weechat_config_write_option (script_str2ptr (config_file),
                                 script_str2ptr (option));

    API_RETURN_OK;
}

/*
 * weechat::config_write_line: write a line in configuration file
 */

XS (XS_weechat_api_config_write_line)
{
    char *config_file, *option_name, *value;
    dXSARGS;

    API_FUNC(1, "config_write_line", API_RETURN_ERROR);
    if (items < 3)
        API_WRONG_ARGS(API_RETURN_ERROR);

    config_file = SvPV_nolen (ST (0));
    option_name = SvPV_nolen (ST (1));
    value = SvPV_nolen (ST (2));

    weechat_config_write_line (script_str2ptr (config_file), option_name,
                               "%s", value);

    API_RETURN_OK;
}

/*
 * weechat::config_write: write configuration file
 */

XS (XS_weechat_api_config_write)
{
    int rc;
    dXSARGS;

    API_FUNC(1, "config_write", API_RETURN_INT(-1));
    if (items < 1)
        API_WRONG_ARGS(API_RETURN_INT(-1));

    rc = weechat_config_write (script_str2ptr (SvPV_nolen (ST (0)))); /* config_file */

    API_RETURN_INT(rc);
}

/*
 * weechat::config_read: read configuration file
 */

XS (XS_weechat_api_config_read)
{
    int rc;
    dXSARGS;

    API_FUNC(1, "config_read", API_RETURN_INT(-1));
    if (items < 1)
        API_WRONG_ARGS(API_RETURN_INT(-1));

    rc = weechat_config_read (script_str2ptr (SvPV_nolen (ST (0)))); /* config_file */

    API_RETURN_INT(rc);
}

/*
 * weechat::config_reload: reload configuration file
 */

XS (XS_weechat_api_config_reload)
{
    int rc;
    dXSARGS;

    API_FUNC(1, "config_reload", API_RETURN_INT(-1));
    if (items < 1)
        API_WRONG_ARGS(API_RETURN_INT(-1));

    rc = weechat_config_reload (script_str2ptr (SvPV_nolen (ST (0)))); /* config_file */

    API_RETURN_INT(rc);
}

/*
 * weechat::config_option_free: free an option in configuration file
 */

XS (XS_weechat_api_config_option_free)
{
    dXSARGS;

    API_FUNC(1, "config_option_free", API_RETURN_ERROR);
    if (items < 1)
        API_WRONG_ARGS(API_RETURN_ERROR);

    script_api_config_option_free (weechat_perl_plugin,
                                   perl_current_script,
                                   script_str2ptr (SvPV_nolen (ST (0)))); /* option */

    API_RETURN_OK;
}

/*
 * weechat::config_section_free_options: free options of a section in
 *                                       configuration file
 */

XS (XS_weechat_api_config_section_free_options)
{
    dXSARGS;

    API_FUNC(1, "config_section_free_options", API_RETURN_ERROR);
    if (items < 1)
        API_WRONG_ARGS(API_RETURN_ERROR);

    script_api_config_section_free_options (weechat_perl_plugin,
                                            perl_current_script,
                                            script_str2ptr (SvPV_nolen (ST (0)))); /* section */

    API_RETURN_OK;
}

/*
 * weechat::config_section_free: free section in configuration file
 */

XS (XS_weechat_api_config_section_free)
{
    dXSARGS;

    API_FUNC(1, "config_section_free", API_RETURN_ERROR);
    if (items < 1)
        API_WRONG_ARGS(API_RETURN_ERROR);

    script_api_config_section_free (weechat_perl_plugin,
                                    perl_current_script,
                                    script_str2ptr (SvPV_nolen (ST (0)))); /* section */

    API_RETURN_OK;
}

/*
 * weechat::config_free: free configuration file
 */

XS (XS_weechat_api_config_free)
{
    dXSARGS;

    API_FUNC(1, "config_free", API_RETURN_ERROR);
    if (items < 1)
        API_WRONG_ARGS(API_RETURN_ERROR);

    script_api_config_free (weechat_perl_plugin,
                            perl_current_script,
                            script_str2ptr (SvPV_nolen (ST (0)))); /* config_file */

    API_RETURN_OK;
}

/*
 * weechat::config_get: get config option
 */

XS (XS_weechat_api_config_get)
{
    char *result;
    dXSARGS;

    API_FUNC(1, "config_get", API_RETURN_EMPTY);
    if (items < 1)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    result = script_ptr2str (weechat_config_get (SvPV_nolen (ST (0))));

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat::config_get_plugin: get value of a plugin option
 */

XS (XS_weechat_api_config_get_plugin)
{
    const char *result;
    dXSARGS;

    API_FUNC(1, "config_get_plugin", API_RETURN_EMPTY);
    if (items < 1)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    result = script_api_config_get_plugin (weechat_perl_plugin,
                                           perl_current_script,
                                           SvPV_nolen (ST (0)));

    API_RETURN_STRING(result);
}

/*
 * weechat::config_is_set_plugin: check if a plugin option is set
 */

XS (XS_weechat_api_config_is_set_plugin)
{
    char *option;
    int rc;
    dXSARGS;

    API_FUNC(1, "config_is_set_plugin", API_RETURN_INT(0));
    if (items < 1)
        API_WRONG_ARGS(API_RETURN_INT(0));

    option = SvPV_nolen (ST (0));

    rc = script_api_config_is_set_plugin (weechat_perl_plugin,
                                          perl_current_script,
                                          option);

    API_RETURN_INT(rc);
}

/*
 * weechat::config_set_plugin: set value of a plugin option
 */

XS (XS_weechat_api_config_set_plugin)
{
    char *option, *value;
    int rc;
    dXSARGS;

    API_FUNC(1, "config_set_plugin", API_RETURN_INT(WEECHAT_CONFIG_OPTION_SET_ERROR));
    if (items < 2)
        API_WRONG_ARGS(API_RETURN_INT(WEECHAT_CONFIG_OPTION_SET_ERROR));

    option = SvPV_nolen (ST (0));
    value = SvPV_nolen (ST (1));

    rc = script_api_config_set_plugin (weechat_perl_plugin,
                                       perl_current_script,
                                       option,
                                       value);

    API_RETURN_INT(rc);
}

/*
 * weechat::config_set_desc_plugin: set description of a plugin option
 */

XS (XS_weechat_api_config_set_desc_plugin)
{
    char *option, *description;
    dXSARGS;

    API_FUNC(1, "config_set_desc_plugin", API_RETURN_ERROR);
    if (items < 2)
        API_WRONG_ARGS(API_RETURN_ERROR);

    option = SvPV_nolen (ST (0));
    description = SvPV_nolen (ST (1));

    script_api_config_set_desc_plugin (weechat_perl_plugin,
                                       perl_current_script,
                                       option,
                                       description);

    API_RETURN_OK;
}

/*
 * weechat::config_unset_plugin: unset a plugin option
 */

XS (XS_weechat_api_config_unset_plugin)
{
    char *option;
    int rc;
    dXSARGS;

    API_FUNC(1, "config_unset_plugin", API_RETURN_INT(WEECHAT_CONFIG_OPTION_UNSET_ERROR));
    if (items < 1)
        API_WRONG_ARGS(API_RETURN_INT(WEECHAT_CONFIG_OPTION_UNSET_ERROR));

    option = SvPV_nolen (ST (0));

    rc = script_api_config_unset_plugin (weechat_perl_plugin,
                                         perl_current_script,
                                         option);

    API_RETURN_INT(rc);
}

/*
 * weechat::key_bind: bind key(s)
 */

XS (XS_weechat_api_key_bind)
{
    char *context;
    struct t_hashtable *hashtable;
    int num_keys;
    dXSARGS;

    API_FUNC(1, "key_bind", API_RETURN_INT(0));
    if (items < 2)
        API_WRONG_ARGS(API_RETURN_INT(0));

    context = SvPV_nolen (ST (0));
    hashtable = weechat_perl_hash_to_hashtable (ST (1),
                                                WEECHAT_SCRIPT_HASHTABLE_DEFAULT_SIZE);

    num_keys = weechat_key_bind (context, hashtable);

    if (hashtable)
        weechat_hashtable_free (hashtable);

    API_RETURN_INT(num_keys);
}

/*
 * weechat::key_unbind: unbind key(s)
 */

XS (XS_weechat_api_key_unbind)
{
    char *context, *key;
    int num_keys;
    dXSARGS;

    API_FUNC(1, "key_unbind", API_RETURN_INT(0));
    if (items < 2)
        API_WRONG_ARGS(API_RETURN_INT(0));

    context = SvPV_nolen (ST (0));
    key = SvPV_nolen (ST (1));

    num_keys = weechat_key_unbind (context, key);

    API_RETURN_INT(num_keys);
}

/*
 * weechat::prefix: get a prefix, used for display
 */

XS (XS_weechat_api_prefix)
{
    const char *result;
    dXSARGS;

    API_FUNC(0, "prefix", API_RETURN_EMPTY);
    if (items < 1)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    result = weechat_prefix (SvPV_nolen (ST (0))); /* prefix */

    API_RETURN_STRING(result);
}

/*
 * weechat::color: get a color code, used for display
 */

XS (XS_weechat_api_color)
{
    const char *result;
    dXSARGS;

    API_FUNC(0, "color", API_RETURN_EMPTY);
    if (items < 1)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    result = weechat_color (SvPV_nolen (ST (0))); /* color */

    API_RETURN_STRING(result);
}

/*
 * weechat::print: print message in a buffer
 */

XS (XS_weechat_api_print)
{
    char *buffer, *message;
    dXSARGS;

    API_FUNC(0, "print", API_RETURN_ERROR);
    if (items < 2)
        API_WRONG_ARGS(API_RETURN_ERROR);

    buffer = SvPV_nolen (ST (0));
    message = SvPV_nolen (ST (1));

    script_api_printf (weechat_perl_plugin,
                       perl_current_script,
                       script_str2ptr (buffer),
                       "%s", message);

    API_RETURN_OK;
}

/*
 * weechat::print_date_tags: print message in a buffer with optional date and
 *                           tags
 */

XS (XS_weechat_api_print_date_tags)
{
    char *buffer, *tags, *message;
    dXSARGS;

    API_FUNC(1, "print_date_tags", API_RETURN_ERROR);
    if (items < 4)
        API_WRONG_ARGS(API_RETURN_ERROR);

    buffer = SvPV_nolen (ST (0));
    tags = SvPV_nolen (ST (2));
    message = SvPV_nolen (ST (3));

    script_api_printf_date_tags (weechat_perl_plugin,
                                 perl_current_script,
                                 script_str2ptr (buffer),
                                 SvIV (ST (1)),
                                 tags,
                                 "%s", message);

    API_RETURN_OK;
}

/*
 * weechat::print_y: print message in a buffer with free content
 */

XS (XS_weechat_api_print_y)
{
    char *buffer, *message;
    dXSARGS;

    API_FUNC(1, "print_y", API_RETURN_ERROR);
    if (items < 3)
        API_WRONG_ARGS(API_RETURN_ERROR);

    buffer = SvPV_nolen (ST (0));
    message = SvPV_nolen (ST (2));

    script_api_printf_y (weechat_perl_plugin,
                         perl_current_script,
                         script_str2ptr (buffer),
                         SvIV (ST (1)),
                         "%s", message);

    API_RETURN_OK;
}

/*
 * weechat::log_print: print message in WeeChat log file
 */

XS (XS_weechat_api_log_print)
{
    dXSARGS;

    API_FUNC(1, "log_print", API_RETURN_ERROR);

    if (items < 1)
        API_WRONG_ARGS(API_RETURN_ERROR);

    script_api_log_printf (weechat_perl_plugin,
                           perl_current_script,
                           "%s", SvPV_nolen (ST (0))); /* message */

    API_RETURN_OK;
}

/*
 * weechat_perl_api_hook_command_cb: callback for command hooked
 */

int
weechat_perl_api_hook_command_cb (void *data, struct t_gui_buffer *buffer,
                                  int argc, char **argv, char **argv_eol)
{
    struct t_script_callback *script_callback;
    void *func_argv[3];
    char empty_arg[1] = { '\0' };
    int *rc, ret;

    /* make C compiler happy */
    (void) argv;

    script_callback = (struct t_script_callback *)data;

    if (script_callback && script_callback->function && script_callback->function[0])
    {
        func_argv[0] = (script_callback->data) ? script_callback->data : empty_arg;
        func_argv[1] = script_ptr2str (buffer);
        func_argv[2] = (argc > 1) ? argv_eol[1] : empty_arg;

        rc = (int *) weechat_perl_exec (script_callback->script,
                                        WEECHAT_SCRIPT_EXEC_INT,
                                        script_callback->function,
                                        "sss", func_argv);

        if (!rc)
            ret = WEECHAT_RC_ERROR;
        else
        {
            ret = *rc;
            free (rc);
        }
        if (func_argv[1])
            free (func_argv[1]);

        return ret;
    }

    return WEECHAT_RC_ERROR;
}

/*
 * weechat::hook_command: hook a command
 */

XS (XS_weechat_api_hook_command)
{
    char *result, *command, *description, *args, *args_description;
    char *completion, *function, *data;
    dXSARGS;

    API_FUNC(1, "hook_command", API_RETURN_EMPTY);
    if (items < 7)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    command = SvPV_nolen (ST (0));
    description = SvPV_nolen (ST (1));
    args = SvPV_nolen (ST (2));
    args_description = SvPV_nolen (ST (3));
    completion = SvPV_nolen (ST (4));
    function = SvPV_nolen (ST (5));
    data = SvPV_nolen (ST (6));

    result = script_ptr2str (script_api_hook_command (weechat_perl_plugin,
                                                      perl_current_script,
                                                      command,
                                                      description,
                                                      args,
                                                      args_description,
                                                      completion,
                                                      &weechat_perl_api_hook_command_cb,
                                                      function,
                                                      data));

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat_perl_api_hook_command_run_cb: callback for command_run hooked
 */

int
weechat_perl_api_hook_command_run_cb (void *data, struct t_gui_buffer *buffer,
                                      const char *command)
{
    struct t_script_callback *script_callback;
    void *func_argv[3];
    char empty_arg[1] = { '\0' };
    int *rc, ret;

    script_callback = (struct t_script_callback *)data;

    if (script_callback && script_callback->function && script_callback->function[0])
    {
        func_argv[0] = (script_callback->data) ? script_callback->data : empty_arg;
        func_argv[1] = script_ptr2str (buffer);
        func_argv[2] = (command) ? (char *)command : empty_arg;

        rc = (int *) weechat_perl_exec (script_callback->script,
                                        WEECHAT_SCRIPT_EXEC_INT,
                                        script_callback->function,
                                        "sss", func_argv);

        if (!rc)
            ret = WEECHAT_RC_ERROR;
        else
        {
            ret = *rc;
            free (rc);
        }
        if (func_argv[1])
            free (func_argv[1]);

        return ret;
    }

    return WEECHAT_RC_ERROR;
}

/*
 * weechat::hook_command_run: hook a command_run
 */

XS (XS_weechat_api_hook_command_run)
{
    char *result, *command, *function, *data;
    dXSARGS;

    API_FUNC(1, "hook_command_run", API_RETURN_EMPTY);
    if (items < 3)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    command = SvPV_nolen (ST (0));
    function = SvPV_nolen (ST (1));
    data = SvPV_nolen (ST (2));

    result = script_ptr2str (script_api_hook_command_run (weechat_perl_plugin,
                                                          perl_current_script,
                                                          command,
                                                          &weechat_perl_api_hook_command_run_cb,
                                                          function,
                                                          data));

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat_perl_api_hook_timer_cb: callback for timer hooked
 */

int
weechat_perl_api_hook_timer_cb (void *data, int remaining_calls)
{
    struct t_script_callback *script_callback;
    void *func_argv[2];
    char str_remaining_calls[32], empty_arg[1] = { '\0' };
    int *rc, ret;

    script_callback = (struct t_script_callback *)data;

    if (script_callback && script_callback->function && script_callback->function[0])
    {
        snprintf (str_remaining_calls, sizeof (str_remaining_calls),
                  "%d", remaining_calls);

        func_argv[0] = (script_callback->data) ? script_callback->data : empty_arg;
        func_argv[1] = str_remaining_calls;

        rc = (int *) weechat_perl_exec (script_callback->script,
                                        WEECHAT_SCRIPT_EXEC_INT,
                                        script_callback->function,
                                        "ss", func_argv);

        if (!rc)
            ret = WEECHAT_RC_ERROR;
        else
        {
            ret = *rc;
            free (rc);
        }

        return ret;
    }

    return WEECHAT_RC_ERROR;
}

/*
 * weechat::hook_timer: hook a timer
 */

XS (XS_weechat_api_hook_timer)
{
    char *result;
    dXSARGS;

    API_FUNC(1, "hook_timer", API_RETURN_EMPTY);
    if (items < 5)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    result = script_ptr2str (script_api_hook_timer (weechat_perl_plugin,
                                                    perl_current_script,
                                                    SvIV (ST (0)), /* interval */
                                                    SvIV (ST (1)), /* align_second */
                                                    SvIV (ST (2)), /* max_calls */
                                                    &weechat_perl_api_hook_timer_cb,
                                                    SvPV_nolen (ST (3)), /* perl function */
                                                    SvPV_nolen (ST (4)))); /* data */

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat_perl_api_hook_fd_cb: callback for fd hooked
 */

int
weechat_perl_api_hook_fd_cb (void *data, int fd)
{
    struct t_script_callback *script_callback;
    void *func_argv[2];
    char str_fd[32], empty_arg[1] = { '\0' };
    int *rc, ret;

    script_callback = (struct t_script_callback *)data;

    if (script_callback && script_callback->function && script_callback->function[0])
    {
        snprintf (str_fd, sizeof (str_fd), "%d", fd);

        func_argv[0] = (script_callback->data) ? script_callback->data : empty_arg;
        func_argv[1] = str_fd;

        rc = (int *) weechat_perl_exec (script_callback->script,
                                        WEECHAT_SCRIPT_EXEC_INT,
                                        script_callback->function,
                                        "ss", func_argv);

        if (!rc)
            ret = WEECHAT_RC_ERROR;
        else
        {
            ret = *rc;
            free (rc);
        }

        return ret;
    }

    return WEECHAT_RC_ERROR;
}

/*
 * weechat::hook_fd: hook a fd
 */

XS (XS_weechat_api_hook_fd)
{
    char *result;
    dXSARGS;

    API_FUNC(1, "hook_fd", API_RETURN_EMPTY);
    if (items < 6)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    result = script_ptr2str (script_api_hook_fd (weechat_perl_plugin,
                                                 perl_current_script,
                                                 SvIV (ST (0)), /* fd */
                                                 SvIV (ST (1)), /* read */
                                                 SvIV (ST (2)), /* write */
                                                 SvIV (ST (3)), /* exception */
                                                 &weechat_perl_api_hook_fd_cb,
                                                 SvPV_nolen (ST (4)), /* perl function */
                                                 SvPV_nolen (ST (5)))); /* data */

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat_perl_api_hook_process_cb: callback for process hooked
 */

int
weechat_perl_api_hook_process_cb (void *data,
                                  const char *command, int return_code,
                                  const char *out, const char *err)
{
    struct t_script_callback *script_callback;
    void *func_argv[5];
    char str_rc[32], empty_arg[1] = { '\0' };
    int *rc, ret;

    script_callback = (struct t_script_callback *)data;

    if (script_callback && script_callback->function && script_callback->function[0])
    {
        snprintf (str_rc, sizeof (str_rc), "%d", return_code);

        func_argv[0] = (script_callback->data) ? script_callback->data : empty_arg;
        func_argv[1] = (command) ? (char *)command : empty_arg;
        func_argv[2] = str_rc;
        func_argv[3] = (out) ? (char *)out : empty_arg;
        func_argv[4] = (err) ? (char *)err : empty_arg;

        rc = (int *) weechat_perl_exec (script_callback->script,
                                        WEECHAT_SCRIPT_EXEC_INT,
                                        script_callback->function,
                                        "sssss", func_argv);

        if (!rc)
            ret = WEECHAT_RC_ERROR;
        else
        {
            ret = *rc;
            free (rc);
        }

        return ret;
    }

    return WEECHAT_RC_ERROR;
}

/*
 * weechat::hook_process: hook a process
 */

XS (XS_weechat_api_hook_process)
{
    char *command, *function, *data, *result;
    dXSARGS;

    API_FUNC(1, "hook_process", API_RETURN_EMPTY);
    if (items < 4)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    command = SvPV_nolen (ST (0));
    function = SvPV_nolen (ST (2));
    data = SvPV_nolen (ST (3));

    result = script_ptr2str (script_api_hook_process (weechat_perl_plugin,
                                                      perl_current_script,
                                                      command,
                                                      SvIV (ST (1)), /* timeout */
                                                      &weechat_perl_api_hook_process_cb,
                                                      function,
                                                      data));

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat::hook_process_hashtable: hook a process with options in a hashtable
 */

XS (XS_weechat_api_hook_process_hashtable)
{
    char *command, *function, *data, *result;
    struct t_hashtable *options;
    dXSARGS;

    API_FUNC(1, "hook_process_hashtable", API_RETURN_EMPTY);
    if (items < 5)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    command = SvPV_nolen (ST (0));
    options = weechat_perl_hash_to_hashtable (ST (1),
                                              WEECHAT_SCRIPT_HASHTABLE_DEFAULT_SIZE);
    function = SvPV_nolen (ST (3));
    data = SvPV_nolen (ST (4));

    result = script_ptr2str (script_api_hook_process_hashtable (weechat_perl_plugin,
                                                                perl_current_script,
                                                                command,
                                                                options,
                                                                SvIV (ST (2)), /* timeout */
                                                                &weechat_perl_api_hook_process_cb,
                                                                function,
                                                                data));

    if (options)
        weechat_hashtable_free (options);

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat_perl_api_hook_connect_cb: callback for connect hooked
 */

int
weechat_perl_api_hook_connect_cb (void *data, int status, int gnutls_rc,
                                  const char *error, const char *ip_address)
{
    struct t_script_callback *script_callback;
    void *func_argv[5];
    char str_status[32], str_gnutls_rc[32];
    char empty_arg[1] = { '\0' };
    int *rc, ret;

    script_callback = (struct t_script_callback *)data;

    if (script_callback && script_callback->function && script_callback->function[0])
    {
        snprintf (str_status, sizeof (str_status), "%d", status);
        snprintf (str_gnutls_rc, sizeof (str_gnutls_rc), "%d", gnutls_rc);

        func_argv[0] = (script_callback->data) ? script_callback->data : empty_arg;
        func_argv[1] = str_status;
        func_argv[2] = str_gnutls_rc;
        func_argv[3] = (ip_address) ? (char *)ip_address : empty_arg;
        func_argv[4] = (error) ? (char *)error : empty_arg;

        rc = (int *) weechat_perl_exec (script_callback->script,
                                        WEECHAT_SCRIPT_EXEC_INT,
                                        script_callback->function,
                                        "sssss", func_argv);

        if (!rc)
            ret = WEECHAT_RC_ERROR;
        else
        {
            ret = *rc;
            free (rc);
        }

        return ret;
    }

    return WEECHAT_RC_ERROR;
}

/*
 * weechat::hook_connect: hook a connection
 */

XS (XS_weechat_api_hook_connect)
{
    char *proxy, *address, *local_hostname, *function, *data, *result;
    dXSARGS;

    API_FUNC(1, "hook_connect", API_RETURN_EMPTY);
    if (items < 8)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    proxy = SvPV_nolen (ST (0));
    address = SvPV_nolen (ST (1));
    local_hostname = SvPV_nolen (ST (5));
    function = SvPV_nolen (ST (6));
    data = SvPV_nolen (ST (7));

    result = script_ptr2str (script_api_hook_connect (weechat_perl_plugin,
                                                      perl_current_script,
                                                      proxy,
                                                      address,
                                                      SvIV (ST (2)), /* port */
                                                      SvIV (ST (3)), /* sock */
                                                      SvIV (ST (4)), /* ipv6 */
                                                      NULL, /* gnutls session */
                                                      NULL, /* gnutls callback */
                                                      0,    /* gnutls DH key size */
                                                      NULL, /* gnutls priorities */
                                                      local_hostname,
                                                      &weechat_perl_api_hook_connect_cb,
                                                      function,
                                                      data));

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat_perl_api_hook_print_cb: callback for print hooked
 */

int
weechat_perl_api_hook_print_cb (void *data, struct t_gui_buffer *buffer,
                                time_t date,
                                int tags_count, const char **tags,
                                int displayed, int highlight,
                                const char *prefix, const char *message)
{
    struct t_script_callback *script_callback;
    void *func_argv[8];
    char empty_arg[1] = { '\0' };
    static char timebuffer[64];
    int *rc, ret;

    /* make C compiler happy */
    (void) tags_count;

    script_callback = (struct t_script_callback *)data;

    if (script_callback && script_callback->function && script_callback->function[0])
    {
        snprintf (timebuffer, sizeof (timebuffer) - 1, "%ld", (long int)date);

        func_argv[0] = (script_callback->data) ? script_callback->data : empty_arg;
        func_argv[1] = script_ptr2str (buffer);
        func_argv[2] = timebuffer;
        func_argv[3] = weechat_string_build_with_split_string (tags, ",");
        if (!func_argv[3])
            func_argv[3] = strdup ("");
        func_argv[4] = (displayed) ? strdup ("1") : strdup ("0");
        func_argv[5] = (highlight) ? strdup ("1") : strdup ("0");
        func_argv[6] = (prefix) ? (char *)prefix : empty_arg;
        func_argv[7] = (message) ? (char *)message : empty_arg;

        rc = (int *) weechat_perl_exec (script_callback->script,
                                        WEECHAT_SCRIPT_EXEC_INT,
                                        script_callback->function,
                                        "ssssssss", func_argv);

        if (!rc)
            ret = WEECHAT_RC_ERROR;
        else
        {
            ret = *rc;
            free (rc);
        }
        if (func_argv[1])
            free (func_argv[1]);
        if (func_argv[3])
            free (func_argv[3]);
        if (func_argv[4])
            free (func_argv[4]);
        if (func_argv[5])
            free (func_argv[5]);

        return ret;
    }

    return WEECHAT_RC_ERROR;
}

/*
 * weechat::hook_print: hook a print
 */

XS (XS_weechat_api_hook_print)
{
    char *result, *buffer, *tags, *message, *function, *data;
    dXSARGS;

    API_FUNC(1, "hook_print", API_RETURN_EMPTY);
    if (items < 6)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    buffer = SvPV_nolen (ST (0));
    tags = SvPV_nolen (ST (1));
    message = SvPV_nolen (ST (2));
    function = SvPV_nolen (ST (4));
    data = SvPV_nolen (ST (5));

    result = script_ptr2str (script_api_hook_print (weechat_perl_plugin,
                                                    perl_current_script,
                                                    script_str2ptr (buffer),
                                                    tags,
                                                    message,
                                                    SvIV (ST (3)), /* strip_colors */
                                                    &weechat_perl_api_hook_print_cb,
                                                    function,
                                                    data));

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat_perl_api_hook_signal_cb: callback for signal hooked
 */

int
weechat_perl_api_hook_signal_cb (void *data, const char *signal, const char *type_data,
                                 void *signal_data)
{
    struct t_script_callback *script_callback;
    void *func_argv[3];
    char empty_arg[1] = { '\0' };
    static char value_str[64];
    int *rc, ret, free_needed;

    script_callback = (struct t_script_callback *)data;

    if (script_callback && script_callback->function && script_callback->function[0])
    {
        func_argv[0] = (script_callback->data) ? script_callback->data : empty_arg;
        func_argv[1] = (signal) ? (char *)signal : empty_arg;
        free_needed = 0;
        if (strcmp (type_data, WEECHAT_HOOK_SIGNAL_STRING) == 0)
        {
            func_argv[2] = (signal_data) ? (char *)signal_data : empty_arg;
        }
        else if (strcmp (type_data, WEECHAT_HOOK_SIGNAL_INT) == 0)
        {
            snprintf (value_str, sizeof (value_str) - 1,
                      "%d", *((int *)signal_data));
            func_argv[2] = value_str;
        }
        else if (strcmp (type_data, WEECHAT_HOOK_SIGNAL_POINTER) == 0)
        {
            func_argv[2] = script_ptr2str (signal_data);
            free_needed = 1;
        }
        else
            func_argv[2] = empty_arg;

        rc = (int *) weechat_perl_exec (script_callback->script,
                                        WEECHAT_SCRIPT_EXEC_INT,
                                        script_callback->function,
                                        "sss", func_argv);

        if (!rc)
            ret = WEECHAT_RC_ERROR;
        else
        {
            ret = *rc;
            free (rc);
        }
        if (free_needed && func_argv[2])
            free (func_argv[2]);

        return ret;
    }

    return WEECHAT_RC_ERROR;
}

/*
 * weechat::hook_signal: hook a signal
 */

XS (XS_weechat_api_hook_signal)
{
    char *result, *signal, *function, *data;
    dXSARGS;

    API_FUNC(1, "hook_signal", API_RETURN_EMPTY);
    if (items < 3)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    signal = SvPV_nolen (ST (0));
    function = SvPV_nolen (ST (1));
    data = SvPV_nolen (ST (2));

    result = script_ptr2str (script_api_hook_signal (weechat_perl_plugin,
                                                     perl_current_script,
                                                     signal,
                                                     &weechat_perl_api_hook_signal_cb,
                                                     function,
                                                     data));

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat::hook_signal_send: send a signal
 */

XS (XS_weechat_api_hook_signal_send)
{
    char *signal, *type_data;
    int number;
    dXSARGS;

    API_FUNC(1, "hook_signal_send", API_RETURN_ERROR);
    if (items < 3)
        API_WRONG_ARGS(API_RETURN_ERROR);

    signal = SvPV_nolen (ST (0));
    type_data = SvPV_nolen (ST (1));
    if (strcmp (type_data, WEECHAT_HOOK_SIGNAL_STRING) == 0)
    {
        weechat_hook_signal_send (signal,
                                  type_data,
                                  SvPV_nolen (ST (2))); /* signal_data */
        API_RETURN_OK;
    }
    else if (strcmp (type_data, WEECHAT_HOOK_SIGNAL_INT) == 0)
    {
        number = SvIV(ST (2));
        weechat_hook_signal_send (signal,
                                  type_data,
                                  &number); /* signal_data */
        API_RETURN_OK;
    }
    else if (strcmp (type_data, WEECHAT_HOOK_SIGNAL_POINTER) == 0)
    {
        weechat_hook_signal_send (signal,
                                  type_data,
                                  script_str2ptr (SvPV_nolen (ST (2)))); /* signal_data */
        API_RETURN_OK;
    }

    API_RETURN_ERROR;
}

/*
 * weechat_perl_api_hook_hsignal_cb: callback for hsignal hooked
 */

int
weechat_perl_api_hook_hsignal_cb (void *data, const char *signal,
                                  struct t_hashtable *hashtable)
{
    struct t_script_callback *script_callback;
    void *func_argv[3];
    char empty_arg[1] = { '\0' };
    int *rc, ret;

    script_callback = (struct t_script_callback *)data;

    if (script_callback && script_callback->function && script_callback->function[0])
    {
        func_argv[0] = (script_callback->data) ? script_callback->data : empty_arg;
        func_argv[1] = (signal) ? (char *)signal : empty_arg;
        func_argv[2] = hashtable;

        rc = (int *) weechat_perl_exec (script_callback->script,
                                        WEECHAT_SCRIPT_EXEC_INT,
                                        script_callback->function,
                                        "ssh", func_argv);

        if (!rc)
            ret = WEECHAT_RC_ERROR;
        else
        {
            ret = *rc;
            free (rc);
        }

        return ret;
    }

    return WEECHAT_RC_ERROR;
}

/*
 * weechat::hook_hsignal: hook a hsignal
 */

XS (XS_weechat_api_hook_hsignal)
{
    char *result, *signal, *function, *data;
    dXSARGS;

    API_FUNC(1, "hook_hsignal", API_RETURN_EMPTY);
    if (items < 3)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    signal = SvPV_nolen (ST (0));
    function = SvPV_nolen (ST (1));
    data = SvPV_nolen (ST (2));

    result = script_ptr2str (script_api_hook_hsignal (weechat_perl_plugin,
                                                      perl_current_script,
                                                      signal,
                                                      &weechat_perl_api_hook_hsignal_cb,
                                                      function,
                                                      data));

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat::hook_hsignal_send: send a hsignal
 */

XS (XS_weechat_api_hook_hsignal_send)
{
    char *signal;
    struct t_hashtable *hashtable;
    dXSARGS;

    API_FUNC(1, "hook_hsignal_send", API_RETURN_ERROR);
    if (items < 2)
        API_WRONG_ARGS(API_RETURN_ERROR);

    signal = SvPV_nolen (ST (0));
    hashtable = weechat_perl_hash_to_hashtable (ST (1),
                                                WEECHAT_SCRIPT_HASHTABLE_DEFAULT_SIZE);

    weechat_hook_hsignal_send (signal, hashtable);

    if (hashtable)
        weechat_hashtable_free (hashtable);

    API_RETURN_OK;
}

/*
 * weechat_perl_api_hook_config_cb: callback for config option hooked
 */

int
weechat_perl_api_hook_config_cb (void *data, const char *option, const char *value)
{
    struct t_script_callback *script_callback;
    void *func_argv[3];
    char empty_arg[1] = { '\0' };
    int *rc, ret;

    script_callback = (struct t_script_callback *)data;

    if (script_callback && script_callback->function && script_callback->function[0])
    {
        func_argv[0] = (script_callback->data) ? script_callback->data : empty_arg;
        func_argv[1] = (option) ? (char *)option : empty_arg;
        func_argv[2] = (value) ? (char *)value : empty_arg;

        rc = (int *) weechat_perl_exec (script_callback->script,
                                        WEECHAT_SCRIPT_EXEC_INT,
                                        script_callback->function,
                                        "sss", func_argv);

        if (!rc)
            ret = WEECHAT_RC_ERROR;
        else
        {
            ret = *rc;
            free (rc);
        }

        return ret;
    }

    return WEECHAT_RC_ERROR;
}

/*
 * weechat::hook_config: hook a config option
 */

XS (XS_weechat_api_hook_config)
{
    char *result, *option, *function, *data;
    dXSARGS;

    API_FUNC(1, "hook_config", API_RETURN_EMPTY);
    if (items < 3)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    option = SvPV_nolen (ST (0));
    function = SvPV_nolen (ST (1));
    data = SvPV_nolen (ST (2));

    result = script_ptr2str (script_api_hook_config (weechat_perl_plugin,
                                                     perl_current_script,
                                                     option,
                                                     &weechat_perl_api_hook_config_cb,
                                                     function,
                                                     data));

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat_perl_api_hook_completion_cb: callback for completion hooked
 */

int
weechat_perl_api_hook_completion_cb (void *data, const char *completion_item,
                                     struct t_gui_buffer *buffer,
                                     struct t_gui_completion *completion)
{
    struct t_script_callback *script_callback;
    void *func_argv[4];
    char empty_arg[1] = { '\0' };
    int *rc, ret;

    script_callback = (struct t_script_callback *)data;

    if (script_callback && script_callback->function && script_callback->function[0])
    {
        func_argv[0] = (script_callback->data) ? script_callback->data : empty_arg;
        func_argv[1] = (completion_item) ? (char *)completion_item : empty_arg;
        func_argv[2] = script_ptr2str (buffer);
        func_argv[3] = script_ptr2str (completion);

        rc = (int *) weechat_perl_exec (script_callback->script,
                                        WEECHAT_SCRIPT_EXEC_INT,
                                        script_callback->function,
                                        "ssss", func_argv);

        if (!rc)
            ret = WEECHAT_RC_ERROR;
        else
        {
            ret = *rc;
            free (rc);
        }
        if (func_argv[2])
            free (func_argv[2]);
        if (func_argv[3])
            free (func_argv[3]);

        return ret;
    }

    return WEECHAT_RC_ERROR;
}

/*
 * weechat::hook_completion: hook a completion
 */

XS (XS_weechat_api_hook_completion)
{
    char *result, *completion, *description, *function, *data;
    dXSARGS;

    API_FUNC(1, "hook_completion", API_RETURN_EMPTY);
    if (items < 4)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    completion = SvPV_nolen (ST (0));
    description = SvPV_nolen (ST (1));
    function = SvPV_nolen (ST (2));
    data = SvPV_nolen (ST (3));

    result = script_ptr2str (script_api_hook_completion (weechat_perl_plugin,
                                                         perl_current_script,
                                                         completion,
                                                         description,
                                                         &weechat_perl_api_hook_completion_cb,
                                                         function,
                                                         data));

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat::hook_completion_list_add: add a word to list for a completion
 */

XS (XS_weechat_api_hook_completion_list_add)
{
    char *completion, *word, *where;
    dXSARGS;

    API_FUNC(1, "hook_completion_list_add", API_RETURN_ERROR);
    if (items < 4)
        API_WRONG_ARGS(API_RETURN_ERROR);

    completion = SvPV_nolen (ST (0));
    word = SvPV_nolen (ST (1));
    where = SvPV_nolen (ST (3));

    weechat_hook_completion_list_add (script_str2ptr (completion),
                                      word,
                                      SvIV (ST (2)), /* nick_completion */
                                      where);

    API_RETURN_OK;
}

/*
 * weechat_perl_api_hook_modifier_cb: callback for modifier hooked
 */

char *
weechat_perl_api_hook_modifier_cb (void *data, const char *modifier,
                                   const char *modifier_data, const char *string)
{
    struct t_script_callback *script_callback;
    void *func_argv[4];
    char empty_arg[1] = { '\0' };

    script_callback = (struct t_script_callback *)data;

    if (script_callback && script_callback->function && script_callback->function[0])
    {
        func_argv[0] = (script_callback->data) ? script_callback->data : empty_arg;
        func_argv[1] = (modifier) ? (char *)modifier : empty_arg;
        func_argv[2] = (modifier_data) ? (char *)modifier_data : empty_arg;
        func_argv[3] = (string) ? (char *)string : empty_arg;

        return (char *)weechat_perl_exec (script_callback->script,
                                          WEECHAT_SCRIPT_EXEC_STRING,
                                          script_callback->function,
                                          "ssss", func_argv);
    }

    return NULL;
}

/*
 * weechat::hook_modifier: hook a modifier
 */

XS (XS_weechat_api_hook_modifier)
{
    char *result, *modifier, *function, *data;
    dXSARGS;

    API_FUNC(1, "hook_modifier", API_RETURN_EMPTY);
    if (items < 3)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    modifier = SvPV_nolen (ST (0));
    function = SvPV_nolen (ST (1));
    data = SvPV_nolen (ST (2));

    result = script_ptr2str (script_api_hook_modifier (weechat_perl_plugin,
                                                       perl_current_script,
                                                       modifier,
                                                       &weechat_perl_api_hook_modifier_cb,
                                                       function,
                                                       data));

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat::hook_modifier_exec: execute a modifier hook
 */

XS (XS_weechat_api_hook_modifier_exec)
{
    char *result, *modifier, *modifier_data, *string;
    dXSARGS;

    API_FUNC(1, "hook_modifier_exec", API_RETURN_EMPTY);
    if (items < 3)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    modifier = SvPV_nolen (ST (0));
    modifier_data = SvPV_nolen (ST (1));
    string = SvPV_nolen (ST (2));

    result = weechat_hook_modifier_exec (modifier, modifier_data, string);

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat_perl_api_hook_info_cb: callback for info hooked
 */

const char *
weechat_perl_api_hook_info_cb (void *data, const char *info_name,
                               const char *arguments)
{
    struct t_script_callback *script_callback;
    void *func_argv[3];
    char empty_arg[1] = { '\0' };

    script_callback = (struct t_script_callback *)data;

    if (script_callback && script_callback->function && script_callback->function[0])
    {
        func_argv[0] = (script_callback->data) ? script_callback->data : empty_arg;
        func_argv[1] = (info_name) ? (char *)info_name : empty_arg;
        func_argv[2] = (arguments) ? (char *)arguments : empty_arg;

        return (const char *)weechat_perl_exec (script_callback->script,
                                                WEECHAT_SCRIPT_EXEC_STRING,
                                                script_callback->function,
                                                "sss", func_argv);
    }

    return NULL;
}

/*
 * weechat::hook_info: hook an info
 */

XS (XS_weechat_api_hook_info)
{
    char *result, *info_name, *description, *args_description, *function, *data;
    dXSARGS;

    API_FUNC(1, "hook_info", API_RETURN_EMPTY);
    if (items < 5)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    info_name = SvPV_nolen (ST (0));
    description = SvPV_nolen (ST (1));
    args_description = SvPV_nolen (ST (2));
    function = SvPV_nolen (ST (3));
    data = SvPV_nolen (ST (4));

    result = script_ptr2str (script_api_hook_info (weechat_perl_plugin,
                                                   perl_current_script,
                                                   info_name,
                                                   description,
                                                   args_description,
                                                   &weechat_perl_api_hook_info_cb,
                                                   function,
                                                   data));

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat_perl_api_hook_info_hashtable_cb: callback for info_hashtable hooked
 */

struct t_hashtable *
weechat_perl_api_hook_info_hashtable_cb (void *data, const char *info_name,
                                         struct t_hashtable *hashtable)
{
    struct t_script_callback *script_callback;
    void *func_argv[3];
    char empty_arg[1] = { '\0' };

    script_callback = (struct t_script_callback *)data;

    if (script_callback && script_callback->function && script_callback->function[0])
    {
        func_argv[0] = (script_callback->data) ? script_callback->data : empty_arg;
        func_argv[1] = (info_name) ? (char *)info_name : empty_arg;
        func_argv[2] = hashtable;

        return (struct t_hashtable *)weechat_perl_exec (script_callback->script,
                                                        WEECHAT_SCRIPT_EXEC_HASHTABLE,
                                                        script_callback->function,
                                                        "ssh", func_argv);
    }

    return NULL;
}

/*
 * weechat::hook_info_hashtable: hook an info_hashtable
 */

XS (XS_weechat_api_hook_info_hashtable)
{
    char *result, *info_name, *description, *args_description;
    char *output_description, *function, *data;
    dXSARGS;

    API_FUNC(1, "hook_info_hashtable", API_RETURN_EMPTY);
    if (items < 6)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    info_name = SvPV_nolen (ST (0));
    description = SvPV_nolen (ST (1));
    args_description = SvPV_nolen (ST (2));
    output_description = SvPV_nolen (ST (3));
    function = SvPV_nolen (ST (4));
    data = SvPV_nolen (ST (5));

    result = script_ptr2str (script_api_hook_info_hashtable (weechat_perl_plugin,
                                                             perl_current_script,
                                                             info_name,
                                                             description,
                                                             args_description,
                                                             output_description,
                                                             &weechat_perl_api_hook_info_hashtable_cb,
                                                             function,
                                                             data));

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat_perl_api_hook_infolist_cb: callback for infolist hooked
 */

struct t_infolist *
weechat_perl_api_hook_infolist_cb (void *data, const char *infolist_name,
                                   void *pointer, const char *arguments)
{
    struct t_script_callback *script_callback;
    void *func_argv[4];
    char empty_arg[1] = { '\0' };
    struct t_infolist *result;

    script_callback = (struct t_script_callback *)data;

    if (script_callback && script_callback->function && script_callback->function[0])
    {
        func_argv[0] = (script_callback->data) ? script_callback->data : empty_arg;
        func_argv[1] = (infolist_name) ? (char *)infolist_name : empty_arg;
        func_argv[2] = script_ptr2str (pointer);
        func_argv[3] = (arguments) ? (char *)arguments : empty_arg;

        result = (struct t_infolist *)weechat_perl_exec (script_callback->script,
                                                         WEECHAT_SCRIPT_EXEC_STRING,
                                                         script_callback->function,
                                                         "ssss", func_argv);

        if (func_argv[2])
            free (func_argv[2]);

        return result;
    }

    return NULL;
}

/*
 * weechat::hook_infolist: hook an infolist
 */

XS (XS_weechat_api_hook_infolist)
{
    char *result, *infolist_name, *description, *pointer_description;
    char *args_description, *function, *data;
    dXSARGS;

    API_FUNC(1, "hook_infolist", API_RETURN_EMPTY);
    if (items < 6)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    infolist_name = SvPV_nolen (ST (0));
    description = SvPV_nolen (ST (1));
    pointer_description = SvPV_nolen (ST (2));
    args_description = SvPV_nolen (ST (3));
    function = SvPV_nolen (ST (4));
    data = SvPV_nolen (ST (5));

    result = script_ptr2str (script_api_hook_infolist (weechat_perl_plugin,
                                                       perl_current_script,
                                                       infolist_name,
                                                       description,
                                                       pointer_description,
                                                       args_description,
                                                       &weechat_perl_api_hook_infolist_cb,
                                                       function,
                                                       data));

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat_perl_api_hook_focus_cb: callback for focus hooked
 */

struct t_hashtable *
weechat_perl_api_hook_focus_cb (void *data,
                                struct t_hashtable *info)
{
    struct t_script_callback *script_callback;
    void *func_argv[2];
    char empty_arg[1] = { '\0' };

    script_callback = (struct t_script_callback *)data;

    if (script_callback && script_callback->function && script_callback->function[0])
    {
        func_argv[0] = (script_callback->data) ? script_callback->data : empty_arg;
        func_argv[1] = info;

        return (struct t_hashtable *)weechat_perl_exec (script_callback->script,
                                                        WEECHAT_SCRIPT_EXEC_HASHTABLE,
                                                        script_callback->function,
                                                        "sh", func_argv);
    }

    return NULL;
}

/*
 * weechat::hook_focus: hook a focus
 */

XS (XS_weechat_api_hook_focus)
{
    char *result, *area, *function, *data;
    dXSARGS;

    API_FUNC(1, "hook_focus", API_RETURN_EMPTY);
    if (items < 3)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    area = SvPV_nolen (ST (0));
    function = SvPV_nolen (ST (1));
    data = SvPV_nolen (ST (2));

    result = script_ptr2str (script_api_hook_focus (weechat_perl_plugin,
                                                    perl_current_script,
                                                    area,
                                                    &weechat_perl_api_hook_focus_cb,
                                                    function,
                                                    data));

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat::unhook: unhook something
 */

XS (XS_weechat_api_unhook)
{
    dXSARGS;

    API_FUNC(1, "unhook", API_RETURN_ERROR);
    if (items < 1)
        API_WRONG_ARGS(API_RETURN_ERROR);

    script_api_unhook (weechat_perl_plugin,
                       perl_current_script,
                       script_str2ptr (SvPV_nolen (ST (0)))); /* hook */

    API_RETURN_OK;
}

/*
 * weechat::unhook_all: unhook all for script
 */

XS (XS_weechat_api_unhook_all)
{
    dXSARGS;

    /* make C compiler happy */
    (void) cv;
    (void) items;

    API_FUNC(1, "unhook_all", API_RETURN_ERROR);

    script_api_unhook_all (perl_current_script);

    API_RETURN_OK;
}

/*
 * weechat_perl_api_buffer_input_data_cb: callback for input data in a buffer
 */

int
weechat_perl_api_buffer_input_data_cb (void *data, struct t_gui_buffer *buffer,
                                       const char *input_data)
{
    struct t_script_callback *script_callback;
    void *func_argv[3];
    char empty_arg[1] = { '\0' };
    int *rc, ret;

    script_callback = (struct t_script_callback *)data;

    if (script_callback && script_callback->function && script_callback->function[0])
    {
        func_argv[0] = (script_callback->data) ? script_callback->data : empty_arg;
        func_argv[1] = script_ptr2str (buffer);
        func_argv[2] = (input_data) ? (char *)input_data : empty_arg;

        rc = (int *) weechat_perl_exec (script_callback->script,
                                        WEECHAT_SCRIPT_EXEC_INT,
                                        script_callback->function,
                                        "sss", func_argv);
        if (!rc)
            ret = WEECHAT_RC_ERROR;
        else
        {
            ret = *rc;
            free (rc);
        }
        if (func_argv[1])
            free (func_argv[1]);

        return ret;
    }

    return WEECHAT_RC_ERROR;
}

/*
 * weechat_perl_api_buffer_close_cb: callback for buffer closed
 */

int
weechat_perl_api_buffer_close_cb (void *data, struct t_gui_buffer *buffer)
{
    struct t_script_callback *script_callback;
    void *func_argv[2];
    char empty_arg[1] = { '\0' };
    int *rc, ret;

    script_callback = (struct t_script_callback *)data;

    if (script_callback && script_callback->function && script_callback->function[0])
    {
        func_argv[0] = (script_callback->data) ? script_callback->data : empty_arg;
        func_argv[1] = script_ptr2str (buffer);

        rc = (int *) weechat_perl_exec (script_callback->script,
                                        WEECHAT_SCRIPT_EXEC_INT,
                                        script_callback->function,
                                        "ss", func_argv);
        if (!rc)
            ret = WEECHAT_RC_ERROR;
        else
        {
            ret = *rc;
            free (rc);
        }
        if (func_argv[1])
            free (func_argv[1]);

        return ret;
    }

    return WEECHAT_RC_ERROR;
}

/*
 * weechat::buffer_new: create a new buffer
 */

XS (XS_weechat_api_buffer_new)
{
    char *result, *name, *function_input, *data_input, *function_close;
    char *data_close;
    dXSARGS;

    API_FUNC(1, "buffer_new", API_RETURN_EMPTY);
    if (items < 5)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    name = SvPV_nolen (ST (0));
    function_input = SvPV_nolen (ST (1));
    data_input = SvPV_nolen (ST (2));
    function_close = SvPV_nolen (ST (3));
    data_close = SvPV_nolen (ST (4));

    result = script_ptr2str (script_api_buffer_new (weechat_perl_plugin,
                                                    perl_current_script,
                                                    name,
                                                    &weechat_perl_api_buffer_input_data_cb,
                                                    function_input,
                                                    data_input,
                                                    &weechat_perl_api_buffer_close_cb,
                                                    function_close,
                                                    data_close));

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat::buffer_search: search a buffer
 */

XS (XS_weechat_api_buffer_search)
{
    char *result, *plugin, *name;
    dXSARGS;

    API_FUNC(1, "buffer_search", API_RETURN_EMPTY);
    if (items < 2)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    plugin = SvPV_nolen (ST (0));
    name = SvPV_nolen (ST (1));

    result = script_ptr2str (weechat_buffer_search (plugin, name));

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat::buffer_search_main: search main buffer (WeeChat core buffer)
 */

XS (XS_weechat_api_buffer_search_main)
{
    char *result;
    dXSARGS;

    /* make C compiler happy */
    (void) items;
    (void) cv;

    API_FUNC(1, "buffer_search_main", API_RETURN_EMPTY);

    result = script_ptr2str (weechat_buffer_search_main ());

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat::current_buffer: get current buffer
 */

XS (XS_weechat_api_current_buffer)
{
    char *result;
    dXSARGS;

    /* make C compiler happy */
    (void) items;
    (void) cv;

    API_FUNC(1, "current_buffer", API_RETURN_EMPTY);

    result = script_ptr2str (weechat_current_buffer ());

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat::buffer_clear: clear a buffer
 */

XS (XS_weechat_api_buffer_clear)
{
    dXSARGS;

    API_FUNC(1, "buffer_clear", API_RETURN_ERROR);
    if (items < 1)
        API_WRONG_ARGS(API_RETURN_ERROR);

    weechat_buffer_clear (script_str2ptr (SvPV_nolen (ST (0)))); /* buffer */

    API_RETURN_OK;
}

/*
 * weechat::buffer_close: close a buffer
 */

XS (XS_weechat_api_buffer_close)
{
    dXSARGS;

    API_FUNC(1, "buffer_close", API_RETURN_ERROR);
    if (items < 1)
        API_WRONG_ARGS(API_RETURN_ERROR);

    script_api_buffer_close (weechat_perl_plugin,
                             perl_current_script,
                             script_str2ptr (SvPV_nolen (ST (0)))); /* buffer */

    API_RETURN_OK;
}

/*
 * weechat::buffer_merge: merge a buffer to another buffer
 */

XS (XS_weechat_api_buffer_merge)
{
    dXSARGS;

    API_FUNC(1, "buffer_merge", API_RETURN_ERROR);
    if (items < 2)
        API_WRONG_ARGS(API_RETURN_ERROR);

    weechat_buffer_merge (script_str2ptr (SvPV_nolen (ST (0))), /* buffer */
                          script_str2ptr (SvPV_nolen (ST (1)))); /* target_buffer */

    API_RETURN_OK;
}

/*
 * weechat::buffer_unmerge: unmerge a buffer from group of merged buffers
 */

XS (XS_weechat_api_buffer_unmerge)
{
    dXSARGS;

    API_FUNC(1, "buffer_unmerge", API_RETURN_ERROR);
    if (items < 2)
        API_WRONG_ARGS(API_RETURN_ERROR);

    weechat_buffer_unmerge (script_str2ptr (SvPV_nolen (ST (0))), /* buffer */
                            SvIV (ST (1))); /* number */

    API_RETURN_OK;
}

/*
 * weechat::buffer_get_integer: get a buffer property as integer
 */

XS (XS_weechat_api_buffer_get_integer)
{
    char *buffer, *property;
    int value;
    dXSARGS;

    API_FUNC(1, "buffer_get_integer", API_RETURN_INT(-1));
    if (items < 2)
        API_WRONG_ARGS(API_RETURN_INT(-1));

    buffer = SvPV_nolen (ST (0));
    property = SvPV_nolen (ST (1));

    value = weechat_buffer_get_integer (script_str2ptr (buffer), property);

    API_RETURN_INT(value);
}

/*
 * weechat::buffer_get_string: get a buffer property as string
 */

XS (XS_weechat_api_buffer_get_string)
{
    char *buffer, *property;
    const char *result;
    dXSARGS;

    API_FUNC(1, "buffer_get_string", API_RETURN_EMPTY);
    if (items < 2)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    buffer = SvPV_nolen (ST (0));
    property = SvPV_nolen (ST (1));

    result = weechat_buffer_get_string (script_str2ptr (buffer), property);

    API_RETURN_STRING(result);
}

/*
 * weechat::buffer_get_pointer: get a buffer property as pointer
 */

XS (XS_weechat_api_buffer_get_pointer)
{
    char *result, *buffer, *property;
    dXSARGS;

    API_FUNC(1, "buffer_get_pointer", API_RETURN_EMPTY);
    if (items < 2)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    buffer = SvPV_nolen (ST (0));
    property = SvPV_nolen (ST (1));

    result = script_ptr2str (weechat_buffer_get_pointer (script_str2ptr (buffer),
                                                         property));

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat::buffer_set: set a buffer property
 */

XS (XS_weechat_api_buffer_set)
{
    char *buffer, *property, *value;
    dXSARGS;

    API_FUNC(1, "buffer_set", API_RETURN_ERROR);
    if (items < 3)
        API_WRONG_ARGS(API_RETURN_ERROR);

    buffer = SvPV_nolen (ST (0));
    property = SvPV_nolen (ST (1));
    value = SvPV_nolen (ST (2));

    weechat_buffer_set (script_str2ptr (buffer), property, value);

    API_RETURN_OK;
}

/*
 * weechat::buffer_string_replace_local_var: replace local variables ($var) in a string,
 *                                           using value of local variables
 */

XS (XS_weechat_api_buffer_string_replace_local_var)
{
    char *buffer, *string, *result;
    dXSARGS;

    API_FUNC(1, "buffer_string_replace_local_var", API_RETURN_ERROR);
    if (items < 2)
        API_WRONG_ARGS(API_RETURN_ERROR);

    buffer = SvPV_nolen (ST (0));
    string = SvPV_nolen (ST (1));

    result = weechat_buffer_string_replace_local_var (script_str2ptr (buffer), string);

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat::buffer_match_list: return 1 if buffer matches list of buffers
 */

XS (XS_weechat_api_buffer_match_list)
{
    char *buffer, *string;
    int value;
    dXSARGS;

    API_FUNC(1, "buffer_match_list", API_RETURN_INT(0));
    if (items < 2)
        API_WRONG_ARGS(API_RETURN_INT(0));

    buffer = SvPV_nolen (ST (0));
    string = SvPV_nolen (ST (1));

    value = weechat_buffer_match_list (script_str2ptr (buffer), string);

    API_RETURN_INT(value);
}

/*
 * weechat::current_window: get current window
 */

XS (XS_weechat_api_current_window)
{
    char *result;
    dXSARGS;

    /* make C compiler happy */
    (void) items;
    (void) cv;

    API_FUNC(1, "current_window", API_RETURN_EMPTY);

    result = script_ptr2str (weechat_current_window ());

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat::window_search_with_buffer: search a window with buffer pointer
 */

XS (XS_weechat_api_window_search_with_buffer)
{
    char *result;
    dXSARGS;

    API_FUNC(1, "window_search_with_buffer", API_RETURN_EMPTY);
    if (items < 1)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    result = script_ptr2str (weechat_window_search_with_buffer (script_str2ptr (SvPV_nolen (ST (0)))));

    API_RETURN_STRING_FREE(result);
}


/*
 * weechat::window_get_integer: get a window property as integer
 */

XS (XS_weechat_api_window_get_integer)
{
    char *window, *property;
    int value;
    dXSARGS;

    API_FUNC(1, "window_get_integer", API_RETURN_INT(-1));
    if (items < 2)
        API_WRONG_ARGS(API_RETURN_INT(-1));

    window = SvPV_nolen (ST (0));
    property = SvPV_nolen (ST (1));

    value = weechat_window_get_integer (script_str2ptr (window), property);

    API_RETURN_INT(value);
}

/*
 * weechat::window_get_string: get a window property as string
 */

XS (XS_weechat_api_window_get_string)
{
    char *window, *property;
    const char *result;
    dXSARGS;

    API_FUNC(1, "window_get_string", API_RETURN_EMPTY);
    if (items < 2)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    window = SvPV_nolen (ST (0));
    property = SvPV_nolen (ST (1));

    result = weechat_window_get_string (script_str2ptr (window), property);

    API_RETURN_STRING(result);
}

/*
 * weechat::window_get_pointer: get a window property as pointer
 */

XS (XS_weechat_api_window_get_pointer)
{
    char *result, *window, *property;
    dXSARGS;

    API_FUNC(1, "window_get_pointer", API_RETURN_EMPTY);
    if (items < 2)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    window = SvPV_nolen (ST (0));
    property = SvPV_nolen (ST (1));

    result = script_ptr2str (weechat_window_get_pointer (script_str2ptr (window),
                                                         property));

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat::window_set_title: set window title
 */

XS (XS_weechat_api_window_set_title)
{
    dXSARGS;

    API_FUNC(1, "window_set_title", API_RETURN_ERROR);
    if (items < 1)
        API_WRONG_ARGS(API_RETURN_ERROR);

    weechat_window_set_title (SvPV_nolen (ST (0))); /* title */

    API_RETURN_OK;
}

/*
 * weechat::nicklist_add_group: add a group in nicklist
 */

XS (XS_weechat_api_nicklist_add_group)
{
    char *result, *buffer, *parent_group, *name, *color;
    dXSARGS;

    API_FUNC(1, "nicklist_add_group", API_RETURN_EMPTY);
    if (items < 5)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    buffer = SvPV_nolen (ST (0));
    parent_group = SvPV_nolen (ST (1));
    name = SvPV_nolen (ST (2));
    color = SvPV_nolen (ST (3));

    result = script_ptr2str (weechat_nicklist_add_group (script_str2ptr (buffer),
                                                         script_str2ptr (parent_group),
                                                         name,
                                                         color,
                                                         SvIV (ST (4)))); /* visible */

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat::nicklist_search_group: search a group in nicklist
 */

XS (XS_weechat_api_nicklist_search_group)
{
    char *result, *buffer, *from_group, *name;
    dXSARGS;

    API_FUNC(1, "nicklist_search_group", API_RETURN_EMPTY);
    if (items < 3)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    buffer = SvPV_nolen (ST (0));
    from_group = SvPV_nolen (ST (1));
    name = SvPV_nolen (ST (2));

    result = script_ptr2str (weechat_nicklist_search_group (script_str2ptr (buffer),
                                                            script_str2ptr (from_group),
                                                            name));

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat::nicklist_add_nick: add a nick in nicklist
 */

XS (XS_weechat_api_nicklist_add_nick)
{
    char *result, *buffer, *group, *name, *color, *prefix, *prefix_color;
    dXSARGS;

    API_FUNC(1, "nicklist_add_nick", API_RETURN_EMPTY);
    if (items < 7)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    buffer = SvPV_nolen (ST (0));
    group = SvPV_nolen (ST (1));
    name = SvPV_nolen (ST (2));
    color = SvPV_nolen (ST (3));
    prefix = SvPV_nolen (ST (4));
    prefix_color = SvPV_nolen (ST (5));

    result = script_ptr2str (weechat_nicklist_add_nick (script_str2ptr (buffer),
                                                        script_str2ptr (group),
                                                        name,
                                                        color,
                                                        prefix,
                                                        prefix_color,
                                                        SvIV (ST (6)))); /* visible */

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat::nicklist_search_nick: search a nick in nicklist
 */

XS (XS_weechat_api_nicklist_search_nick)
{
    char *result, *buffer, *from_group, *name;
    dXSARGS;

    API_FUNC(1, "nicklist_search_nick", API_RETURN_EMPTY);
    if (items < 3)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    buffer = SvPV_nolen (ST (0));
    from_group = SvPV_nolen (ST (1));
    name = SvPV_nolen (ST (2));

    result = script_ptr2str (weechat_nicklist_search_nick (script_str2ptr (buffer),
                                                           script_str2ptr (from_group),
                                                           name));

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat::nicklist_remove_group: remove a group from nicklist
 */

XS (XS_weechat_api_nicklist_remove_group)
{
    char *buffer, *group;
    dXSARGS;

    API_FUNC(1, "nicklist_remove_group", API_RETURN_ERROR);
    if (items < 2)
        API_WRONG_ARGS(API_RETURN_ERROR);

    buffer = SvPV_nolen (ST (0));
    group = SvPV_nolen (ST (1));

    weechat_nicklist_remove_group (script_str2ptr (buffer),
                                   script_str2ptr (group));

    API_RETURN_OK;
}

/*
 * weechat::nicklist_remove_nick: remove a nick from nicklist
 */

XS (XS_weechat_api_nicklist_remove_nick)
{
    char *buffer, *nick;
    dXSARGS;

    API_FUNC(1, "nicklist_remove_nick", API_RETURN_ERROR);
    if (items < 2)
        API_WRONG_ARGS(API_RETURN_ERROR);

    buffer = SvPV_nolen (ST (0));
    nick = SvPV_nolen (ST (1));

    weechat_nicklist_remove_nick (script_str2ptr (buffer),
                                  script_str2ptr (nick));

    API_RETURN_OK;
}

/*
 * weechat::nicklist_remove_all: remove all groups/nicks from nicklist
 */

XS (XS_weechat_api_nicklist_remove_all)
{
    dXSARGS;

    API_FUNC(1, "nicklist_remove_all", API_RETURN_ERROR);
    if (items < 2)
        API_WRONG_ARGS(API_RETURN_ERROR);

    weechat_nicklist_remove_all (script_str2ptr (SvPV_nolen (ST (0)))); /* buffer */

    API_RETURN_OK;
}

/*
 * weechat::nicklist_group_get_integer: get a group property as integer
 */

XS (XS_weechat_api_nicklist_group_get_integer)
{
    char *buffer, *group, *property;
    int value;
    dXSARGS;

    API_FUNC(1, "nicklist_group_get_integer", API_RETURN_INT(-1));
    if (items < 3)
        API_WRONG_ARGS(API_RETURN_INT(-1));

    buffer = SvPV_nolen (ST (0));
    group = SvPV_nolen (ST (1));
    property = SvPV_nolen (ST (2));

    value = weechat_nicklist_group_get_integer (script_str2ptr (buffer),
                                                script_str2ptr (group),
                                                property);

    API_RETURN_INT(value);
}

/*
 * weechat::nicklist_group_get_string: get a group property as string
 */

XS (XS_weechat_api_nicklist_group_get_string)
{
    char *buffer, *group, *property;
    const char *result;
    dXSARGS;

    API_FUNC(1, "nicklist_group_get_string", API_RETURN_EMPTY);
    if (items < 3)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    buffer = SvPV_nolen (ST (0));
    group = SvPV_nolen (ST (1));
    property = SvPV_nolen (ST (2));

    result = weechat_nicklist_group_get_string (script_str2ptr (buffer),
                                                script_str2ptr (group),
                                                property);

    API_RETURN_STRING(result);
}

/*
 * weechat::nicklist_group_get_pointer: get a group property as pointer
 */

XS (XS_weechat_api_nicklist_group_get_pointer)
{
    char *result, *buffer, *group, *property;
    dXSARGS;

    API_FUNC(1, "nicklist_group_get_pointer", API_RETURN_EMPTY);
    if (items < 3)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    buffer = SvPV_nolen (ST (0));
    group = SvPV_nolen (ST (1));
    property = SvPV_nolen (ST (2));

    result = script_ptr2str (weechat_nicklist_group_get_pointer (script_str2ptr (buffer),
                                                                 script_str2ptr (group),
                                                                 property));

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat::nicklist_group_set: set a group property
 */

XS (XS_weechat_api_nicklist_group_set)
{
    char *buffer, *group, *property, *value;
    dXSARGS;

    API_FUNC(1, "nicklist_group_set", API_RETURN_ERROR);
    if (items < 4)
        API_WRONG_ARGS(API_RETURN_ERROR);

    buffer = SvPV_nolen (ST (0));
    group = SvPV_nolen (ST (1));
    property = SvPV_nolen (ST (2));
    value = SvPV_nolen (ST (3));

    weechat_nicklist_group_set (script_str2ptr (buffer),
                                script_str2ptr (group),
                                property,
                                value);

    API_RETURN_OK;
}

/*
 * weechat::nicklist_nick_get_integer: get a nick property as integer
 */

XS (XS_weechat_api_nicklist_nick_get_integer)
{
    char *buffer, *nick, *property;
    int value;
    dXSARGS;

    API_FUNC(1, "nicklist_nick_get_integer", API_RETURN_INT(-1));
    if (items < 3)
        API_WRONG_ARGS(API_RETURN_INT(-1));

    buffer = SvPV_nolen (ST (0));
    nick = SvPV_nolen (ST (1));
    property = SvPV_nolen (ST (2));

    value = weechat_nicklist_nick_get_integer (script_str2ptr (buffer),
                                               script_str2ptr (nick),
                                               property);

    API_RETURN_INT(value);
}

/*
 * weechat::nicklist_nick_get_string: get a nick property as string
 */

XS (XS_weechat_api_nicklist_nick_get_string)
{
    char *buffer, *nick, *property;
    const char *result;
    dXSARGS;

    API_FUNC(1, "nicklist_nick_get_string", API_RETURN_EMPTY);
    if (items < 3)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    buffer = SvPV_nolen (ST (0));
    nick = SvPV_nolen (ST (1));
    property = SvPV_nolen (ST (2));

    result = weechat_nicklist_nick_get_string (script_str2ptr (buffer),
                                               script_str2ptr (nick),
                                               property);

    API_RETURN_STRING(result);
}

/*
 * weechat::nicklist_nick_get_pointer: get a nick property as pointer
 */

XS (XS_weechat_api_nicklist_nick_get_pointer)
{
    char *result, *buffer, *nick, *property;
    dXSARGS;

    API_FUNC(1, "nicklist_nick_get_pointer", API_RETURN_EMPTY);
    if (items < 3)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    buffer = SvPV_nolen (ST (0));
    nick = SvPV_nolen (ST (1));
    property = SvPV_nolen (ST (2));

    result = script_ptr2str (weechat_nicklist_nick_get_pointer (script_str2ptr (buffer),
                                                                script_str2ptr (nick),
                                                                property));

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat::nicklist_nick_set: set a nick property
 */

XS (XS_weechat_api_nicklist_nick_set)
{
    char *buffer, *nick, *property, *value;
    dXSARGS;

    API_FUNC(1, "nicklist_nick_set", API_RETURN_ERROR);
    if (items < 4)
        API_WRONG_ARGS(API_RETURN_ERROR);

    buffer = SvPV_nolen (ST (0));
    nick = SvPV_nolen (ST (1));
    property = SvPV_nolen (ST (2));
    value = SvPV_nolen (ST (3));

    weechat_nicklist_nick_set (script_str2ptr (buffer),
                               script_str2ptr (nick),
                               property,
                               value);

    API_RETURN_OK;
}

/*
 * weechat::bar_item_search: search a bar item
 */

XS (XS_weechat_api_bar_item_search)
{
    char *result;
    dXSARGS;

    API_FUNC(1, "bar_item_search", API_RETURN_EMPTY);
    if (items < 1)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    result = script_ptr2str (weechat_bar_item_search (SvPV_nolen (ST (0)))); /* name */

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat_perl_api_bar_item_build_cb: callback for building bar item
 */

char *
weechat_perl_api_bar_item_build_cb (void *data, struct t_gui_bar_item *item,
                                    struct t_gui_window *window)
{
    struct t_script_callback *script_callback;
    void *func_argv[3];
    char empty_arg[1] = { '\0' }, *ret;

    script_callback = (struct t_script_callback *)data;

    if (script_callback && script_callback->function && script_callback->function[0])
    {
        func_argv[0] = (script_callback->data) ? script_callback->data : empty_arg;
        func_argv[1] = script_ptr2str (item);
        func_argv[2] = script_ptr2str (window);

        ret = (char *)weechat_perl_exec (script_callback->script,
                                         WEECHAT_SCRIPT_EXEC_STRING,
                                         script_callback->function,
                                         "sss", func_argv);

        if (func_argv[1])
            free (func_argv[1]);
        if (func_argv[2])
            free (func_argv[2]);

        return ret;
    }

    return NULL;
}

/*
 * weechat::bar_item_new: add a new bar item
 */

XS (XS_weechat_api_bar_item_new)
{
    char *result, *name, *function, *data;
    dXSARGS;

    API_FUNC(1, "bar_item_new", API_RETURN_EMPTY);
    if (items < 3)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    name = SvPV_nolen (ST (0));
    function = SvPV_nolen (ST (1));
    data = SvPV_nolen (ST (2));

    result = script_ptr2str (script_api_bar_item_new (weechat_perl_plugin,
                                                      perl_current_script,
                                                      name,
                                                      &weechat_perl_api_bar_item_build_cb,
                                                      function,
                                                      data));

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat::bar_item_update: update a bar item on screen
 */

XS (XS_weechat_api_bar_item_update)
{
    dXSARGS;

    API_FUNC(1, "bar_item_update", API_RETURN_ERROR);
    if (items < 1)
        API_WRONG_ARGS(API_RETURN_ERROR);

    weechat_bar_item_update (SvPV_nolen (ST (0))); /* name */

    API_RETURN_OK;
}

/*
 * weechat::bar_item_remove: remove a bar item
 */

XS (XS_weechat_api_bar_item_remove)
{
    dXSARGS;

    API_FUNC(1, "bar_item_remove", API_RETURN_ERROR);
    if (items < 1)
        API_WRONG_ARGS(API_RETURN_ERROR);

    script_api_bar_item_remove (weechat_perl_plugin,
                                perl_current_script,
                                script_str2ptr (SvPV_nolen (ST (0)))); /* item */

    API_RETURN_OK;
}

/*
 * weechat::bar_search: search a bar
 */

XS (XS_weechat_api_bar_search)
{
    char *result;
    dXSARGS;

    API_FUNC(1, "bar_search", API_RETURN_EMPTY);
    if (items < 1)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    result = script_ptr2str (weechat_bar_search (SvPV_nolen (ST (0)))); /* name */

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat::bar_new: add a new bar
 */

XS (XS_weechat_api_bar_new)
{
    char *result, *name, *hidden, *priority, *type, *conditions, *position;
    char *filling_top_bottom, *filling_left_right, *size, *size_max, *color_fg;
    char *color_delim, *color_bg, *separator, *bar_items;
    dXSARGS;

    API_FUNC(1, "bar_new", API_RETURN_EMPTY);
    if (items < 15)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    name = SvPV_nolen (ST (0));
    hidden = SvPV_nolen (ST (1));
    priority = SvPV_nolen (ST (2));
    type = SvPV_nolen (ST (3));
    conditions = SvPV_nolen (ST (4));
    position = SvPV_nolen (ST (5));
    filling_top_bottom = SvPV_nolen (ST (6));
    filling_left_right = SvPV_nolen (ST (7));
    size = SvPV_nolen (ST (8));
    size_max = SvPV_nolen (ST (9));
    color_fg = SvPV_nolen (ST (10));
    color_delim = SvPV_nolen (ST (11));
    color_bg = SvPV_nolen (ST (12));
    separator = SvPV_nolen (ST (13));
    bar_items = SvPV_nolen (ST (14));

    result = script_ptr2str (weechat_bar_new (name,
                                              hidden,
                                              priority,
                                              type,
                                              conditions,
                                              position,
                                              filling_top_bottom,
                                              filling_left_right,
                                              size,
                                              size_max,
                                              color_fg,
                                              color_delim,
                                              color_bg,
                                              separator,
                                              bar_items));

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat::bar_set: set a bar property
 */

XS (XS_weechat_api_bar_set)
{
    char *bar, *property, *value;
    dXSARGS;

    API_FUNC(1, "bar_set", API_RETURN_ERROR);
    if (items < 3)
        API_WRONG_ARGS(API_RETURN_ERROR);

    bar = SvPV_nolen (ST (0));
    property = SvPV_nolen (ST (1));
    value = SvPV_nolen (ST (2));

    weechat_bar_set (script_str2ptr (bar), property, value);

    API_RETURN_OK;
}

/*
 * weechat::bar_update: update a bar on screen
 */

XS (XS_weechat_api_bar_update)
{
    dXSARGS;

    API_FUNC(1, "bar_update", API_RETURN_ERROR);
    if (items < 1)
        API_WRONG_ARGS(API_RETURN_ERROR);

    weechat_bar_update (SvPV_nolen (ST (0))); /* name */

    API_RETURN_OK;
}

/*
 * weechat::bar_remove: remove a bar
 */

XS (XS_weechat_api_bar_remove)
{
    dXSARGS;

    API_FUNC(1, "bar_remove", API_RETURN_ERROR);
    if (items < 1)
        API_WRONG_ARGS(API_RETURN_ERROR);

    weechat_bar_remove (script_str2ptr (SvPV_nolen (ST (0)))); /* bar */

    API_RETURN_OK;
}

/*
 * weechat::command: execute a command on a buffer
 */

XS (XS_weechat_api_command)
{
    char *buffer, *command;
    dXSARGS;

    API_FUNC(1, "command", API_RETURN_ERROR);
    if (items < 2)
        API_WRONG_ARGS(API_RETURN_ERROR);

    buffer = SvPV_nolen (ST (0));
    command = SvPV_nolen (ST (1));

    script_api_command (weechat_perl_plugin,
                        perl_current_script,
                        script_str2ptr (buffer),
                        command);

    API_RETURN_OK;
}

/*
 * weechat::info_get: get info (as string)
 */

XS (XS_weechat_api_info_get)
{
    char *info_name, *arguments;
    const char *result;
    dXSARGS;

    API_FUNC(1, "info_get", API_RETURN_EMPTY);
    if (items < 2)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    info_name = SvPV_nolen (ST (0));
    arguments = SvPV_nolen (ST (1));

    result = weechat_info_get (info_name, arguments);

    API_RETURN_STRING(result);
}

/*
 * weechat::info_get_hashtable: get info (as hashtable)
 */

XS (XS_weechat_api_info_get_hashtable)
{
    char *info_name;
    struct t_hashtable *hashtable, *result_hashtable;
    HV *result_hash;
    dXSARGS;

    API_FUNC(1, "info_get_hashtable", API_RETURN_EMPTY);
    if (items < 2)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    info_name = SvPV_nolen (ST (0));
    hashtable = weechat_perl_hash_to_hashtable (ST (1),
                                                WEECHAT_SCRIPT_HASHTABLE_DEFAULT_SIZE);

    result_hashtable = weechat_info_get_hashtable (info_name, hashtable);
    result_hash = weechat_perl_hashtable_to_hash (result_hashtable);

    if (hashtable)
        weechat_hashtable_free (hashtable);
    if (result_hashtable)
        weechat_hashtable_free (result_hashtable);

    API_RETURN_OBJ(result_hash);
}

/*
 * weechat::infolist_new: create new infolist
 */

XS (XS_weechat_api_infolist_new)
{
    char *result;
    dXSARGS;

    /* make C compiler happy */
    (void) items;
    (void) cv;

    API_FUNC(1, "infolist_new", API_RETURN_EMPTY);

    result = script_ptr2str (weechat_infolist_new ());

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat::infolist_new_item: create new item in infolist
 */

XS (XS_weechat_api_infolist_new_item)
{
    char *infolist, *result;
    dXSARGS;

    API_FUNC(1, "infolist_new_item", API_RETURN_EMPTY);
    if (items < 1)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    infolist = SvPV_nolen (ST (0));

    result = script_ptr2str (weechat_infolist_new_item (script_str2ptr (infolist)));

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat::infolist_new_var_integer: create new integer variable in infolist
 */

XS (XS_weechat_api_infolist_new_var_integer)
{
    char *infolist, *name, *result;
    dXSARGS;

    API_FUNC(1, "infolist_new_var_integer", API_RETURN_EMPTY);
    if (items < 3)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    infolist = SvPV_nolen (ST (0));
    name = SvPV_nolen (ST (1));

    result = script_ptr2str (weechat_infolist_new_var_integer (script_str2ptr (infolist),
                                                               name,
                                                               SvIV (ST (2)))); /* value */

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat::infolist_new_var_string: create new string variable in infolist
 */

XS (XS_weechat_api_infolist_new_var_string)
{
    char *infolist, *name, *value, *result;
    dXSARGS;

    API_FUNC(1, "infolist_new_var_string", API_RETURN_EMPTY);
    if (items < 3)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    infolist = SvPV_nolen (ST (0));
    name = SvPV_nolen (ST (1));
    value = SvPV_nolen (ST (2));

    result = script_ptr2str (weechat_infolist_new_var_string (script_str2ptr (infolist),
                                                              name,
                                                              value));

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat::infolist_new_var_pointer: create new pointer variable in infolist
 */

XS (XS_weechat_api_infolist_new_var_pointer)
{
    char *infolist, *name, *value, *result;
    dXSARGS;

    API_FUNC(1, "infolist_new_var_pointer", API_RETURN_EMPTY);
    if (items < 3)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    infolist = SvPV_nolen (ST (0));
    name = SvPV_nolen (ST (1));
    value = SvPV_nolen (ST (2));

    result = script_ptr2str (weechat_infolist_new_var_pointer (script_str2ptr (infolist),
                                                               name,
                                                               script_str2ptr (value)));

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat::infolist_new_var_time: create new time variable in infolist
 */

XS (XS_weechat_api_infolist_new_var_time)
{
    char *infolist, *name, *result;
    dXSARGS;

    API_FUNC(1, "infolist_new_var_time", API_RETURN_EMPTY);
    if (items < 3)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    infolist = SvPV_nolen (ST (0));
    name = SvPV_nolen (ST (1));

    result = script_ptr2str (weechat_infolist_new_var_time (script_str2ptr (infolist),
                                                            name,
                                                            SvIV (ST (2)))); /* value */

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat::infolist_get: get list with infos
 */

XS (XS_weechat_api_infolist_get)
{
    char *result, *name, *pointer, *arguments;
    dXSARGS;

    API_FUNC(1, "infolist_get", API_RETURN_EMPTY);
    if (items < 3)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    name = SvPV_nolen (ST (0));
    pointer = SvPV_nolen (ST (1));
    arguments = SvPV_nolen (ST (2));

    result = script_ptr2str (weechat_infolist_get (name,
                                                   script_str2ptr (pointer),
                                                   arguments));

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat::infolist_next: move item pointer to next item in infolist
 */

XS (XS_weechat_api_infolist_next)
{
    int value;
    dXSARGS;

    API_FUNC(1, "infolist_next", API_RETURN_INT(0));
    if (items < 1)
        API_WRONG_ARGS(API_RETURN_INT(0));

    value = weechat_infolist_next (script_str2ptr (SvPV_nolen (ST (0)))); /* infolist */

    API_RETURN_INT(value);
}

/*
 * weechat::infolist_prev: move item pointer to previous item in infolist
 */

XS (XS_weechat_api_infolist_prev)
{
    int value;
    dXSARGS;

    API_FUNC(1, "infolist_prev", API_RETURN_INT(0));
    if (items < 1)
        API_WRONG_ARGS(API_RETURN_INT(0));

    value = weechat_infolist_prev (script_str2ptr (SvPV_nolen (ST (0)))); /* infolist */

    API_RETURN_INT(value);
}

/*
 * weechat::infolist_reset_item_cursor: reset pointer to current item in
 *                                      infolist
 */

XS (XS_weechat_api_infolist_reset_item_cursor)
{
    dXSARGS;

    API_FUNC(1, "infolist_reset_item_cursor", API_RETURN_ERROR);
    if (items < 1)
        API_WRONG_ARGS(API_RETURN_ERROR);

    weechat_infolist_reset_item_cursor (script_str2ptr (SvPV_nolen (ST (0)))); /* infolist */

    API_RETURN_OK;
}

/*
 * weechat::infolist_fields: get list of fields for current item of infolist
 */

XS (XS_weechat_api_infolist_fields)
{
    const char *result;
    dXSARGS;

    API_FUNC(1, "infolist_fields", API_RETURN_EMPTY);
    if (items < 1)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    result = weechat_infolist_fields (script_str2ptr (SvPV_nolen (ST (0)))); /* infolist */

    API_RETURN_STRING(result);
}

/*
 * weechat::infolist_integer: get integer value of a variable in infolist
 */

XS (XS_weechat_api_infolist_integer)
{
    char *infolist, *variable;
    int value;
    dXSARGS;

    API_FUNC(1, "infolist_integer", API_RETURN_INT(0));
    if (items < 2)
        API_WRONG_ARGS(API_RETURN_INT(0));

    infolist = SvPV_nolen (ST (0));
    variable = SvPV_nolen (ST (1));

    value = weechat_infolist_integer (script_str2ptr (infolist), variable);

    API_RETURN_INT(value);
}

/*
 * weechat::infolist_string: get string value of a variable in infolist
 */

XS (XS_weechat_api_infolist_string)
{
    char *infolist, *variable;
    const char *result;
    dXSARGS;

    API_FUNC(1, "infolist_string", API_RETURN_EMPTY);
    if (items < 2)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    infolist = SvPV_nolen (ST (0));
    variable = SvPV_nolen (ST (1));

    result = weechat_infolist_string (script_str2ptr (infolist), variable);

    API_RETURN_STRING(result);
}

/*
 * weechat::infolist_pointer: get pointer value of a variable in infolist
 */

XS (XS_weechat_api_infolist_pointer)
{
    char *infolist, *variable;
    char *result;
    dXSARGS;

    API_FUNC(1, "infolist_pointer", API_RETURN_EMPTY);
    if (items < 2)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    infolist = SvPV_nolen (ST (0));
    variable = SvPV_nolen (ST (1));

    result = script_ptr2str (weechat_infolist_pointer (script_str2ptr (infolist), variable));

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat::infolist_time: get time value of a variable in infolist
 */

XS (XS_weechat_api_infolist_time)
{
    time_t time;
    struct tm *date_tmp;
    char timebuffer[64], *result, *infolist, *variable;
    dXSARGS;

    API_FUNC(1, "infolist_time", API_RETURN_EMPTY);
    if (items < 2)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    infolist = SvPV_nolen (ST (0));
    variable = SvPV_nolen (ST (1));

    timebuffer[0] = '\0';
    time = weechat_infolist_time (script_str2ptr (infolist), variable);
    date_tmp = localtime (&time);
    if (date_tmp)
        strftime (timebuffer, sizeof (timebuffer), "%F %T", date_tmp);
    result = strdup (timebuffer);

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat::infolist_free: free infolist
 */

XS (XS_weechat_api_infolist_free)
{
    dXSARGS;

    API_FUNC(1, "infolist_free", API_RETURN_ERROR);
    if (items < 1)
        API_WRONG_ARGS(API_RETURN_ERROR);

    weechat_infolist_free (script_str2ptr (SvPV_nolen (ST (0)))); /* infolist */

    API_RETURN_OK;
}

/*
 * weechat::hdata_get: get hdata
 */

XS (XS_weechat_api_hdata_get)
{
    char *result, *name;
    dXSARGS;

    API_FUNC(1, "hdata_get", API_RETURN_EMPTY);
    if (items < 1)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    name = SvPV_nolen (ST (0));

    result = script_ptr2str (weechat_hdata_get (name));

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat::hdata_get_var_offset: get offset of variable in hdata
 */

XS (XS_weechat_api_hdata_get_var_offset)
{
    char *hdata, *name;
    int value;
    dXSARGS;

    API_FUNC(1, "hdata_get_var_offset", API_RETURN_INT(0));
    if (items < 2)
        API_WRONG_ARGS(API_RETURN_INT(0));

    hdata = SvPV_nolen (ST (0));
    name = SvPV_nolen (ST (1));

    value = weechat_hdata_get_var_offset (script_str2ptr (hdata), name);

    API_RETURN_INT(value);
}

/*
 * weechat::hdata_get_var_type_string: get type of variable as string in hdata
 */

XS (XS_weechat_api_hdata_get_var_type_string)
{
    const char *result;
    char *hdata, *name;
    dXSARGS;

    API_FUNC(1, "hdata_get_var_type_string", API_RETURN_EMPTY);
    if (items < 2)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    hdata = SvPV_nolen (ST (0));
    name = SvPV_nolen (ST (1));

    result = weechat_hdata_get_var_type_string (script_str2ptr (hdata), name);

    API_RETURN_STRING(result);
}

/*
 * weechat::hdata_get_var_hdata: get hdata for variable in hdata
 */

XS (XS_weechat_api_hdata_get_var_hdata)
{
    const char *result;
    char *hdata, *name;
    dXSARGS;

    API_FUNC(1, "hdata_get_var_hdata", API_RETURN_EMPTY);
    if (items < 2)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    hdata = SvPV_nolen (ST (0));
    name = SvPV_nolen (ST (1));

    result = weechat_hdata_get_var_hdata (script_str2ptr (hdata), name);

    API_RETURN_STRING(result);
}

/*
 * weechat::hdata_get_list: get list pointer in hdata
 */

XS (XS_weechat_api_hdata_get_list)
{
    char *hdata, *name;
    char *result;
    dXSARGS;

    API_FUNC(1, "hdata_get_list", API_RETURN_EMPTY);
    if (items < 2)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    hdata = SvPV_nolen (ST (0));
    name = SvPV_nolen (ST (1));

    result = script_ptr2str (weechat_hdata_get_list (script_str2ptr (hdata),
                                                     name));

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat::hdata_check_pointer: check pointer with hdata/list
 */

XS (XS_weechat_api_hdata_check_pointer)
{
    char *hdata, *list, *pointer;
    int value;
    dXSARGS;

    API_FUNC(1, "hdata_check_pointer", API_RETURN_INT(0));
    if (items < 3)
        API_WRONG_ARGS(API_RETURN_INT(0));

    hdata = SvPV_nolen (ST (0));
    list = SvPV_nolen (ST (1));
    pointer = SvPV_nolen (ST (2));

    value = weechat_hdata_check_pointer (script_str2ptr (hdata),
                                         script_str2ptr (list),
                                         script_str2ptr (pointer));

    API_RETURN_INT(value);
}

/*
 * weechat::hdata_move: move pointer to another element in list
 */

XS (XS_weechat_api_hdata_move)
{
    char *result, *hdata, *pointer;
    int count;
    dXSARGS;

    API_FUNC(1, "hdata_move", API_RETURN_EMPTY);
    if (items < 3)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    hdata = SvPV_nolen (ST (0));
    pointer = SvPV_nolen (ST (1));
    count = SvIV(ST (2));

    result = script_ptr2str (weechat_hdata_move (script_str2ptr (hdata),
                                                 script_str2ptr (pointer),
                                                 count));

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat::hdata_char: get char value of a variable in structure using hdata
 */

XS (XS_weechat_api_hdata_char)
{
    char *hdata, *pointer, *name;
    int value;
    dXSARGS;

    API_FUNC(1, "hdata_char", API_RETURN_INT(0));
    if (items < 3)
        API_WRONG_ARGS(API_RETURN_INT(0));

    hdata = SvPV_nolen (ST (0));
    pointer = SvPV_nolen (ST (1));
    name = SvPV_nolen (ST (2));

    value = (int)weechat_hdata_char (script_str2ptr (hdata),
                                     script_str2ptr (pointer),
                                     name);

    API_RETURN_INT(value);
}

/*
 * weechat::hdata_integer: get integer value of a variable in structure using
 *                         hdata
 */

XS (XS_weechat_api_hdata_integer)
{
    char *hdata, *pointer, *name;
    int value;
    dXSARGS;

    API_FUNC(1, "hdata_integer", API_RETURN_INT(0));
    if (items < 3)
        API_WRONG_ARGS(API_RETURN_INT(0));

    hdata = SvPV_nolen (ST (0));
    pointer = SvPV_nolen (ST (1));
    name = SvPV_nolen (ST (2));

    value = weechat_hdata_integer (script_str2ptr (hdata),
                                   script_str2ptr (pointer),
                                   name);

    API_RETURN_INT(value);
}

/*
 * weechat::hdata_long: get long value of a variable in structure using hdata
 */

XS (XS_weechat_api_hdata_long)
{
    char *hdata, *pointer, *name;
    long value;
    dXSARGS;

    API_FUNC(1, "hdata_long", API_RETURN_LONG(0));
    if (items < 3)
        API_WRONG_ARGS(API_RETURN_LONG(0));

    hdata = SvPV_nolen (ST (0));
    pointer = SvPV_nolen (ST (1));
    name = SvPV_nolen (ST (2));

    value = weechat_hdata_long (script_str2ptr (hdata),
                                script_str2ptr (pointer),
                                name);

    API_RETURN_LONG(value);
}

/*
 * weechat::hdata_string: get string value of a variable in structure using
 *                        hdata
 */

XS (XS_weechat_api_hdata_string)
{
    char *hdata, *pointer, *name;
    const char *result;
    dXSARGS;

    API_FUNC(1, "hdata_string", API_RETURN_EMPTY);
    if (items < 3)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    hdata = SvPV_nolen (ST (0));
    pointer = SvPV_nolen (ST (1));
    name = SvPV_nolen (ST (2));

    result = weechat_hdata_string (script_str2ptr (hdata),
                                   script_str2ptr (pointer),
                                   name);

    API_RETURN_STRING(result);
}

/*
 * weechat::hdata_pointer: get pointer value of a variable in structure using
 *                         hdata
 */

XS (XS_weechat_api_hdata_pointer)
{
    char *hdata, *pointer, *name;
    char *result;
    dXSARGS;

    API_FUNC(1, "hdata_pointer", API_RETURN_EMPTY);
    if (items < 3)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    hdata = SvPV_nolen (ST (0));
    pointer = SvPV_nolen (ST (1));
    name = SvPV_nolen (ST (2));

    result = script_ptr2str (weechat_hdata_pointer (script_str2ptr (hdata),
                                                    script_str2ptr (pointer),
                                                    name));

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat::hdata_time: get time value of a variable in structure using hdata
 */

XS (XS_weechat_api_hdata_time)
{
    time_t time;
    struct tm *date_tmp;
    char timebuffer[64], *result, *hdata, *pointer, *name;
    dXSARGS;

    API_FUNC(1, "hdata_time", API_RETURN_EMPTY);
    if (items < 3)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    hdata = SvPV_nolen (ST (0));
    pointer = SvPV_nolen (ST (1));
    name = SvPV_nolen (ST (2));

    timebuffer[0] = '\0';
    time = weechat_hdata_time (script_str2ptr (hdata),
                               script_str2ptr (pointer),
                               name);
    date_tmp = localtime (&time);
    if (date_tmp)
        strftime (timebuffer, sizeof (timebuffer), "%F %T", date_tmp);
    result = strdup (timebuffer);

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat::hdata_hashtable: get hashtable value of a variable in structure
 *                           using hdata
 */

XS (XS_weechat_api_hdata_hashtable)
{
    char *hdata, *pointer, *name;
    HV *result_hash;
    dXSARGS;

    API_FUNC(1, "hdata_hashtable", API_RETURN_EMPTY);
    if (items < 3)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    hdata = SvPV_nolen (ST (0));
    pointer = SvPV_nolen (ST (1));
    name = SvPV_nolen (ST (2));

    result_hash = weechat_perl_hashtable_to_hash (
        weechat_hdata_hashtable (script_str2ptr (hdata),
                                 script_str2ptr (pointer),
                                 name));

    API_RETURN_OBJ(result_hash);
}

/*
 * weechat::hdata_get_string: get hdata property as string
 */

XS (XS_weechat_api_hdata_get_string)
{
    char *hdata, *property;
    const char *result;
    dXSARGS;

    API_FUNC(1, "hdata_get_string", API_RETURN_EMPTY);
    if (items < 2)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    hdata = SvPV_nolen (ST (0));
    property = SvPV_nolen (ST (1));

    result = weechat_hdata_get_string (script_str2ptr (hdata), property);

    API_RETURN_STRING(result);
}

/*
 * weechat::upgrade_new: create an upgrade file
 */

XS (XS_weechat_api_upgrade_new)
{
    char *result, *filename;
    dXSARGS;

    API_FUNC(1, "upgrade_new", API_RETURN_EMPTY);
    if (items < 2)
        API_WRONG_ARGS(API_RETURN_EMPTY);

    filename = SvPV_nolen (ST (0));

    result = script_ptr2str (weechat_upgrade_new (filename,
                                                  SvIV (ST (1)))); /* write */

    API_RETURN_STRING_FREE(result);
}

/*
 * weechat::upgrade_write_object: write object in upgrade file
 */

XS (XS_weechat_api_upgrade_write_object)
{
    char *upgrade_file, *infolist;
    int rc;
    dXSARGS;

    API_FUNC(1, "upgrade_write_object", API_RETURN_INT(0));
    if (items < 3)
        API_WRONG_ARGS(API_RETURN_INT(0));

    upgrade_file = SvPV_nolen (ST (0));
    infolist = SvPV_nolen (ST (2));

    rc = weechat_upgrade_write_object (script_str2ptr (upgrade_file),
                                       SvIV (ST (1)), /* object_id */
                                       script_str2ptr (infolist));

    API_RETURN_INT(rc);
}

/*
 * weechat_perl_api_upgrade_read_cb: callback for reading object in upgrade file
 */

int
weechat_perl_api_upgrade_read_cb (void *data,
                                  struct t_upgrade_file *upgrade_file,
                                  int object_id,
                                  struct t_infolist *infolist)
{
    struct t_script_callback *script_callback;
    void *func_argv[4];
    char empty_arg[1] = { '\0' }, str_object_id[32];
    int *rc, ret;

    script_callback = (struct t_script_callback *)data;

    if (script_callback && script_callback->function && script_callback->function[0])
    {
        snprintf (str_object_id, sizeof (str_object_id), "%d", object_id);

        func_argv[0] = (script_callback->data) ? script_callback->data : empty_arg;
        func_argv[1] = script_ptr2str (upgrade_file);
        func_argv[2] = str_object_id;
        func_argv[3] = script_ptr2str (infolist);

        rc = (int *) weechat_perl_exec (script_callback->script,
                                        WEECHAT_SCRIPT_EXEC_INT,
                                        script_callback->function,
                                        "ssss", func_argv);

        if (!rc)
            ret = WEECHAT_RC_ERROR;
        else
        {
            ret = *rc;
            free (rc);
        }
        if (func_argv[1])
            free (func_argv[1]);
        if (func_argv[3])
            free (func_argv[3]);

        return ret;
    }

    return WEECHAT_RC_ERROR;
}

/*
 * weechat::config_upgrade_read: read upgrade file
 */

XS (XS_weechat_api_upgrade_read)
{
    char *upgrade_file, *function, *data;
    int rc;
    dXSARGS;

    API_FUNC(1, "upgrade_read", API_RETURN_INT(0));
    if (items < 3)
        API_WRONG_ARGS(API_RETURN_INT(0));

    upgrade_file = SvPV_nolen (ST (0));
    function = SvPV_nolen (ST (1));
    data = SvPV_nolen (ST (2));

    rc = script_api_upgrade_read (weechat_perl_plugin,
                                  perl_current_script,
                                  script_str2ptr (upgrade_file),
                                  &weechat_perl_api_upgrade_read_cb,
                                  function,
                                  data);

    API_RETURN_INT(rc);
}

/*
 * weechat::upgrade_close: close upgrade file
 */

XS (XS_weechat_api_upgrade_close)
{
    char *upgrade_file;
    dXSARGS;

    API_FUNC(1, "upgrade_close", API_RETURN_ERROR);
    if (items < 1)
        API_WRONG_ARGS(API_RETURN_ERROR);

    upgrade_file = SvPV_nolen (ST (0));

    weechat_upgrade_close (script_str2ptr (upgrade_file));

    API_RETURN_OK;
}

/*
 * weechat_perl_api_init: initialize subroutines
 */

void
weechat_perl_api_init (pTHX)
{
    HV *stash;

    newXS ("DynaLoader::boot_DynaLoader", boot_DynaLoader, __FILE__);

    /* interface functions */
    API_DEF_FUNC(register);
    API_DEF_FUNC(plugin_get_name);
    API_DEF_FUNC(charset_set);
    API_DEF_FUNC(iconv_to_internal);
    API_DEF_FUNC(iconv_from_internal);
    API_DEF_FUNC(gettext);
    API_DEF_FUNC(ngettext);
    API_DEF_FUNC(string_match);
    API_DEF_FUNC(string_has_highlight);
    API_DEF_FUNC(string_has_highlight_regex);
    API_DEF_FUNC(string_mask_to_regex);
    API_DEF_FUNC(string_remove_color);
    API_DEF_FUNC(string_is_command_char);
    API_DEF_FUNC(string_input_for_buffer);
    API_DEF_FUNC(mkdir_home);
    API_DEF_FUNC(mkdir);
    API_DEF_FUNC(mkdir_parents);
    API_DEF_FUNC(list_new);
    API_DEF_FUNC(list_add);
    API_DEF_FUNC(list_search);
    API_DEF_FUNC(list_search_pos);
    API_DEF_FUNC(list_casesearch);
    API_DEF_FUNC(list_casesearch_pos);
    API_DEF_FUNC(list_get);
    API_DEF_FUNC(list_set);
    API_DEF_FUNC(list_next);
    API_DEF_FUNC(list_prev);
    API_DEF_FUNC(list_string);
    API_DEF_FUNC(list_size);
    API_DEF_FUNC(list_remove);
    API_DEF_FUNC(list_remove_all);
    API_DEF_FUNC(list_free);
    API_DEF_FUNC(config_new);
    API_DEF_FUNC(config_new_section);
    API_DEF_FUNC(config_search_section);
    API_DEF_FUNC(config_new_option);
    API_DEF_FUNC(config_search_option);
    API_DEF_FUNC(config_string_to_boolean);
    API_DEF_FUNC(config_option_reset);
    API_DEF_FUNC(config_option_set);
    API_DEF_FUNC(config_option_set_null);
    API_DEF_FUNC(config_option_unset);
    API_DEF_FUNC(config_option_rename);
    API_DEF_FUNC(config_option_is_null);
    API_DEF_FUNC(config_option_default_is_null);
    API_DEF_FUNC(config_boolean);
    API_DEF_FUNC(config_boolean_default);
    API_DEF_FUNC(config_integer);
    API_DEF_FUNC(config_integer_default);
    API_DEF_FUNC(config_string);
    API_DEF_FUNC(config_string_default);
    API_DEF_FUNC(config_color);
    API_DEF_FUNC(config_color_default);
    API_DEF_FUNC(config_write_option);
    API_DEF_FUNC(config_write_line);
    API_DEF_FUNC(config_write);
    API_DEF_FUNC(config_read);
    API_DEF_FUNC(config_reload);
    API_DEF_FUNC(config_option_free);
    API_DEF_FUNC(config_section_free_options);
    API_DEF_FUNC(config_section_free);
    API_DEF_FUNC(config_free);
    API_DEF_FUNC(config_get);
    API_DEF_FUNC(config_get_plugin);
    API_DEF_FUNC(config_is_set_plugin);
    API_DEF_FUNC(config_set_plugin);
    API_DEF_FUNC(config_set_desc_plugin);
    API_DEF_FUNC(config_unset_plugin);
    API_DEF_FUNC(key_bind);
    API_DEF_FUNC(key_unbind);
    API_DEF_FUNC(prefix);
    API_DEF_FUNC(color);
    API_DEF_FUNC(print);
    API_DEF_FUNC(print_date_tags);
    API_DEF_FUNC(print_y);
    API_DEF_FUNC(log_print);
    API_DEF_FUNC(hook_command);
    API_DEF_FUNC(hook_command_run);
    API_DEF_FUNC(hook_timer);
    API_DEF_FUNC(hook_fd);
    API_DEF_FUNC(hook_process);
    API_DEF_FUNC(hook_process_hashtable);
    API_DEF_FUNC(hook_connect);
    API_DEF_FUNC(hook_print);
    API_DEF_FUNC(hook_signal);
    API_DEF_FUNC(hook_signal_send);
    API_DEF_FUNC(hook_hsignal);
    API_DEF_FUNC(hook_hsignal_send);
    API_DEF_FUNC(hook_config);
    API_DEF_FUNC(hook_completion);
    API_DEF_FUNC(hook_completion_list_add);
    API_DEF_FUNC(hook_modifier);
    API_DEF_FUNC(hook_modifier_exec);
    API_DEF_FUNC(hook_info);
    API_DEF_FUNC(hook_info_hashtable);
    API_DEF_FUNC(hook_infolist);
    API_DEF_FUNC(hook_focus);
    API_DEF_FUNC(unhook);
    API_DEF_FUNC(unhook_all);
    API_DEF_FUNC(buffer_new);
    API_DEF_FUNC(buffer_search);
    API_DEF_FUNC(buffer_search_main);
    API_DEF_FUNC(current_buffer);
    API_DEF_FUNC(buffer_clear);
    API_DEF_FUNC(buffer_close);
    API_DEF_FUNC(buffer_merge);
    API_DEF_FUNC(buffer_unmerge);
    API_DEF_FUNC(buffer_get_integer);
    API_DEF_FUNC(buffer_get_string);
    API_DEF_FUNC(buffer_get_pointer);
    API_DEF_FUNC(buffer_set);
    API_DEF_FUNC(buffer_string_replace_local_var);
    API_DEF_FUNC(buffer_match_list);
    API_DEF_FUNC(current_window);
    API_DEF_FUNC(window_search_with_buffer);
    API_DEF_FUNC(window_get_integer);
    API_DEF_FUNC(window_get_string);
    API_DEF_FUNC(window_get_pointer);
    API_DEF_FUNC(window_set_title);
    API_DEF_FUNC(nicklist_add_group);
    API_DEF_FUNC(nicklist_search_group);
    API_DEF_FUNC(nicklist_add_nick);
    API_DEF_FUNC(nicklist_search_nick);
    API_DEF_FUNC(nicklist_remove_group);
    API_DEF_FUNC(nicklist_remove_nick);
    API_DEF_FUNC(nicklist_remove_all);
    API_DEF_FUNC(nicklist_group_get_integer);
    API_DEF_FUNC(nicklist_group_get_string);
    API_DEF_FUNC(nicklist_group_get_pointer);
    API_DEF_FUNC(nicklist_group_set);
    API_DEF_FUNC(nicklist_nick_get_integer);
    API_DEF_FUNC(nicklist_nick_get_string);
    API_DEF_FUNC(nicklist_nick_get_pointer);
    API_DEF_FUNC(nicklist_nick_set);
    API_DEF_FUNC(bar_item_search);
    API_DEF_FUNC(bar_item_new);
    API_DEF_FUNC(bar_item_update);
    API_DEF_FUNC(bar_item_remove);
    API_DEF_FUNC(bar_search);
    API_DEF_FUNC(bar_new);
    API_DEF_FUNC(bar_set);
    API_DEF_FUNC(bar_update);
    API_DEF_FUNC(bar_remove);
    API_DEF_FUNC(command);
    API_DEF_FUNC(info_get);
    API_DEF_FUNC(info_get_hashtable);
    API_DEF_FUNC(infolist_new);
    API_DEF_FUNC(infolist_new_item);
    API_DEF_FUNC(infolist_new_var_integer);
    API_DEF_FUNC(infolist_new_var_string);
    API_DEF_FUNC(infolist_new_var_pointer);
    API_DEF_FUNC(infolist_new_var_time);
    API_DEF_FUNC(infolist_get);
    API_DEF_FUNC(infolist_next);
    API_DEF_FUNC(infolist_prev);
    API_DEF_FUNC(infolist_reset_item_cursor);
    API_DEF_FUNC(infolist_fields);
    API_DEF_FUNC(infolist_integer);
    API_DEF_FUNC(infolist_string);
    API_DEF_FUNC(infolist_pointer);
    API_DEF_FUNC(infolist_time);
    API_DEF_FUNC(infolist_free);
    API_DEF_FUNC(hdata_get);
    API_DEF_FUNC(hdata_get_var_offset);
    API_DEF_FUNC(hdata_get_var_type_string);
    API_DEF_FUNC(hdata_get_var_hdata);
    API_DEF_FUNC(hdata_get_list);
    API_DEF_FUNC(hdata_check_pointer);
    API_DEF_FUNC(hdata_move);
    API_DEF_FUNC(hdata_char);
    API_DEF_FUNC(hdata_integer);
    API_DEF_FUNC(hdata_long);
    API_DEF_FUNC(hdata_string);
    API_DEF_FUNC(hdata_pointer);
    API_DEF_FUNC(hdata_time);
    API_DEF_FUNC(hdata_hashtable);
    API_DEF_FUNC(hdata_get_string);
    API_DEF_FUNC(upgrade_new);
    API_DEF_FUNC(upgrade_write_object);
    API_DEF_FUNC(upgrade_read);
    API_DEF_FUNC(upgrade_close);

    /* interface constants */
    stash = gv_stashpv ("weechat", TRUE);
    newCONSTSUB (stash, "weechat::WEECHAT_RC_OK", newSViv (WEECHAT_RC_OK));
    newCONSTSUB (stash, "weechat::WEECHAT_RC_OK_EAT", newSViv (WEECHAT_RC_OK_EAT));
    newCONSTSUB (stash, "weechat::WEECHAT_RC_ERROR", newSViv (WEECHAT_RC_ERROR));

    newCONSTSUB (stash, "weechat::WEECHAT_CONFIG_READ_OK", newSViv (WEECHAT_CONFIG_READ_OK));
    newCONSTSUB (stash, "weechat::WEECHAT_CONFIG_READ_MEMORY_ERROR", newSViv (WEECHAT_CONFIG_READ_MEMORY_ERROR));
    newCONSTSUB (stash, "weechat::WEECHAT_CONFIG_READ_FILE_NOT_FOUND", newSViv (WEECHAT_CONFIG_READ_FILE_NOT_FOUND));
    newCONSTSUB (stash, "weechat::WEECHAT_CONFIG_WRITE_OK", newSViv (WEECHAT_CONFIG_WRITE_OK));
    newCONSTSUB (stash, "weechat::WEECHAT_CONFIG_WRITE_ERROR", newSViv (WEECHAT_CONFIG_WRITE_ERROR));
    newCONSTSUB (stash, "weechat::WEECHAT_CONFIG_WRITE_MEMORY_ERROR", newSViv (WEECHAT_CONFIG_WRITE_MEMORY_ERROR));
    newCONSTSUB (stash, "weechat::WEECHAT_CONFIG_OPTION_SET_OK_CHANGED", newSViv (WEECHAT_CONFIG_OPTION_SET_OK_CHANGED));
    newCONSTSUB (stash, "weechat::WEECHAT_CONFIG_OPTION_SET_OK_SAME_VALUE", newSViv (WEECHAT_CONFIG_OPTION_SET_OK_SAME_VALUE));
    newCONSTSUB (stash, "weechat::WEECHAT_CONFIG_OPTION_SET_ERROR", newSViv (WEECHAT_CONFIG_OPTION_SET_ERROR));
    newCONSTSUB (stash, "weechat::WEECHAT_CONFIG_OPTION_SET_OPTION_NOT_FOUND", newSViv (WEECHAT_CONFIG_OPTION_SET_OPTION_NOT_FOUND));
    newCONSTSUB (stash, "weechat::WEECHAT_CONFIG_OPTION_UNSET_OK_NO_RESET", newSViv (WEECHAT_CONFIG_OPTION_UNSET_OK_NO_RESET));
    newCONSTSUB (stash, "weechat::WEECHAT_CONFIG_OPTION_UNSET_OK_RESET", newSViv (WEECHAT_CONFIG_OPTION_UNSET_OK_RESET));
    newCONSTSUB (stash, "weechat::WEECHAT_CONFIG_OPTION_UNSET_OK_REMOVED", newSViv (WEECHAT_CONFIG_OPTION_UNSET_OK_REMOVED));
    newCONSTSUB (stash, "weechat::WEECHAT_CONFIG_OPTION_UNSET_ERROR", newSViv (WEECHAT_CONFIG_OPTION_UNSET_ERROR));

    newCONSTSUB (stash, "weechat::WEECHAT_LIST_POS_SORT", newSVpv (WEECHAT_LIST_POS_SORT, PL_na));
    newCONSTSUB (stash, "weechat::WEECHAT_LIST_POS_BEGINNING", newSVpv (WEECHAT_LIST_POS_BEGINNING, PL_na));
    newCONSTSUB (stash, "weechat::WEECHAT_LIST_POS_END", newSVpv (WEECHAT_LIST_POS_END, PL_na));

    newCONSTSUB (stash, "weechat::WEECHAT_HOTLIST_LOW", newSVpv (WEECHAT_HOTLIST_LOW, PL_na));
    newCONSTSUB (stash, "weechat::WEECHAT_HOTLIST_MESSAGE", newSVpv (WEECHAT_HOTLIST_MESSAGE, PL_na));
    newCONSTSUB (stash, "weechat::WEECHAT_HOTLIST_PRIVATE", newSVpv (WEECHAT_HOTLIST_PRIVATE, PL_na));
    newCONSTSUB (stash, "weechat::WEECHAT_HOTLIST_HIGHLIGHT", newSVpv (WEECHAT_HOTLIST_HIGHLIGHT, PL_na));

    newCONSTSUB (stash, "weechat::WEECHAT_HOOK_PROCESS_RUNNING", newSViv (WEECHAT_HOOK_PROCESS_RUNNING));
    newCONSTSUB (stash, "weechat::WEECHAT_HOOK_PROCESS_ERROR", newSViv (WEECHAT_HOOK_PROCESS_ERROR));

    newCONSTSUB (stash, "weechat::WEECHAT_HOOK_CONNECT_OK", newSViv (WEECHAT_HOOK_CONNECT_OK));
    newCONSTSUB (stash, "weechat::WEECHAT_HOOK_CONNECT_ADDRESS_NOT_FOUND", newSViv (WEECHAT_HOOK_CONNECT_ADDRESS_NOT_FOUND));
    newCONSTSUB (stash, "weechat::WEECHAT_HOOK_CONNECT_IP_ADDRESS_NOT_FOUND", newSViv (WEECHAT_HOOK_CONNECT_IP_ADDRESS_NOT_FOUND));
    newCONSTSUB (stash, "weechat::WEECHAT_HOOK_CONNECT_CONNECTION_REFUSED", newSViv (WEECHAT_HOOK_CONNECT_CONNECTION_REFUSED));
    newCONSTSUB (stash, "weechat::WEECHAT_HOOK_CONNECT_PROXY_ERROR", newSViv (WEECHAT_HOOK_CONNECT_PROXY_ERROR));
    newCONSTSUB (stash, "weechat::WEECHAT_HOOK_CONNECT_LOCAL_HOSTNAME_ERROR", newSViv (WEECHAT_HOOK_CONNECT_LOCAL_HOSTNAME_ERROR));
    newCONSTSUB (stash, "weechat::WEECHAT_HOOK_CONNECT_GNUTLS_INIT_ERROR", newSViv (WEECHAT_HOOK_CONNECT_GNUTLS_INIT_ERROR));
    newCONSTSUB (stash, "weechat::WEECHAT_HOOK_CONNECT_GNUTLS_HANDSHAKE_ERROR", newSViv (WEECHAT_HOOK_CONNECT_GNUTLS_HANDSHAKE_ERROR));
    newCONSTSUB (stash, "weechat::WEECHAT_HOOK_CONNECT_MEMORY_ERROR", newSViv (WEECHAT_HOOK_CONNECT_MEMORY_ERROR));

    newCONSTSUB (stash, "weechat::WEECHAT_HOOK_SIGNAL_STRING", newSVpv (WEECHAT_HOOK_SIGNAL_STRING, PL_na));
    newCONSTSUB (stash, "weechat::WEECHAT_HOOK_SIGNAL_INT", newSVpv (WEECHAT_HOOK_SIGNAL_INT, PL_na));
    newCONSTSUB (stash, "weechat::WEECHAT_HOOK_SIGNAL_POINTER", newSVpv (WEECHAT_HOOK_SIGNAL_POINTER, PL_na));
}
