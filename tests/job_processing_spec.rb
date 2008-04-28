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
    stop_classifier
    @http.shutdown
  end
  
  it "should attempt to fetch a tag for the URL given in the Job description" do
    job = create_job('http://localhost:8888/mytag-training.atom')
    @http.should receive_request("/mytag-training.atom") do |r|
      r.request_method.should == 'GET'
    end    
  end
    
  it "should accept application/atom+xml" do
    job = create_job('http://localhost:8888/mytag-training.atom')
    @http.should receive_request("/mytag-training.atom") do |r|
      r['ACCEPT'].should == "application/atom+xml"
    end
  end
  
  def create_job(training_url)
    job = Job.create(:tag_url => training_url)
    job.errors.should be_empty
    job
  end
end