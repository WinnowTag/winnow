TESTS = check_all
check_PROGRAMS = check_all

check_all_SOURCES = check_all.c \
                check_classifier.c $(top_builddir)/src/classifier.h \
                check_item_loader.c $(top_builddir)/src/item.h \
                check_pool.c $(top_builddir)/src/pool.h \
                check_tag.c $(top_builddir)/src/tag.h \
                check_random_background.c $(top_builddir)/src/random_background.h
                
check_all_CFLAGS = @CHECK_CFLAGS@
check_all_LDADD = @CHECK_LIBS@  \
              $(top_builddir)/src/libclassifier.la