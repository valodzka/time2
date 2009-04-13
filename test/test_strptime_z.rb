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
    # TODO
    #t = Time.strptime("2007-10-10 GMT", "%Y-%m-%d %Z")
    #assert_equal("UTC", t.zone)
  end

  def test_procent_z_default_for_procent_s
    t = Time.strptime("-1 ", "%s")
    assert_equal(Time.now.zone, t.zone)
  end
end
