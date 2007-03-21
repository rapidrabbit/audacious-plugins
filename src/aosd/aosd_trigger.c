/*
*
* Author: Giacomo Lozito <james@develia.org>, (C) 2005-2007
*
* This program is free software; you can redistribute it and/or modify it
* under the terms of the GNU General Public License as published by the
* Free Software Foundation; either version 2 of the License, or (at your
* option) any later version.
*
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* General Public License for more details.
*
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*
*/

#include "aosd_trigger.h"
#include "aosd_trigger_private.h"
#include "aosd_cfg.h"
#include <glib.h>
#include <audacious/i18n.h>
#include <audacious/playlist.h>
#include <audacious/strings.h>
#include <audacious/hook.h>


extern aosd_cfg_t * global_config;


/* trigger codes ( the code values do not need to be sequential ) */
enum
{
  AOSD_TRIGGER_PB_START = 0,
  AOSD_TRIGGER_PB_TITLECHANGE = 1,
  AOSD_TRIGGER_VOL_CHANGE = 2
};

/* trigger codes array size */
#define AOSD_TRIGGER_CODES_ARRAY_SIZE 3

/* trigger codes array */
gint aosd_trigger_codes[] =
{
  AOSD_TRIGGER_PB_START,
  AOSD_TRIGGER_PB_TITLECHANGE,
  AOSD_TRIGGER_VOL_CHANGE
};

/* prototypes of trigger functions */
static void aosd_trigger_func_pb_start_onoff ( gboolean );
static void aosd_trigger_func_pb_start_cb ( gpointer , gpointer );
static void aosd_trigger_func_pb_titlechange_onoff ( gboolean );
static void aosd_trigger_func_pb_titlechange_cb ( gpointer , gpointer );
static void aosd_trigger_func_vol_change_onoff ( gboolean );
static void aosd_trigger_func_vol_change_cb ( gpointer , gpointer );

/* map trigger codes to trigger objects */
aosd_trigger_t aosd_triggers[] =
{
  [AOSD_TRIGGER_PB_START] = { N_("Playback Start") ,
                              N_("Triggers OSD when a playlist entry is played.") ,
                              aosd_trigger_func_pb_start_onoff ,
                              aosd_trigger_func_pb_start_cb },

  [AOSD_TRIGGER_PB_TITLECHANGE] = { N_("Title Change") ,
                                    N_("Triggers OSD when, during playback, the song title changes "
                                       "but the filename is the same. This is mostly useful to display "
                                       "title changes in internet streams.") ,
                                    aosd_trigger_func_pb_titlechange_onoff ,
                                    aosd_trigger_func_pb_titlechange_cb },

  [AOSD_TRIGGER_VOL_CHANGE] = { N_("Volume Change") ,
                                N_("Triggers OSD when volume is changed.") ,
                                aosd_trigger_func_vol_change_onoff ,
                                aosd_trigger_func_vol_change_cb }
};



/* TRIGGER API */

void
aosd_trigger_get_codes_array ( gint ** array , gint * array_size )
{
  *array = aosd_trigger_codes;
  *array_size = AOSD_TRIGGER_CODES_ARRAY_SIZE;
  return;
}


const gchar *
aosd_trigger_get_name ( gint trig_code )
{
  return aosd_triggers[trig_code].name;
}


const gchar *
aosd_trigger_get_desc ( gint trig_code )
{
  return aosd_triggers[trig_code].desc;
}


void
aosd_trigger_start ( aosd_cfg_osd_trigger_t * cfg_trigger )
{
  gint i = 0;
  for ( i = 0 ; i < cfg_trigger->active->len ; i++ )
  {
    gint trig_code = g_array_index( cfg_trigger->active , gint , i );
    aosd_triggers[trig_code].onoff_func( TRUE );
  }
  return;
}


void
aosd_trigger_stop ( aosd_cfg_osd_trigger_t * cfg_trigger )
{
  gint i = 0;
  for ( i = 0 ; i < cfg_trigger->active->len ; i++ )
  {
    gint trig_code = g_array_index( cfg_trigger->active , gint , i );
    aosd_triggers[trig_code].onoff_func( FALSE );
  }
  return;
}


/* TRIGGER FUNCTIONS */

static void
aosd_trigger_func_pb_start_onoff ( gboolean turn_on )
{
  if ( turn_on == TRUE )
    hook_associate( "playback begin" , aosd_trigger_func_pb_start_cb , NULL );
  else
    hook_dissociate( "playback begin" , aosd_trigger_func_pb_start_cb );
  return;
}

static void
aosd_trigger_func_pb_start_cb ( gpointer plentry_gp , gpointer unused )
{
  PlaylistEntry *pl_entry = plentry_gp;
  if ( plentry_gp != NULL )
  {
    gchar *title;
    if ( pl_entry->title != NULL )
    {
      /* if there is a proper title, use it */
      title = g_strdup(pl_entry->title);
    }
    else
    {
      /* pick what we have as song title */
      Playlist *active = playlist_get_active();
      gint pos = playlist_get_position(active);
      title = playlist_get_songtitle(active, pos);
    }
    gchar *utf8_title = g_strdup( title );
    if ( g_utf8_validate( utf8_title , -1 , NULL ) == TRUE )
    {
      gchar *utf8_title_markup = g_markup_printf_escaped(
        "<span font_desc='%s'>%s</span>" , global_config->osd->text.fonts_name[0] , utf8_title );
      aosd_osd_display( utf8_title_markup , global_config->osd , FALSE );
      g_free( utf8_title_markup );
    }
    g_free( utf8_title );
    g_free( title );
  }
  return;
}



typedef struct
{
  gchar *title;
  gchar *filename;
}
aosd_pb_titlechange_prevs_t;


static void
aosd_trigger_func_pb_titlechange_onoff ( gboolean turn_on )
{
  static aosd_pb_titlechange_prevs_t *prevs = NULL;

  if ( turn_on == TRUE )
  {
    prevs = g_malloc0(sizeof(aosd_pb_titlechange_prevs_t));
    prevs->title = NULL;
    prevs->filename = NULL;
    hook_associate( "playlist set info" , aosd_trigger_func_pb_titlechange_cb , prevs );
  }
  else
  {
    hook_dissociate( "playlist set info" , aosd_trigger_func_pb_titlechange_cb );
    if ( prevs != NULL )
    {
      if ( prevs->title != NULL ) g_free( prevs->title );
      if ( prevs->filename != NULL ) g_free( prevs->filename );
      g_free( prevs );
      prevs = NULL;
    }
  }
  return;
}

static void
aosd_trigger_func_pb_titlechange_cb ( gpointer plentry_gp , gpointer prevs_gp )
{
  if ( ip_data.playing )
  {
    aosd_pb_titlechange_prevs_t *prevs = prevs_gp;
    PlaylistEntry *pl_entry = plentry_gp;

    /* same filename but title changed, useful to detect http stream song changes */

    if ( ( prevs->title != NULL ) && ( prevs->filename != NULL ) )
    {
      if ( ( pl_entry->filename != NULL ) && ( !strcmp(pl_entry->filename,prevs->filename) ) )
      {
        if ( ( pl_entry->title != NULL ) && ( strcmp(pl_entry->title,prevs->title) ) )
        {
          /* string formatting is done here a.t.m. - TODO - improve this area */
          gchar *utf8_title = g_strdup( pl_entry->title );
          if ( g_utf8_validate( utf8_title , -1 , NULL ) == TRUE )
          {
            gchar *utf8_title_markup = g_markup_printf_escaped(
              "<span font_desc='%s'>%s</span>" , global_config->osd->text.fonts_name[0] , utf8_title );
            aosd_osd_display( utf8_title_markup , global_config->osd , FALSE );
            g_free( utf8_title_markup );
          }
          g_free( utf8_title );
          g_free( prevs->title );
          prevs->title = g_strdup(pl_entry->title);
        }
      }
      else
      {
        g_free(prevs->filename);
        prevs->filename = g_strdup(pl_entry->filename);
        /* if filename changes, reset title as well */
        if ( prevs->title != NULL )
          g_free(prevs->title);
        prevs->title = g_strdup(pl_entry->title);
      }
    }
    else
    {
      if ( prevs->title != NULL )
        g_free(prevs->title);
      prevs->title = g_strdup(pl_entry->title);
      if ( prevs->filename != NULL )
        g_free(prevs->filename);
      prevs->filename = g_strdup(pl_entry->filename);
    }
  }
}


static void
aosd_trigger_func_vol_change_onoff ( gboolean turn_on )
{
  if ( turn_on == TRUE )
    hook_associate( "volume set" , aosd_trigger_func_vol_change_cb , NULL );
  else
    hook_dissociate( "volume set" , aosd_trigger_func_vol_change_cb );
  return;
}

typedef struct
{
  gint h_vol[2];
  gint sid;
}
aosd_vol_change_bucket_t;

static gboolean
aosd_trigger_func_vol_change_timeout ( gpointer bucket_gp )
{
  aosd_vol_change_bucket_t *bucket = bucket_gp;
  gchar *utf8_title_markup = g_markup_printf_escaped(
    "<span font_desc='%s'>Volume Change - L: %i , R: %i</span>" ,
    global_config->osd->text.fonts_name[0] , bucket->h_vol[0] , bucket->h_vol[1] );
  aosd_osd_display( utf8_title_markup , global_config->osd , FALSE );
  g_free( utf8_title_markup );
  bucket->sid = 0; /* reset source id value */
  return FALSE;
}

static void
aosd_trigger_func_vol_change_cb ( gpointer h_vol_gp , gpointer unused )
{
  gint *h_vol = h_vol_gp;
  static aosd_vol_change_bucket_t bucket = { { 0 , 0 } , 0 };
  
  bucket.h_vol[0] = h_vol[0];
  bucket.h_vol[1] = h_vol[1];
  
  /* in order to avoid repeated display of osd for each volume variation, use a
     timer to prevent it from appearing more than once when multiple volume
     changes are performed in a short time interval (500 msec) */
  if ( bucket.sid == 0 )
  {
    /* first call in the time interval */
    bucket.sid = g_timeout_add( 500 , aosd_trigger_func_vol_change_timeout , &bucket );
  }
  else
  {
    /* another call in the same interval, reset the interval */
    g_source_remove( bucket.sid );
    bucket.sid = g_timeout_add( 500 , aosd_trigger_func_vol_change_timeout , &bucket );
  }
  return;
}
