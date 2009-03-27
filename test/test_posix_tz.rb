require File.join(File.dirname(__FILE__), 'helper')

class TestPosixTZ < Test::Unit::TestCase
  def test_plus
    with_tz('UTC-120') {
      t = Time.now
      #TODO: should this work? assert_equal "UTC+100", t.zone
      assert_equal 120*3600, t.gmtoff
      t = Time.local(2009, 1, 6)
      assert_equal 2009, t.year
      assert_equal 1, t.mon
      assert_equal Time.utc(2009, 1, 1).to_i, t.to_i
    }
  end

  def test_complex
    with_tz('GMT0BST-1,87/01:00:00,297/02:00:00') do
      t1 = Time.local(2009, 5, 30, 1, 0, 0)
      t2 = Time.local(2009, 1, 30, 1, 0, 0)

      assert_equal t1.gmtoff, 3600
      assert_equal t2.gmtoff, 0
    end
  end
end
