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
                           cls_config.c cls_config.h
bin_PROGRAMS = cls_bench
cls_bench_SOURCES = bench.c
cls_bench_LDADD = libclassifier.la
