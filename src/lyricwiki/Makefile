PLUGIN = lyricwiki${PLUGIN_SUFFIX}

SRCS = lyricwiki.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${XML_CFLAGS} -I../..
LIBS += ${GTK_LIBS} ${GLIB_LIBS}  ${XML_LIBS}
