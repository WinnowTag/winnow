// Copyright (c) 2007 The Kaphan Foundation
//
// Possession of a copy of this file grants no permission or license
// to use, modify, or create derivate works.
//
// Please contact info@peerworks.org for further information.

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include "item.h"
#include "pool.h"
#include "random_background.h"
#include "samples.h"
#include "tag.h"
#include "classifier.h"

void do_classification(void);

ItemSource is;
Samples samples;
Pool random_background;
TagList tags;
int num_tags;
int current_tag;
float training_clocks = 0;
float precomputing_clocks = 0;
float classification_clocks = 0;

int main(int argc, char ** argv) {
  is = create_file_item_source("corpus");
  random_background = create_random_background_from_file(is, "corpus/random_background.txt");
  printf("About to load samples - this could take a while.\n");
  samples = load_samples(is, "corpus/samples.txt");
  const int sample_size = sample_size(samples);
  printf("Finished loading %d samples.\n", sample_size);
 
  tags = load_tags_from_file("corpus", "jedharris");
  num_tags = taglist_size(tags);
  current_tag = 1;
  printf("Loaded %d tags\n", num_tags);
 
  do_classification();
  
  printf("Training:       %.2f s, %.1f tags trained per second\n", 
    training_clocks / CLOCKS_PER_SEC, (float)num_tags / (training_clocks / CLOCKS_PER_SEC));
  printf("Precomputing:   %.2f s, %.1f precomputations per second\n",
    precomputing_clocks / CLOCKS_PER_SEC, (float)num_tags / (precomputing_clocks / CLOCKS_PER_SEC));
  printf("Classification: %.2f s, %.0f classifications per second\n", 
    classification_clocks / CLOCKS_PER_SEC, ((float)sample_size * num_tags) / (classification_clocks / CLOCKS_PER_SEC));
  
  free_taglist(tags);
  free_pool(random_background);
  const Item *items = sample_items(samples);
  int i;
  
  for (i = 0; i < sample_size; i++) {
    free_item(items[i]);
  }
  free_samples(samples);
  free_item_source(is);

  return 0;
}

void do_classification() {
  for (; current_tag <= num_tags; current_tag++) {
    Tag tag = taglist_tag_at(tags, current_tag);
    clock_t train_start = clock();
    TrainedClassifier trained = train(tag, is);
    clock_t train_end = clock();
    training_clocks += (train_end - train_start);
    
    Classifier classifier = precompute(trained, random_background);
    clock_t precompute_end = clock();
    precomputing_clocks += (precompute_end - train_end);
    
    const Item *items = sample_items(samples);
    const int sample_size = sample_size(samples);
    int i;

    clock_t cls_start = clock();
    for (i = 0; i < sample_size; i++) {
      Tagging tagging = classify(classifier, items[i]);
      //fprintf(stderr, "%s,%i,%.9f\n", tagging->tag_name, item_get_id(items[i]), tagging->strength);    
      free(tagging);
    }
    clock_t cls_end = clock();
    classification_clocks += (cls_end - cls_start);
  
    free_classifier(classifier);  
    tc_free(trained);
  }
}
