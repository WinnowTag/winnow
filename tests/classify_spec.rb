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
      classify(Database, 'file:' + FixBase + "/file_tag.atom")
      File.exist?("/tmp/taggings.atom").should be_true
    end

    xit "should have taggings for each entry" do
      classify(Database, 'file:' + FixBase + "/file_tag.atom")
      Atom::Feed.load_feed(File.open("/tmp/taggings.atom")).should have(10).entries    
    end
  end
  
  describe "directory" do
    before(:each) do
      FileUtils.rm_rf("/tmp/taggings")
      FileUtils.mkdir_p("/tmp/taggings")
    end
    
    it "should classify all files" do
      classify(Database, File.dirname(__FILE__) + '/fixtures/tag_dir')
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
