# Copyright (c) 2008 The Kaphan Foundation
#
# Possession of a copy of this file grants no permission or license
# to use, modify, or create derivate works.
# Please contact info@peerworks.org for further information.
#

require File.dirname(__FILE__) + "/spec_helper.rb"
require File.dirname(__FILE__) + "/test_http_server.rb"

gem 'ratom'
require 'atom'

describe "Classifier Job Processing" do
  before(:each) do
    system("rm /tmp/perf.log")   
    start_classifier
    @http = TestHttpServer.new(:port => 8888)
  end
  
  after(:each) do
    stop_classifier
    @http.shutdown
  end
  
  it "should attempt to fetch a tag for the URL given in the Job description" do
    job = create_job('http://localhost:8888/mytag-training.atom')
    @http.should receive_request("/mytag-training.atom") do |req, res|
      req.request_method.should == 'GET'
    end    
  end
  
  it "should be a buffer against something" do
    # This is an empty test, it exists to provide a buffer between the first test
    # and subsequent tests since for some reason the second test always fails and
    # I think it is a timing thing.  This seems to fix it, but I have a feeling
    # I'll be fighting against these timing problems forever.
    job = create_job('http://localhost:8888/mytag-training.atom')
  end  
  
  it "should not include IF-MODIFIED-SINCE on first request" do
    job = create_job('http://localhost:8888/mytag-training.atom')
    @http.should receive_request("/mytag-training.atom") do |req, res|
      req['IF-MODIFIED-SINCE'].should be_nil
    end    
  end
  
  it "should include IF-MODIFIED-SINCE on second request" do
    job = create_job('http://localhost:8888/mytag-training.atom')
    @http.should receive_request("/mytag-training.atom") do |req, res|
      res.body = File.read(File.join(File.dirname(__FILE__), 'fixtures', 'complete_tag.atom'))
    end
    
    sleep(1)
    create_job('http://localhost:8888/mytag-training.atom')    
    
    @http.should receive_request("/mytag-training.atom") do |req, res|
      req['IF-MODIFIED-SINCE'].should == "Sun, 30 Mar 2008 01:24:18 GMT" # Date in atom:updated
      res.status = 304
    end
  end
    
  it "should accept application/atom+xml" do
    job = create_job('http://localhost:8888/mytag-training.atom')
    @http.should receive_request("/mytag-training.atom") do |req, res|
      req['ACCEPT'].should == "application/atom+xml"
    end
  end
  
  it "should not crash if it gets a 404" do
    job = create_job('http://localhost:8888/mytag-training.atom')
    @http.should receive_request("/mytag-training.atom") do |req, res|
      res.status = 404
    end
    
    should_not_crash
  end
  
  it "should not crash if it gets a 500" do
    job = create_job('http://localhost:8888/mytag-training.atom')
    @http.should receive_request("/mytag-training.atom") do |req, res|
      res.status = 500
    end
    
    should_not_crash
  end
  
  it "should not crash if it gets junk back in the body" do
    job = create_job('http://localhost:8888/mytag-training.atom')
    @http.should receive_request("/mytag-training.atom") do |req, res|
      res.body = "blahbalh"
    end
    
    should_not_crash
  end
  
  it "should PUT results to the classifier url" do    
    job_results do |req, res|
      res.request_method.should == 'PUT'
    end
  end
  
  it "should have application/atom+xml as the content type" do
    job_results do |req, res|
      req['content-type'].should == 'application/atom+xml'
    end
  end
  
  it "should have the classifier in the user agent" do
    job_results do |req, res|
      req['user-agent'].should match(/classifier/)
    end
  end
  
  it "should have a valid atom document as the body" do
    job_results do |req, res|
      lambda { Atom::Feed.load_feed(req.body) }.should_not raise_error
    end
  end
  
  it "should have a tag's id as the id of the feed in the atom document" do
    job_results do |req, res|
      feed = Atom::Feed.load_feed(req.body)
      feed.id.should == "http://trunk.mindloom.org:80/seangeo/tags/a-religion"
    end
  end
  
  it "should have an entry for each item" do
    job_results do |req, res|
      feed = Atom::Feed.load_feed(req.body)
      feed.should have(10).entries
    end
  end
  
  it "should have ids for each entry that match the item id" do
    job_results do |req, res|
      feed = Atom::Feed.load_feed(req.body)
      feed.entries.each do |e|
        e.id.should match(/urn:peerworks.org:entry#/)
      end
    end
  end
  
  it "should have a single category for each entry" do
    job_results do |req, res|
      feed = Atom::Feed.load_feed(req.body)
      feed.entries.each do |e|
        e.should have(1).categories
      end
    end
  end
  
  it "should have a strength attribute on each category"
  it "should have a term and scheme that match the term and scheme on the tag definition"
  it "should include a new classified date on the tag"
  
  def job_results
    job = create_job('http://localhost:8888/mytag-training.atom')
    @http.should receive_requests(5) {|http|
      http.request("/mytag-training.atom") do |req, res|
        res.body = File.read(File.join(File.dirname(__FILE__), 'fixtures', 'complete_tag.atom'))
      end
      http.request("/results") do |req, res|
        yield(req, res)
      end
    }
    job
  end
  
  def should_not_crash
    lambda {create_job('http://localhost:8888/mytag-training.atom')}.should_not raise_error
  end
end
