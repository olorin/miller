noinst_LTLIBRARIES=	liboutput.la
liboutput_la_SOURCES=	\
			lrec_writer.h \
			lrec_writer_csv.c \
			lrec_writer_csvlite.c \
			lrec_writer_dkvp.c \
			lrec_writer_nidx.c \
			lrec_writer_pprint.c \
			lrec_writer_xtab.c \
			lrec_writers.h
liboutput_la_LIBADD=	../lib/libmlr.la
liboutput_la_CPPFLAGS=	-I${srcdir}/../
liboutput_la_CFLAGS=	-std=gnu99
