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
    start_tokenizer
  end
  
  after(:each) do
    stop_tokenizer
    stop_classifier
  end
  
  it 'should not leak memory after adding an item' do
    create_entry
    sleep(3)
    'classifier'.should not_leak
  end
  
  it 'should not leak memory after adding a duplicate item' do
    create_entry
    create_entry
    sleep(3)
    'classifier'.should not_leak
  end
  
  it "should not leak memory after adding a large item" do
    create_big_entry
    sleep(3)
    'classifier'.should not_leak
  end
  
  it "should not leak memory after adding a feed" do
    create_feed
    create_feed(:title => 'My new feed', :id => 'urn:peerworks.org:feeds#1338')
    # There is a weird one off memory leak on OSX with the regex in add_feed.
    'classifier'.should have_no_more_than_leaks(1) 
  end
  
  describe "job functionality" do
    before(:each) do
      @http = TestHttpServer.new(:port => 8888)
    end
    
    it "should not leak memory while processing a classification job" do
      run_job
      sleep(1)
      'classifier'.should have_no_more_than_leaks(1)
    end
  end
end

describe 'the classifier with a high mintokens' do
  before(:each) do
    start_classifier(:malloc_log => true, :min_tokens => 100)
    start_tokenizer
    sleep(0.5)
  end
  
  after(:each) do
    stop_tokenizer
    stop_classifier
  end
  
  it "should not leak memory after removing all small items" do
    'classifier'.should not_leak
  end
  
  it "should not leak memory after adding a small item" do
    create_entry
    create_entry(:id => '1112')
    sleep(4)
    'classifier'.should not_leak
  end
end
