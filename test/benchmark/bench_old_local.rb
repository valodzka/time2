# Not load 'time2', we mesure speed of system local functions
require 'benchmark'

WIDTH = 15

n = 500000
ti = Time.utc(1971, 12, 1,  1, 30, 30)

Benchmark.benchmark("old Time#local" + Benchmark::CAPTION, WIDTH, Benchmark::FMTSTR){|r|
  r.report("Time#local"){
    n.times{
      t = Time.local(1971, 12, 1,  1, 30, 30)
    }
  }
  r.report("Time#getlocal"){
    n.times{
      t = ti.getlocal
    }
  }
}

