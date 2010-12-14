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

gem 'auth-hmac'
require 'auth-hmac'

describe "Classifier Job Control" do
  before(:each) do
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
        http.send_request('GET', "/classifier/jobs/#{id}").code.should == "404"
      end
    end
  end
  
  describe "GET job errors" do    
    it "should return 404 when the job is missing" do
      classifier_http do |http|
        http.request_get("/classifier/jobs/missing").code.should == "404"
      end
    end
    
    it "should return 405 when not providing a job id" do
      classifier_http do |http|
        http.request_get("/classifier/jobs").code.should == "405"
      end
    end
    
    it "should return 404 when getting a cancelled job" do
      id = Job.create(:tag_url => "http://localhost/tag.atom").id
      classifier_http do |http|
        http.send_request('DELETE', "/classifier/jobs/#{id}").code.should == "200"
      end
      classifier_http do |http|
        http.request_get("/classifier/jobs/#{id}").code.should == "404"
      end        
    end
  end
  
  describe "GET job" do
    before(:each) do
      @job = Job.create(:tag_url => "http://localhost:60000/tag.atom")
    end
    
    it "should return 200 for valid job" do
      classifier_http do |http|
        http.request_get("/classifier/jobs/#{@job.id}").code.should == "200"
      end
    end
    
    it "should return the job status" do
      @job.reload
      @job.should respond_to(:progress, :duration, :status)
    end
    
    it "should return the job status when there is an xml suffix" do
      classifier_http do |http|
        http.request_get("/classifier/jobs/#{@job.id}.xml").code.should == "200"
      end
    end
    
    it "should return an error job status because the tag url is unreachable" do
      sleep(2)
      @job.reload
      @job.status.should == "Error"
      @job.error_message.should == "Tag could not be retrieved: couldn't connect to host"
    end
  end
end


describe "Item Cache Authentication" do
  before(:each) do
    start_classifier(:credentials => File.join(ROOT, 'fixtures', 'credentials.js'))
  end
  
  after(:each) do
    stop_classifier
  end
  
  it "should reject unauthenticated create job requests" do
    classifier_http do |http|
      http.request_post("/classifier/jobs", "blahblah").code.should == "401"
    end
  end
  
  it "should allow authenticated create job requests" do
    classifier_http do |http|
      request = Net::HTTP::Post.new("/classifier/jobs", 'Content-Type' => 'application/xml')
      AuthHMAC.new("winnow_id" => "winnow_secret").sign!(request, 'winnow_id')
      http.request(request, "blah").code.should == "400"
    end
  end  
end
