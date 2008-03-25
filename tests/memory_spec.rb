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
    start_classifier(true)
    start_tokenizer
  end
  
  after(:each) do
    stop_tokenizer
    stop_classifier
  end
  
  it 'should not leak memory after adding an item' do
    create_entry
    sleep(1)
    'classifier'.should not_leak
  end
  
  it 'should not leak memory after adding a duplicate item' do
    create_entry
    create_entry
    sleep(3)
    'classifier'.should not_leak
  end
  
  it 'should not leak memory after adding an item and waiting for classification' do
    create_entry
    sleep(1.5)
    'classifier'.should not_leak
  end
  
  it "should not leak memory after adding a large item" do
    create_big_entry
    sleep(1)
    'classifier'.should not_leak
  end
  
  it "should not leak memory after adding a feed" do
    create_feed
    create_feed(:title => 'My new feed', :id => 'urn:peerworks.org:feeds#1338')
    # There is a weird one off memory leak on OSX with the regex in add_feed.
    'classifier'.should have_no_more_than_leaks(1) 
  end
  
  def not_leak
    return have_no_more_than_leaks(0)
  end
  
  def have_no_more_than_leaks(n)
    return MemoryLeaks.new(n)
  end
  
  class MemoryLeaks
    def initialize(n)
      @n = n
    end
    
    def matches?(target)
      @target = target
      @result = `leaks #{@target}`
      if @result =~ /(\d+) leaks?/
        @leaks = $1.to_i
        @leaks <= @n
      end
    end
    
    def failure_message
      "#{@target} has #{@leaks} memory leaks, expected less than or equal to #{@n}\n#{@result}"
    end
  end
end