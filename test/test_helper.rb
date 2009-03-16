$: << File.join(File.dirname(__FILE__), '..', 'lib')

p $:

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
