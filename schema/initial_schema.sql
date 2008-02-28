-- This schema uses Atom 1.0 nomenclature --
begin;

pragma user_version = 1;

create table "feeds" (
  "id"          integer NOT NULL PRIMARY KEY,
  "title"       text
);
  
create table "entries" (
  "id"          integer NOT NULL PRIMARY KEY,
  "full_id"     text,
  "title"       text,
  "author"      text,
  "alternate"   text,
  "self"        text,
  "content"     text,
  "updated"     real,
  "feed_id"     integer NOT NULL,
  "created_at"  real,
  constraint "entry_feed" foreign key ("feed_id")
    references "feeds" ("id") ON DELETE CASCADE
);
  
create table "tokens" (
  "id"       integer NOT NULL PRIMARY KEY,
  "token"    text    NOT NULL UNIQUE
);
  
create table "entry_tokens" (
  "entry_id" integer NOT NULL,
  "token_id"     integer NOT NULL,
  "frequency"    integer NOT NULL,
  primary key ("entry_id", "token_id"),
  constraint "entry_tokens_entry_id" foreign key ("entry_id")
    references "entries" ("id") ON DELETE CASCADE,
  constraint "entry_tokens_token_id" foreign key ("token_id")
    references "tokens" ("id")
);

create table "random_backgrounds" (
  "entry_id" integer NOT NULL PRIMARY KEY,
  constraint "random_backgrounds_entry_id" foreign key ("entry_id")
    references "entries" ("id")
);

-- Triggers to handle foreign keys

-- Cascading delete from entries to entry_tokens
create trigger entry_tokens_entry_id
  BEFORE DELETE ON entries
  FOR EACH ROW BEGIN
      DELETE from entry_tokens WHERE entry_id = OLD.id;
  END;

-- Cascade delete from feeds to entries  
create trigger entry_feed
  BEFORE DELETE ON feeds
  FOR EACH ROW BEGIN
      DELETE from entries WHERE feed_id = OLD.id;
  END;
  
-- Prevent deletion of tokens when there is a matching entry_token
CREATE TRIGGER entry_tokens_token_id
  BEFORE DELETE ON tokens
  FOR EACH ROW BEGIN
      SELECT RAISE(ROLLBACK, 'delete on table "tokens" violates foreign key constraint "entry_tokens_token_id"')
      WHERE (SELECT token_id FROM entry_tokens WHERE token_id = OLD.id) IS NOT NULL;
  END;

-- Prevent deletion of items that are in the random background  
CREATE TRIGGER random_backgrounds_entry_id
  BEFORE DELETE ON entries
  FOR EACH ROW BEGIN
      SELECT RAISE(ROLLBACK, 'delete on table "entries" violates foreign key constraint "random_backgrounds_entry_id"')
      WHERE (SELECT entry_id FROM random_backgrounds WHERE entry_id = OLD.id) IS NOT NULL;
  END;
  
commit;