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

describe "Tag Index" do
  before(:each) do 
    @http = TestHttpServer.new(:port => 8888)
  end
  
  after(:each) do
    stop_classifier
  end
  
  it "should fetch GET tag index on load" do
    @http.should_receive do
      request("/tags.atom") do |req, res|
        req.request_method.should == 'GET'
      end
    end
    
    start_classifier(:tag_index => 'http://localhost:8888/tags.atom', :sleep => false)
    
    @http.should have_received_requests
  end
  
  it "should handle 404 of tag index load" do
    @http.should_receive do
      request("/tags.atom") do |req, res|
        res.status = 404
      end
    end
    
    start_classifier(:tag_index => 'http://localhost:8888/tags.atom', :sleep => false)
    @http.should have_received_requests
  end
  
  it "should handle junk in the response" do
    @http.should_receive do
      request("/tags.atom") do |req, res|
        res.body = "blah blah blah"
      end
    end
    
    start_classifier(:tag_index => 'http://localhost:8888/tags.atom', :sleep => false)
    @http.should have_received_requests
  end
  
  it "should handle real content" do
    @http.should_receive do 
      request("/tags.atom") do |req, res|
        res.body = File.read(File.dirname(__FILE__) + "/fixtures/tag_index.atom")
      end
    end
    
    start_classifier(:tag_index => 'http://localhost:8888/tags.atom', :sleep => false)
    @http.should have_received_requests
  end
end

describe "after item addition" do
  before(:each) do
    @http = TestHttpServer.new(:port => 8888)
  end

  after(:each) do
    stop_classifier
  end

  it "should fetch again after adding an item" do
    requests = 0
    @http.should_receive do
      request("/tags.atom", 2) do |req, res|
        requests += 1
        if requests == 1
          res.body = File.read(File.dirname(__FILE__) + "/fixtures/tag_index.atom")
        else
          req['IF-MODIFIED-SINCE'].should == "Mon, 12 May 2008 02:42:14 GMT"
          res.status = 304
        end
      end
    end

    sleep(0.1)
    start_classifier(:tag_index => 'http://localhost:8888/tags.atom', :sleep => false)
    create_entry
    @http.should have_received_requests
  end    
end