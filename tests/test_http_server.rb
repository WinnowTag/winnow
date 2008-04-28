# Copyright (c) 2008 The Kaphan Foundation
#
# Possession of a copy of this file grants no permission or license
# to use, modify, or create derivate works.
# Please contact info@peerworks.org for further information.
#

require 'webrick'

def receive_request(path, timeout = 2, &block)
  HttpRequestMatcher.new(path, timeout, block)
end

class HttpRequestMatcher
  def initialize(path, timeout, block)
    @path = path
    @timeout = timeout
    @block = block    
  end
  
  def matches?(http)
    request_received = false
    request_received.extend(MonitorMixin)
    cond = request_received.new_cond
    
    http.start do |server|
      server.mount_proc(@path) do |req, res|
        request_received.synchronize do
          request_received = true
          cond.signal
        end
        yield(req) if block_given?
      end
    end
    
    request_received.synchronize do      
      cond.wait(@timeout) unless request_received
      http.shutdown
      request_received
    end
  end
  
  def failure_message
    "No request received for #{@path} after #{@timeout} seconds"
  end
end

class TestHttpServer  
  attr_accessor :server
  def initialize(opts = {})
    @port = opts[:port]
    @server = WEBrick::HTTPServer.new(:Port => @port, :Logger => WEBrick::Log.new('/dev/null'))
    ['INT', 'TERM'].each { |signal|
       trap(signal){ @server.shutdown} 
    }
  end
  
  def start
    yield(@server) if block_given?
    Thread.new do      
      @server.start
    end
  end
  
  def shutdown
    @server.shutdown
  end
end
