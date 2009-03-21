require File.join(File.dirname(__FILE__), 'helper')

class TestTimeStrptimeAMPM < Test::Unit::TestCase
  def test_am_pm_before_after_hour
    t = Time.strptime("2 PM", "%I %p")
    assert_equal(14, t.hour)
    t = Time.strptime("PM 2", "%p %I")
    assert_equal(14, t.hour)
  end
end
