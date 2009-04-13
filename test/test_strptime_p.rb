require File.join(File.dirname(__FILE__), 'helper')

class TestTimeStrptimeAMPM < Test::Unit::TestCase
  def test_am_pm_before_after_hour
    t = Time.strptime("2 PM", "%I %p")
    assert_equal(14, t.hour)
    t = Time.strptime("PM 2", "%p %I")
    assert_equal(14, t.hour)
  end

  def test_L_flag
    ruby19 do
      t = Time.strptime('1', '%L')
      assert_equal 1_000_000, t.nsec
      t = Time.strptime('999', '%L')
      assert_equal 999_000_000, t.nsec
      assert_raise ArgumentError do
        t = Time.strptime('9995', '%L')
      end
    end
  end
end
