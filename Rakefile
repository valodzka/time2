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

desc "initialize Visual Studio Command Prompt"
task :vscp do
  if (vs=ENV["VS90COMNTOOLS"]) && !ENV['VCINSTALLDIR'] 
    sh %Q[%comspec% /k ""#{File.join(vs, '..', '..', 'VC', 'vcvarsall.bat')}"" x86]
  else
    puts(ENV['VCINSTALLDIR'] ? "VSCP already initialized" : "enviroment variable VS90COMNTOOLS not found")
  end
end

desc "run benchmarks"
task :bench do
  ruby "test/benchmark/bench_tz.rb"
  ruby "test/benchmark/bench_strptime.rb"
end

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
    sh(RUBY_PLATFORM =~ /mswin/ ? "nmake" : 'make')    
  end
  cp time2_so, "lib"
end

file "#{time2_dir}Makefile" => "#{time2_dir}extconf.rb" do
  Dir.chdir(time2_dir) do
    sh "#{Config::CONFIG['prefix']}/bin/#{Config::CONFIG['ruby_install_name']} extconf.rb"
  end
end
