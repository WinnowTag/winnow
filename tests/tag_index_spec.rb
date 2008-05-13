#!/usr/bin/env ruby
#
# Copyright (c) 2008 The Kaphan Foundation
#
# Possession of a copy of this file grants no permission or license
# to use, modify, or create derivate works.
# Please contact info@peerworks.org for further information.
#

require File.dirname(__FILE__) + "/spec_helper.rb"

describe "Tag Index" do
  before(:each) do 
    system("rm /tmp/perf.log")   
    start_classifier(:tag_index => 'http://localhost:8888/tags.atom', :sleep => false)
    @http = TestHttpServer.new(:port => 8888)
  end
  
  after(:each) do
    stop_classifier
  end
  
  it "should fetch GET tag index on load" do
    @http.should receive_request("/tags.atom") do |req, res|
      req.request_method.should == 'GET'
    end
  end
  
  it "should handle 404 of tag index load" do
    @http.should receive_request("/tags.atom") do |req, res|
      res.status = 404
    end
  end
  
  it "should handle junk in the response" do
    @http.should receive_request("/tags.atom") do |req, res|
      res.body = "blah blah blah"
    end
  end
  
  describe "after item addition" do
    it "should fetch again after adding an item"
    it "should include a IF-MODIFIED-SINCE header"
  end
end