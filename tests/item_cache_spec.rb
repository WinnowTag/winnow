#!/usr/bin/env ruby
#
# Copyright (c) 2008 The Kaphan Foundation
#
# Possession of a copy of this file grants no permission or license
# to use, modify, or create derivate works.
# Please contact info@peerworks.org for further information.
#

begin
  require 'spec'
rescue LoadError
  require 'rubygems'
  gem 'rspec'
  require 'spec'
end

gem 'ratom'
require 'atom'
require 'atom/pub'
require 'sqlite3'

CLASSIFIER_URL = "http://localhost:8008"
ROOT = File.expand_path(File.dirname(__FILE__))
Database = File.join(ROOT, 'fixtures/copy.db')

require 'active_record'
require 'active_resource'
class Tagging < ActiveRecord::Base; end
class Job < ActiveResource::Base 
  self.site = CLASSIFIER_URL + "/classifier"
end

describe "The Classifier's Item Cache" do
  before(:all) do
    Tagging.establish_connection(:adapter => 'mysql', :database => 'classifier_test', :username => 'seangeo', :password => 'seangeo')
  end
  
  before(:each) do
    system("cp #{File.join(ROOT, 'fixtures/valid.db')} #{Database}")
    start_classifier
    @sqlite = SQLite3::Database.open(Database)
  end
  
  after(:each) do
    system("kill `cat classifier.pid`")
    @sqlite.close
  end
  
  describe "feed creation" do    
    it "should create a feed without error" do
      lambda { create_feed(:title => 'My new feed', :id => 'urn:peerworks.org:feeds#1337') }.should_not raise_error
    end
    
    it "should store feed in the database" do
      create_feed(:title => 'My new feed', :id => 'urn:peerworks.org:feeds#1337')
      @sqlite.get_first_value("select title from feeds where id = 1337").should == 'My new feed'
    end
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
    
    it "should store entry in the database" do
      create_entry
      @sqlite.get_first_value("select count(*) from entries where id = 1111").should == "1"
    end
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
      @sqlite.get_first_value("select count(*) from entry_tokens where entry_id = 1111").to_i.should > 0
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
    end
    
    it "should be equal to the number of items in the cache" do
      job = Job.create(:tag_id => 48)
      while job.progress < 100
        job.reload
      end
      
      Tagging.count(:conditions => "classifier_tagging = 1 and tag_id = 48").should == @item_count
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
        job = Job.create(:tag_id => 48)
        while job.progress < 100
          job.reload
        end

        Tagging.count(:conditions => "classifier_tagging = 1 and tag_id = 48").should == (@item_count + 1)        
      end
    end
  end
  
  def create_feed(opts)
    collection = Atom::Pub::Collection.new(:href => CLASSIFIER_URL + '/feeds')
    feed_entry = Atom::Entry.new(opts)
    collection.publish(feed_entry)
  end
  
  def create_entry
    collection = Atom::Pub::Collection.new(:href => CLASSIFIER_URL + '/feeds/426/feed_items')
    entry = Atom::Entry.new do |entry|
      entry.title = 'My Feed'
      entry.id = "urn:peerworks.org:entries#1111"
      entry.links << Atom::Link.new(:href => 'http://example.org/1111.html', :rel => 'alternate')
      entry.links << Atom::Link.new(:href => 'http://example.org/1111.atom', :rel => 'self')
      entry.updated = Time.now
      entry.content = Atom::Content::Html.new("this is the html content for entry 1111 there should be enough to tokenize")
    end
    
    collection.publish(entry)
  end
  
  def destroy_entry(id)
    Atom::Entry.new do |e|
      e.links << Atom::Link.new(:href => CLASSIFIER_URL + "/feed_items/#{id}", :rel => 'edit')
    end.destroy!
  end
  
  def start_classifier    
    system("#{File.join(ROOT, "../src/classifier")} -d --pid classifier.pid " +
                                                   "-l classifier-item_cache_spec.log " +
                                                   "-c #{File.join(ROOT, "fixtures/real-db.conf")} " +
                                                   "--db #{Database} 2> /dev/null")
    sleep(0.0001)
  end
  
  def start_tokenizer
    system("tokenizer_control start -- -p8009 #{Database}")
    sleep(1)
  end
end
