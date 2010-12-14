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
    'winnow'.should not_leak
  end
  
  it 'should not leak memory after adding a duplicate item' do
    create_entry
    create_entry
    'winnow'.should not_leak
  end
  
  it "should not leak memory after adding a large item" do
    create_big_entry
    'winnow'.should not_leak
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
      'winnow'.should not_leak #have_no_more_than_leaks(1)
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
    'winnow'.should not_leak
  end
  
  it "should not leak memory after adding a small item" do
    create_entry
    create_entry(:id => '1112')
    'winnow'.should not_leak
  end
end
