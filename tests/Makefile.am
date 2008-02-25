TESTS = check_all check_db check_http
CLEANFILES = http_test.log http_test_data.log test.log

LDFLAGS = -static @SQLITE3_LDFLAGS@
CFLAGS = -g -DDEBUG @SQLITE3_CFLAGS@
check_PROGRAMS = check_all check_db check_http 

check_http_SOURCE = check_http.c
check_http_LDADD  = @CHECK_LIBS@ $(top_builddir)/src/libclassifier.la

check_all_SOURCES = check_all.c \
                check_classifier.c $(top_builddir)/src/classifier.h \
                check_pool.c \
                check_tag.c $(top_builddir)/src/tag.h \
                check_clue.c $(top_builddir)/src/clue.h \
                check_queue.c $(top_builddir)/src/job_queue.h \
                check_config.c $(top_builddir)/src/cls_config.h \
                check_item_cache.c $(top_builddir)/src/item_cache.h \
                $(top_builddir)/src/tagging.h \
                assertions.h  \
                mock_item_source.h \
                tagging_store_fixtures.h \
                fixtures.h
                
                
check_all_CFLAGS = @CHECK_CFLAGS@
check_all_LDADD = @CHECK_LIBS@  \
              $(top_builddir)/src/libclassifier.la
              
check_db_SOURCES = check_db.c                    \
            check_tag_db.c                       \
            check_tagging_store.c                \
            check_classification_engine.c        \
            $(top_builddir)/src/classification_engine.h \
            $(top_builddir)/src/item.h           \
            $(top_builddir)/src/item_source.h    \
            $(top_builddir)/src/tag.h            \
            $(top_builddir)/src/tagging.h
              
check_db_CFLAGS = @CHECK_CFLAGS@
check_db_LDADD = @CHECK_LIBS@  \
            $(top_builddir)/src/libclassifier.la 


dist_check_DATA = fixtures conf
