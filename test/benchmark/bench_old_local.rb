# Not load 'time2', we mesure speed of system local functions
require 'benchmark'

WIDTH = 15

n = 500000

Benchmark.benchmark("Time#local" + Benchmark::CAPTION, WIDTH, Benchmark::FMTSTR){|r|
  r.report("old"){
    n.times{
      t = Time.local(2008, 12, 1,  1, 30, 30)
    }
  }
}

