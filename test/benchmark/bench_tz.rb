require File.join(File.dirname(__FILE__), 'helper')

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
  tz2 = 'US/Pacific'

  Benchmark.benchmark("US/Pacific->UTC" + Benchmark::CAPTION, WIDTH, Benchmark::FMTSTR){|r|
    r.report("TZInfo"){
      n.times{
        nt1 = tz1.local_to_utc(t)
      }
    }
    r.report("TimeZone"){
      n.times{
        nt2 = Time.utc(t.sec, t.min, t.hour, t.day, t.mon, t.year, t.wday, t.yday, t.dst?, tz2)
      }
    }
  }

rescue LoadError
  warn "tzinfo not found, benchmark skipped"
end
