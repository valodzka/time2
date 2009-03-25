require 'time2'
require 'time2/tzinfo/time_or_datetime'

module TZInfo
  class Timezone
    class << self
      def get(tzname)
        Timezone.new(TimeZone[tzname])
      end
    end

    def initialize(tz)
      @tz = tz
    end

    def local_to_utc(local, dst=nil)
      TimeOrDateTime.wrap(local) {|t|
        t.getlocal(@tz).getutc
      }
    end

    def utc_to_local(utc)
      TimeOrDateTime.wrap(utc) {|t|
        t.getlocal(@tz)
      }
    end

    def now
      Time.now.local(@tz)
    end

    def current_period
      Time.now.local(@tz).period
    end

    def current_period_and_time
      [self.now, self.current_period]
    end

    alias :current_time_and_period :current_period_and_time

    def period_for_utc(utc)
    end

    def period_for_local(loca)
    end

    def periods_for_local(local)
    end

    def strftime(format, utc=Time.now.utc)
      utc.getlocal(@tz).strftime format
    end

    def to_s
      @tz.to_s
    end
  end

  class TimezonePeriod
    def initialize(time, tz)
      @time = time
      @tz = tz
    end

    def utc_offset
      utc_total_offset - std_offset
    end

    def std_offset
      if @time.dst? then 60 else 0 end
    end

    def abbreviation
      @time.zone.to_sym
    end

    def utc_total_offset
      @time.gmtoff
    end

    alias :zone_identifier :abbreviation

    def utc_start
      nil
    end

    def utc_end
      nil
    end

    def local_start
      nil
    end

    def local_end
      nil
    end

    def dst?
      @time.dst?
    end
  end
end