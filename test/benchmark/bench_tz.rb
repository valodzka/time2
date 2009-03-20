require File.join(File.dirname(__FILE__), 'benchmark_helper')

require 'rational'

unless Rational.respond_to? :new! # TZinfo has a problem on some version
  Rational.instance_eval do
    def new!(*args)
      Rational(*args)
    end
  end
end

begin
  require 'tzinfo'

  n = 500000
  t = Time.now.utc
  tz1 = TZInfo::Timezone.get('US/Pacific')
  tz2 = TimeZone['US/Pacific']

  Benchmark.benchmark("UTC->US/Pacific" + Benchmark::CAPTION, WIDTH, Benchmark::FMTSTR){|r| 
    r.report("TZInfo"){ 
      n.times{
        tz1.utc_to_local(t)
      } 
    }
    r.report("TimeZone"){ 
      n.times{
        t.getlocal(tz2)
      } 
    } 
  }

rescue LoadError
  warn "tzinfo not found, benchmark skipped"
end
