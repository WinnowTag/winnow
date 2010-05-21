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
#include <string.h>
#include "assertions.h"
#include "read_document.h"
#include "../src/item_cache.h"
#include "../src/misc.h"
#include "../src/item_cache.h"
#include "../src/logging.h"

static ItemCacheOptions item_cache_options = {1, 3650, 2};

START_TEST (creating_with_missing_db_file_fails) {
  ItemCache *item_cache;
  int rc = item_cache_create(&item_cache, "/tmp/missing", &item_cache_options);
  assert_equal(CLASSIFIER_FAIL, rc);
  const char *msg = item_cache_errmsg(item_cache);
  assert_equal_s("unable to open database file", msg);
} END_TEST

START_TEST (creating_with_empty_db_file_fails) {
  setup_fixture_path();
  ItemCache *item_cache;
  int rc = item_cache_create(&item_cache, "fixtures/empty", &item_cache_options);
  assert_equal(CLASSIFIER_FAIL, rc);
  const char *msg = item_cache_errmsg(item_cache);
  assert_equal_s("Database file's user version does not match classifier version. Trying running classifier-db-migrate from the classifier-tools package.", msg);
  teardown_fixture_path();
} END_TEST

START_TEST (create_with_valid_db) {
  setup_fixture_path();
  ItemCache *item_cache;
  int rc = item_cache_create(&item_cache, "fixtures/valid", &item_cache_options);
  assert_equal(CLASSIFIER_OK, rc);
  teardown_fixture_path();
} END_TEST

/* Tests for fetching an item */

ItemCache *item_cache;
int free_when_done;

static void setup_cache(void) {
  setup_fixture_path();
  system("rm -Rf /tmp/valid-copy && cp -R fixtures/valid /tmp/valid-copy && chmod -R 755 /tmp/valid-copy");
  item_cache_create(&item_cache, "/tmp/valid-copy", &item_cache_options);
}

static void teardown_item_cache(void) {
  teardown_fixture_path();
  free_item_cache(item_cache);
}

START_TEST (test_fetch_item_returns_null_when_item_doesnt_exist) {
  Item *item = item_cache_fetch_item(item_cache, (unsigned char*) "urn:peerworks.org:entry#111", &free_when_done);
  assert_null(item);
  free_item(item);
} END_TEST

START_TEST (test_fetch_item_contains_item_id) {
  Item *item = item_cache_fetch_item(item_cache, (unsigned char*) "urn:peerworks.org:entry#890806", &free_when_done);

  assert_not_null(item);
  //assert_equal_s("urn:peerworks.org:entry#890806", item_get_id(item));

  //free_item(item);
} END_TEST

START_TEST (test_fetch_item_contains_item_time) {
  Item *item = item_cache_fetch_item(item_cache, (unsigned char*) "urn:peerworks.org:entry#890806", &free_when_done);
  assert_not_null(item);
  assert_equal(1178551672, item_get_time(item));
  free_item(item);
} END_TEST

START_TEST (test_fetch_item_contains_the_right_number_of_tokens) {
 Item *item = item_cache_fetch_item(item_cache, (unsigned char*) "urn:peerworks.org:entry#890806", &free_when_done);
 assert_not_null(item);
 assert_equal(76, item_get_num_tokens(item));
 free_item(item);
} END_TEST

START_TEST (test_fetch_item_contains_the_right_frequency_for_a_given_token) {
  Item *item = item_cache_fetch_item(item_cache, (unsigned char*) "urn:peerworks.org:entry#890806", &free_when_done);
  assert_not_null(item);
  short freq = item_get_token_frequency(item, 9949);
  assert_equal(3, freq);
} END_TEST

START_TEST (test_free_when_done_is_true_when_the_item_is_not_in_the_memory_cache) {
  Item *item = item_cache_fetch_item(item_cache, (unsigned char*) "urn:peerworks.org:entry#890806", &free_when_done);
  assert_equal(true, free_when_done);
} END_TEST

START_TEST (test_fetch_item_after_load) {
  item_cache_load(item_cache);
  Item *item = item_cache_fetch_item(item_cache, (unsigned char*) "urn:peerworks.org:entry#890806", &free_when_done);
  assert_not_null(item);
} END_TEST

START_TEST (test_free_when_done_is_false_when_the_item_is_in_the_memory_cache) {
  item_cache_load(item_cache);
  Item *item = item_cache_fetch_item(item_cache, (unsigned char*) "urn:peerworks.org:entry#890806", &free_when_done);
  assert_equal(false, free_when_done);
} END_TEST

START_TEST (test_fetch_item_after_load_contains_tokens) {
  item_cache_load(item_cache);
  Item *item = item_cache_fetch_item(item_cache, (unsigned char*) "urn:peerworks.org:entry#890806", &free_when_done);
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

START_TEST (test_load_respects_min_tokens) {
  ItemCache *min_token_item_cache;
  item_cache_options.min_tokens = 80;
  item_cache_create(&min_token_item_cache, "/tmp/valid-copy", &item_cache_options);
  int rc = item_cache_load(min_token_item_cache);
  assert_equal(CLASSIFIER_OK, rc);
  assert_equal(6, item_cache_cached_size(min_token_item_cache));
  free_item_cache(min_token_item_cache);
} END_TEST

/* Test iteration */
void setup_iteration(void) {
  setup_fixture_path();
  item_cache_create(&item_cache, "fixtures/valid", &item_cache_options);
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
  unsigned char **ids = (unsigned char **) memo;
  unsigned char *id = (unsigned char *) item_get_id(item);
  ids[i++] = id;
  return CLASSIFIER_OK;
}

START_TEST (test_iteration_happens_in_reverse_updated_order) {
  i = 0;
  unsigned char *ids[10];
  item_cache_each_item(item_cache, stores_ids, ids);
  assert_equal(10, i);
  assert_equal_s("urn:peerworks.org:entry#709254", ids[0]);
  assert_equal_s("urn:peerworks.org:entry#880389", ids[1]);
  assert_equal_s("urn:peerworks.org:entry#888769", ids[2]);
  assert_equal_s("urn:peerworks.org:entry#886643", ids[3]);
  assert_equal_s("urn:peerworks.org:entry#890806", ids[4]);
  assert_equal_s("urn:peerworks.org:entry#802739", ids[5]);
  assert_not_null(ids[6]);
  assert_equal_s("urn:peerworks.org:entry#884409", ids[6]);
  assert_equal_s("urn:peerworks.org:entry#753459", ids[7]);
  assert_equal_s("urn:peerworks.org:entry#878944", ids[8]);
  assert_equal_s("urn:peerworks.org:entry#886294", ids[9]);
} END_TEST

/* Test RandomBackground */
START_TEST (test_random_background_is_empty_pool_before_load) {
  assert_not_null(item_cache_random_background(item_cache));
} END_TEST

START_TEST (test_creates_random_background_after_load) {
  item_cache_load(item_cache);
  assert_not_null(item_cache_random_background(item_cache));
} END_TEST

START_TEST (test_random_background_is_correct_size) {
  item_cache_load(item_cache);
  const Pool *bg = item_cache_random_background(item_cache);
  assert_not_null(bg);
  assert_equal(750, pool_num_tokens(bg));
} END_TEST

START_TEST (test_random_background_has_right_count_for_a_token) {
  item_cache_load(item_cache);
  const Pool *bg = item_cache_random_background(item_cache);
  assert_not_null(bg);
  assert_equal(2, pool_token_frequency(bg, 2515));
} END_TEST

/* Item Cache modification */
#include <sqlite3.h>

static char *entry_document;

static void setup_modification(void) {
  setup_fixture_path();
  system("rm -Rf /tmp/valid-copy && cp -R fixtures/valid /tmp/valid-copy && chmod -R 755 /tmp/valid-copy");
  item_cache_create(&item_cache, "/tmp/valid-copy", &item_cache_options);
  entry_document = read_document("fixtures/entry.atom");
}

static void teardown_modification(void) {
  teardown_fixture_path();
  free_item_cache(item_cache);
  free(entry_document);
}

START_TEST (adding_an_entry_saves_all_its_attributes) {
  ItemCacheEntry *entry = create_entry_from_atom_xml(entry_document);
  int rc = item_cache_add_entry(item_cache, entry);
  assert_equal(CLASSIFIER_OK, rc);

  sqlite3 *db;
  sqlite3_stmt *stmt;
  sqlite3_open_v2("/tmp/valid-copy/catalog.db", &db, SQLITE_OPEN_READONLY, NULL);
  sqlite3_prepare_v2(db, "select * from entries where full_id = 'urn:peerworks.org:entry#1'", -1, &stmt, NULL);
  if (SQLITE_ROW == sqlite3_step(stmt)) {
    assert_equal_s("urn:peerworks.org:entry#1", sqlite3_column_text(stmt, 1));
    assert_equal_f(2453583.02047454, sqlite3_column_double(stmt, 2));
    assert_equal(NULL, sqlite3_column_text(stmt, 4));
  } else {
    fail("Could not get record");
  }

  sqlite3_close(db);
} END_TEST

START_TEST (adding_an_entry_saves_its_xml) {
  ItemCacheEntry *entry = create_entry_from_atom_xml(entry_document);
  int rc = item_cache_add_entry(item_cache, entry);
  assert_equal(CLASSIFIER_OK, rc);

  sqlite3 *db;
  sqlite3_stmt *stmt;
  sqlite3_open_v2("/tmp/valid-copy/atom.db", &db, SQLITE_OPEN_READONLY, NULL);
  sqlite3_exec(db, "ATTACH DATABASE '/tmp/valid-copy/catalog.db' as cat", NULL, NULL, NULL);
  sqlite3_prepare_v2(db, "select * from entry_atom where id = (select id from cat.entries where full_id = 'urn:peerworks.org:entry#1')", -1, &stmt, NULL);
  if (SQLITE_ROW != sqlite3_step(stmt)) {
    fail("Could not get atom record");
  }

  sqlite3_close(db);
} END_TEST

START_TEST (test_can_add_entry_without_a_feed_id) {
  ItemCacheEntry *entry = create_entry_from_atom_xml(entry_document);
  int rc = item_cache_add_entry(item_cache, entry);
  assert_equal(CLASSIFIER_OK, rc);

  sqlite3 *db;
  sqlite3_stmt *stmt;
  sqlite3_open_v2("/tmp/valid-copy/catalog.db", &db, SQLITE_OPEN_READONLY, NULL);
  sqlite3_prepare_v2(db, "select * from entries where id = ?", -1, &stmt, NULL);
  sqlite3_bind_int(stmt, 1, item_cache_entry_id(entry));
  if (SQLITE_ROW != sqlite3_step(stmt)) {
    fail("Could not get record");
  }

  sqlite3_close(db);
} END_TEST

START_TEST (adding_an_entry_twice_does_not_fail) {
  ItemCacheEntry *entry = create_entry_from_atom_xml(entry_document);
  int rc = item_cache_add_entry(item_cache, entry);
  assert_equal(CLASSIFIER_OK, rc);
  rc = item_cache_add_entry(item_cache, entry);
  assert_equal(CLASSIFIER_OK, rc);
} END_TEST

START_TEST (adding_an_entry_twice_does_not_add_a_duplicate) {
  ItemCacheEntry *entry = create_entry_from_atom_xml(entry_document);
  item_cache_add_entry(item_cache, entry);
  item_cache_add_entry(item_cache, entry);

  sqlite3 *db;
  sqlite3_stmt *stmt;
  sqlite3_open_v2("/tmp/valid-copy/catalog.db", &db, SQLITE_OPEN_READONLY, NULL);
  sqlite3_prepare_v2(db, "select count(*) from entries", -1, &stmt, NULL);

  if (SQLITE_ROW == sqlite3_step(stmt)) {
    assert_equal(11, sqlite3_column_int(stmt, 0));
  } else {
    fail("could not get count");
  }

  sqlite3_close(db);
} END_TEST

START_TEST (test_destroying_an_entry_removes_it_from_the_database_file) {
  int rc = item_cache_remove_entry(item_cache, 753459);
  assert_equal(CLASSIFIER_OK, rc);
  Item *item = item_cache_fetch_item(item_cache, (unsigned char*) "urn:peerworks.org:entry#753459", &free_when_done);
  assert_null(item);

  sqlite3 *db;
  sqlite3_stmt *stmt;
  sqlite3_open_v2("/tmp/valid-copy/catalog.db", &db, SQLITE_OPEN_READONLY, NULL);
  sqlite3_prepare_v2(db, "select * from entries where id = 753459", -1, &stmt, NULL);
  rc = sqlite3_step(stmt);
  assert_equal(SQLITE_DONE, rc);
  sqlite3_close(db);
} END_TEST

START_TEST (test_destroying_an_entry_removes_its_xml_document) {
  int rc = item_cache_remove_entry(item_cache, 753459);

  sqlite3 *db;
  sqlite3_stmt *stmt;
  sqlite3_open_v2("/tmp/valid-copy/atom.db", &db, SQLITE_OPEN_READONLY, NULL);
  sqlite3_prepare_v2(db, "select * from entry_atom where id = ?", -1, &stmt, NULL);
  sqlite3_bind_int(stmt, 1, 753459);
  if (SQLITE_ROW == sqlite3_step(stmt)) {
	  fail("XML not deleted");
  }

  sqlite3_close(db);
} END_TEST

START_TEST (test_destroying_an_entry_removes_tokens) {
  int rc = item_cache_remove_entry(item_cache, 753459);
  sqlite3 *db;
  sqlite3_stmt *stmt;
  sqlite3_open_v2("/tmp/valid-copy/tokens.db", &db, SQLITE_OPEN_READONLY, NULL);
  sqlite3_prepare_v2(db, "select * from entry_tokens where id = ?", -1, &stmt, NULL);
  sqlite3_bind_int(stmt, 1, 753459);
  if (SQLITE_ROW == sqlite3_step(stmt)) {
  	  fail("Tokens not deleted");
  }

  sqlite3_close(db);
} END_TEST

START_TEST (test_cant_delete_an_item_that_is_used_in_the_random_background) {
  int rc = item_cache_remove_entry(item_cache, 890806);
  assert_equal(ITEM_CACHE_ENTRY_PROTECTED, rc);

  sqlite3 *db;
  sqlite3_stmt *stmt;
  sqlite3_open_v2("/tmp/valid-copy/catalog.db", &db, SQLITE_OPEN_READONLY, NULL);
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
  sqlite3_open_v2("/tmp/valid-copy/tokens.db", &db, SQLITE_OPEN_READONLY, NULL);
  sqlite3_prepare_v2(db, "select * from entry_tokens where id = ?", -1, &stmt, NULL);
  sqlite3_bind_int(stmt, 1, 890806);
  if (SQLITE_ROW != sqlite3_step(stmt)) {
	  fail("Tokens not deleted");
  }

  sqlite3_close(db);
} END_TEST

#include <sched.h>
#include <sys/unistd.h>
int tokens[][2] = {1, 2, 3, 4, 5, 6, 7, 8};
Item *item;

static void setup_loaded_modification(void) {
  setup_fixture_path();
  system("rm -Rf /tmp/valid-copy && cp -R fixtures/valid /tmp/valid-copy && chmod -R 755 /tmp/valid-copy");
  item_cache_create(&item_cache, "/tmp/valid-copy", &item_cache_options);
  //item_cache_set_feature_extractor(item_cache, NULL);
  item_cache_load(item_cache);
  entry_document = read_document("fixtures/entry.atom");
  item = create_item_with_tokens_and_time((unsigned char*) "urn:peerworks.org:entry#1", tokens, 4, (time_t) 1178683198L);
}

static void teardown_loaded_modification(void) {
  teardown_fixture_path();
  free_item_cache(item_cache);
}

START_TEST (item_cache_add_item_returns_CLASSIFIER_OK_if_the_item_is_added) {
  int rc = item_cache_add_item(item_cache, item);
  assert_equal(CLASSIFIER_OK, rc);
} END_TEST

START_TEST (item_cache_add_item_returns_CLASSIFIER_FAIL_if_the_item_is_not_added_because_it_doesn_have_enough_tokens) {
  Item *small_item = create_item_with_tokens_and_time((unsigned char*) "urn:890807", tokens, 1, (time_t) 1178683198L);
  int rc = item_cache_add_item(item_cache, small_item);
  assert_equal(CLASSIFIER_FAIL, rc);
  free_item(small_item);
} END_TEST

START_TEST (item_cache_add_item_doesnt_add_small_items) {
  Item *small_item = create_item_with_tokens_and_time((unsigned char*) "urn:890807", tokens, 1, (time_t) 1178683198L);
  item_cache_add_item(item_cache, small_item);
  assert_equal(10, item_cache_cached_size(item_cache));
} END_TEST

START_TEST (test_add_item_to_in_memory_arrays_adds_an_item) {
  item_cache_add_item(item_cache, item);
  assert_equal(11, item_cache_cached_size(item_cache));
} END_TEST

START_TEST (test_add_item_makes_it_fetchable) {
  item_cache_add_item(item_cache, item);
  assert_equal(item, item_cache_fetch_item(item_cache, (unsigned char*) "urn:peerworks.org:entry#1", &free_when_done));
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
  item = create_item_with_tokens_and_time((unsigned char*) "urn:890807", tokens, 4, (time_t) 1179051840L);
  item_cache_add_item(item_cache, item);
  int position = 0;
  item_cache_each_item(item_cache, adding_item_position_count, &position);
  assert_equal(1, position);
} END_TEST

START_TEST (test_add_item_puts_it_in_the_right_position_at_end) {
  item = create_item_with_tokens_and_time((unsigned char*) "urn:890807", tokens, 4, (time_t) 1177975519L);
  item_cache_add_item(item_cache, item);
  int position = 0;
  item_cache_each_item(item_cache, adding_item_position_count, &position);
  assert_equal(11, position);
} END_TEST

static int get_entry_id(char *db_file, char *full_id) {
  int id = -1;

  sqlite3 *db;
  sqlite3_stmt *stmt;
  sqlite3_open_v2(db_file, &db, SQLITE_OPEN_READONLY, NULL);
  sqlite3_prepare_v2(db, "select id from entries where full_id = ?", -1, &stmt, NULL);
  sqlite3_bind_text(stmt, 1, full_id, -1, NULL);
  int rc = sqlite3_step(stmt);
  assert_equal(SQLITE_ROW, rc);
  id = sqlite3_column_int(stmt, 0);
  sqlite3_finalize(stmt);
  sqlite3_close(db);

  return id;
}

START_TEST (test_save_item_stores_it_in_the_database) {
  // Need a corresponding entry
  ItemCacheEntry *entry = create_entry_from_atom_xml(entry_document);

  int rc = item_cache_add_entry(item_cache, entry);
  assert_equal(CLASSIFIER_OK, rc);

  int id = get_entry_id("/tmp/valid-copy/catalog.db", "urn:peerworks.org:entry#1");

  sqlite3 *db;
  sqlite3_stmt *stmt;
  sqlite3_open_v2("/tmp/valid-copy/tokens.db", &db, SQLITE_OPEN_READONLY, NULL);
  sqlite3_prepare_v2(db, "select tokens from entry_tokens where id = ?", -1, &stmt, NULL);
  sqlite3_bind_int(stmt, 1, id);
  rc = sqlite3_step(stmt);
  assert_equal(SQLITE_ROW, rc);
  char* tokens = (char*) sqlite3_column_blob(stmt, 0);
  assert_not_null(tokens);
  assert_equal(8 * 6 /* num tokens * TOKEN_BYTES */, sqlite3_column_bytes(stmt, 0));
  sqlite3_finalize(stmt);
  sqlite3_close(db);
} END_TEST

START_TEST (test_save_item_stores_the_correct_tokens) {
  ItemCacheEntry *entry = create_entry_from_atom_xml(entry_document);

  int rc = item_cache_add_entry(item_cache, entry);
  assert_equal(CLASSIFIER_OK, rc);

  int freeit;
  Item *new_item = item_cache_fetch_item(item_cache, "urn:peerworks.org:entry#1", &freeit);
  assert_not_null(new_item);
  assert_true(freeit);
  assert_equal(1, item_get_token_frequency(new_item, 1247));
  assert_equal(1, item_get_token_frequency(new_item, 1248));
  assert_equal(1, item_get_token_frequency(new_item, 1249));
  assert_equal(1, item_get_token_frequency(new_item, 1250));
  assert_equal(1, item_get_token_frequency(new_item, 1251));
  assert_equal(2, item_get_token_frequency(new_item, 1252));
  assert_equal(1, item_get_token_frequency(new_item, 1253));
  assert_equal(1, item_get_token_frequency(new_item, 1254));
} END_TEST


START_TEST (test_save_item_without_an_entry_wont_store_it_in_the_database) {
  int rc = item_cache_save_item(item_cache, item);
  assert_equal(CLASSIFIER_FAIL, rc);
} END_TEST

/* Cache updating tests */
static int item_id = 9;
static char * entry_document2;

static void setup_full_update(void) {
  setup_fixture_path();
  system("rm -Rf /tmp/valid-copy && cp -R fixtures/valid /tmp/valid-copy && chmod -R 755 /tmp/valid-copy");
  item_cache_create(&item_cache, "/tmp/valid-copy", &item_cache_options);
  item_cache_load(item_cache);
  item_cache_start_cache_updater(item_cache);

  entry_document = read_document("fixtures/entry.atom");
  entry_document2 = read_document("fixtures/entry2.atom");
}

static void teardown_full_update(void) {
  teardown_fixture_path();
  free_item_cache(item_cache);
}

START_TEST (test_adding_entry_causes_item_added_to_cache) {
  ItemCacheEntry *entry = create_entry_from_atom_xml(entry_document);
  item_cache_add_entry(item_cache, entry);
  sleep(1);
  assert_equal(11, item_cache_cached_size(item_cache));
} END_TEST

START_TEST (test_adding_entry_causes_tokens_to_be_added_to_the_db) {
  ItemCacheEntry *entry = create_entry_from_atom_xml(entry_document);
  item_cache_add_entry(item_cache, entry);
  sleep(1);

  int entry_id = get_entry_id("/tmp/valid-copy/catalog.db", "urn:peerworks.org:entry#1");
  sqlite3 *db;
  sqlite3_stmt *stmt;
  sqlite3_open_v2("/tmp/valid-copy/tokens.db", &db, SQLITE_OPEN_READONLY, NULL);
  sqlite3_prepare_v2(db, "select tokens from entry_tokens where id = ?", -1, &stmt, NULL);
  sqlite3_bind_int(stmt, 1, entry_id);
  int rc = sqlite3_step(stmt);
  assert_equal(SQLITE_ROW, rc);
  sqlite3_finalize(stmt);
  sqlite3_close(db);
} END_TEST

START_TEST (test_adding_multiple_entries_causes_item_added_to_cache) {
  ItemCacheEntry *entry1 = create_entry_from_atom_xml(entry_document);
  ItemCacheEntry *entry2 = create_entry_from_atom_xml(entry_document2); // TODO create another document

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
  ItemCacheEntry *entry1 = create_entry_from_atom_xml(entry_document);

  item_cache_add_entry(item_cache, entry1);
  sleep(1);

  if (!memo_ref) {
    sleep(1);
  }

  assert_equal(&memo, memo_ref);
} END_TEST


/* Cache pruning */
time_t purge_time;

static void setup_purging(void) {
  setup_fixture_path();
  item_cache_options.load_items_since = 30;
  system("rm -Rf /tmp/valid-copy && cp -R fixtures/valid /tmp/valid-copy && chmod -R 755 /tmp/valid-copy");
  item_cache_create(&item_cache, "/tmp/valid-copy", &item_cache_options);

  time_t now = time(NULL);
  struct tm item_time;
  gmtime_r(&now, &item_time);
  //item_time.tm_mday--;
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
  Item *old_item = create_item_with_tokens_and_time((unsigned char*) "urn:peerworks.org:entry#23", tokens, 4, purge_time - 2);
  mark_point();
  item_cache_add_item(item_cache, old_item);
  assert_not_null(item_cache_fetch_item(item_cache, (unsigned char*) "urn:peerworks.org:entry#23", &free_when_done));
  item_cache_purge_old_items(item_cache);
  assert_null(item_cache_fetch_item(item_cache, (unsigned char*) "urn:peerworks.org:entry#23", &free_when_done));
} END_TEST

START_TEST (test_purging_cache_does_nothing_with_one_new_item) {
  Item *non_purged_item = create_item_with_tokens_and_time((unsigned char*) "urn:peerworks.org:entry#23", tokens, 4, purge_time + 2);
  item_cache_add_item(item_cache, non_purged_item);
  assert_not_null(item_cache_fetch_item(item_cache, (unsigned char*) "urn:peerworks.org:entry#23", &free_when_done));
  item_cache_purge_old_items(item_cache);
  assert_not_null(item_cache_fetch_item(item_cache, (unsigned char*) "urn:peerworks.org:entry#23", &free_when_done));
} END_TEST

START_TEST (test_purging_half_of_the_cache) {
  Item *non_purged_item = create_item_with_tokens_and_time((unsigned char*) "urn:peerworks.org:entry#23", tokens, 4, purge_time + 2);
  Item *purged_item = create_item_with_tokens_and_time((unsigned char*) "urn:peerworks.org:entry#24", tokens, 4, purge_time - 2);

  item_cache_add_item(item_cache, non_purged_item);
  item_cache_add_item(item_cache, purged_item);

  assert_not_null(item_cache_fetch_item(item_cache, (unsigned char*) "urn:peerworks.org:entry#23", &free_when_done));
  assert_not_null(item_cache_fetch_item(item_cache, (unsigned char*) "urn:peerworks.org:entry#24", &free_when_done));

  item_cache_purge_old_items(item_cache);

  assert_not_null(item_cache_fetch_item(item_cache, (unsigned char*) "urn:peerworks.org:entry#23", &free_when_done));
  assert_null(item_cache_fetch_item(item_cache, (unsigned char*) "urn:peerworks.org:entry#24", &free_when_done));
} END_TEST

START_TEST (test_purging_entire_cache_with_multiple_items) {
  Item *purged_item1 = create_item_with_tokens_and_time((unsigned char*) "urn:peerworks.org:entry#23", tokens, 4, purge_time - 1);
  Item *purged_item2 = create_item_with_tokens_and_time((unsigned char*) "urn:peerworks.org:entry#24", tokens, 4, purge_time - 2);

  item_cache_add_item(item_cache, purged_item1);
  item_cache_add_item(item_cache, purged_item2);

  assert_not_null(item_cache_fetch_item(item_cache, (unsigned char*) "urn:peerworks.org:entry#23", &free_when_done));
  assert_not_null(item_cache_fetch_item(item_cache, (unsigned char*) "urn:peerworks.org:entry#24", &free_when_done));

  item_cache_purge_old_items(item_cache);

  assert_null(item_cache_fetch_item(item_cache, (unsigned char*) "urn:peerworks.org:entry#23", &free_when_done));
  assert_null(item_cache_fetch_item(item_cache, (unsigned char*) "urn:peerworks.org:entry#24", &free_when_done));
} END_TEST

START_TEST (test_purging_half_cache_with_multiple_items_from_thread) {
  Item *non_purged_item1 = create_item_with_tokens_and_time((unsigned char*) "urn:peerworks.org:entry#21", tokens, 4, purge_time + 4);
  Item *non_purged_item2 = create_item_with_tokens_and_time((unsigned char*) "urn:peerworks.org:entry#22", tokens, 4, purge_time + 5);

  Item *purged_item1 = create_item_with_tokens_and_time((unsigned char*) "urn:peerworks.org:entry#23", tokens, 4, purge_time - 4);
  Item *purged_item2 = create_item_with_tokens_and_time((unsigned char*) "urn:peerworks.org:entry#24", tokens, 4, purge_time - 5);

  item_cache_add_item(item_cache, non_purged_item1);
  item_cache_add_item(item_cache, non_purged_item2);
  item_cache_add_item(item_cache, purged_item1);
  item_cache_add_item(item_cache, purged_item2);

  assert_not_null(item_cache_fetch_item(item_cache, (unsigned char*) "urn:peerworks.org:entry#21", &free_when_done));
  assert_not_null(item_cache_fetch_item(item_cache, (unsigned char*) "urn:peerworks.org:entry#22", &free_when_done));
  assert_not_null(item_cache_fetch_item(item_cache, (unsigned char*) "urn:peerworks.org:entry#23", &free_when_done));
  assert_not_null(item_cache_fetch_item(item_cache, (unsigned char*) "urn:peerworks.org:entry#24", &free_when_done));

  item_cache_start_purger(item_cache, 1);
  sleep(2);

  assert_not_null(item_cache_fetch_item(item_cache, (unsigned char*) "urn:peerworks.org:entry#21", &free_when_done));
  assert_not_null(item_cache_fetch_item(item_cache, (unsigned char*) "urn:peerworks.org:entry#22", &free_when_done));
  assert_null(item_cache_fetch_item(item_cache, (unsigned char*) "urn:peerworks.org:entry#23", &free_when_done));
  assert_null(item_cache_fetch_item(item_cache, (unsigned char*) "urn:peerworks.org:entry#24", &free_when_done));
} END_TEST

START_TEST (test_purge_loaded_cache_doesnt_crash) {
  item_cache_start_purger(item_cache, 1);
  item_cache_load(item_cache);
  sleep(2);
} END_TEST

/* Atomizer tests */
START_TEST (test_atomize_a_token) {
  int atom = item_cache_atomize(item_cache, "one");
  assert_equal(1, atom);
} END_TEST

START_TEST (test_atomize_a_new_token) {
  int atom = item_cache_atomize(item_cache, "new");
  assert_equal(1247, atom);
  int atom2 = item_cache_atomize(item_cache, "new");
  assert_equal(atom, atom2);
} END_TEST

START_TEST (test_globalize_a_token) {
  char *s = item_cache_globalize(item_cache, 1);
  assert_equal_s("one", s);
} END_TEST

START_TEST (test_globalize_a_missing_token_returns_NULL) {
  char *s = item_cache_globalize(item_cache, 2);
  assert_null(s);
} END_TEST

START_TEST (test_globalize_a_new_token) {
  int atom = item_cache_atomize(item_cache, "new");
  char *s = item_cache_globalize(item_cache, atom);
  assert_not_null(s);
  assert_equal_s("new", s);
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
   tcase_add_test(fetch_item_case, test_free_when_done_is_true_when_the_item_is_not_in_the_memory_cache);
   tcase_add_test(fetch_item_case, test_free_when_done_is_false_when_the_item_is_in_the_memory_cache);
   
   TCase *load = tcase_create("load");
   tcase_add_checked_fixture(load, setup_cache, teardown_item_cache);
   tcase_add_test(load, test_load_loads_the_right_number_of_items);
   tcase_add_test(load, test_load_sets_cache_loaded_to_true);
   tcase_add_test(load, test_load_respects_min_tokens);
   
   TCase *iteration = tcase_create("iteration");
   tcase_add_checked_fixture(iteration, setup_iteration, teardown_iteration);
   tcase_add_test(iteration, test_iterates_over_all_items);
   tcase_add_test(iteration, test_iteration_stops_when_iterator_returns_CLASSIFIER_FAIL);
   tcase_add_test(iteration, test_iteration_happens_in_reverse_updated_order);
   
   TCase *rndbg = tcase_create("random background");
   tcase_add_checked_fixture(rndbg, setup_cache, teardown_item_cache);
   tcase_add_test(rndbg, test_random_background_is_empty_pool_before_load);
   tcase_add_test(rndbg, test_creates_random_background_after_load);
   tcase_add_test(rndbg, test_random_background_is_correct_size);
   tcase_add_test(rndbg, test_random_background_has_right_count_for_a_token);
   
   TCase *modification = tcase_create("modification");
   tcase_add_checked_fixture(modification, setup_modification, teardown_modification);
   tcase_add_test(modification, adding_an_entry_twice_does_not_fail);
   tcase_add_test(modification, adding_an_entry_twice_does_not_add_a_duplicate);
   tcase_add_test(modification, adding_an_entry_saves_all_its_attributes);
   tcase_add_test(modification, adding_an_entry_saves_its_xml);
   tcase_add_test(modification, test_can_add_entry_without_a_feed_id);
   tcase_add_test(modification, test_destroying_an_entry_removes_its_xml_document);
   tcase_add_test(modification, test_destroying_an_entry_removes_tokens);
   tcase_add_test(modification, test_destroying_an_entry_removes_it_from_the_database_file);
   tcase_add_test(modification, test_cant_delete_an_item_that_is_used_in_the_random_background);
   tcase_add_test(modification, test_failed_deletion_doesnt_delete_tokens);
   
   TCase *loaded_modification = tcase_create("loaded modification");
   tcase_add_checked_fixture(loaded_modification, setup_loaded_modification, teardown_loaded_modification);
   tcase_add_test(loaded_modification, item_cache_add_item_returns_CLASSIFIER_OK_if_the_item_is_added);
   tcase_add_test(loaded_modification, item_cache_add_item_returns_CLASSIFIER_FAIL_if_the_item_is_not_added_because_it_doesn_have_enough_tokens);
   tcase_add_test(loaded_modification, item_cache_add_item_doesnt_add_small_items);
   tcase_add_test(loaded_modification, test_add_item_to_in_memory_arrays_adds_an_item);
   tcase_add_test(loaded_modification, test_add_item_makes_it_fetchable);
   tcase_add_test(loaded_modification, test_add_item_makes_it_iteratable);
   tcase_add_test(loaded_modification, test_add_item_puts_it_in_the_right_position);
   tcase_add_test(loaded_modification, test_add_item_puts_it_in_the_right_position_at_beginning);
   tcase_add_test(loaded_modification, test_add_item_puts_it_in_the_right_position_at_end);
   tcase_add_test(loaded_modification, test_save_item_stores_it_in_the_database);
   tcase_add_test(loaded_modification, test_save_item_without_an_entry_wont_store_it_in_the_database);
   tcase_add_test(loaded_modification, test_save_item_stores_the_correct_tokens);
   
   TCase *full_update = tcase_create("full update");
   tcase_add_checked_fixture(full_update, setup_full_update, teardown_full_update);
   tcase_set_timeout(full_update, 5);
   tcase_add_test(full_update, test_update_callback);
   tcase_add_test(full_update, test_adding_multiple_entries_causes_item_added_to_cache);
   tcase_add_test(full_update, test_adding_entry_causes_item_added_to_cache);
   tcase_add_test(full_update, test_adding_entry_causes_tokens_to_be_added_to_the_db);
 
  TCase *purging = tcase_create("purging");
  tcase_add_checked_fixture(purging, setup_purging, teardown_purging);
  tcase_add_test(purging, test_purging_cache_does_nothing_with_one_new_item);
  tcase_add_test(purging, test_purging_cache_of_one_old_item);
  tcase_add_test(purging, test_purging_cache_does_nothing_with_no_items);
  tcase_add_test(purging, test_purging_half_of_the_cache);
  tcase_add_test(purging, test_purging_entire_cache_with_multiple_items);
  tcase_add_test(purging, test_purging_half_cache_with_multiple_items_from_thread);
  tcase_add_test(purging, test_purge_loaded_cache_doesnt_crash);

  TCase *atomization = tcase_create("atomization");
  tcase_add_checked_fixture(atomization, setup_modification, teardown_modification);
  tcase_add_test(atomization, test_atomize_a_token);
  tcase_add_test(atomization, test_atomize_a_new_token);
  tcase_add_test(atomization, test_globalize_a_token);
  tcase_add_test(atomization, test_globalize_a_missing_token_returns_NULL);
  tcase_add_test(atomization, test_globalize_a_new_token);

  suite_add_tcase(s, tc_case);
  suite_add_tcase(s, fetch_item_case);
  suite_add_tcase(s, load);
  suite_add_tcase(s, iteration);
  suite_add_tcase(s, rndbg);
  suite_add_tcase(s, modification);
  suite_add_tcase(s, loaded_modification);
  suite_add_tcase(s, full_update);
  suite_add_tcase(s, purging);
  suite_add_tcase(s, atomization);
  return s;
}

int main(void) {
  initialize_logging("test.log");
  int number_failed;

  SRunner *sr = srunner_create(item_cache_suite());
  srunner_run_all(sr, CK_NORMAL);
  number_failed = srunner_ntests_failed(sr);
  srunner_free(sr);
  close_log();
  return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
