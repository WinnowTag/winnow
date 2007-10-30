TESTS = check_classifier check_item_loader check_pool
check_PROGRAMS = check_classifier check_item_loader check_pool

check_classifier_SOURCES = check_classifier.c $(top_builddir)/src/classifier.h
check_classifier_CFLAGS = @CHECK_CFLAGS@
check_classifier_LDADD = @CHECK_LIBS@ $(top_builddir)/src/libclassifier.la

check_item_loader_SOURCES = check_item_loader.c $(top_builddir)/src/item.h
check_item_loader_CFLAGS = @CHECK_CFLAGS@
check_item_loader_LDADD = @CHECK_LIBS@ $(top_builddir)/src/libitem.la

check_pool_SOURCES = check_pool.c $(top_builddir)/src/pool.h
check_pool_CFLAGS = @CHECK_CFLAGS@
check_pool_LDADD = @CHECK_LIBS@ $(top_builddir)/src/libpool.la $(top_builddir)/src/libitem.la
