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
    sleep(0.5)
    'classifier'.should have_no_leaks
  end
  
  def have_no_leaks
    return MemoryLeaks.new
  end
  
  class MemoryLeaks
    def matches?(target)
      @target = target
      @result = `leaks #{@target}`
      if @result =~ /(\d+) leaks/
        @leaks = $1.to_i
        @leaks == 0
      end
    end
    
    def failure_message
      "#{@target} has #{@leaks} memory leaks\n#{@result}"
    end
  end
end