require File.join(File.dirname(__FILE__), 'helper')

n = 500000
ti = Time.utc(1971, 12, 1,  1, 30, 30)

Benchmark.benchmark("Time#local" + Benchmark::CAPTION, WIDTH, Benchmark::FMTSTR){|r|
  r.report("getlocal") {
    n.times {
      t = ti.getlocal
    }
  }
  
  r.report("local(...)"){
    n.times{
      t = Time.local(1971, 12, 1,  1, 30, 30)
    }
  }
  r.report("local({...})"){
    n.times{
      t = Time.local :year => 1971, :mon => 12, :day => 1, :hour => 1, :min => 30, :sec => 30
    }
  }
}


