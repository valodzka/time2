# -*- coding: utf-8 -*-
require File.join(File.dirname(__FILE__), 'helper')

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
    assert_equal 1199138400, t.to_i
  end

  def test_fill_gap_if_low_given
    ts = Time.now
    t = Time.strptime "1", "%N"
    assert ts.to_i - t.to_i < 2
    assert_equal 1, t.nsec
  end

  def test_j_replaces_m_and_d
    ts = Time.now
    t = Time.strptime "3 3 10", "%m %d %j"
    assert_equal ts.year, t.year
    assert_equal 1, t.mon
    assert_equal 10, t.day
  end

  def test_utf_8
    t = Time.strptime "День:3, Месяц:3, Год:2008", "День:%d, Месяц:%m, Год:%Y"
    assert_equal 2008, t.year
    assert_equal 3, t.mon
    assert_equal 3, t.day
  end

  def test_utf_16
    ruby19 do
      assert_raise ArgumentError do
        Time.strptime "День:3, Месяц:3, Год:2008", "День:%d, Месяц:%m, Год:%Y".encode("UTF-16be")
      end
    end
  end

  def test_slash_0
    t = Time.strptime "2008\0 5 \0 3", "%Y\0 %m \0 %d"
    assert_equal 2008, t.year
    assert_equal 5, t.mon
    assert_equal 3, t.day
  end
end
