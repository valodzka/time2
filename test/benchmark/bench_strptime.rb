require File.join(File.dirname(__FILE__), 'helper')

fmt = "%Y-%m-%d %Z"
str = "2007-10-10 GMT"

OLD_STRPTIME = if Time.respond_to? :old_strptime # check for ruby 1.8
                 Time.method(:old_strptime)
               else
                 require 'date'
                 DateTime.method(:strptime)
               end

NEW_STRPTIME = Time.method(:strptime)

n = 50000

Benchmark.benchmark("strptime " + Benchmark::CAPTION, WIDTH, Benchmark::FMTSTR){|b|
  b.report("build-in"){
    n.times{
      OLD_STRPTIME.call(str, fmt)
    }
  }
  b.report("new"){
    n.times{
      NEW_STRPTIME.call(str, fmt)
    }
  }
}
