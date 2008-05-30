#!/usr/bin/env ruby
#
# Copyright (c) 2008 The Kaphan Foundation
#
# Possession of a copy of this file grants no permission or license
# to use, modify, or create derivate works.
# Please contact info@peerworks.org for further information.
#

require File.dirname(__FILE__) + "/spec_helper.rb"

describe "get_clues" do
  before(:each) do
    start_classifier
  end
  
  after(:each) do
    stop_classifier
  end
  
  it "should return a 404 for an item that doesn't exist" do
    classifier_http do |http|
      http.send_request('GET', '/classifier/clues?tag=foo&item=blah').code.should == "404"
    end
  end
  
  it "should return a message for an item that doesn't exist" do
    classifier_http do |http|
      http.send_request('GET', '/classifier/clues?tag=foo&item=blah').body.should == "The item does not exist in the classifier's item cache."
    end
  end
  
  it "should return a 400 if the item parameter was not set" do
    classifier_http do |http|
      http.send_request('GET', '/classifier/clues?tag=foo').code.should == "400"
    end
  end
  
  it "should return a message if the item parameter was not set" do
    classifier_http do |http|
      http.send_request('GET', '/classifier/clues?tag=foo').body.should == "Missing item parameter"
    end
  end
  
  it "should return a 400 if the tag parameter was not set" do
    classifier_http do |http|
      http.send_request('GET', '/classifier/clues?item=blah').code.should == "400"
    end
  end
  
  it "should return a message if the tag parameter was not set" do
    classifier_http do |http|
      http.send_request('GET', '/classifier/clues?item=blah').body.should == "Missing tag parameter"
    end
  end
  
  it "should return 405 if the request is not GET" do
    classifier_http do |http|
      http.send_request('POST', '/classifier/clues?item=blah&tag=foo').code.should == "405"
    end
  end
  
  it "should return 302 if the tag is not in the cache" do
    classifier_http do |http|
      http.send_request('GET', '/classifier/clues?item=urn:peerworks.org:entry#709254&tag=foo').code.should == "302"
    end
  end
  
  it "should return 404 if the tag is not able to be fetched" do
    classifier_http do |http|
      http.send_request('GET', '/classifier/clues?item=urn:peerworks.org:entry#709254&tag=foo').code.should == "302" # The first request triggers the fetching
    end
      sleep(1)
    classifier_http do |http|
      http.send_request('GET', '/classifier/clues?item=urn:peerworks.org:entry#709254&tag=foo').code.should == "404"
    end
  end
  
  it "should return 200 if the tag was able to be fetched" do
    tag = "file:#{File.join(File.expand_path(File.dirname(__FILE__)), 'fixtures', 'complete_tag.atom')}"
    classifier_http do |http|
      http.send_request('GET', '/classifier/clues?item=urn:peerworks.org:entry#709254&tag=' + tag).code.should == "302" # The first request triggers the fetching
    end
      sleep(1)
    classifier_http do |http|
      http.send_request('GET', '/classifier/clues?item=urn:peerworks.org:entry#709254&tag=' + tag).code.should == "200"
    end
  end
  
  it "should return 424 if the tag is incomplete" do
    tag = "file:#{File.join(File.expand_path(File.dirname(__FILE__)), 'fixtures', 'incomplete_tag.atom')}"
    classifier_http do |http|
      http.send_request('GET', '/classifier/clues?item=urn:peerworks.org:entry#709254&tag=' + tag).code.should == "302" # The first request triggers the fetching
    end
      sleep(1)
    classifier_http do |http|
      http.send_request('GET', '/classifier/clues?item=urn:peerworks.org:entry#709254&tag=' + tag).code.should == "424"
    end
  end
  
  it "should return a message if the tag is incomplete" do
    tag = "file:#{File.join(File.expand_path(File.dirname(__FILE__)), 'fixtures', 'incomplete_tag.atom')}"
    classifier_http do |http|
      http.send_request('GET', '/classifier/clues?item=urn:peerworks.org:entry#709254&tag=' + tag).code.should == "302" # The first request triggers the fetching
    end
      sleep(1)
    classifier_http do |http|
      http.send_request('GET', '/classifier/clues?item=urn:peerworks.org:entry#709254&tag=' + tag).body.should == "The classifier is missing some items required to perform this operation.  Please try again later."
    end
  end
end
