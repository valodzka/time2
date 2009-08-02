require File.join(File.dirname(__FILE__), 'helper')

class TestTimeStrptimeZone < Test::Unit::TestCase
  def test_procent_z_local_zone
    l = Time.local(2007, 10, 10)
    t = Time.strptime("2007-10-10", "%Y-%m-%d")
    assert_equal(l.zone, t.zone)
    assert_equal(2007, t.year)
    assert_equal(10, t.month)
    assert_equal(10, t.day)
    assert_equal(0, t.hour)
    assert_equal(0, t.min)
    assert_equal(0, t.sec)
    assert_equal(0, t.usec)
    assert_equal(l.dst?, t.dst?)
  end

  def test_procent_z_gmt
    t = Time.strptime("2007-10-10 GMT", "%Y-%m-%d %Z")
    utc = Time.utc(2007, 10, 10)
    utc_t = t.getutc
    assert_equal utc.year, utc_t.year
    assert_equal utc.mon,  utc_t.mon
    assert_equal utc.day,  utc_t.day
    assert_equal utc.hour, utc_t.hour
    assert_equal utc.min,  utc_t.hour
    assert_equal utc.sec,  utc_t.sec
  end

  def test_procent_z_default_for_procent_s
    t = Time.strptime("-1 ", "%s")
    assert_equal(Time.now.zone, t.zone)
  end

  def test_defined_zone
    # standard timezone of Japan, and is 9 hours ahead of UTC
    t = Time.strptime("2007-10-10 JST", "%Y-%m-%d %Z").getutc
    utc = Time.utc(2007, 10, 10)
    assert_equal utc.hour, t.hour-9
  end
end
