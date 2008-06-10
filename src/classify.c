/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#include <config.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include "git_revision.h"
#include "logging.h"
#include "classification_engine.h"
#include "httpd.h"
#include "misc.h"
#include "feature_extractor.h"
#include "fetch_url.h"

void print_help() {
	printf("Peerworks Classifier\n\n");
	printf("This version of the classifier will classify a\n single tag file or directory of tag files.\n\n");
	printf("Usage: classify <item_cache> <tag-file-url-or-directory>\n");
}

#define SHORT_OPTS "hv:"

static ItemCacheOptions item_cache_options = {60, 1000, 50};
static ItemCache *item_cache;
static TaggerCacheOptions tagger_cache_options;
static TaggerCache *tagger_cache;
static ClassificationEngineOptions ce_options = {1, 0.0, 10, NULL};
static ClassificationEngine *engine;

int start_classifier(char * corpus) {
	if (CLASSIFIER_OK != item_cache_create(&item_cache, corpus, &item_cache_options)) {
    fprintf(stderr, "Error opening classifier database file at %s: %s\n", corpus, item_cache_errmsg(item_cache));
    free_item_cache(item_cache);
    return EXIT_FAILURE;
  } else {
    item_cache_load(item_cache);
    // item_cache_set_feature_extractor(item_cache, tokenize_entry, tokenizer_url);
    // item_cache_start_feature_extractor(item_cache);
    // item_cache_start_cache_updater(item_cache);

    tagger_cache = create_tagger_cache(item_cache, &tagger_cache_options);
    tagger_cache->tag_retriever = &fetch_url;
    tagger_cache->tag_index_retriever = &fetch_url;

    engine = create_classification_engine(item_cache, tagger_cache, &ce_options);
    return !ce_start(engine);
  }
}

int main(int argc, char ** argv) {
	int longindex;
	int opt;
	static struct option long_options[] = {
	      {"version", no_argument, 0, 'v'},
	      {"help", no_argument, 0, 'h'},
	      {0,0,0,0}
			};

	while (-1 != (opt = getopt_long(argc, argv, SHORT_OPTS, long_options, &longindex))) {
		switch (opt) {
		case 'h':
			print_help();
			return EXIT_SUCCESS;
		case 'v':
			printf("%s, build: %s\n", PACKAGE_STRING, git_revision);
			return EXIT_SUCCESS;
		}
	}

	if (argc != 3) {
		print_help();
	} else {
		char *item_cache = argv[1];
		char *tag = argv[2];
		initialize_logging("/dev/null");

		if (start_classifier(item_cache)) {
			printf("Error starting the classifier");
		} else {
			ce_add_classification_job(engine, tag);
			ce_stop(engine);
		}
	}

	return EXIT_SUCCESS;
}
