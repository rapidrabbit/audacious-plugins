/*
 * ui_playlist_widget.c
 * Copyright 2011 John Lindgren
 *
 * This file is part of Audacious.
 *
 * Audacious is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, version 2 or version 3 of the License.
 *
 * Audacious is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * Audacious. If not, see <http://www.gnu.org/licenses/>.
 *
 * The Audacious team does not consider modular code linking to Audacious or
 * using our public API to be a derived work.
 */

#include <string.h>

#include <gtk/gtk.h>

#include <audacious/audconfig.h>
#include <audacious/drct.h>
#include <audacious/playlist.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/list.h>

#include "gtkui_cfg.h"
#include "playlist_util.h"
#include "ui_manager.h"
#include "ui_playlist_widget.h"

static const GType pw_col_types[PW_COLS] = {G_TYPE_INT, G_TYPE_STRING,
 G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
 G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING};
static const gboolean pw_col_expand[PW_COLS] = {FALSE, TRUE, TRUE, FALSE, TRUE,
 FALSE, FALSE, FALSE, TRUE, TRUE};
static const gboolean pw_col_label[PW_COLS] = {FALSE, TRUE, TRUE, TRUE, TRUE,
 FALSE, FALSE, FALSE, TRUE, TRUE};

typedef struct {
    gint list;
    GList * queue;
} PlaylistWidgetData;

static void set_int_from_tuple (GValue * value, const Tuple * tuple, gint field)
{
    gint i = tuple ? tuple_get_int (tuple, field, NULL) : 0;
    if (i > 0)
        g_value_take_string (value, g_strdup_printf ("%d", i));
    else
        g_value_set_string (value, "");
}

static void set_string_from_tuple (GValue * value, const Tuple * tuple,
 gint field)
{
    g_value_set_string (value, tuple ? tuple_get_string (tuple, field, NULL) :
     NULL);
}

static void set_queued (GValue * value, gint list, gint row)
{
    int q = aud_playlist_queue_find_entry (list, row);
    if (q < 0)
        g_value_set_string (value, "");
    else
        g_value_take_string (value, g_strdup_printf ("#%d", 1 + q));
}

static void set_length (GValue * value, gint list, gint row)
{
    gint len = aud_playlist_entry_get_length (list, row, TRUE);
    if (len)
    {
        len /= 1000;

        gchar s[16];
        if (len < 3600)
            snprintf (s, sizeof s, aud_cfg->leading_zero ? "%02d:%02d" :
             "%d:%02d", len / 60, len % 60);
        else
            snprintf (s, sizeof s, "%d:%02d:%02d", len / 3600, (len / 60) % 60,
             len % 60);

        g_value_set_string (value, s);
    }
    else
        g_value_set_string (value, "");
}

static void get_value (void * user, gint row, gint column, GValue * value)
{
    PlaylistWidgetData * data = user;
    g_return_if_fail (column >= 0 && column < pw_num_cols);
    g_return_if_fail (row >= 0 && row < aud_playlist_entry_count (data->list));

    column = pw_cols[column];

    const gchar * title = NULL, * artist = NULL, * album = NULL;
    const Tuple * tuple = NULL;

    if (column == PW_COL_TITLE || column == PW_COL_ARTIST || column ==
     PW_COL_ALBUM)
        aud_playlist_entry_describe (data->list, row, & title, & artist,
         & album, TRUE);
    else if (column == PW_COL_YEAR || column == PW_COL_TRACK || column ==
     PW_COL_FILENAME || column == PW_COL_PATH)
        tuple = aud_playlist_entry_get_tuple (data->list, row, TRUE);

    switch (column)
    {
    case PW_COL_NUMBER:
        g_value_set_int (value, 1 + row);
        break;
    case PW_COL_TITLE:
        g_value_set_string (value, title);
        break;
    case PW_COL_ARTIST:
        g_value_set_string (value, artist);
        break;
    case PW_COL_YEAR:
        set_int_from_tuple (value, tuple, FIELD_YEAR);
        break;
    case PW_COL_ALBUM:
        g_value_set_string (value, album);
        break;
    case PW_COL_TRACK:
        set_int_from_tuple (value, tuple, FIELD_TRACK_NUMBER);
        break;
    case PW_COL_QUEUED:
        set_queued (value, data->list, row);
        break;
    case PW_COL_LENGTH:
        set_length (value, data->list, row);
        break;
    case PW_COL_FILENAME:
        set_string_from_tuple (value, tuple, FIELD_FILE_NAME);
        break;
    case PW_COL_PATH:
        set_string_from_tuple (value, tuple, FIELD_FILE_PATH);
        break;
    }
}

static gboolean get_selected (void * user, gint row)
{
    return aud_playlist_entry_get_selected (((PlaylistWidgetData *) user)->list,
     row);
}

static void set_selected (void * user, gint row, gboolean selected)
{
    aud_playlist_entry_set_selected (((PlaylistWidgetData *) user)->list, row,
     selected);
}

static void select_all (void * user, gboolean selected)
{
    aud_playlist_select_all (((PlaylistWidgetData *) user)->list, selected);
}

static void activate_row (void * user, gint row)
{
    gint list = ((PlaylistWidgetData *) user)->list;
    aud_playlist_set_playing (list);
    aud_playlist_set_position (list, row);
    aud_drct_play ();
}

static void right_click (void * user, GdkEventButton * event)
{
    ui_manager_popup_menu_show ((GtkMenu *) playlistwin_popup_menu,
     event->x_root, event->y_root, event->button, event->time);
}

static void shift_rows (void * user, gint row, gint before)
{
    gint list = ((PlaylistWidgetData *) user)->list;

    /* Adjust the shift amount so that the selected entry closest to the
     * destination ends up at the destination. */
    if (before > row)
        before -= playlist_count_selected_in_range (list, row, before - row);
    else
        before += playlist_count_selected_in_range (list, before, row - before);

    aud_playlist_shift (list, row, before - row);
}

static void get_data (void * user, void * * data, gint * length)
{
    gchar * text = audgui_urilist_create_from_selected
     (((PlaylistWidgetData *) user)->list);
    g_return_if_fail (text);
    * data = text;
    * length = strlen (text);
}

static void receive_data (void * user, gint row, const void * data, gint length)
{
    gchar * text = g_malloc (length + 1);
    memcpy (text, data, length);
    text[length] = 0;
    audgui_urilist_insert (((PlaylistWidgetData *) user)->list, row, text);
    g_free (text);
}

static const AudguiListCallbacks callbacks = {
 .get_value = get_value,
 .get_selected = get_selected,
 .set_selected = set_selected,
 .select_all = select_all,
 .activate_row = activate_row,
 .right_click = right_click,
 .shift_rows = shift_rows,
 .data_type = "text/uri-list",
 .get_data = get_data,
 .receive_data = receive_data};

static void destroy_cb (PlaylistWidgetData * data)
{
    g_list_free (data->queue);
    g_free (data);
}

GtkWidget * ui_playlist_widget_new (gint playlist)
{
    PlaylistWidgetData * data = g_malloc0 (sizeof (PlaylistWidgetData));
    data->list = playlist;
    data->queue = NULL;

    GtkWidget * list = audgui_list_new (& callbacks, data,
     aud_playlist_entry_count (playlist));
    gtk_tree_view_set_headers_visible ((GtkTreeView *) list,
     config.playlist_headers);
    g_signal_connect_swapped (list, "destroy", (GCallback) destroy_cb, data);

    for (gint i = 0; i < pw_num_cols; i ++)
    {
        gint n = pw_cols[i];
        audgui_list_add_column (list, pw_col_label[n] ? _(pw_col_names[n]) :
         NULL, i, pw_col_types[n], pw_col_expand[n]);
    }

    return list;
}

gint ui_playlist_widget_get_playlist (GtkWidget * widget)
{
    PlaylistWidgetData * data = audgui_list_get_user (widget);
    g_return_val_if_fail (data, -1);
    return data->list;
}

void ui_playlist_widget_set_playlist (GtkWidget * widget, gint list)
{
    PlaylistWidgetData * data = audgui_list_get_user (widget);
    g_return_if_fail (data);
    data->list = list;
}

static void update_queue (GtkWidget * widget, PlaylistWidgetData * data)
{
    for (GList * node = data->queue; node; node = node->next)
        audgui_list_update_rows (widget, GPOINTER_TO_INT (node->data), 1);

    g_list_free (data->queue);
    data->queue = NULL;

    for (gint i = aud_playlist_queue_count (data->list); i --; )
        data->queue = g_list_prepend (data->queue, GINT_TO_POINTER
         (aud_playlist_queue_get_entry (data->list, i)));

    for (GList * node = data->queue; node; node = node->next)
        audgui_list_update_rows (widget, GPOINTER_TO_INT (node->data), 1);
}

void ui_playlist_widget_update (GtkWidget * widget, gint type, gint at,
 gint count)
{
    PlaylistWidgetData * data = audgui_list_get_user (widget);
    g_return_if_fail (data);

    if (type >= PLAYLIST_UPDATE_STRUCTURE)
    {
        gint diff = aud_playlist_entry_count (data->list) -
         audgui_list_row_count (widget);

        if (diff > 0)
            audgui_list_insert_rows (widget, at, diff);
        else if (diff < 0)
            audgui_list_delete_rows (widget, at, -diff);

        audgui_list_set_highlight (widget, aud_playlist_get_position (data->list));
    }

    if (type >= PLAYLIST_UPDATE_METADATA)
        audgui_list_update_rows (widget, at, count);

    audgui_list_update_selection (widget, at, count);
    update_queue (widget, data);
}
