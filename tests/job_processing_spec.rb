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
  end
  
  it "should attempt to fetch a tag for the URL given in the Job description" do
    job = Job.create(:tag_id => 48)
    @http.should receive_request("/")
  end
end