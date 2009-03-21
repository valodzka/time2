require File.join(File.dirname(__FILE__), 'helper')

if RUBY_VERSION !~ /^1.9/
  exit Test::Unit::AutoRunner.run(true, File.dirname(__FILE__), %w{-a test_mri_date_strptime.rb -a .})
else
  Test::Unit.setup_argv {|files|
    Dir['test/test_*.rb']
  }
end

