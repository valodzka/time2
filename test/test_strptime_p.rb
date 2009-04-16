require File.join(File.dirname(__FILE__), 'helper')

class TestTimeStrptimeAMPM < Test::Unit::TestCase
  def test_am_pm_before_after_hour
    t = Time.strptime("2 PM", "%I %p")
    assert_equal(14, t.hour)
    t = Time.strptime("PM 2", "%p %I")
    assert_equal(14, t.hour)
  end

  def test_L_flag
    t = Time.strptime('1', '%L')
    ruby19 do
      assert_equal 1_000_000, t.nsec
    end
    assert_equal 1_000, t.usec

    t = Time.strptime('999', '%L')
    ruby19 do
      assert_equal 999_000_000, t.nsec
    end
    assert_equal 999_000, t.usec

    assert_raise ArgumentError do
      t = Time.strptime('9995', '%L')
    end
  end
end
