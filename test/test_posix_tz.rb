require File.join(File.dirname(__FILE__), 'helper')

class TestPosixTZ < Test::Unit::TestCase
  def test_plus
    with_tz('UTC+100') {
      t = Time.now
      #TODO: should this work? assert_equal "UTC+100", t.zone
      assert_equal -100*3600, t.gmtoff
    }
  end
end
