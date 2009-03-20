require File.join(File.dirname(__FILE__), 'test_helper')

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
end
