lib_LTLIBRARIES = libclassifier.la libitem.la libpool.la libtag.la

libclassifier_la_SOURCES = classifier.c classifier.h
libitem_la_SOURCES = item.c item.h logging.h logging.c
libpool_la_SOURCES = pool.c pool.h logging.h logging.c
libtag_la_SOURCES  = tag.c tag.h logging.c logging.h

bin_PROGRAMS = classifier
classifier_SOURCES = main.c
classifier_LDADD = libclassifier.la

