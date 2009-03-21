require 'rake'
require 'rake/clean'
require 'rake/rdoctask'
require 'rake/testtask'
#require 'rdoc/rdoc'

BIN = "*.{bundle,so,o,obj,pdb,lib,def,exp}"
CLEAN.include ["ext/time2/#{BIN}", "lib/**/#{BIN}", 'ext/time2/Makefile',
               '**/.*.sw?', '*.gem', '.config', 'pkg', 'html']

Rake::TestTask.new do |t|
  t.libs = []
  t.test_files = FileList['test/runner.rb']
  t.verbose = true
end

Rake::RDocTask.new do |rdoc|
  rdoc.rdoc_files.add('ext/time2/time.c')
  rdoc.rdoc_dir = 'html' # rdoc output folder
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
  ruby "--version"
  ruby "test/benchmark/bench_tz.rb"
  ruby "test/benchmark/bench_strptime.rb"
end

time2_dir = "ext/time2/"
time2_so = "#{time2_dir}time2.#{Config::CONFIG['DLEXT']}"
time2_files = FileList[
  "#{time2_dir}*.c",
  "#{time2_dir}*.h",
  "#{time2_dir}extconf.rb",
  "#{time2_dir}Makefile",
  "#{time2_dir}tz_countries.h",
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

file "#{time2_dir}tz_countries.h" => "zoneinfo/zone.tab" do  
  countries = {}

  IO.foreach("zoneinfo/zone.tab") do |line|
    line.strip!
    next if line =~ /^#/ || line.empty?
    # example:
    #   AR	-3124-06411	America/Argentina/Cordoba	most locations (CB, CC, CN, ER, FM, MN, SE, SF)
    if line =~ /^([A-Z]{2})\t+[+-]\d+[+-]\d+\t+(\S+)(\t.*)?$/
      (countries[$1] ||= []) << $2     
    else
      puts "not match #{line}"
    end
  end
  
  File.open("#{time2_dir}tz_countries.h", "w") {|src|
    countries.each{|k,zones|
      ary = "rb_ary_new3(#{zones.size},\n\t\t#{zones.map{|z| %Q[rb_str_new("#{z}", #{z.length})] }.join(",\n\t\t")})"
      src.puts "rb_hash_aset(countries, rb_str_new(\"#{k}\", #{k.size}), \n\t#{ary});\n\n"
    }
  }  
end

