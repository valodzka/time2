require File.join(File.dirname(__FILE__), 'test_helper')

class TestDateStrptime < Test::Unit::TestCase
  def test_fill_gaps_if_high_given
    t = Time.strptime("2008", "%Y")
    assert_equal 2008, t.year
    assert_equal 1, t.mon
    assert_equal 1, t.day
    assert_equal 0, t.hour
    assert_equal 0, t.min
    assert_equal 0, t.sec
    assert_equal 0, t.nsec
  end

  def test_fill_gap_if_low_given
    ts = Time.now
    t = Time.strptime "1", "%N"
    assert_equal ts.year,  t.year
    assert_equal ts.mon, t.mon
    assert_equal ts.day, t.day
    assert_equal ts.hour, t.hour
    assert_equal ts.min, t.min
    assert_in_delta ts.sec, 3, t.sec
    assert_equal 1, t.nsec
  end

  def test_j_replaces_m_and_d
    ts = Time.now
    t = Time.strptime "3 3 10", "%m %d %j"
    assert_equal ts.year, t.year
    assert_equal 1, t.mon
    assert_equal 10, t.day
  end
end
