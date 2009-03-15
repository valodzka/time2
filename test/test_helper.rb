require File.join(File.dirname(__FILE__), 'test_helper')

require 'time'
require 'timetz'
require 'test/unit'

def with_tz(tz)
  before = Time::Zone.default
  Time::Zone.default = tz
  yield
ensure
  Time::Zone.default = before
end
