$: << File.join(File.dirname(__FILE__), '..', 'lib')

require 'timetz'
require 'test/unit'

def with_tz(tz)
  before = TimeZone.local(tz)
  yield
ensure
  TimeZone.local(before)
end
