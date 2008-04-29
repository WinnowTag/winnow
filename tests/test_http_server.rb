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
    exception = nil
    
    http.start do |server|
      server.mount_proc(@path) do |req, res|
        request_received.synchronize do
          request_received = true
          cond.signal
        end
        begin
          yield(req, res) if block_given?
        rescue Exception => e
          exception = e
        end
      end
    end
    
    request_received.synchronize do      
      cond.wait(@timeout) unless request_received
      http.shutdown      
    end
    
    exception and raise(exception) or request_received
  end
  
  def failure_message
    "No request received for #{@path} after #{@timeout} seconds"
  end
end

class TestHttpServer  
  attr_accessor :server
  def initialize(opts = {})
    @port = opts[:port]    
  end
  
  def start
    @server = WEBrick::HTTPServer.new(:Port => @port)
    ['INT', 'TERM'].each { |signal|
       trap(signal){ @server.shutdown} 
    }
    yield(@server) if block_given?
    Thread.new do      
      @server.start
    end
  end
  
  def shutdown
    @server.shutdown
  end
end
