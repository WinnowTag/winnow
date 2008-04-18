-- Migration from version 1 - 2 of classifier database.
--
-- Changes entries.feed_id to remove the NOT NULL
begin;

CREATE TEMPORARY TABLE TEMP_TABLE(
  id integer PRIMARY KEY, 
  full_id text, 
  title text, 
  author text, 
  alternate text, 
  self text, 
  spider text, 
  content text, 
  updated real, 
  feed_id integer, 
  created_at real
);

INSERT INTO TEMP_TABLE SELECT id, full_id, title, author, alternate, self, spider, content, updated, feed_id, created_at FROM entries;
DROP TABLE entries;

CREATE TABLE entries (
  id integer NOT NULL PRIMARY KEY, 
  full_id text, 
  title text, 
  author text, 
  alternate text, 
  self text, 
  spider text, 
  content text, 
  updated real, 
  feed_id integer, 
  created_at real  
);
  

INSERT INTO entries SELECT id, full_id, title, author, alternate, self, spider, content, updated, feed_id, created_at FROM TEMP_TABLE;
DROP TABLE TEMP_TABLE;

-- Trigger prevent non-null feed ids that aren't feeds
CREATE TRIGGER entry_feed_insert
  BEFORE INSERT ON entries
  FOR EACH ROW BEGIN
      SELECT RAISE(ROLLBACK, 'insert on table "entries" violates foreign key constraint "entry_feed"')
      WHERE  NEW.feed_id IS NOT NULL
             AND (SELECT id FROM feeds WHERE id = new.feed_id) IS NULL;
  END;
  
-- Cascade deletes to tokens
CREATE TRIGGER entry_tokens_entry_id
  BEFORE DELETE ON entries
  FOR EACH ROW BEGIN
      DELETE from entry_tokens WHERE entry_id = OLD.id;
  END;
  
-- Don't delete background items
CREATE TRIGGER random_backgrounds_entry_id
  BEFORE DELETE ON entries
  FOR EACH ROW BEGIN
      SELECT RAISE(ROLLBACK, 'delete on table "entries" violates foreign key constraint "random_backgrounds_entry_id"')
      WHERE (SELECT entry_id FROM random_backgrounds WHERE entry_id = OLD.id) IS NOT NULL;
  END;

CREATE INDEX entry_updated on entries(updated);
CREATE UNIQUE INDEX entry_full_id on entries(full_id);

PRAGMA user_version = 2;

commit;
