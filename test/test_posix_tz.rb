require File.join(File.dirname(__FILE__), 'helper')

class TestPosixTZ < Test::Unit::TestCase
  def test_plus
    #TODO: should this work? assert_equal "UTC+100", t.zone
    tz = 'UTC-120'
    t = Time.local(0, 0, 0, 6, 1, 2009, nil, nil, nil, tz)
    #TODO: fix : assert_equal 120*3600, t.gmtoff
    assert_equal 2009, t.year
    assert_equal 1, t.mon
    assert_equal Time.utc(2009, 1, 1).to_i, t.to_i
  end

  def test_complex
    tz = 'GMT0BST-1,87/01:00:00,297/02:00:00'
    t1 = Time.local(0, 0, 1, 30, 5, 2009, nil, nil, nil, tz)
    t2 = Time.local(0, 0, 1, 30, 1, 2009, nil, nil, nil, tz)
    # TODO: how to test it ?
    #assert_equal 3600, t1.gmtoff-t2.gmtoff
    #assert_equal 0, t2.gmtoff
  end
end
