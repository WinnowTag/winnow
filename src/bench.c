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

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include "item_cache.h"
#include "tag.h"
#include "classifier.h"

void *do_classification(void *nothing);
Pool *random_background;
TagList *tags;
int num_tags;
int current_tag;
int threads_that_did_work = 0;
double training = 0;
double precomputing = 0;
double classification = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

double now() {
  struct timeval t;
  gettimeofday(&t, NULL);
  return t.tv_sec + (t.tv_usec / 1000000.0);
}

int main(int argc, char ** argv) {
//  int i;
//  int num_threads = 1;
//  if (argc == 2) {
//    int new_num_threads;
//    if (1 == sscanf(argv[1], "%d", &new_num_threads)) {
//      printf("Using %d threads\n", new_num_threads);
//      num_threads = new_num_threads;
//    }
//  }
//  
//  is = create_file_item_source("corpus");
//  random_background = create_random_background_from_file(is, "corpus/random_background.txt");
//  printf("About to load samples - this could take a while.\n");
//  samples = load_samples(is, "corpus/samples.txt");
//  const int sample_size = sample_size(samples);
//  printf("Finished loading %d samples.\n", sample_size);
// 
//  tags = load_tags_from_file("corpus", "jedharris");
//  num_tags = tags->size;
//  current_tag = 1;
//  printf("Loaded %d tags\n", num_tags);
// 
//  double start = now();
//  pthread_t threads[num_threads];
//  for (i = 0; i < num_threads; i++) {
//    pthread_create(&threads[i], NULL, do_classification, NULL);
//  }
//  
//  for (i = 0; i < num_threads; i++) {
//    pthread_join(threads[i], NULL);
//  }
//  double end = now();
//  
//  printf("Total (includes thread creation and memory management):\n");
//  printf("                %.2f s, %7.0f classifications / second\n",
//        end - start,  (sample_size * num_tags) / (end - start));
//  
//  training = training / threads_that_did_work;
//  precomputing = precomputing / threads_that_did_work;
//  classification = classification / threads_that_did_work;    
//  
//  printf("Training:       %.2f s, %7.1f    tags trained / second\n", 
//    training, (float)num_tags / (training));
//  printf("Precomputing:   %.2f s, %7.1f precomputations / second\n",
//    precomputing, (float)num_tags / (precomputing));
//  printf("Classification: %.2f s, %7.0f classifications / second\n", 
//    classification, 
//    ((float)sample_size * num_tags) / (classification));
//  
//  free_taglist(tags);
//  free_pool(random_background);
//  Item **items = sample_items(samples);
//  
//  for (i = 0; i < sample_size; i++) {
//    free_item(items[i]);
//  }
//  free_samples(samples);
//  free_item_source(is);

  return 0;
}
//
//void *do_classification(void *nothing) {
//  int did_work = 0;
//  double _training = 0;
//  float _precomputing = 0;
//  float _classification = 0;
//  
//  while (current_tag < num_tags) {
//    did_work = 1;
//    pthread_mutex_lock(&mutex);
//    const Tag *tag = tags->tags[current_tag];
//    current_tag++;
//    pthread_mutex_unlock(&mutex);
//    
//    double train_start = now();
//    TrainedClassifier *trained = train(tag, is);
//    double train_end = now();
//    _training += (train_end - train_start);
//    
//    Classifier *classifier = precompute(trained, random_background);
//    double precompute_end = now();
//    _precomputing += (precompute_end - train_end);
//    
//    Item **items = sample_items(samples);
//    const int sample_size = sample_size(samples);
//    int i;
//
//    double cls_start = now();
//    for (i = 0; i < sample_size; i++) {
//      Tagging *tagging = classify(classifier, items[i]);
//      //fprintf(stderr, "%s,%i,%.9f\n", tagging->tag_name, item_get_id(items[i]), tagging->strength);    
//      free(tagging);
//    }
//    double cls_end = now();
//    _classification += (cls_end - cls_start);
//  
//    free_classifier(classifier);  
//    tc_free(trained);
//  }
//  
//  if (did_work) {
//    pthread_mutex_lock(&mutex);
//      threads_that_did_work++;
//      training += _training;
//      precomputing += _precomputing;
//      classification += _classification;    
//    pthread_mutex_unlock(&mutex);
//  }
//  
//  return 0;
//}
