/* Copyright (c) 2008 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#include <check.h>
#include "fixtures.h"
#include <stdio.h>
#include "assertions.h"
#include "../src/item_cache.h"
#include "../src/misc.h"
#include "../src/item_cache.h"
#include "../src/logging.h"

START_TEST (creating_with_missing_db_file_fails) {
  ItemCache *item_cache;
  int rc = item_cache_create(&item_cache, "/tmp/missing.db");
  assert_equal(CLASSIFIER_FAIL, rc);
  const char *msg = item_cache_errmsg(item_cache);
  assert_equal_s("unable to open database file", msg);
} END_TEST

START_TEST (creating_with_empty_db_file_fails) {
  setup_fixture_path();
  ItemCache *item_cache;
  int rc = item_cache_create(&item_cache, "fixtures/empty.db");
  assert_equal(CLASSIFIER_FAIL, rc);
  const char *msg = item_cache_errmsg(item_cache); 
  assert_equal_s("Database file's user version does not match classifier version. Trying running classifier-db-migrate.", msg);
  teardown_fixture_path();
} END_TEST

START_TEST (create_with_valid_db) {
  setup_fixture_path();
  ItemCache *item_cache;
  int rc = item_cache_create(&item_cache, "fixtures/valid.db");
  assert_equal(CLASSIFIER_OK, rc);
  teardown_fixture_path();
} END_TEST

/* Tests for fetching an item */
ItemCache *item_cache;

static void setup_cache(void) {
  setup_fixture_path();
  item_cache_create(&item_cache, "fixtures/valid.db");
}

static void teardown_item_cache(void) {
  teardown_fixture_path();
  free_item_cache(item_cache);
}

START_TEST (test_fetch_item_returns_null_when_item_doesnt_exist) {
  Item *item = item_cache_fetch_item(item_cache, 111);
  assert_null(item);
  free_item(item);
} END_TEST

START_TEST (test_fetch_item_contains_item_id) {
  Item *item = item_cache_fetch_item(item_cache, 890806);
  assert_not_null(item);
  assert_equal(890806, item_get_id(item));
  free_item(item);
} END_TEST

START_TEST (test_fetch_item_contains_item_time) {
  Item *item = item_cache_fetch_item(item_cache, 890806);
  assert_not_null(item);
  assert_equal(1178551672, item_get_time(item));
  free_item(item);
} END_TEST

START_TEST (test_fetch_item_contains_the_right_number_of_tokens) {
 Item *item = item_cache_fetch_item(item_cache, 890806);
 assert_not_null(item);
 assert_equal(76, item_get_num_tokens(item));
 free_item(item);
} END_TEST

START_TEST (test_fetch_item_contains_the_right_frequency_for_a_given_token) {
  Item *item = item_cache_fetch_item(item_cache, 890806);
  assert_not_null(item);
  Token token;
  item_get_token(item, 9949, &token);
  assert_equal(9949, token.id);
  assert_equal(3, token.frequency);
} END_TEST

START_TEST (test_fetch_item_after_load) {
  item_cache_load(item_cache);
  Item *item = item_cache_fetch_item(item_cache, 890806);
  assert_not_null(item);
} END_TEST

START_TEST (test_fetch_item_after_load_contains_tokens) {
  item_cache_load(item_cache);
  Item *item = item_cache_fetch_item(item_cache, 890806);
  assert_not_null(item);
  assert_equal(76, item_get_num_tokens(item));
} END_TEST

/* Test loading the item cache */
START_TEST (test_load_loads_the_right_number_of_items) {
  int rc = item_cache_load(item_cache);
  assert_equal(CLASSIFIER_OK, rc);
  assert_equal(10, item_cache_cached_size(item_cache));
} END_TEST

START_TEST (test_load_sets_cache_loaded_to_true) {
  int rc = item_cache_load(item_cache);
  assert_equal(CLASSIFIER_OK, rc);
  assert_equal(true, item_cache_loaded(item_cache));
} END_TEST

/* Test iteration */
void setup_iteration(void) {
  setup_fixture_path();
  item_cache_create(&item_cache, "fixtures/valid.db");
  item_cache_load(item_cache);
}

void teardown_iteration(void) {
  teardown_fixture_path();
  free_item_cache(item_cache);
}

int iterates_over_all_items(const Item *item, void *memo) {
  if (item) {
    int *iteration_count = (int*) memo;
    (*iteration_count)++;
  }
  
  return CLASSIFIER_OK;
}

START_TEST (test_iterates_over_all_items) {
  int iteration_count = 0;
  item_cache_each_item(item_cache, iterates_over_all_items, &iteration_count);
  assert_equal(10, iteration_count);
} END_TEST

int cancels_iteration_when_it_returns_fail(const Item *item, void *memo) {
  int *iteration_count = (int*) memo;
  (*iteration_count)++;
  
  if (*iteration_count < 5) {
    return CLASSIFIER_OK;
  } else {
    return CLASSIFIER_FAIL;
  }
}

START_TEST (test_iteration_stops_when_iterator_returns_CLASSIFIER_FAIL) {
  int iteration_count = 0;
  item_cache_each_item(item_cache, cancels_iteration_when_it_returns_fail, &iteration_count);
  assert_equal(5, iteration_count);
} END_TEST

static int i = 0;

int stores_ids(const Item * item, void *memo) {
  int *ids = (int*) memo;
  ids[i++] = item_get_id(item);
  return CLASSIFIER_OK;
}

START_TEST (test_iteration_happens_in_reverse_updated_order) {
  i = 0;
  int ids[10];
  item_cache_each_item(item_cache, stores_ids, ids);
  assert_equal(709254, ids[0]);
  assert_equal(880389, ids[1]);
  assert_equal(888769, ids[2]);
  assert_equal(886643, ids[3]);
  assert_equal(890806, ids[4]);
  assert_equal(802739, ids[5]);
  assert_equal(884409, ids[6]);
  assert_equal(753459, ids[7]);
  assert_equal(878944, ids[8]);
  assert_equal(886294, ids[9]);
} END_TEST

/* Test RandomBackground */
START_TEST (test_random_background_is_null_before_load) {
  assert_null(item_cache_random_background(item_cache));
} END_TEST

START_TEST (test_creates_random_background_after_load) {
  item_cache_load(item_cache);
  assert_not_null(item_cache_random_background(item_cache));
} END_TEST

START_TEST (test_random_background_is_correct_size) {
  item_cache_load(item_cache);
  assert_equal(750, pool_num_tokens(item_cache_random_background(item_cache)));
} END_TEST

START_TEST (test_random_background_has_right_count_for_a_token) {
  item_cache_load(item_cache);
  const Pool *bg = item_cache_random_background(item_cache);
  assert_equal(2, pool_token_frequency(bg, 2515));
} END_TEST

/* Item Cache modification */
#include <sqlite3.h>

static void setup_modification(void) {
  setup_fixture_path();
  system("cp fixtures/valid.db /tmp/valid-copy.db");
  item_cache_create(&item_cache, "/tmp/valid-copy.db");
}

static void teardown_modification(void) {
  teardown_fixture_path();
  free_item_cache(item_cache);
}

START_TEST (test_adding_an_entry_stores_it_in_the_database) {
  ItemCacheEntry *entry = create_item_cache_entry(11, "id#11", "Entry 11", "Author 11",
                                        "http://example.org/11",
                                        "http://example.org/11.html",
                                        "http://example.org/11/spider",
                                        "<p>This is some content</p>",
                                        1178551600, 141, 1178551601);
  int rc = item_cache_add_entry(item_cache, entry);
  assert_equal(CLASSIFIER_OK, rc);
  Item *item = item_cache_fetch_item(item_cache, 11);
  assert_not_null(item);
  free_item(item);
} END_TEST

START_TEST (adding_an_entry_saves_all_its_attributes) {
  ItemCacheEntry *entry = create_item_cache_entry(11, "id#11", "Entry 11", "Author 11",
                                          "http://example.org/11",
                                          "http://example.org/11.html",
                                          "http://example.org/11/spider",
                                          "<p>This is some content</p>",
                                          1178551600, 141, 1178551601);
  int rc = item_cache_add_entry(item_cache, entry);
  assert_equal(CLASSIFIER_OK, rc);
  
  sqlite3 *db;
  sqlite3_stmt *stmt;
  sqlite3_open_v2("/tmp/valid-copy.db", &db, SQLITE_OPEN_READONLY, NULL);
  sqlite3_prepare_v2(db, "select * from entries where id = 11", -1, &stmt, NULL);
  if (SQLITE_ROW == sqlite3_step(stmt)) {
    assert_equal_s("id#11", sqlite3_column_text(stmt, 1));
    assert_equal_s("Entry 11", sqlite3_column_text(stmt, 2));
    assert_equal_s("Author 11", sqlite3_column_text(stmt, 3));
    assert_equal_s("http://example.org/11", sqlite3_column_text(stmt, 4));
    assert_equal_s("http://example.org/11.html", sqlite3_column_text(stmt, 5));
    assert_equal_s("http://example.org/11/spider", sqlite3_column_text(stmt, 6));
    assert_equal_s("<p>This is some content</p>", sqlite3_column_text(stmt, 7));
    assert_equal_f(2454228.14351852, sqlite3_column_double(stmt, 8));
    assert_equal(141, sqlite3_column_int(stmt, 9));
    assert_equal_f(2454228.14353009, sqlite3_column_double(stmt, 10));
  } else {
    fail("Could not get record");
  }
  
  sqlite3_close(db);
} END_TEST

START_TEST (adding_an_entry_twice_does_not_fail) {
  ItemCacheEntry *entry = create_item_cache_entry(11, "id#11", "Entry 11", "Author 11",
                                          "http://example.org/11",
                                          "http://example.org/11.html",
                                          "http://example.org/11/spider",
                                          "<p>This is some content</p>",
                                          1178551600, 141, 1178551601);
  int rc = item_cache_add_entry(item_cache, entry);
  assert_equal(CLASSIFIER_OK, rc);
  rc = item_cache_add_entry(item_cache, entry);
  assert_equal(CLASSIFIER_OK, rc);
} END_TEST

START_TEST (test_destroying_an_entry_removes_it_from_database) {
  Item *item = item_cache_fetch_item(item_cache, 753459);
  assert_not_null(item);
  free_item(item);
  
  int rc = item_cache_remove_entry(item_cache, 753459);
  assert_equal(CLASSIFIER_OK, rc);
  item = item_cache_fetch_item(item_cache, 753459);
  assert_null(item);  
} END_TEST

START_TEST (test_destroying_an_entry_removes_it_from_the_database_file) {
  int rc = item_cache_remove_entry(item_cache, 753459);
  assert_equal(CLASSIFIER_OK, rc);
  Item *item = item_cache_fetch_item(item_cache, 753459);
  assert_null(item);
    
  sqlite3 *db;
  sqlite3_stmt *stmt;
  sqlite3_open_v2("/tmp/valid-copy.db", &db, SQLITE_OPEN_READONLY, NULL);
  sqlite3_prepare_v2(db, "select * from entries where id = 753459", -1, &stmt, NULL);
  rc = sqlite3_step(stmt);
  assert_equal(SQLITE_DONE, rc);
  sqlite3_close(db);
} END_TEST

START_TEST (test_destroying_an_entry_removes_tokens_from_the_database_file) {
  int rc = item_cache_remove_entry(item_cache, 753459);
  assert_equal(CLASSIFIER_OK, rc); 
    
  sqlite3 *db;
  sqlite3_stmt *stmt;
  sqlite3_open_v2("/tmp/valid-copy.db", &db, SQLITE_OPEN_READONLY, NULL);
  sqlite3_prepare_v2(db, "select * from entry_tokens where entry_id = 753459", -1, &stmt, NULL);
  rc = sqlite3_step(stmt);
  assert_equal(SQLITE_DONE, rc);
  sqlite3_close(db);
} END_TEST

START_TEST (test_cant_delete_an_item_that_is_used_in_the_random_background) {
  int rc = item_cache_remove_entry(item_cache, 890806);
  assert_equal(ITEM_CACHE_ENTRY_PROTECTED, rc);
  
  sqlite3 *db;
  sqlite3_stmt *stmt;
  sqlite3_open_v2("/tmp/valid-copy.db", &db, SQLITE_OPEN_READONLY, NULL);
  sqlite3_prepare_v2(db, "select * from entries where id = 890806", -1, &stmt, NULL);
  rc = sqlite3_step(stmt);
  assert_equal(SQLITE_ROW, rc); 
  sqlite3_close(db);
} END_TEST

START_TEST (test_failed_deletion_doesnt_delete_tokens) {
  int rc = item_cache_remove_entry(item_cache, 890806);
  assert_equal(ITEM_CACHE_ENTRY_PROTECTED, rc); 
    
  sqlite3 *db;
  sqlite3_stmt *stmt;
  sqlite3_open_v2("/tmp/valid-copy.db", &db, SQLITE_OPEN_READONLY, NULL);
  sqlite3_prepare_v2(db, "select * from entry_tokens where entry_id = 890806", -1, &stmt, NULL);
  rc = sqlite3_step(stmt);
  assert_equal(SQLITE_ROW, rc);
  sqlite3_close(db);
} END_TEST

/* Feed addition */
START_TEST (test_add_feed_to_item_cache) {
  Feed *feed = create_feed(10, "Feed 10");
  int rc = item_cache_add_feed(item_cache, feed);
  assert_equal(CLASSIFIER_OK, rc);
  
  sqlite3 *db;
  sqlite3_stmt *stmt;
  sqlite3_open_v2("/tmp/valid-copy.db", &db, SQLITE_OPEN_READONLY, NULL);
  sqlite3_prepare_v2(db, "select * from feeds where id = 10", -1, &stmt, NULL);
  rc = sqlite3_step(stmt);
  assert_equal(SQLITE_ROW, rc);
  assert_equal_s("Feed 10", sqlite3_column_text(stmt, 1));
  sqlite3_close(db);
  free_feed(feed);
} END_TEST

START_TEST (test_add_feed_that_already_exists_updates_attributes) {
  Feed *feed = create_feed(141, "Feed 999");
  int rc = item_cache_add_feed(item_cache, feed);
  assert_equal(CLASSIFIER_OK, rc);
  
  sqlite3 *db;
  sqlite3_stmt *stmt;
  sqlite3_open_v2("/tmp/valid-copy.db", &db, SQLITE_OPEN_READONLY, NULL);
  sqlite3_prepare_v2(db, "select * from feeds where id = 141", -1, &stmt, NULL);
  rc = sqlite3_step(stmt);
  assert_equal(SQLITE_ROW, rc);
  assert_equal_s("Feed 999", sqlite3_column_text(stmt, 1));
  sqlite3_close(db);
  free_feed(feed);
} END_TEST

/* Feed deletion */
START_TEST (test_deleting_a_feed_removes_it_from_the_database) {
  int rc = item_cache_remove_feed(item_cache, 141);
  assert_equal(CLASSIFIER_OK, rc);
  
  sqlite3 *db;
  sqlite3_stmt *stmt;
  sqlite3_open_v2("/tmp/valid-copy.db", &db, SQLITE_OPEN_READONLY, NULL);
  sqlite3_prepare_v2(db, "select * from feeds where id = 141", -1, &stmt, NULL);
  rc = sqlite3_step(stmt);
  assert_equal(SQLITE_DONE, rc);
  
  sqlite3_close(db);
} END_TEST

START_TEST (test_deleting_a_feed_removes_its_entries_from_the_database) {
  int rc = item_cache_remove_feed(item_cache, 141);
  assert_equal(CLASSIFIER_OK, rc);
  
  sqlite3 *db;
  sqlite3_stmt *stmt;
  sqlite3_open_v2("/tmp/valid-copy.db", &db, SQLITE_OPEN_READONLY, NULL);
  sqlite3_prepare_v2(db, "select * from entries where feed_id = 141", -1, &stmt, NULL);
  rc = sqlite3_step(stmt);
  assert_equal(SQLITE_DONE, rc);
  
  sqlite3_close(db);
} END_TEST

START_TEST (test_adding_entry_causes_it_to_be_added_to_the_tokenization_queue) {
  ItemCacheEntry *entry = create_item_cache_entry(11, "id#11", "Entry 11", "Author 11",
                                            "http://example.org/11",
                                            "http://example.org/11.html",
                                            "http://example.org/11/spider",
                                            "<p>This is some content</p>",
                                            1178551600, 141, 1178551601);
  item_cache_add_entry(item_cache, entry);
  assert_equal(1, item_cache_feature_extraction_queue_size(item_cache));
} END_TEST

START_TEST (test_adding_entry_twice_causes_it_to_be_added_to_the_tokenization_queue_only_once) {
  ItemCacheEntry *entry = create_item_cache_entry(11, "id#11", "Entry 11", "Author 11",
                                            "http://example.org/11",
                                            "http://example.org/11.html",
                                            "http://example.org/11/spider",
                                            "<p>This is some content</p>",
                                            1178551600, 141, 1178551601);
  item_cache_add_entry(item_cache, entry);
  item_cache_add_entry(item_cache, entry);
  assert_equal(1, item_cache_feature_extraction_queue_size(item_cache));
} END_TEST

#include <sched.h>
int tokens[][2] = {1, 2, 3, 4, 5, 6, 7, 8};
Item *item;

static void setup_loaded_modification(void) {
  setup_fixture_path();
  system("cp fixtures/valid.db /tmp/valid-copy.db");
  item_cache_create(&item_cache, "/tmp/valid-copy.db");
  //item_cache_set_feature_extractor(item_cache, NULL);
  item_cache_load(item_cache);
  
  item = create_item_with_tokens_and_time(9, tokens, 4, (time_t) 1178683198L);
}

static void teardown_loaded_modification(void) {
  teardown_fixture_path();
  free_item_cache(item_cache);
}

START_TEST (test_add_item_to_in_memory_arrays_adds_an_item) {
  item_cache_add_item(item_cache, item);
  assert_equal(11, item_cache_cached_size(item_cache));
} END_TEST

START_TEST (test_add_item_makes_it_fetchable) {
  item_cache_add_item(item_cache, item);
  assert_equal(item, item_cache_fetch_item(item_cache, 9));  
} END_TEST

static int adding_item_iterator(const Item *iter_item, void *memo) {
  if (iter_item == item) {
    int *found = (int*) memo;
    *found = true;
  }
  
  return CLASSIFIER_OK;
}

START_TEST (test_add_item_makes_it_iteratable) {
  item_cache_add_item(item_cache, item);
  int found = false;
  item_cache_each_item(item_cache, adding_item_iterator, &found);
  assert_equal(true, found);
} END_TEST

static int adding_item_position_count(const Item *iter_item, void *memo) {
  int *position = (int*) memo;
  (*position)++;
  
  if (iter_item == item) {
    return CLASSIFIER_FAIL;
  }
  
  return CLASSIFIER_OK;
}

START_TEST (test_add_item_puts_it_in_the_right_position) {
  item_cache_add_item(item_cache, item);
  int position = 0;
  item_cache_each_item(item_cache, adding_item_position_count, &position);
  assert_equal(4, position);
} END_TEST

START_TEST (test_add_item_puts_it_in_the_right_position_at_beginning) {
  item = create_item_with_tokens_and_time(9, tokens, 4, (time_t) 1179051840L);
  item_cache_add_item(item_cache, item);
  int position = 0;
  item_cache_each_item(item_cache, adding_item_position_count, &position);
  assert_equal(1, position);
} END_TEST

START_TEST (test_add_item_puts_it_in_the_right_position_at_end) {
  item = create_item_with_tokens_and_time(9, tokens, 4, (time_t) 1177975519L);
  item_cache_add_item(item_cache, item);
  int position = 0;
  item_cache_each_item(item_cache, adding_item_position_count, &position);
  assert_equal(11, position);
} END_TEST

/* Feature Extraction tests */
static const ItemCacheEntry *tokenizer_called_with = NULL;
static Item *tokenized_item;

static Item * mock_feature_extractor(ItemCache * item_cache, const ItemCacheEntry * entry, void *ignore) {
  tokenizer_called_with = entry;
  return tokenized_item;
}

static void setup_feature_extraction(void) {
  setup_fixture_path();
  system("cp fixtures/valid.db /tmp/valid-copy.db");
  item_cache_create(&item_cache, "/tmp/valid-copy.db");
  item_cache_set_feature_extractor(item_cache, mock_feature_extractor, NULL);
  item_cache_load(item_cache);
  item_cache_start_feature_extractor(item_cache);
    
  tokenized_item = create_item_with_tokens_and_time(9, tokens, 4, (time_t) 1178683198L);
}

static void teardown_feature_extraction(void) {
  teardown_fixture_path();
  free_item_cache(item_cache);
}

START_TEST (test_adding_entry_results_in_calling_the_tokenizer_with_the_entry) {
  ItemCacheEntry *entry = create_item_cache_entry(11, "id#11", "Entry 11", "Author 11",
                                              "http://example.org/11",
                                              "http://example.org/11.html",
                                              "http://example.org/11/spider",
                                              "<p>This is some content</p>",
                                              1178551600, 1, 1178551601);
  item_cache_add_entry(item_cache, entry);
  sleep(1);
  assert_equal(entry, tokenizer_called_with);  
} END_TEST

START_TEST (test_adding_entry_and_tokenizing_it_results_in_it_being_stored_in_update_queue) {
  ItemCacheEntry *entry = create_item_cache_entry(11, "id#11", "Entry 11", "Author 11",
                                                "http://example.org/11",
                                                "http://example.org/11.html",
                                                "http://example.org/11/spider",
                                                "<p>This is some content</p>",
                                                1178551600, 1, 1178551601);
  item_cache_add_entry(item_cache, entry);  
  sleep(1);
  assert_equal(1, item_cache_update_queue_size(item_cache));
} END_TEST

/* NULL feature extractor */

static Item * null_feature_extractor(ItemCache *item_cache, const ItemCacheEntry * entry, void *ignore) {
  return NULL;
}

static void setup_null_feature_extraction(void) {
  setup_fixture_path();
  system("cp fixtures/valid.db /tmp/valid-copy.db");
  item_cache_create(&item_cache, "/tmp/valid-copy.db");
  item_cache_set_feature_extractor(item_cache, null_feature_extractor, NULL);
  item_cache_load(item_cache);
  item_cache_start_feature_extractor(item_cache);

  tokenized_item = create_item_with_tokens_and_time(9, tokens, 4, (time_t) 1178683198L);
}

static void teardown_null_feature_extraction(void) {
  teardown_fixture_path();
  free_item_cache(item_cache);
}

START_TEST (test_null_feature_extraction) {
  ItemCacheEntry *entry = create_item_cache_entry(11, "id#11", "Entry 11", "Author 11",
                                                "http://example.org/11",
                                                "http://example.org/11.html",
                                                "http://example.org/11/spider",
                                                "<p>This is some content</p>",
                                                1178551600, 1, 1178551601);
  item_cache_add_entry(item_cache, entry);  
  sleep(1);
  assert_equal(0, item_cache_update_queue_size(item_cache));
} END_TEST


/* Cache updating tests */
static int item_id = 9; 
static Item * mock_feature_extractor2(ItemCache *item_cache, const ItemCacheEntry * entry, void *ignore) {
  return create_item_with_tokens_and_time(item_id++, tokens, 4, (time_t) 1178683198L);;
}

static void setup_full_update(void) {
  setup_fixture_path();
  system("cp fixtures/valid.db /tmp/valid-copy.db");
  item_cache_create(&item_cache, "/tmp/valid-copy.db");
  item_cache_set_feature_extractor(item_cache, mock_feature_extractor2, NULL);
  item_cache_load(item_cache);
  item_cache_start_feature_extractor(item_cache);
  item_cache_start_cache_updater(item_cache);
    
  tokenized_item = create_item_with_tokens_and_time(9, tokens, 4, (time_t) 1178683198L);
}

static void teardown_full_update(void) {
  teardown_fixture_path();
  free_item_cache(item_cache);
}

START_TEST (test_adding_entry_causes_item_added_to_cache) {
  ItemCacheEntry *entry = create_item_cache_entry(11, "id#11", "Entry 11", "Author 11",
                                                  "http://example.org/11",
                                                  "http://example.org/11.html",
                                                  "http://example.org/11/spider",
                                                  "<p>This is some content</p>",
                                                  1178551600, 1, 1178551601);
  item_cache_add_entry(item_cache, entry);
  sleep(1);
  assert_equal(11, item_cache_cached_size(item_cache));
} END_TEST

START_TEST (test_adding_multiple_entries_causes_item_added_to_cache) {
  ItemCacheEntry *entry1 = create_item_cache_entry(11, "id#11", "Entry 11", "Author 11",
                                                  "http://example.org/11",
                                                  "http://example.org/11.html",
                                                  "http://example.org/11/spider",
                                                  "<p>This is some content</p>",
                                                  1178551600, 1, 1178551601);
  ItemCacheEntry *entry2 = create_item_cache_entry(12, "id#12", "Entry 12", "Author 12",
                                                    "http://example.org/12",
                                                    "http://example.org/12.html",
                                                    "http://example.org/11/spider",
                                                    "<p>This is some content</p>",
                                                    1178551600, 1, 1178551601);
  
  item_cache_add_entry(item_cache, entry1);
  item_cache_add_entry(item_cache, entry2);
  sleep(1);
  assert_equal(12, item_cache_cached_size(item_cache));
} END_TEST

static int *memo_ref = NULL;
static void update_callback(ItemCache * item_cache, void *memo) {
  memo_ref = (int*) memo;
}

START_TEST (test_update_callback) {
  int memo = 21;
  item_cache_set_update_callback(item_cache, update_callback, &memo);
  ItemCacheEntry *entry1 = create_item_cache_entry(11, "id#11", "Entry 11", "Author 11",
                                                    "http://example.org/11",
                                                    "http://example.org/11.html",
                                                    "http://example.org/11/spider",
                                                    "<p>This is some content</p>",
                                                    1178551600, 1, 1178551601);
  
  item_cache_add_entry(item_cache, entry1);  
  sleep(1);
  assert_equal(&memo, memo_ref);
} END_TEST

/* Cache pruning */
time_t purge_time;

static void setup_purging(void) {
  setup_fixture_path();
  system("cp fixtures/valid.db /tmp/valid-copy.db");
  item_cache_create(&item_cache, "/tmp/valid-copy.db");
  
  time_t now = time(NULL);
  struct tm item_time;
  gmtime_r(&now, &item_time);
  item_time.tm_mday--;
  item_time.tm_mon--;
  purge_time = timegm(&item_time);
}

static void teardown_purging(void) {
  teardown_fixture_path();
  free_item_cache(item_cache);
}

START_TEST (test_purging_cache_does_nothing_with_no_items) {
  item_cache_purge_old_items(item_cache);
} END_TEST

START_TEST (test_purging_cache_of_one_old_item) {
  Item *old_item = create_item_with_tokens_and_time(23, tokens, 4, purge_time - 2);
  mark_point();
  item_cache_add_item(item_cache, old_item);
  assert_not_null(item_cache_fetch_item(item_cache, 23));
  item_cache_purge_old_items(item_cache);
  assert_null(item_cache_fetch_item(item_cache, 23));
} END_TEST

START_TEST (test_purging_cache_does_nothing_with_one_new_item) {
  Item *non_purged_item = create_item_with_tokens_and_time(23, tokens, 4, purge_time + 2);
  item_cache_add_item(item_cache, non_purged_item);
  assert_not_null(item_cache_fetch_item(item_cache, 23));
  item_cache_purge_old_items(item_cache);
  assert_not_null(item_cache_fetch_item(item_cache, 23));
} END_TEST

START_TEST (test_purging_half_of_the_cache) {
  Item *non_purged_item = create_item_with_tokens_and_time(23, tokens, 4, purge_time + 2);
  Item *purged_item = create_item_with_tokens_and_time(24, tokens, 4, purge_time - 2);
  
  item_cache_add_item(item_cache, non_purged_item);
  item_cache_add_item(item_cache, purged_item);
  
  assert_not_null(item_cache_fetch_item(item_cache, 23));
  assert_not_null(item_cache_fetch_item(item_cache, 24));
  
  item_cache_purge_old_items(item_cache);
  
  assert_not_null(item_cache_fetch_item(item_cache, 23));
  assert_null(item_cache_fetch_item(item_cache, 24));
} END_TEST

Suite *
item_cache_suite(void) {
  Suite *s = suite_create("ItemCache");  
  TCase *tc_case = tcase_create("case");

  tcase_add_test(tc_case, creating_with_missing_db_file_fails);
  tcase_add_test(tc_case, creating_with_empty_db_file_fails);
  tcase_add_test(tc_case, create_with_valid_db);
      
  TCase *fetch_item_case = tcase_create("fetch_item");
  tcase_add_checked_fixture(fetch_item_case, setup_cache, teardown_item_cache);
  tcase_add_test(fetch_item_case, test_fetch_item_returns_null_when_item_doesnt_exist);
  tcase_add_test(fetch_item_case, test_fetch_item_contains_item_id);
  tcase_add_test(fetch_item_case, test_fetch_item_contains_item_time);
  tcase_add_test(fetch_item_case, test_fetch_item_contains_the_right_number_of_tokens);
  tcase_add_test(fetch_item_case, test_fetch_item_contains_the_right_frequency_for_a_given_token);
  tcase_add_test(fetch_item_case, test_fetch_item_after_load);
  tcase_add_test(fetch_item_case, test_fetch_item_after_load_contains_tokens);
  
  TCase *load = tcase_create("load");
  tcase_add_checked_fixture(load, setup_cache, teardown_item_cache);
  tcase_add_test(load, test_load_loads_the_right_number_of_items);  
  tcase_add_test(load, test_load_sets_cache_loaded_to_true);
  
  TCase *iteration = tcase_create("iteration");
  tcase_add_checked_fixture(iteration, setup_iteration, teardown_iteration);
  tcase_add_test(iteration, test_iterates_over_all_items);
  tcase_add_test(iteration, test_iteration_stops_when_iterator_returns_CLASSIFIER_FAIL);
  tcase_add_test(iteration, test_iteration_happens_in_reverse_updated_order);
  
  TCase *rndbg = tcase_create("random background");
  tcase_add_checked_fixture(rndbg, setup_cache, teardown_item_cache);
  tcase_add_test(rndbg, test_random_background_is_null_before_load);
  tcase_add_test(rndbg, test_creates_random_background_after_load);
  tcase_add_test(rndbg, test_random_background_is_correct_size);
  tcase_add_test(rndbg, test_random_background_has_right_count_for_a_token);
  
  TCase *modification = tcase_create("modification");
  tcase_add_checked_fixture(modification, setup_modification, teardown_modification);
  tcase_add_test(modification, test_adding_an_entry_stores_it_in_the_database);
  tcase_add_test(modification, adding_an_entry_twice_does_not_fail);
  tcase_add_test(modification, adding_an_entry_saves_all_its_attributes);
  tcase_add_test(modification, test_destroying_an_entry_removes_it_from_database);
  tcase_add_test(modification, test_destroying_an_entry_removes_tokens_from_the_database_file);
  tcase_add_test(modification, test_destroying_an_entry_removes_it_from_the_database_file);
  tcase_add_test(modification, test_cant_delete_an_item_that_is_used_in_the_random_background);
  tcase_add_test(modification, test_failed_deletion_doesnt_delete_tokens);
  tcase_add_test(modification, test_add_feed_to_item_cache);
  tcase_add_test(modification, test_add_feed_that_already_exists_updates_attributes);
  tcase_add_test(modification, test_deleting_a_feed_removes_it_from_the_database);
  tcase_add_test(modification, test_deleting_a_feed_removes_its_entries_from_the_database);
  tcase_add_test(modification, test_adding_entry_causes_it_to_be_added_to_the_tokenization_queue);
  tcase_add_test(modification, test_adding_entry_twice_causes_it_to_be_added_to_the_tokenization_queue_only_once);
  
  TCase *loaded_modification = tcase_create("loaded modification");
  tcase_add_checked_fixture(loaded_modification, setup_loaded_modification, teardown_loaded_modification);
  tcase_add_test(loaded_modification, test_add_item_to_in_memory_arrays_adds_an_item);
  tcase_add_test(loaded_modification, test_add_item_makes_it_fetchable);
  tcase_add_test(loaded_modification, test_add_item_makes_it_iteratable);
  tcase_add_test(loaded_modification, test_add_item_puts_it_in_the_right_position);
  tcase_add_test(loaded_modification, test_add_item_puts_it_in_the_right_position_at_beginning);
  tcase_add_test(loaded_modification, test_add_item_puts_it_in_the_right_position_at_end);

  
  TCase *feature_extraction = tcase_create("feature extraction");
  tcase_add_checked_fixture(feature_extraction, setup_feature_extraction, teardown_feature_extraction);
  tcase_add_test(feature_extraction, test_adding_entry_results_in_calling_the_tokenizer_with_the_entry);
  tcase_add_test(feature_extraction, test_adding_entry_and_tokenizing_it_results_in_it_being_stored_in_update_queue);
  
  TCase *null_feature_extraction = tcase_create("null feature extraction");
  tcase_add_checked_fixture(null_feature_extraction, setup_null_feature_extraction, teardown_null_feature_extraction);
  tcase_add_test(null_feature_extraction, test_null_feature_extraction);
     
  TCase *full_update = tcase_create("full update");
  tcase_add_checked_fixture(full_update, setup_full_update, teardown_full_update);
  tcase_add_test(full_update, test_adding_entry_causes_item_added_to_cache); 
  tcase_add_test(full_update, test_adding_multiple_entries_causes_item_added_to_cache);
  tcase_add_test(full_update, test_update_callback);
  
  TCase *purging = tcase_create("purging");
  tcase_add_checked_fixture(purging, setup_purging, teardown_purging);
  tcase_add_test(purging, test_purging_cache_does_nothing_with_one_new_item);
  tcase_add_test(purging, test_purging_cache_of_one_old_item);
  tcase_add_test(purging, test_purging_cache_does_nothing_with_no_items);
  tcase_add_test(purging, test_purging_half_of_the_cache);
  
  suite_add_tcase(s, tc_case);
  suite_add_tcase(s, fetch_item_case);
  suite_add_tcase(s, load);
  suite_add_tcase(s, iteration);
  suite_add_tcase(s, rndbg);
  suite_add_tcase(s, modification);
  suite_add_tcase(s, loaded_modification);
  suite_add_tcase(s, feature_extraction);
  suite_add_tcase(s, null_feature_extraction);
  suite_add_tcase(s, full_update);
  suite_add_tcase(s, purging);
  return s;
}
