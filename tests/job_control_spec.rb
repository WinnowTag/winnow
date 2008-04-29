# Copyright (c) 2008 The Kaphan Foundation
#
# Possession of a copy of this file grants no permission or license
# to use, modify, or create derivate works.
# Please contact info@peerworks.org for further information.
#

require File.dirname(__FILE__) + "/spec_helper.rb"

describe "Classifier Job Processing" do
  before(:each) do
    system("rm /tmp/perf.log")   
    start_classifier
  end
  
  after(:each) do
    stop_classifier
  end
  
  describe "POST job" do
    it "should return 422 when the tag id is missing" do
      Job.new.save.should be_false
    end
    
    it "should return 400 with invalid xml" do
      classifier_http do |http|
        http.request_post("/classifier/jobs", "blahblah").code.should == "400"
      end
    end
    
    it "should return 415 with missing xml" do
      classifier_http do |http|
        http.request_post("/classifier/jobs", "").code.should == "415"
      end
    end
    
    it "should return 201 when job is valid" do
      Job.new(:tag_url => "http://localhost/tag.atom").save.should be_true
    end
  end
  
  describe "DELETE job" do    
    it "should return 405 when a job is is not provided" do
      classifier_http do |http|
        http.send_request('DELETE', "/classifier/jobs/").code.should == "405"
      end
    end
    
    it "should return 404 when the job is missing" do
      classifier_http do |http|
        http.send_request('DELETE', "/classifier/jobs/missing").code.should == "404"
      end
    end
    
    it "should return 404 on subsequent gets" do
      id = Job.create(:tag_url => "http://localhost/tag.atom").id
      classifier_http do |http|
        http.send_request('DELETE', "/classifier/jobs/#{id}").code.should == "200"
      end
      classifier_http do |http|
        http.send_request('DELETE', "/classifier/jobs/#{id}").code.should == "404"
      end
    end
  end
  
  describe "GET job" do
    it "should return the job status"
    it "should return the job status when there is an xml suffix"
    it "should return 404 when getting a cancelled job"
    it "should return an error job status"
    it "should return 404 when the job is missing"
    it "should return 405 when not providing a job id"
  end
end
