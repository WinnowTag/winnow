lib_LTLIBRARIES = libclassifier.la

libclassifier_la_SOURCES = classifier.c classifier.h \
                           item.c item.h             \
                           item_source.c             \
                           item_source.h             \
                           file_item_source.c        \
                           file_item_source.h        \
                           logging.h logging.c       \
                           pool.c pool.h             \
                           tag.c tag.h               \
                           random_background.c       \
                           random_background.h       \
                           clue.h clue.c             \
                           samples.c samples.h       \
                           job_queue.c job_queue.h   \
                           cls_config.c cls_config.h \
                           db_item_source.h          \
                           db_item_source.c          \
                           tagging.h tagging.c       \
                           classification_engine.h   \
                           classification_engine.c   \
                           httpd.h httpd.c           \
                           misc.h                    \
                           time_helper.h             \
                           time_helper.c
                           
bin_PROGRAMS = classifier
classifier_SOURCES = main.c
classifier_LDADD = libclassifier.la


cls_bench_SOURCES = bench.c
cls_bench_LDADD = libclassifier.la

if DEBUG
classifier_debug_SOURCES = main.c
classifier_debug_LDADD = libclassifier.la
classifier_debug_CFLAGS = -g -DDEBUG
classifier_debug_LDFLAGS = -static
DBG = classifier.debug
else !DEBUG
DBG = 
endif !DEBUG

noinst_PROGRAMS = cls_bench $(DBG)
