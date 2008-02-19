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

commit;