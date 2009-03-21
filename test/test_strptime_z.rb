require File.join(File.dirname(__FILE__), 'helper')

class TestTimeStrptimeZone < Test::Unit::TestCase
  def test_procent_z_local_zone
    with_tz 'Japan' do
      t = Time.strptime("2007-10-10", "%Y-%m-%d")
      assert_equal("JST", t.zone)
      assert_equal(2007, t.year)
      assert_equal(10, t.month)
      assert_equal(10, t.day)
      assert_equal(0, t.hour)
      assert_equal(0, t.min)
      assert_equal(0, t.sec)
      assert_equal(0, t.usec)
      assert_equal(false, t.dst?)
    end
  end

  def test_procent_z_gmt
    t = Time.strptime("2007-10-10 GMT", "%Y-%m-%d %Z")
    assert_equal("UTC", t.zone)
  end

  def test_procent_z_default_for_procent_s
    with_tz 'Japan' do
      t = Time.strptime("-1 ", "%s")
      assert_equal("JST", t.zone)
    end
  end
end
