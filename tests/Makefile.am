TESTS =  check_tagger_builder check_train_tagger check_precompute_tagger  check_tag_index \
         check_classifier check_pool check_queue check_url_fetching check_clue \
         check_classify check_get_tagger check_item_cache check_classification_engine specs
CLEANFILES = http_test.log http_test_data.log test.log

LDFLAGS = -static @SQLITE3_LDFLAGS@ @CHECK_LIBS@
CFLAGS = -g -DDEBUG @SQLITE3_CFLAGS@ @CHECK_CFLAGS@
LDADD =  $(top_builddir)/src/libclassifier.la $(top_builddir)/judy/src/obj/libJudy.la
check_PROGRAMS = check_classifier check_pool check_queue check_item_cache \
                 check_classification_engine check_clue check_url_fetching  \
                 check_tagger_builder check_train_tagger check_precompute_tagger \
                 check_classify check_get_tagger check_tag_index

shared_SOURCES = assertions.h mock_items.h fixtures.h read_document.h
check_classifier_SOURCES = check_classifier.c $(top_builddir)/src/classifier.h $(shared_SOURCES)
check_pool_SOURCES       = check_pool.c $(shared_SOURCES)
check_queue_SOURCES      = check_queue.c $(shared_SOURCES)
check_clue_SOURCES       = check_clue.c $(top_builddir)/src/clue.h $(shared_SOURCES)
check_item_cache_SOURCES = check_item_cache.c $(top_builddir)/src/item_cache.h $(shared_SOURCES)
check_classification_engine_SOURCES = check_classification_engine.c $(top_builddir)/src/classification_engine.h $(shared_SOURCES)
check_url_fetching_SOURCES = check_url_fetching.c $(top_builddir)/src/fetch_url.h $(shared_SOURCES)
check_tagger_builder_SOURCES = check_tagger_builder.c $(top_builddir)/src/tagger.h $(shared_SOURCES)
check_train_tagger_SOURCES = check_train_tagger.c $(top_builddir)/src/tagger.h $(shared_SOURCES)
check_precompute_tagger_SOURCE = check_precompute_tagger.c $(top_builddir)/src/tagger.h $(top_builddir)/src/classifier.h $(shared_SOURCES)
check_classify_SOURCE = check_classify.c $(top_builddir)/src/tagger.h $(top_builddir)/src/classifier.h $(shared_SOURCES)
check_get_tagger_SOURCE = check_get_tagger.c $(top_builddir)/src/tagger.h $(top_builddir)/src/classifier.h $(shared_SOURCES)
check_tag_index_SOURCE = check_tag_index.c $(top_builddir)/src/tagger.h $(shared_SOURCES)

dist_check_DATA = fixtures conf spec.opts
dist_check_SCRIPTS = specs about_spec.rb  \
                      job_control_spec.rb \
                      memory_spec.rb      \
                      tag_index_spec.rb   \
                      item_cache_spec.rb  \
                      job_processing_spec.rb \
                      spec_helper.rb      \
                      test_http_server.rb
