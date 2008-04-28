TESTS =  check_tagger_builder check_pool check_train_tagger check_precompute_tagger \
          check_classifier check_pool check_queue check_url_fetching check_clue \
          check_classify check_get_tagger check_item_cache check_classification_engine

#           check_config \
         # check_http    \
         #  check_classification_engine specs
CLEANFILES = http_test.log http_test_data.log test.log

LDFLAGS = -static @SQLITE3_LDFLAGS@ @CHECK_LIBS@
CFLAGS = -g -DDEBUG @SQLITE3_CFLAGS@ @CHECK_CFLAGS@
LDADD =  $(top_builddir)/src/libclassifier.la 
check_PROGRAMS = check_classifier check_pool check_queue check_config check_item_cache \
                 check_classification_engine check_clue check_url_fetching  \
                 check_tagger_builder check_train_tagger check_precompute_tagger \
                 check_classify check_get_tagger

check_classifier_SOURCES = check_classifier.c $(top_builddir)/src/classifier.h
check_pool_SOURCES       = check_pool.c
check_queue_SOURCES      = check_queue.c
check_clue_SOURCES       = check_clue.c $(top_builddir)/src/clue.h
check_config_SOURCES     = check_config.c $(top_builddir)/src/cls_config.h
check_item_cache_SOURCES = check_item_cache.c $(top_builddir)/src/item_cache.h
# check_http_SOURCE        = check_http.c
check_classification_engine_SOURCES = check_classification_engine.c $(top_builddir)/src/classification_engine.h
check_url_fetching_SOURCES = check_url_fetching.c $(top_builddir)/src/fetch_url.h
check_tagger_builder_SOURCES = check_tagger_builder.c $(top_builddir)/src/tagger.h
check_train_tagger_SOURCES = check_train_tagger.c $(top_builddir)/src/tagger.h
check_precompute_tagger_SOURCE = check_precompute_tagger.c $(top_builddir)/src/tagger.h $(top_builddir)/src/classifier.h
check_classify_SOURCE = check_classify.c $(top_builddir)/src/tagger.h $(top_builddir)/src/classifier.h
check_get_tagger_SOURCE = check_get_tagger.c $(top_builddir)/src/tagger.h $(top_builddir)/src/classifier.h

dist_check_DATA = fixtures conf spec.opts
dist_check_SCRIPTS = specs item_cache_spec.rb memory_spec.rb spec_helper.rb