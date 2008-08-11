#!/usr/bin/env ruby
#
# Copyright (c) 2008 The Kaphan Foundation
#
# Possession of a copy of this file grants no permission or license
# to use, modify, or create derivate works.
# Please contact info@peerworks.org for further information.
#

require File.dirname(__FILE__) + "/spec_helper.rb"

describe "The Classifier's Item Cache" do
  before(:all) do
    Tagging.establish_connection(:adapter => 'mysql', :database => 'classifier_test', :username => 'classifier', :password => 'classifier')
  end
  
  before(:each) do
    system("rm /tmp/perf.log")   
    start_classifier
    @sqlite = SQLite3::Database.open("#{Database}/catalog.db")
  end
  
  after(:each) do
    @sqlite.close
    stop_classifier
  end
  
  describe "feed creation" do    
    it "should create a feed without error" do
      lambda { create_feed(:title => 'My new feed', :id => 'urn:peerworks.org:feeds#1337') }.should_not raise_error
    end
    
    it "should create a feed without a title without error" do
      lambda { create_feed(:title => nil, :id => 'urn:peerworks.org:feeds#1337') }.should_not raise_error
    end
    
    it "should store feed in the database" do
      create_feed(:title => 'My new feed', :id => 'urn:peerworks.org:feeds#1337')
      @sqlite.get_first_value("select title from feeds where id = 1337").should == 'My new feed'
    end
    
    it "should return 400 when there is no content"
    it "should return 202 when updating a feed"
  end
  
  describe "feed deletion" do
    it "should delete the feed without error" do
      lambda { create_feed(:title => 'My new feed', :id => 'urn:peerworks.org:feeds#1337').destroy! }.should_not raise_error
    end
    
    it "should remove the feed from the database" do
      create_feed(:title => 'My new feed', :id => 'urn:peerworks.org:feeds#1337').destroy!
      @sqlite.get_first_value("select title from feeds where id = 1337").should be_nil
    end
  end
  
  describe "entry creation" do
    it "should create an entry without error" do
      lambda { create_entry }.should_not raise_error
    end
    
    it "should create an content-less entry without error" do
      lambda { create_empty_entry }.should_not raise_error
    end
    
    it "should create an big entry without error" do
      lambda { create_big_entry }.should_not raise_error
    end
    
    it "should create another big entry without error" do
      lambda { create_another_big_entry }.should_not raise_error
    end
    
    it "should create big entry followed by a small entry without error" do
      lambda { create_another_big_entry }.should_not raise_error
      lambda { create_empty_entry }.should_not raise_error
    end
    
    it "should store entry in the database" do
      create_entry
      @sqlite.get_first_value("select count(*) from entries where full_id = 'urn:peerworks.org:entries#1111'").should == "1"
    end
    
    it "should store the XML in the file system" do
      create_entry
      id = @sqlite.get_first_value("select id from entries where full_id = 'urn:peerworks.org:entries#1111'")
      File.exists?(File.join(Database, "items", "#{id}.atom")).should be_true
    end
    
    it "should update an existing item in the database" do
      @sqlite.execute("insert into entries (full_id) values ('urn:peerworks.org:entries#1111')")
      create_entry
      @sqlite.get_first_value("select updated from entries where full_id = 'urn:peerworks.org:entries#1111'").should_not be_nil
    end
  
    it "should properly parse the updated date" do
      create_entry(:updated => Time.now.yesterday.yesterday)
      r = @sqlite.get_first_row("select updated, created_at from entries where full_id = 'urn:peerworks.org:entries#1111'")
      r[0].to_f.should < r[1].to_f
    end
    
    it "should return 400 for an empty entry"
    it "should return 422 when adding an entry to an non-existent feed"
    it "should return 405 when trying to GET the feed_items"
    
  end
  
  describe "entry tokenization" do
    before(:each) do
      start_tokenizer
    end
    
    after(:each) do
      system("tokenizer_control stop")
    end
    
    it "should tokenize the item" do
      create_entry
      sleep(1)
      id = @sqlite.get_first_value("select id from entries where full_id = 'urn:peerworks.org:entries#1111'")
      File.exists?(File.join(Database, "tokens", "#{id}.tokens")).should be_true
    end
    
    it "should insert tokens on the correct item when an item is added twice" do
      create_entry
      create_entry
      sleep(1)
      id = @sqlite.get_first_value("select id from entries where full_id = 'urn:peerworks.org:entries#1111'")
      File.exists?(File.join(Database, "tokens", "#{id}.tokens")).should be_true
    end
    
    it "should tokenize an existing item if it doesn't have tokens" do
      @sqlite.execute("insert into entries (full_id) values ('urn:peerworks.org:entries#1111')")
      create_entry
      sleep(1)
      id = @sqlite.get_first_value("select id from entries where full_id = 'urn:peerworks.org:entries#1111'")
      File.exists?(File.join(Database, "tokens", "#{id}.tokens")).should be_true
    end
  end
  
  describe "entry deletion" do
    it "should delete the entry without error" do
      lambda { create_entry.destroy! }.should_not raise_error
    end
    
    it "should remove the entry from the database" do
      create_entry.destroy!
      @sqlite.get_first_value("select count(*) from entries where id = 1111").should == "0"
    end
    
    it "should remove the XML from the cache" do
      File.exists?(File.join(Database, "items", "888769.atom")).should be_true
      destroy_entry(888769)
      File.exists?(File.join(Database, "items", "888769.atom")).should be_false
    end
    
    it "should remove the tokens from the database" do
      File.exists?(File.join(Database, "tokens", "888769.tokens")).should be_true
      destroy_entry(888769)
      File.exists?(File.join(Database, "tokens", "888769.tokens")).should be_false
    end
  end  
end

describe "Item Cache Authentication" do
  before(:each) do
    system("rm /tmp/perf.log")   
    start_classifier(:credentials => File.join(ROOT, 'fixtures', 'credentials.js'))
  end
  
  after(:each) do
    stop_classifier
  end
  
  it "should reject unauthenticated create feed requests" do
    lambda { create_feed }.should raise_error
  end
  
  it "should allow authenticated create feed requests" do
    lambda { create_feed(:access_id => 'collector_id', :secret => 'collector_secret') }.should_not raise_error
  end
  
  it "should reject unauthenticated create entry requests" do
    lambda { create_entry }.should raise_error
  end
  
  it "should allow authenticated create entry requests" do
    lambda { create_entry(:access_id => 'collector_id', :secret => 'collector_secret') }.should_not raise_error
  end
end
