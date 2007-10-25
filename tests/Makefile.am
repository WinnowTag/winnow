TESTS = check_classifier check_item_loader
check_PROGRAMS = check_classifier check_item_loader

check_classifier_SOURCES = check_classifier.c $(top_builddir)/src/classifier.h
check_classifier_CFLAGS = @CHECK_CFLAGS@
check_classifier_LDADD = @CHECK_LIBS@ $(top_builddir)/src/libclassifier.la

check_item_loader_SOURCES = check_item_loader.c $(top_builddir)/src/item_loader.h
check_item_loader_CFLAGS = @CHECK_CFLAGS@
check_item_loader_LDADD = @CHECK_LIBS@ $(top_builddir)/src/libitem_loader.la
