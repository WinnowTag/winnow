lib_LTLIBRARIES = libclassifier.la

# Force path to be $(libdir) on all linkages.
# This seems to fix a problem on Linux with:
#   - automake 1.9.5
#   - autoconf 2.61
#   - libtool  1.5.14
# where the -rpth would get set to the mysql library
# path and nothing in /usr/local/lib would get found.
#
# This might be fixed in later versions, I guess we will
# see as we deploy to more sites.
#
LDFLAGS = -rpath $(libdir) @SQLITE3_LDFLAGS@
CFLAGS = -Wall @SQLITE3_CFLAGS@

libclassifier_la_SOURCES = classifier.c classifier.h \
                           logging.h logging.c       \
                           pool.c                    \
                           tag.c tag.h               \
                           clue.h clue.c             \
                           job_queue.c job_queue.h   \
                           cls_config.c cls_config.h \
                           tagging.h tagging.c       \
                           classification_engine.h   \
                           classification_engine.c   \
                           httpd.h httpd.c           \
                           misc.h                    \
                           time_helper.h             \
                           time_helper.c						 \
                           create_file.c             \
                           item_cache.c      				 \
                           item_cache.h				       \
                           uuid.h                    \
                           svnversion.h              \
                           feature_extractor.c       \
                           feature_extractor.h       \
                           xml.c xml.h

libclassifier_la_LIBADD = @LTLIBOBJS@
       
BUILT_SOURCES=svnversion.h
CLEANFILES=svnversion.h
bin_PROGRAMS = classifier
classifier_SOURCES = svnversion.h main.c 
classifier_LDADD = libclassifier.la

#cls_bench_SOURCES = bench.c
#cls_bench_LDADD = libclassifier.la

if DEBUG
classifier_CFLAGS = -g -D_DEBUG
classifier_LDFLAGS = -static
libclassifier_la_CFLAGS = -g -D_DEBUG
endif

# noinst_PROGRAMS = cls_bench

svnversion.h:
	echo "static const char *svn_version = \"`svnversion`\";" > svnversion.h


