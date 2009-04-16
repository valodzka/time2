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
  tz = ENV['TZ'] || 'Europe/Minsk'
  t = [1, 1, 1, 1, 1, 2008, 1, 1, nil]
  t1 = t + [nil]
  t2 = t + [tz]
  tzinfo = TZInfo::Timezone.get(tz)


  Benchmark.benchmark("US/Pacific->UTC" + Benchmark::CAPTION, WIDTH, Benchmark::FMTSTR){|r|
    r.report("TZInfo"){
      n.times{
        lt1 = Time.local(*t1)
        nt1 = tzinfo.local_to_utc(lt1)
      }
    }
    r.report("Time.utc"){
      n.times{
        nt2 = Time.utc(*t2)
      }
    }
  }

rescue LoadError
  warn "tzinfo not found, benchmark skipped"
end
