TESTS = check_all check_db
check_PROGRAMS = check_all check_db

check_all_SOURCES = check_all.c \
                check_classifier.c $(top_builddir)/src/classifier.h \
                check_item_loader.c $(top_builddir)/src/item.h \
                $(top_builddir)/src/item_source.h \
                $(top_builddir)/src/file_item_source.h \
                check_pool.c $(top_builddir)/src/pool.h \
                check_tag.c $(top_builddir)/src/tag.h \
                check_random_background.c $(top_builddir)/src/random_background.h \
                check_clue.c $(top_builddir)/src/clue.h \
                check_samples.c $(top_builddir)/src/samples.h \
                check_queue.c $(top_builddir)/src/queue.h \
                check_config.c $(top_builddir)/src/config.h \
                $(top_builddir)/src/tagging.h
                
check_all_CFLAGS = @CHECK_CFLAGS@
check_all_LDADD = @CHECK_LIBS@  \
              $(top_builddir)/src/libclassifier.la
              
check_db_SOURCES = check_db.c                    \
            check_db_item_source.c               \
            check_tag_db.c                       \
            check_tagging_store.c                \
            $(top_builddir)/src/db_item_source.h \
            $(top_builddir)/src/item.h           \
            $(top_builddir)/src/item_source.h    \
            $(top_builddir)/src/tag.h            \
            $(top_builddir)/src/tagging.h
              
check_db_CFLAGS = @CHECK_CFLAGS@
check_db_LDADD = @CHECK_LIBS@  \
            $(top_builddir)/src/libclassifier.la
