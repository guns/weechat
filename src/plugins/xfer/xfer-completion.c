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
 * xfer-completion.c: nick completion for xfer chats
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../weechat-plugin.h"
#include "xfer.h"
#include "xfer-completion.h"


/*
 * xfer_completion_nick_cb: callback for completion with nick
 */

int
xfer_completion_nick_cb (void *data, const char *completion_item,
                         struct t_gui_buffer *buffer,
                         struct t_gui_completion *completion)
{
    struct t_xfer *ptr_xfer;

    /* make C compiler happy */
    (void) data;
    (void) completion_item;

    ptr_xfer = xfer_search_by_buffer (buffer);
    if (ptr_xfer)
    {
        /* remote nick */
        weechat_hook_completion_list_add (completion,
                                          ptr_xfer->remote_nick,
                                          0,
                                          WEECHAT_LIST_POS_SORT);
        /* add self nick at the end */
        weechat_hook_completion_list_add (completion,
                                          ptr_xfer->local_nick,
                                          1,
                                          WEECHAT_LIST_POS_END);
    }

    return WEECHAT_RC_OK;
}

/*
 * xfer_completion_init: init completion for xfer plugin
 */

void
xfer_completion_init ()
{
    weechat_hook_completion ("nick",
                             N_("nicks of DCC chat"),
                             &xfer_completion_nick_cb, NULL);
}
