lib_LTLIBRARIES = libclassifier.la

libclassifier_la_SOURCES = classifier.c classifier.h \
                           item.c item.h             \
                           logging.h logging.c       \
                           pool.c pool.h             \
                           tag.c tag.h               \
                           random_background.c       \
                           random_background.h       \
                           clue.h clue.c             \
                           samples.c samples.h
bin_PROGRAMS = classifier
classifier_SOURCES = main.c
classifier_LDADD = libclassifier.la

