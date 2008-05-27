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
    system("rm -f /tmp/perf.log")   
    start_classifier
    @http = TestHttpServer.new(:port => 8888)
  end
  
  after(:each) do
    stop_classifier
    @http.shutdown
  end
  
  it "should attempt to fetch a tag for the URL given in the Job description" do
    @http.should_receive do
      request("/mytag-training.atom") do |req, res|
        req.request_method.should == 'GET'
      end
    end    
    
    job = create_job('http://localhost:8888/mytag-training.atom')
    @http.should have_received_requests
  end
  
  it "should not include IF-MODIFIED-SINCE on first request" do
    @http.should_receive do
      request("/mytag-training.atom") do |req, res|
        req['IF-MODIFIED-SINCE'].should be_nil
      end
    end    
    
    job = create_job('http://localhost:8888/mytag-training.atom')
    @http.should have_received_requests
  end
  
  it "should include IF-MODIFIED-SINCE on second request" do
    requests = 0
    @http.should_receive do
      request("/mytag-training.atom", 2) do |req, res|
        requests += 1
        
        if requests == 1
          res.body = File.read(File.join(File.dirname(__FILE__), 'fixtures', 'complete_tag.atom'))
        else
          req['IF-MODIFIED-SINCE'].should == "Sun, 30 Mar 2008 01:24:18 GMT" # Date in atom:updated
          res.status = 304
        end
      end
    end
    
    job = create_job('http://localhost:8888/mytag-training.atom')
    sleep(2)
    create_job('http://localhost:8888/mytag-training.atom')    
    
    @http.should have_received_requests
  end
    
  it "should update the tagger on the second request" do
    @http.should_receive do
      request("/mytag-training.atom", 2) do |req, res|
        res.body = File.read(File.join(File.dirname(__FILE__), 'fixtures', 'complete_tag.atom'))        
      end
    end
    
    job = create_job('http://localhost:8888/mytag-training.atom')
    sleep(2)
    create_job('http://localhost:8888/mytag-training.atom')    
    
    @http.should have_received_requests
  end
  
  it "should accept application/atom+xml" do
    @http.should_receive do
      request("/mytag-training.atom") do |req, res|
        req['ACCEPT'].should == "application/atom+xml"
      end
    end
    
    job = create_job('http://localhost:8888/mytag-training.atom')
    @http.should have_received_requests
  end
  
  it "should not crash if it gets a 404" do
    @http.should_receive do
      request("/mytag-training.atom") do |req, res|
        res.status = 404
      end
    end
    
    job = create_job('http://localhost:8888/mytag-training.atom')
    
    should_not_crash
    @http.should have_received_requests
  end
  
  it "should not crash if it gets a 500" do
    @http.should_receive do
      request("/mytag-training.atom") do |req, res|
        res.status = 500
      end
    end
    
    job = create_job('http://localhost:8888/mytag-training.atom')
    should_not_crash
    @http.should have_received_requests
  end
  
  it "should not crash if it gets junk back in the body" do
    @http.should_receive do
      request("/mytag-training.atom") do |req, res|
        res.body = "blahbalh"
      end
    end
    
    job = create_job('http://localhost:8888/mytag-training.atom')
    should_not_crash
    @http.should have_received_requests
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
      req['user-agent'].should match(/PeerworksClassifier/)
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
  
  it "should have a strength attribute on each category" do
    job_results do |req, res|
      Atom::Feed.load_feed(req.body).entries.each do |e|
        e.categories.first['http://peerworks.org/classifier', 'strength'].should_not be_empty
        e.categories.first['http://peerworks.org/classifier', 'strength'].first.should match(/\d+\.\d+/)
      end
    end    
  end
    
  it "should have a term and scheme that match the term and scheme on the tag definition" do
    job_results do |req, res|
      feed = Atom::Feed.load_feed(req.body)
      feed.entries.each do |e|
        e.categories.first.term.should == 'a-religion'
        e.categories.first.scheme.should == 'http://trunk.mindloom.org:80/seangeo/tags/'
      end
    end
  end
  
  it "should include a new classified date on the tag" do
    job_results do |req, res|
      Atom::Feed.load_feed(req.body)['http://peerworks.org/classifier', 'classified'].should_not be_empty
      new_time = Time.parse(Atom::Feed.load_feed(req.body)['http://peerworks.org/classifier', 'classified'].first)
      new_time.should > Time.parse('2008-04-15T01:16:23Z')
    end
  end
end

describe "Job Processing with a threshold set" do
  before(:each) do
    system("rm /tmp/perf.log")   
    start_classifier(:positive_threshold => 0.9)
    @http = TestHttpServer.new(:port => 8888)
  end
  
  after(:each) do
    stop_classifier
    @http.shutdown
  end
  
  it "should only send entries for items above the threshold" do
    job_results do |req, res|
      Atom::Feed.load_feed(req.body).should have(4).entries
    end
  end
end
  
describe "Job Processing with an incomplete tag" do
  before(:each) do
    system("rm /tmp/perf.log")   
    start_classifier(:positive_threshold => 0.9)
    start_tokenizer
    @http = TestHttpServer.new(:port => 8888)
  end
  
  after(:each) do
    stop_classifier
    stop_tokenizer
    @http.shutdown
  end
  
  it "should result in new items in the item cache" do
    job_results('incomplete_tag.atom')
    sqlite = SQLite3::Database.open(Database)
    sqlite.get_first_value("select count(*) from entries").should == '13'
    sqlite.close
  end
  
  it "should PUT results" do
    job_results('incomplete_tag.atom') do |req, res|
      req.request_method.should == 'PUT'
    end
  end
end


describe "Job Processing with missing items and no tokenizer" do
  before(:each) do
    system("rm /tmp/perf.log")   
    start_classifier(:missing_item_timeout => 1)
    @http = TestHttpServer.new(:port => 8888)
  end
  
  after(:each) do
    stop_classifier
    @http.shutdown
  end
  
  it "should timeout and return an error for the job" do
    @http.should_receive do
      request("/mytag-training.atom") do |req, res|
        res.body = File.read(File.dirname(__FILE__) + "/fixtures/incomplete_tag.atom")
      end
    end
    
    job = create_job('http://localhost:8888/mytag-training.atom')
    sleep(2)
    job.reload
    job.status.should == "Error"
    job.error_message.should match(/The job timed out waiting for some resources/)
    job.destroy
  end
end 

describe "Job Processing after item addition" do
  before(:each) do
    @item_count = 10    
    @http = TestHttpServer.new(:port => 8888)
    start_classifier(:tag_index => 'http://localhost:8888/tags.atom')
    start_tokenizer
  end

  after(:each) do
    stop_classifier
    stop_tokenizer    
  end

  it "should include the added item" do
    create_entry
    
    job_results do |req, res|
      Atom::Feed.load_feed(req.body).should have(@item_count + 1).entries
    end
  end

  it "should automatically classify the new item" do
    @http.should_receive do
      request("/tags.atom") do |req, res|
        res.body = File.read(File.dirname(__FILE__) + "/fixtures/tag_index.atom")
      end
      
      request("/quentin/tags/tag/training.atom") do |req, res|
        res.body = File.read(File.dirname(__FILE__) + "/fixtures/complete_tag.atom")
      end
      
      request("/results") do |req, res|
        req.request_method.should == "POST"
        req['content-type'].should == 'application/atom+xml'
        Atom::Feed.load_feed(req.body).should have(1).entries
      end
    end

    create_entry
    @http.should have_received_requests
  end

  it "should only create a single job that classifies both items for each tag" do
    @http.should_receive do
      request("/tags.atom") do |req, res|
        res.body = File.read(File.dirname(__FILE__) + "/fixtures/tag_index.atom")
      end
      
      request("/quentin/tags/tag/training.atom") do |req, res|
        res.body = File.read(File.dirname(__FILE__) + "/fixtures/complete_tag.atom")
      end
      
      request("/results") do |req, res|
        req.request_method.should == "POST"
        req['content-type'].should == 'application/atom+xml'
        Atom::Feed.load_feed(req.body).should have(2).entries
      end
    end
    
    create_entry(:id => "1111")
    create_entry(:id => "1112")
    @http.should have_received_requests
    #`wc -l /tmp/perf.log`.to_i.should == Tagging.count(:select => 'distinct tag_id') + 2 # +1 for header, +1 for previous job
  end
end
    
def job_results(atom = 'complete_tag.atom', times_gotten = 1)
  gets = 0
  @http.should_receive do
    request("/mytag-training.atom", times_gotten) do |req, res|
      gets += 1
      
      if gets > 1
        res.status = '304'
      else
        res.body = File.read(File.join(File.dirname(__FILE__), 'fixtures', atom))
      end
    end
    request("/results") do |req, res|
      yield(req, res) if block_given?
    end
  end
  job = create_job('http://localhost:8888/mytag-training.atom')
  
  @http.should have_received_requests
end

def should_not_crash
  lambda {create_job('http://localhost:8888/mytag-training.atom')}.should_not raise_error
end
