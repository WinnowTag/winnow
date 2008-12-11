# Copyright (c) 2008 The Kaphan Foundation
#
# Possession of a copy of this file grants no permission or license
# to use, modify, or create derivate works.
# Please contact info@peerworks.org for further information.
#

begin
  require 'spec'
rescue LoadError
  require 'rubygems'
  gem 'rspec'
  require 'spec'
end

gem 'auth-hmac'
require 'auth-hmac'
gem 'ratom'
require 'atom'
require 'atom/pub'
require 'sqlite3'
require 'net/http'
require File.dirname(__FILE__) + "/test_http_server.rb"

CLASSIFIER_URL = "http://localhost:8008"
ROOT = File.expand_path(File.dirname(__FILE__))
Database = '/tmp/classifier-copy'

require 'active_record'
require 'active_resource'
class Tagging < ActiveRecord::Base; end
class Job < ActiveResource::Base 
  with_auth_hmac("winnow_id", "winnow_secret")
  self.site = CLASSIFIER_URL + "/classifier"
end

def create_feed(opts = {})
  opts = {:title => 'My new feed', :id => 'urn:peerworks.org:feeds#1337'}.merge(opts)
  hmac_access_id = opts.delete(:access_id)
  hmac_secret = opts.delete(:secret)
  collection = Atom::Pub::Collection.new(:href => CLASSIFIER_URL + '/feeds')
  feed_entry = Atom::Entry.new(opts)
  collection.publish(feed_entry, :hmac_access_id => hmac_access_id, :hmac_secret_key => hmac_secret)
end

def create_entry(opts = {})
  opts = {:id => '1111', :title => 'My Entry', :content => "this is the html content for entry 1111 there should be enough to tokenize"}.merge(opts)
  collection = Atom::Pub::Collection.new(:href => CLASSIFIER_URL + '/feed_items')
  entry = Atom::Entry.new do |entry|
    entry.title = opts[:title]
    entry.id = "urn:peerworks.org:entries##{opts[:id]}"
    entry.links << Atom::Link.new(:href => 'http://example.org/1111.html', :rel => 'alternate')
    entry.links << Atom::Link.new(:href => 'http://example.org/1111.atom', :rel => 'self')
    entry.updated = (opts[:updated] or Time.now)
    entry.content = Atom::Content::Html.new(opts[:content])
  end
  
  collection.publish(entry, :hmac_access_id => opts[:access_id], :hmac_secret_key => opts[:secret])
end

def create_empty_entry(opts = {})
  opts = {:id => '1111', :title => 'My Entry'}.merge(opts)
  collection = Atom::Pub::Collection.new(:href => CLASSIFIER_URL + '/feed_items')
  entry = Atom::Entry.new do |entry|
    entry.title = opts[:title]
    entry.id = "urn:peerworks.org:entries##{opts[:id]}"
    entry.links << Atom::Link.new(:href => 'http://example.org/1111.html', :rel => 'alternate')
    entry.links << Atom::Link.new(:href => 'http://example.org/1111.atom', :rel => 'self')
    entry.updated = (opts[:updated] or Time.now)
  end
  
  collection.publish(entry)
end

def create_big_entry  
  collection = Atom::Pub::Collection.new(:href => CLASSIFIER_URL + '/feed_items')
  entry = Atom::Entry.new do |entry|
    entry.title = 'My Feed'
    entry.id = "urn:peerworks.org:entries#1111"
    entry.links << Atom::Link.new(:href => 'http://example.org/1111.html', :rel => 'alternate')
    entry.links << Atom::Link.new(:href => 'http://example.org/1111.atom', :rel => 'self')
    entry.updated = Time.now
    entry.content = Atom::Content::Html.new("&lt;p&gt;&lt;img src='http://www.gravatar.com/avatar.php?gravatar_id=e9d797bffffd51cf67866a6e5af8648c&amp;amp;size=80&amp;amp;default=http://use.perl.org/images/pix.gif' alt='' style='float: left; margin-right: 10px; border: none;'/&gt;I&amp;#8217;ve often wondered why Judaism doesn&amp;#8217;t tick me off like Christianity and Islam do. Is it because Jews don&amp;#8217;t try to convert anyone? Perhaps. Is it because of the Holocaust? Maybe. Is it because of my own Jewish roots and my Jewish-atheist grandfather? I doubt it. After all, I also have Christian roots and had a devout Catholic grandmother.&lt;/p&gt;
  &lt;p&gt;I think it might be because Judaism is not about belief. You wouldn&amp;#8217;t hear this, for example, in a church or a mosque:&lt;/p&gt;
  &lt;p&gt;&lt;a href='http://www.glumbert.com/media/barmitzvah'&gt;&lt;img src='http://farm3.static.flickr.com/2025/2339772983_0d72afd3d9_m.jpg' alt='Barmitzvah Movie'/&gt;&lt;/a&gt;&lt;/p&gt;
  &lt;p&gt;Even in Israel, where Jews are often accused of being fundamentalists, and where orthodoxy arguably does have a stronghold (the differences between orthodoxy and fundamentalism in all religions requiring a completely separate discussion&amp;#8230;) &lt;a href='http://www.jewcy.com/post/secular_israelis_seek_jewish_tradition_belief_god_not_required'&gt;belief in God is optional&lt;/a&gt;. You can, with a straight face, call someone a secular Jew or even an atheist Jew, and they won&amp;#8217;t even be insulted. Who among you has ever heard of a secular Christian or an atheist Muslim?&lt;/p&gt;
  &lt;blockquote&gt;&lt;p&gt;The ambivalence about Judaism in Israel became clear to me one night as I sat drinking in an alleyway bar in Tel Aviv with my Israeli friend Omer. Omer has been studying abroad in Germany for the past few years, and admitted that he felt disconnected there, and had started attending a Friday night dinner with other Jewish students. &#x201C;My father would disown me if he knew I was lighting Shabbat candles,&#x201D; said Omer guiltily. &#x201C;We come from a long line of staunch Tel Aviv atheists.&#x201D;&lt;/p&gt;
  &lt;p&gt;In order to counteract this deep rooted aversion to religion, the Jewish Renewal movement (different from the 1960s American movement of the same name) takes a more flexible approach, focusing on ritual, tradition and spirituality rather than outright faith. While the term &#x201C;secular synagogue&#x201D; may seem like an oxymoron,to proponents of Jewish Renewal, it&#x2019;s the basis of their ideology.&lt;/p&gt;&lt;/blockquote&gt;
  &lt;p&gt;When reading Shalom Auslander&amp;#8217;s &lt;em&gt;Foreskin&amp;#8217;s Lament&lt;/em&gt;, the author&amp;#8217;s story about growing up in an abusive home and a suffocating Orthodox community, I just didn&amp;#8217;t find the bile rising to my throat the way I do when I read &lt;em&gt;Infidel&lt;/em&gt; or any of the plethora of Christian de-conversion memoirs I&amp;#8217;ve read over the past couple of years.&lt;/p&gt;
  &lt;p&gt;I can&amp;#8217;t believe I haven&amp;#8217;t made &lt;a href='http://www.shalomauslander.com/book_foreskins_lament.php'&gt;&lt;em&gt;Foreskin&amp;#8217;s Lament&lt;/em&gt;&lt;/a&gt; one of our reading selections! I guess it&amp;#8217;s because I&amp;#8217;m trying to cover a range of topics and not inundate you solely with my own preoccupations. But this book is definitely in the must read category. Jewish writers, for some reason, can make these things &lt;em&gt;funny&lt;/em&gt;, while Christians and Muslims seem to think that humor is a sin.&lt;/p&gt;
  &lt;blockquote&gt;&lt;p&gt;&amp;#8220;If you read this while you&amp;#8217;re eating, the food will come out your nose. Foreskin&amp;#8217;s Lament is a filthy and slightly troubling dialogue with God, the big, old, physically abusive ultra orthodox God who brought His Chosen People out of Egypt to torture them with non-kosher Slim Jims. I loved this book and will never again look at the isolated religious nutjobs on the fringe of American society with anything less than love and understanding.&amp;#8221;&lt;/p&gt;&lt;/blockquote&gt;
  &lt;p&gt;On the other hand, &lt;a href='http://www.nydailynews.com/entertainment/movies/2008/03/15/2008-03-15_hasidic_actor_walks_off_portman_movie.html'&gt;Jewish fundamentalists are stupid control freaks, just like fundies of other religions&lt;/a&gt;:&lt;/p&gt;
  &lt;blockquote&gt;&lt;p&gt;Abe Karpen, 25, a married father of three, was cast as [Natalie] Portman&amp;#8217;s husband in &amp;#8220;New York I Love You,&amp;#8221; a film composed of 12 short stories about love in the five boroughs.&lt;/p&gt;
  &lt;p&gt;&amp;#8220;I am backing out of the movie,&amp;#8221; said Karpen, a kitchen cabinet salesman. &amp;#8220;It&amp;#8217;s not acceptable in my community. It&amp;#8217;s a lot of pressure I am getting. They [the rabbis] didn&amp;#8217;t like the idea of a Hasidic guy playing in Hollywood.&lt;/p&gt;
  &lt;p&gt;&amp;#8220;I have my kids in religious schools and the rabbi called me over yesterday and said in order for me to keep my kids in the school I have to do what they tell me and back out,&amp;#8221; Karpen said.&lt;/p&gt;&lt;/blockquote&gt;
  &lt;p&gt;Well, those are a few of my subjective and highly personal thoughts on the matter. Discuss.&lt;/p&gt;")
  end
  
  collection.publish(entry)
end

def create_another_big_entry
  s = <<-END
<?xml version="1.0"?>
  <entry xmlns="http://www.w3.org/2005/Atom">
    <title>Car Advisory Network Takes $6.5 Million Series A</title>
    <author>
      <name>Duncan Riley</name>
    </author>
    <id>urn:peerworks.org:entry#1740153</id>
    <content type="html">&lt;p&gt;&lt;a href='http://www.caradvisorynetwork.com'&gt;&lt;img class='shot2' src='http://www.techcrunch.com/wp-content/can.jpg' alt='can.jpg'/&gt;Car Advisory Network&lt;/a&gt; has taken $6.5 million Series A in a round that included Accel Partners and Greylock Partners.&lt;/p&gt;
  &lt;p&gt;Seattle based Car Advisory Network operates &lt;a href='http://www.thecarconnection.com'&gt;TheCarConnection.com&lt;/a&gt;, a &amp;#8220;source for news and reviews, spy shots and shopping guides, tips and expert advice&amp;#8221; on cars. The site also operates several email lists and publishes the &amp;#8220;Weekly Car Guide.&amp;#8221;&lt;/p&gt;
  &lt;p&gt;A company listing &lt;a href='http://www.ivc-online.com/G_info.asp?objectType=1&amp;amp;fObjectID=9729&amp;amp;CameFrom=GoogleSearch'&gt;at IVC-Online&lt;/a&gt; notes that Car Advisory Network is &amp;#8220;developing a next generation network media platform in the auto industry,&amp;#8221; was established in April 2007 (TheCarConnection.com was founded in 1997), has five employees and is currently in &amp;#8220;stealth mode.&amp;#8221; &lt;/p&gt;
  &lt;p&gt;(via &lt;a href='http://www.pehub.com/article/articledetail.php?articlepostid=10891'&gt;PEHub&lt;/a&gt;)
  &lt;/p&gt;&lt;p&gt;&lt;strong&gt;&lt;em&gt;Crunch Network&lt;/em&gt;&lt;/strong&gt;:  &lt;a href='http://mobilecrunch.com/'&gt;MobileCrunch&lt;/a&gt;&lt;em&gt; &lt;/em&gt;Mobile Gadgets and Applications, Delivered Daily.&lt;/p&gt;

  &lt;p&gt;&lt;a href='http://feeds.feedburner.com/~a/Techcrunch?a=mFfbQ6'&gt;&lt;img src='http://feeds.feedburner.com/~a/Techcrunch?i=mFfbQ6' border='0'/&gt;&lt;/a&gt;&lt;/p&gt;&lt;div class='feedflare'&gt;
  &lt;a href='http://feeds.feedburner.com/~f/Techcrunch?a=n2htEEF'&gt;&lt;img src='http://feeds.feedburner.com/~f/Techcrunch?i=n2htEEF' border='0'/&gt;&lt;/a&gt; &lt;a href='http://feeds.feedburner.com/~f/Techcrunch?a=TrdhbWf'&gt;&lt;img src='http://feeds.feedburner.com/~f/Techcrunch?i=TrdhbWf' border='0'/&gt;&lt;/a&gt; &lt;a href='http://feeds.feedburner.com/~f/Techcrunch?a=knotoLF'&gt;&lt;img src='http://feeds.feedburner.com/~f/Techcrunch?i=knotoLF' border='0'/&gt;&lt;/a&gt; &lt;a href='http://feeds.feedburner.com/~f/Techcrunch?a=tLBh9kF'&gt;&lt;img src='http://feeds.feedburner.com/~f/Techcrunch?i=tLBh9kF' border='0'/&gt;&lt;/a&gt;
  &lt;/div&gt;&lt;img src='http://feeds.feedburner.com/~r/Techcrunch/~4/253253539' height='1' width='1'/&gt;</content>
    <link href="/feed_items/1740153.atom" rel="self"/>
    <link href="http://feeds.feedburner.com/%7Er/Techcrunch/%7E3/253253539/" rel="alternate"/>
    <link href="/feed_items/1740153/spider" rel="http://peerworks.org/rel/spider"/>
    <updated>2008-03-17T21:30:56Z</updated>
  </entry>
  END
  
  collection = Atom::Pub::Collection.new(:href => CLASSIFIER_URL + '/feed_items')
  entry = Atom::Entry.load_entry(s)
  collection.publish(entry)
end

def destroy_entry(id)
  Atom::Entry.new do |e|
    e.links << Atom::Link.new(:href => CLASSIFIER_URL + "/feed_items/#{id}", :rel => 'edit')
  end.destroy!
end

def start_classifier(opts = {})
  @classifier_url = URI.parse("http://localhost:8008")
  options = {:min_tokens => 0, :load_items_since => 3650}.update(opts)
  system("rm -Rf #{Database} && cp -R #{File.join(ROOT, 'fixtures/valid')} #{Database}")
  system("chmod -R 755 #{Database}") 
  system("rm -f /tmp/classifier-test.pid")
  classifier = File.join(ROOT, "../src/classifier")
  
  tag_index = "--tag-index #{opts[:tag_index]}" if opts[:tag_index]
  credentials = "--credentials #{opts[:credentials]}" if opts[:credentials]
  
  if ENV['srcdir']
    classifier = File.join(ENV['PWD'], '../src/classifier')
  end
  classifier_cmd = "#{classifier} --pid /tmp/classifier-test.pid " +
                                     "--tokenizer-url http://localhost:8010/tokenize " +
                                     "-o /tmp/classifier-item_cache_spec.log " +
                                     "--performance-log /tmp/perf.log " +
                                     "--cache-update-wait-time 1 " +
                                     "-p 8008 " +
                                     "--load-items-since #{options[:load_items_since]} " +
                                     "--min-tokens #{options[:min_tokens] or 0} " +
                                     "--positive-threshold #{options[:positive_threshold] or 0} " +
                                     "--missing-item-timeout #{options[:missing_item_timeout] or 60} " +
                                     "--db #{Database} #{tag_index} #{credentials}"
                                       
  if options[:malloc_log]
    @classifier_pid = fork do
       STDERR.close
      ENV['MallocStackLogging'] = '1'
      classifier_cmd = "#{classifier_cmd}"    
      exec(classifier_cmd)
    end
  else
    system("#{classifier_cmd} -d")
  end  
    
  sleep(0.1) if opts[:sleep] != false
end

def classifier_http(&block)
  Net::HTTP.start(@classifier_url.host, @classifier_url.port, &block) 
end

def stop_classifier
  if @classifier_pid
    system("kill -9 #{@classifier_pid}")
    @classifier_pid = nil
  else
    system("kill -9 `cat /tmp/classifier-test.pid`")
  end
end

def start_tokenizer
  system("tokenizer_control start -- -p8010 #{Database}")
  sleep(1)
end

def stop_tokenizer
  system("tokenizer_control stop")
end

def create_job(training_url)
  job = Job.new(:tag_url => training_url)
  job.save.should be_true
  job
end

def run_job(training_url = 'http://localhost:8888/mytag-training.atom')
 job = create_job(training_url)
  
  @http.should receive_request("/mytag-training.atom") do |req, res|
    res.body = File.read(File.join(File.dirname(__FILE__), 'fixtures', 'complete_tag.atom'))
  end
  
  tries = 0
  while job.progress < 100 && tries < 100
    job.reload
    tries += 1
  end
  
  job.progress.should >= 100
  job.destroy
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
    @result = `leaks -exclude regcomp -exclude JudySLIns -exclude xmlSetStructuredErrorFunc #{@target}`
    if @result =~ /(\d+) leaks?/
      @leaks = @result.scan(/Leak:/).size 
      @leaks <= @n
    end
  end
  
  def failure_message
    "#{@target} has #{@leaks} memory leaks, expected less than or equal to #{@n}\n#{@result}"
  end
end