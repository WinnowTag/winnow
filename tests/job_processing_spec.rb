# Copyright (c) 2008 The Kaphan Foundation
#
# Possession of a copy of this file grants no permission or license
# to use, modify, or create derivate works.
# Please contact info@peerworks.org for further information.
#

require File.dirname(__FILE__) + "/spec_helper.rb"
require File.dirname(__FILE__) + "/test_http_server.rb"

describe "Classifier Job Processing" do
  before(:each) do
    @http = TestHttpServer.new(:port => 8888)
    system("rm /tmp/perf.log")   
    start_classifier
  end
  
  after(:each) do
    @http.shutdown
    stop_classifier
  end
  
  it "should attempt to fetch a tag for the URL given in the Job description" do
    job = create_job('http://localhost:8888/mytag-training.atom')
    @http.should receive_request("/mytag-training.atom", 10) do |req, res|
      req.request_method.should == 'GET'
    end    
  end
  
  it "should not include IF-MODIFIED-SINCE on first request" do
    job = create_job('http://localhost:8888/mytag-training.atom')
    @http.should receive_request("/mytag-training.atom", 10) do |req, res|
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
    job = create_job('http://localhost:8888/mytag-training.atom')
    @http.should receive_requests(5) {|http|
      http.request("/mytag-training.atom") do |req, res|
        res.body = File.read(File.join(File.dirname(__FILE__), 'fixtures', 'complete_tag.atom'))
      end
      http.request("/results") do |req, res|
        res.request_method.should == 'PUT'
      end
    }
  end
  
  def should_not_crash
    lambda {create_job('http://localhost:8888/mytag-training.atom')}.should_not raise_error
  end
end
