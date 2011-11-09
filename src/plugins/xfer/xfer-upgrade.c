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
 * xfer-upgrade.c: save/restore xfer plugin data when upgrading WeeChat
 */

#include <stdlib.h>
#include <string.h>

#include "../weechat-plugin.h"
#include "xfer.h"
#include "xfer-upgrade.h"
#include "xfer-buffer.h"


/*
 * xfer_upgrade_save_xfers: save xfers info to upgrade file
 */

int
xfer_upgrade_save_xfers (struct t_upgrade_file *upgrade_file)
{
    /* TODO: save xfer data */
    (void) upgrade_file;
    return 1;
}

/*
 * xfer_upgrade_save: save upgrade file
 *                    return 1 if ok, 0 if error
 */

int
xfer_upgrade_save ()
{
    int rc;
    struct t_upgrade_file *upgrade_file;

    upgrade_file = weechat_upgrade_new (XFER_UPGRADE_FILENAME, 1);
    if (!upgrade_file)
        return 0;

    rc = xfer_upgrade_save_xfers (upgrade_file);

    weechat_upgrade_close (upgrade_file);

    return rc;
}

/*
 * xfer_upgrade_set_buffer_callbacks: restore buffers callbacks (input and
 *                                    close) for buffers created by xfer plugin
 */

void
xfer_upgrade_set_buffer_callbacks ()
{
    struct t_infolist *infolist;
    struct t_gui_buffer *ptr_buffer;

    infolist = weechat_infolist_get ("buffer", NULL, NULL);
    if (infolist)
    {
        while (weechat_infolist_next (infolist))
        {
            if (weechat_infolist_pointer (infolist, "plugin") == weechat_xfer_plugin)
            {
                ptr_buffer = weechat_infolist_pointer (infolist, "pointer");
                weechat_buffer_set_pointer (ptr_buffer, "close_callback", &xfer_buffer_close_cb);
                weechat_buffer_set_pointer (ptr_buffer, "input_callback", &xfer_buffer_input_cb);
                if (strcmp (weechat_infolist_string (infolist, "name"),
                            XFER_BUFFER_NAME) == 0)
                {
                    xfer_buffer = ptr_buffer;
                }
            }
        }
        weechat_infolist_free (infolist);
    }
}

/*
 * xfer_upgrade_read_cb: read callback for xfer upgrade file
 */

int
xfer_upgrade_read_cb (void *data,
                      struct t_upgrade_file *upgrade_file,
                      int object_id,
                      struct t_infolist *infolist)
{
    /* TODO: write xfer read cb */
    (void) data;
    (void) upgrade_file;
    (void) object_id;
    (void) infolist;

    return WEECHAT_RC_OK;
}

/*
 * xfer_upgrade_load: load upgrade file
 *                    return 1 if ok, 0 if error
 */

int
xfer_upgrade_load ()
{
    int rc;
    struct t_upgrade_file *upgrade_file;

    xfer_upgrade_set_buffer_callbacks ();

    upgrade_file = weechat_upgrade_new (XFER_UPGRADE_FILENAME, 0);
    rc = weechat_upgrade_read (upgrade_file, &xfer_upgrade_read_cb, NULL);

    return rc;
}
