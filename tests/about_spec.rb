# Copyright (c) 2008 The Kaphan Foundation
#
# Possession of a copy of this file grants no permission or license
# to use, modify, or create derivate works.
# Please contact info@peerworks.org for further information.
#

require File.dirname(__FILE__) + '/spec_helper'

describe '/classifier' do
  before(:each) do
    start_classifier
  end
  
  after(:each) do
    stop_classifier
  end
  
  it "should return 200 from /classifier.xml" do
    Net::HTTP.get_response(URI.parse(CLASSIFIER_URL + "/classifier.xml")).code.should == "200"
  end
end