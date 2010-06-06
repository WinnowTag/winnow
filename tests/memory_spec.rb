# Copyright (c) 2008 The Kaphan Foundation
#
# Possession of a copy of this file grants no permission or license
# to use, modify, or create derivate works.
# Please contact info@peerworks.org for further information.
#

require File.dirname(__FILE__) + '/spec_helper'

# This will only work on OSX
#
describe 'the classifier' do
  before(:each) do
    start_classifier(:malloc_log => true, :load_items_since => 336)
    @http = TestHttpServer.new(:port => 8888)
  end
  
  after(:each) do
    stop_classifier
    @http.shutdown
  end
  
  it 'should not leak memory after adding an item' do
    create_entry
    'classifier'.should not_leak
  end
  
  it 'should not leak memory after adding a duplicate item' do
    create_entry
    create_entry
    'classifier'.should not_leak
  end
  
  it "should not leak memory after adding a large item" do
    create_big_entry
    'classifier'.should not_leak
  end
  
  describe "job functionality" do
    xit "should not leak memory while processing a classification job" do
      @http.should_receive do
        request("/mytag-training.atom", 2) do |req, res|
          res.body = File.read(File.join(File.dirname(__FILE__), 'fixtures', 'complete_tag.atom'))
        end
      end
      job = create_job('http://localhost:8888/mytag-training.atom')
      job = create_job('http://localhost:8888/mytag-training.atom')

      @http.should have_received_requests

      tries = 0
      while job.progress < 100 && tries < 5
        job.reload
        tries += 1
        sleep(1)
      end

      job.progress.should >= 100
      job.destroy
      'classifier'.should not_leak #have_no_more_than_leaks(1)
    end
  end
end

describe 'the classifier with a high mintokens' do
  before(:each) do
    start_classifier(:malloc_log => true, :min_tokens => 100)
    sleep(0.5)
  end
  
  after(:each) do
    stop_classifier
  end
  
  it "should not leak memory after removing all small items" do
    'classifier'.should not_leak
  end
  
  it "should not leak memory after adding a small item" do
    create_entry
    create_entry(:id => '1112')
    'classifier'.should not_leak
  end
end
