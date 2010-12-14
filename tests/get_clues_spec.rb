#!/usr/bin/env ruby
#
# Copyright (c) 2007-2010 The Kaphan Foundation
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

require File.dirname(__FILE__) + "/spec_helper.rb"
require 'json'

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
  
  it "should return 424 if the tag is not in the cache" do
    classifier_http do |http|
      http.send_request('GET', '/classifier/clues?item=urn:peerworks.org:entry#709254&tag=foo').code.should == "424"
    end
  end
  
  it "should return 404 if the tag is not able to be fetched" do
    classifier_http do |http|
      http.send_request('GET', '/classifier/clues?item=urn:peerworks.org:entry#709254&tag=foo').code.should == "424" # The first request triggers the fetching
    end
      sleep(1)
    classifier_http do |http|
      http.send_request('GET', '/classifier/clues?item=urn:peerworks.org:entry#709254&tag=foo').code.should == "404"
    end
  end
  
  it "should return 200 if the tag was able to be fetched" do
    tag = "file:#{File.join(File.expand_path(File.dirname(__FILE__)), 'fixtures', 'complete_tag.atom')}"
    classifier_http do |http|
      http.send_request('GET', '/classifier/clues?item=urn:peerworks.org:entry#709254&tag=' + tag).code.should == "424" # The first request triggers the fetching
    end
      sleep(1)
    classifier_http do |http|
      http.send_request('GET', '/classifier/clues?item=urn:peerworks.org:entry#709254&tag=' + tag).code.should == "200"
    end
  end
  
  it "should return clues in the body if the tag was able to be fetched" do
    tag = "file:#{File.join(File.expand_path(File.dirname(__FILE__)), 'fixtures', 'complete_tag.atom')}"
    classifier_http do |http|
      http.send_request('GET', '/classifier/clues?item=urn:peerworks.org:entry#709254&tag=' + tag).code.should == "424" # The first request triggers the fetching
    end
      sleep(1)
    classifier_http do |http|
      response = http.send_request('GET', '/classifier/clues?item=urn:peerworks.org:entry#709254&tag=' + tag)
      
      response['content-type'].should == "application/json"
      json = nil
      lambda { json = JSON.parse(response.body) }.should_not raise_error(JSON::ParserError)
      json.size.should > 0
      json.first['clue'].should == 'bar'
      json.first['prob'].should == 0.054373
    end
  end
  
  it "should still work if the tag is incomplete" do
    tag = "file:#{File.join(File.expand_path(File.dirname(__FILE__)), 'fixtures', 'incomplete_tag.atom')}"
    classifier_http do |http|
      http.send_request('GET', '/classifier/clues?item=urn:peerworks.org:entry#709254&tag=' + tag).code.should == "424" # The first request triggers the fetching
    end
    sleep(1)
    classifier_http do |http|
      http.send_request('GET', '/classifier/clues?item=urn:peerworks.org:entry#709254&tag=' + tag).code.should == "200"
    end
  end
end
