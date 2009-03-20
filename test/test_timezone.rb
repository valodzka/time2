require File.join(File.dirname(__FILE__), 'test_helper')

class TestForCountry < Test::Unit::TestCase
  def test_for_country
    a = TimeZone.for_country('ES')
    assert_equal 3, a.size
  end
end
