# Copyright (c) 2008 The Kaphan Foundation
#
# Possession of a copy of this file grants no permission or license
# to use, modify, or create derivate works.
# Please contact info@peerworks.org for further information.
#

require 'webrick'

# Monkey patch WEBrick's ProcHandler so that it forwards on
# PUT and DELETE requests.
#
class WEBrick::HTTPServlet::ProcHandler
  alias do_PUT do_GET
  alias do_DELETE do_GET
end

def have_received_requests
  HttpMatcher.new
end

class HttpMatcher
  def matches?(http)
    @http = http
    http.matches?
  end
  
  def failure_message
    @http.failure_message
  end
end

class TestHttpServer  
  include Spec::Matchers
  attr_accessor :server
  def initialize(opts = {})
    @port = opts[:port]    
    @timeout = (opts[:timeout] or 5)
    @requests = {}
    @request_paths = []
    @received_requests = []
    @received_requests.extend(MonitorMixin)
    @cond = @received_requests.new_cond
    @exception = nil    
  end
  
  def start_server
    @server = WEBrick::HTTPServer.new(:Port => @port, :Logger => WEBrick::Log.new('/dev/null'), :AccessLog => [])
    ['INT', 'TERM'].each { |signal|
       trap(signal){ @server.shutdown} 
    }
    yield(@server) if block_given?
    Thread.new do      
      @server.start
    end
  end
  
  def shutdown
    @server.shutdown if @server
  end
  
  def start
    self.start_server do |server|
      @requests.each do |path, block|
        server.mount_proc(path) do |req, res|
          begin
            block.call(req, res) if block
          rescue Exception => e
            @exception = e
          end
          
          @received_requests.synchronize do
            @received_requests << path
            @cond.signal if @received_requests.size == @request_paths.size
          end
        end
      end
      
      server.mount_proc("/") do |req, res|
        res.status = 404
      end
    end
  end
  
  def matches?(http = nil)        
    @received_requests.synchronize do
      unless @received_requests.size == @request_paths.size
        @cond.wait(@timeout) 
      end
      self.shutdown
    end
    
    @exception and raise(@exception) or @received_requests == @request_paths
  end
  
  def failure_message
    "Expected requests to [#{@request_paths.join(', ')}] got [#{@received_requests.join(', ')}] after #{@timeout}s."
  end
  
  def should_receive(&block)
    self.instance_eval(&block)
    self.start
  end
  
  def request(path, times = 1, &block)
    times.times do
      @request_paths << path
    end
    @requests[path] = block
  end
end
