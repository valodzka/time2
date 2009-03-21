require File.join(File.dirname(__FILE__), 'helper')

class TestLocalUTC < Test::Unit::TestCase
  def test_utc
    t = Time.utc(0, 0, 9, 1, 1, 2008, nil, nil, nil, "Japan")
    assert t.utc?
    assert_equal 0, t.hour
    assert_equal 1, t.mday
    assert_equal 1, t.month
    assert_equal 2008, t.year
  end

  def test_local
    with_tz("Europe/Prague") do
      t = Time.local(0, 0, 9, 1, 1, 2008, nil, nil, nil, "Japan")
      assert !t.utc?
      assert_equal 1, t.hour
      assert_equal 1, t.mday
      assert_equal 1, t.month
      assert_equal 2008, t.year
      assert_equal "CET", t.zone
    end
  end

  def test_leapsecond
    with_tz("right/UTC") do
      t = Time.local(2005, 12, 31, 23, 59, 59)
      leap = Time.at(t.to_i + 1)
      assert_equal 60, leap.sec
      assert_equal 59, leap.min
      assert_equal 2005, leap.year
    end
  end
end
