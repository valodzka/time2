$: << File.join(File.dirname(__FILE__), '..', '..', 'lib')

require 'time'
OLD_STRPTIME = Time.method(:strptime)
require 'timetz'
require 'benchmark'

WIDTH = 15


