require File.join(File.dirname(__FILE__), 'benchmark_helper')

fmt = "%Y-%m-%d %Z"
str = "2007-10-10 GMT"

OLD_STRPTIME = Time.method(:old_strptime)
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
