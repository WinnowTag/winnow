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

def start_the_world
  system("cp #{File.join(ROOT, 'fixtures/valid.db')} #{File.join(ROOT, 'fixtures/copy.db')}")
  system("#{File.join(ROOT, "../src/classifier")} -d --pid classifier.pid " +
                                                 "-l classifier-item_cache_spec.log " +
                                                 "-c #{File.join(ROOT, "fixtures/real-db.conf")} " +
                                                 "--db #{File.join(ROOT, 'fixtures/copy.db')} 2> /dev/null")
  sleep(0.0001)
end

describe "The Classifier's Item Cache" do
  before(:each) do
    start_the_world
    @sqlite = SQLite3::Database.open(File.join(ROOT, "fixtures/copy.db"))
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
  
  def create_feed(opts)
    collection = Atom::Pub::Collection.new(:href => CLASSIFIER_URL + '/feeds')
    feed_entry = Atom::Entry.new(opts)
    collection.publish(feed_entry)
  end
end