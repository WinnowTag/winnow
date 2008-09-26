# Copyright (c) 2008 The Kaphan Foundation
#
# Possession of a copy of this file grants no permission or license
# to use, modify, or create derivate works.
# Please contact info@peerworks.org for further information.
#
FixBase = File.join(File.expand_path(File.dirname(__FILE__)), 'fixtures')

require File.dirname(__FILE__) + '/spec_helper.rb'
gem 'ratom'
require 'atom'

describe "Classify" do  
  describe "file" do
    before(:each) do
      FileUtils.rm_f("/tmp/taggings.atom")
    end

    it "should classify a file" do
      classify(FixBase + "/valid", 'file:' + FixBase + "/file_tag.atom")
      File.exist?("/tmp/taggings.atom").should be_true
    end

    it "should have taggings for each entry" do
      classify(FixBase + "/valid", 'file:' + FixBase + "/file_tag.atom")
      Atom::Feed.load_feed(File.open("/tmp/taggings.atom")).should have(10).entries    
    end
  end
  
  describe "directory" do
    before(:each) do
      FileUtils.rm_rf("/tmp/taggings")
      FileUtils.mkdir_p("/tmp/taggings")
    end
    
    it "should classify all files" do
      classify(FixBase + "/valid", File.dirname(__FILE__) + '/fixtures/tag_dir')
      Dir.glob("/tmp/taggings/*.atom").size.should == Dir.glob(FixBase + "/tag_dir/*.atom").size
    end
  end
  
  def classify(corpus, tag)
    cmd = File.join(ROOT, '../src/classify')
    
    if ENV['srcdir']
      cmd = File.join(ENV['PWD'], '../src/classify')
    end
    
    system("#{cmd} #{corpus} #{tag} 2> /dev/null")
  end
end
