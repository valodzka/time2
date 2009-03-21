$: << File.join(File.dirname(__FILE__), '..', 'lib')

require 'time2'
require 'test/unit'

def ruby19
  yield if RUBY_VERSION =~ /^1\.9/
end

def with_tz(tz)
  before = TimeZone.local(tz)
  yield
ensure
  TimeZone.local(before)
end
