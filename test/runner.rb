require File.join(File.dirname(__FILE__), 'helper')

if RUBY_VERSION !~ /^1.9/
  exit Test::Unit::AutoRunner.run(true, File.dirname($0), %w{-a test_mri_date_strptime.rb  -a .})
else
  Test::Unit.setup_argv {|files|
    if files.empty?
      [src_testdir]
    else
      files.map {|f|
        if File.exist? "#{src_testdir}/#{f}"
          "#{src_testdir}/#{f}"
        elsif File.exist? "#{srcdir}/#{f}"
          "#{srcdir}/#{f}"
        elsif File.exist? f
          f
        else
          raise ArgumentError, "not found: #{f}"
        end
      }
    end
  }
end
