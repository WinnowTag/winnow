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
    @sqlite = SQLite3::Database.open(Database)
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
    
    it "should store entry in the database" do
      create_entry
      @sqlite.get_first_value("select count(*) from entries where full_id = 'urn:peerworks.org:entries#1111'").should == "1"
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
      @sqlite.get_first_value("select count(*) from entry_tokens where entry_id = (select id from entries where full_id = 'urn:peerworks.org:entries#1111')").to_i.should > 0
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
    
    it "should remove the tokens from the database" do
      @sqlite.get_first_value("select count(*) from entry_tokens where entry_id = 888769").to_i.should > 0
      destroy_entry(888769)
      @sqlite.get_first_value("select count(*) from entry_tokens where entry_id = 888769").to_i.should == 0
    end
  end
  
  describe "number of items classified" do
    before(:each) do
      @item_count = @sqlite.get_first_value("select count(*) from entries;").to_i
      @http = TestHttpServer.new(:port => 8888)
    end
    
    describe "after item addition" do
      before(:each) do
        start_tokenizer
      end
      
      after(:each) do
        system("tokenizer_control stop")
      end
      
      it "should include the added item" do
        create_entry
        sleep(1) # let the item get into the cache
        run_job
        Tagging.count(:conditions => "classifier_tagging = 1 and tag_id = 48").should == (@item_count + 1)        
      end
      
      it "should automatically classify the new item" do
        run_job
        Tagging.count(:conditions => "classifier_tagging = 1 and tag_id = 48").should == @item_count
        
        create_entry
        sleep(2.5) # wait for it to be classified
        Tagging.count(:conditions => "classifier_tagging = 1 and tag_id = 48").should == (@item_count + 1)
      end
      
      it "should only create a single job that classifies both items for each tag" do
        run_job
        Tagging.count(:conditions => "classifier_tagging = 1 and tag_id = 48").should == @item_count
        
        create_entry(:id => "1111")
        create_entry(:id => "1112")
        sleep(2)
        Tagging.count(:conditions => "classifier_tagging = 1 and tag_id = 48").should == (@item_count + 2)
        `wc -l /tmp/perf.log`.to_i.should == Tagging.count(:select => 'distinct tag_id') + 2 # +1 for header, +1 for previous job
      end
    end
  end
end
