require 'rake'
require 'rake/clean'
require 'rake/rdoctask'
require 'rake/testtask'

BIN = "*.{bundle,so,o,obj,pdb,lib,def,exp}"
CLEAN.include ["ext/time2/#{BIN}", "lib/**/#{BIN}", 'ext/time2/Makefile',
               '**/.*.sw?', '*.gem', '.config', 'pkg']

desc "Run all the tests"
Rake::TestTask.new do |t|
    t.libs = []
    t.test_files = FileList['test/test_*.rb']
    t.verbose = true
end

task :default => [:build, :test]

task :build => :time2

time2_dir = "ext/time2/"
time2_so = "#{time2_dir}time2.#{Config::CONFIG['DLEXT']}"
time2_files = FileList[
  "#{time2_dir}/*.c",
  "#{time2_dir}/*.h",
  "#{time2_dir}/extconf.rb",
  "#{time2_dir}/Makefile",
  "lib"
]
task :time2 => ["#{time2_dir}Makefile", time2_so]

file time2_so => time2_files do
  Dir.chdir(time2_dir) do
    if (vs=ENV["VS90COMNTOOLS"]) && !ENV['VCINSTALLDIR'] # initialize VS 9.0
      prefix = %Q[%comspec% /k ""#{File.join(vs, '..', '..', 'VC', 'vcvarsall.bat')}"" x86 &&]
    end
    
    sh(RUBY_PLATFORM =~ /mswin/ ? "#{prefix} nmake" : 'make')
  end
  cp time2_so, "lib"
end

file "#{time2_dir}Makefile" => "#{time2_dir}extconf.rb" do
  Dir.chdir(time2_dir) do
    sh "#{Config::CONFIG['prefix']}/bin/#{Config::CONFIG['ruby_install_name']} extconf.rb"
  end
end
