require File.join(File.dirname(__FILE__), 'helper')

n = 500000

Benchmark.benchmark("Time#local" + Benchmark::CAPTION, WIDTH, Benchmark::FMTSTR){|r|
  r.report("local(...)"){
    n.times{
      t = Time.local(2008, 12, 1,  1, 30, 30)
    }
  }
  r.report("local({...})"){
    n.times{
      t = Time.local :year => 2008, :mon => 12, :day => 1, :hour => 1, :min => 30, :sec => 30
    }
  }
}


