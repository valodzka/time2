$: << File.join(File.dirname(__FILE__), '..', 'lib') << '.'

require 'time2'
require 'test/unit'

def ruby19
  yield if RUBY_VERSION =~ /^1\.9/
end

def with_tz(tz)
  before = TimeZone.local(tz)
  yield
ensure
  TimeZone.local(before)
end

# API missing in ruby 1.8.6
def Dir.mktmpdir(prefix_suffix=nil, tmpdir=nil)
  case prefix_suffix
  when nil
    prefix = "d"
    suffix = ""
  when String
    prefix = prefix_suffix
    suffix = ""
  when Array
    prefix = prefix_suffix[0]
    suffix = prefix_suffix[1]
  else
    raise ArgumentError, "unexpected prefix_suffix: #{prefix_suffix.inspect}"
  end
  tmpdir ||= Dir.tmpdir
  t = Time.now.strftime("%Y%m%d")
  n = nil
  begin
    path = "#{tmpdir}/#{prefix}#{t}-#{$$}-#{rand(0x100000000).to_s(36)}"
    path << "-#{n}" if n
    path << suffix
    Dir.mkdir(path, 0700)
  rescue Errno::EEXIST
    n ||= 0
    n += 1
    retry
  end

  if block_given?
    begin
      yield path
    ensure
      FileUtils.remove_entry_secure path
    end
  else
    path
  end
end unless Dir.respond_to? :mktmpdir

String.class_eval do
  def start_with?(prefix)
  self =~ /^#{Regexp.escape(prefix)}/
  end
end unless String.respond_to? :start_with?
