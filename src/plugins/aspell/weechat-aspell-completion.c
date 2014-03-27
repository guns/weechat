/*
 * weechat-aspell-completion.c - completion for aspell commands
 *
 * Copyright (C) 2013-2014 Sébastien Helleu <flashcode@flashtux.org>
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../weechat-plugin.h"
#include "weechat-aspell.h"


/*
 * Adds aspell langs (all langs, even for dictionaries not installed) to
 * completion list.
 */

int
weechat_aspell_completion_langs_cb (void *data, const char *completion_item,
                                    struct t_gui_buffer *buffer,
                                    struct t_gui_completion *completion)
{
    int i;

    /* make C compiler happy */
    (void) data;
    (void) completion_item;
    (void) buffer;

    for (i = 0; aspell_langs[i].code; i++)
    {
        weechat_hook_completion_list_add (completion,
                                          aspell_langs[i].code,
                                          0, WEECHAT_LIST_POS_SORT);
    }

    return WEECHAT_RC_OK;
}

/*
 * Adds a dictionary to completion when using enchant.
 */

#ifdef USE_ENCHANT
void
weechat_aspell_completion_enchant_add_dict_cb (const char *lang_tag,
                                               const char *provider_name,
                                               const char *provider_desc,
                                               const char *provider_file,
                                               void *user_data)
{
    /* make C compiler happy */
    (void) provider_name;
    (void) provider_desc;
    (void) provider_file;

    weechat_hook_completion_list_add ((struct t_gui_completion *)user_data,
                                      lang_tag, 0, WEECHAT_LIST_POS_SORT);
}
#endif

/*
 * Adds aspell dictionaries (only installed dictionaries) to completion list.
 */

int
weechat_aspell_completion_dicts_cb (void *data,
                                    const char *completion_item,
                                    struct t_gui_buffer *buffer,
                                    struct t_gui_completion *completion)
{
#ifndef USE_ENCHANT
    struct AspellConfig *config;
    AspellDictInfoList *list;
    AspellDictInfoEnumeration *elements;
    const AspellDictInfo *dict;
#endif

    /* make C compiler happy */
    (void) data;
    (void) completion_item;
    (void) buffer;

#ifdef USE_ENCHANT
    enchant_broker_list_dicts (broker,
                               weechat_aspell_completion_enchant_add_dict_cb,
                               completion);
#else
    config = new_aspell_config ();
    list = get_aspell_dict_info_list (config);
    elements = aspell_dict_info_list_elements (list);

    while ((dict = aspell_dict_info_enumeration_next (elements)) != NULL)
    {
        weechat_hook_completion_list_add (completion, dict->name,
                                          0, WEECHAT_LIST_POS_SORT);
    }

    delete_aspell_dict_info_enumeration (elements);
    delete_aspell_config (config);
#endif

    return WEECHAT_RC_OK;
}

/*
 * Hooks completion.
 */

void
weechat_aspell_completion_init ()
{
    weechat_hook_completion ("aspell_langs",
                             N_("list of all languages supported by aspell"),
                             &weechat_aspell_completion_langs_cb, NULL);
    weechat_hook_completion ("aspell_dicts",
                             N_("list of aspell installed dictionaries"),
                             &weechat_aspell_completion_dicts_cb, NULL);
}
