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
    now = Time.local(2008)
    return if now.dst? # canno't happens
    offset = now.gmtoff
    t = Time.local(0, 0, 23, 1, 1, 2008, nil, nil, nil, "Japan")
    assert !t.utc?
    assert_equal 23-9+offset/3600, t.hour
    assert_equal 1, t.mday
    assert_equal 1, t.month
    assert_equal 2008, t.year
    assert_equal now.zone, t.zone
  end

  def test_leapsecond
    leap = Time.local(59, 59, 23, 31, 12, 2005, nil, nil, nil, "right/UTC")
    normal = Time.local(59, 59, 23, 31, 12, 2005, nil, nil, nil, "UTC")
    assert_equal 22, leap.to_i - normal.to_i
  end

  def test_hash_args
    t1 = Time.local(:year => 2009, :mon => 11, :day => 1, :hour => 1,  :min => 30, :dst => false, :tz => "America/New_York").utc
    t2 = Time.local(:year => 2009, :mon => 11, :day => 1, :hour => 1,  :min => 30, :dst => true, :tz => "America/New_York").utc
    assert_equal 3600,  t1.to_i - t2.to_i

    t = Time.utc(:year => 2009, :yday => 100)
    # TODO: implement this
    #assert_equal t.mon, 4
     #assert_equal t.day, 10
  end

  def test_hash_args_error
    assert_raise ArgumentError do
      Time.utc(:yer => 2009)
    end
  end
end
