lib_LTLIBRARIES = libclassifier.la libitem.la libpool.la
libclassifier_la_SOURCES = classifier.c classifier.h
libitem_la_SOURCES = item.c item.h
libpool_la_SOURCES = pool.c pool.h

bin_PROGRAMS = classifier
classifier_SOURCES = main.c
classifier_LDADD = libclassifier.la

