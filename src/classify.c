// General info: http://doc.winnowtag.org/open-source
// Source code repository: http://github.com/winnowtag
// Questions and feedback: contact@winnowtag.org
//
// Copyright (c) 2007-2011 The Kaphan Foundation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

// contact@winnowtag.org

#include <config.h>
#include <stdlib.h>
#include <dirent.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include "logging.h"
#include "classification_engine.h"
#include "httpd.h"
#include "misc.h"
#include "fetch_url.h"

static void print_help() {
	printf("Peerworks Classifier\n\n");
	printf("This version of the classifier will classify a\n single tag file or directory of tag files.\n\n");
	printf("Usage: classify <item_cache> <tag-file-url-or-directory>\n");
}

#define SHORT_OPTS "hv:"

static ItemCacheOptions item_cache_options = {60, 1000, 0};
static ItemCache *item_cache;
static TaggerCacheOptions tagger_cache_options;
static TaggerCache *tagger_cache;
static ClassificationEngineOptions ce_options = {4, 0.9, NULL};
static ClassificationEngine *engine;

static int is_url(const char *path) {
  return path == strstr(path, "file:") || path == strstr(path, "http:");
}

static int start_classifier(char * corpus) {
	if (CLASSIFIER_OK != item_cache_create(&item_cache, corpus, &item_cache_options)) {
    fprintf(stderr, "Error opening classifier database file at %s: %s\n", corpus, item_cache_errmsg(item_cache));
    free_item_cache(item_cache);
    return EXIT_FAILURE;
  } else {
    item_cache_load(item_cache);
    tagger_cache = create_tagger_cache(item_cache, &tagger_cache_options);
    tagger_cache->tag_retriever = &fetch_url;
    tagger_cache->tag_index_retriever = &fetch_url;

    engine = create_classification_engine(item_cache, tagger_cache, &ce_options);
    return !ce_start(engine);
  }
}

static int classify_file(ClassificationEngine * engine, const char * tag) {
  ClassificationJob *job = ce_add_classification_job(engine, tag);
  ce_stop(engine);
  if (job->state != CJOB_STATE_COMPLETE) {
    char buffer[512];
    cjob_error_msg(job, buffer, 512);
    printf("Job not complete: %s\n", buffer);
    return EXIT_FAILURE;
  }
  
  return EXIT_SUCCESS;
}

static int select_atom_files(struct dirent * entry) {
  int length = strlen(entry->d_name);
  return length > 5 && !strcmp(".atom", &entry->d_name[length - 5]);
}

static int classify_directory(ClassificationEngine * engine, const char * directory) {
  struct dirent **entries;  
  int num_entries = scandir(directory, &entries, select_atom_files, alphasort);
  
  if (num_entries > -1) {
    int i;    
    
    for (i = 0; i < num_entries; i++) {
      char buffer[MAXPATHLEN];
      snprintf(buffer, MAXPATHLEN, "file:%s/%s", directory, entries[i]->d_name);
      ce_add_classification_job(engine, buffer);
      free(entries[i]);
    }
    free(entries);
    ce_stop(engine);
    
    return EXIT_SUCCESS;
  }
  
  return EXIT_FAILURE;
}

int main(int argc, char ** argv) {
  int exit_code = EXIT_SUCCESS;
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
			printf("%s\n", PACKAGE_STRING);
			return EXIT_SUCCESS;
		}
	}

	if (argc != 3) {
		print_help();
	} else {
		char *item_cache = argv[1];
		char *tag = argv[2];
    char real_tag[PATH_MAX];
		//initialize_logging("/dev/null");
    
    if (is_url(tag)) {
      strncpy(real_tag, tag, PATH_MAX);
    } else if (NULL == realpath(tag, real_tag)) {
      fprintf(stderr, "Could not find path %s: %s", tag, strerror(errno));
      return EXIT_FAILURE;
    }

		if (start_classifier(item_cache)) {
			printf("Error starting the classifier");
		} else {
      exit_code = EXIT_FAILURE == classify_directory(engine, real_tag) ? classify_file(engine, real_tag) : EXIT_SUCCESS;
		}
	}

	return exit_code;
}
