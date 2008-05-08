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

def receive_request(path, timeout = 2, &block)
  HttpRequestMatcher.new(path, timeout, block)
end

def receive_requests(timeout = 2)
  matcher = MultipleHttpRequestMatcher.new(timeout)
  yield(matcher)
  matcher
end

class MultipleHttpRequestMatcher
  def initialize(timeout)
    @timeout = timeout
    @requests = {}
    @request_paths = []
    @received_requests = []
  end
  
  def request(path, times = 1, &block)
    times.times do
      @request_paths << path
    end
    @requests[path] = block
  end
  
  def matches?(http)
    @received_requests.extend(MonitorMixin)
    cond = @received_requests.new_cond
    exception = nil
    
    http.start do |server|
      @requests.each do |path, block|
        server.mount_proc(path) do |req, res|
          begin
            block.call(req, res) if block
          rescue Exception => e
            exception = e
          end
          
          @received_requests.synchronize do
            @received_requests << path
            cond.signal if @received_requests.size == @requests.size
          end
        end
      end
    end
    
    @received_requests.synchronize do
      cond.wait(@timeout) unless @received_requests.size == @requests.size
      http.shutdown
    end
    
    exception and raise(exception) or @received_requests == @request_paths
  end
  
  def failure_message
    "Expected requests to [#{@request_paths.join(', ')}] got [#{@received_requests.join(', ')}] after #{@timeout}s."
  end
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
end
