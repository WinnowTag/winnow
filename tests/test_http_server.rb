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
