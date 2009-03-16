require File.join(File.dirname(__FILE__), 'test_helper')

class TestTimeStrptimeAMPM < Test::Unit::TestCase
  def test_am_pm_before_after_hour
    t = Time.strptime("2 PM", "%I %p")
    assert_equal(14, t.hour)
    t = Time.strptime("PM 02", "%p %I")
    assert_equal(14, t.hour)
  end

  #TODO:
  #def test_leading 
  #  t = Time.strptime("PM 2", "%p %I")
  #end
end
