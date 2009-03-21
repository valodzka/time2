require File.join(File.dirname(__FILE__), 'helper')

class TestForCountry < Test::Unit::TestCase
  def test_for_country
    a = TimeZone.for_country('ES')
    assert_equal 3, a.size
  end

  def test_strftime_z
    with_tz("Japan") do
      t = Time.local(2009, 3, 3)
      z = t.strftime("%Z")
      assert_equal "JST", z
      z = t.strftime("%z")
      assert_equal "+0900", z
    end
  end
end
