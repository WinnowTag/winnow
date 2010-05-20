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
  end
  
  after(:each) do
    stop_classifier
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
    before(:each) do
      @http = TestHttpServer.new(:port => 8888)
    end
    
    xit "should not leak memory while processing a classification job" do
      run_job
      'classifier'.should have_no_more_than_leaks(1)
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
    sleep(4)
    'classifier'.should not_leak
  end
end
