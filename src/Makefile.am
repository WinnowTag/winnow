lib_LTLIBRARIES = libwinnow.la

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
LDFLAGS = -rpath $(libdir) @SQLITE3_LDFLAGS@ @CURL_LIBS@
CFLAGS = -Wall @SQLITE3_CFLAGS@ @CURL_CFLAGS@ 

libwinnow_la_SOURCES = classifier.c classifier.h \
                           logging.h logging.c       \
                           pool.c                    \
                           clue.h clue.c             \
                           job_queue.c job_queue.h   \
                           classification_engine.h   \
                           classification_engine.c   \
                           httpd.h httpd.c http_responses.h  \
                           misc.h                    \
                           create_file.c             \
                           item_cache.c      				 \
                           item_cache.h				       \
                           uuid.h                    \
                           xml.c xml.h \
                           tagger.c tagger.h tagger_cache.c tagging.c tag_index.c \
                           array.h array.c \
                           fetch_url.h \
                           xml_error_functions.h \
                           curl_response.h \
                           hmac.c hmac_internal.h hmac_sign.h hmac_auth.h hmac_credentials.h \
                           buffer.c buffer.h \
                           tokenizer.h tokenizer.c

libwinnow_la_LIBADD = @LTLIBOBJS@

bin_PROGRAMS = winnow classify
winnow_SOURCES = main.c 
winnow_LDADD = libwinnow.la

classify_SOURCES =classify.c 
classify_LDADD = libwinnow.la

#cls_bench_SOURCES = bench.c
#cls_bench_LDADD = libwinnow.la

if DEBUG
winnow_CFLAGS = -g3 -D_DEBUG -gdwarf-2
winnow_LDFLAGS = -static
libwinnow_la_CFLAGS = -g3 -D_DEBUG -gdwarf-2
endif

# noinst_PROGRAMS = cls_bench



