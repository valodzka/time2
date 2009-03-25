require 'rake'
require 'rake/clean'
require 'rake/rdoctask'
require 'rake/testtask'

TBIN = "*.{bundle,o,obj,pdb,lib,def,exp}"
CLEAN.include ["ext/time2/#{TBIN}", "lib/**/#{TBIN}", 'ext/time2/Makefile', 'ext/time2/ZMakefile',
               'ext/time2/tz_countries.h', '**/.*.sw?', '*.gem', '.config', 'pkg', 'html']
BIN = "*.{so}"
CLOBBER.include ["ext/time2/#{TBIN}", "lib/**/#{TBIN}", 'zoneinfo']

Rake::TestTask.new do |t|
  t.test_files = FileList['test/runner.rb']
  #t.test_files = FileList['test/test_*.rb']
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

desc "build tz database"
task :zoneinfo => :zic do
  # If you want POSIX compatibility, use "America/New_York".
  posixrules =	'America/New_York'
  dir = 'tzdata'
  places = %w{africa asia europe northamerica antarctica
    australasia etcetera pacificnew southamerica
    backward solar87 solar88 solar89 systemv}.map{|file| File.join(dir, file) }
  # timezones without leap seconds
  ruby("-Ilib -rzic -e'zic %w{-dzoneinfo -p#{posixrules} #{places.join(' ')}}'")
  # timezones with leap seconds
  ruby("-Ilib -rzic -e'zic %w{-dzoneinfo/right -Ltzdata/leapseconds #{places.join(' ')}}'")
end

time2_dir = "ext/time2/"
time2_so = "#{time2_dir}time2.#{Config::CONFIG['DLEXT']}"
zic_so = "#{time2_dir}zic.#{Config::CONFIG['DLEXT']}"
time2_files = FileList[
  "#{time2_dir}*.c",
  "#{time2_dir}*.h",
  "#{time2_dir}extconf.rb",
  "#{time2_dir}Makefile",
  "#{time2_dir}tz_countries.h",
  "lib"
]
zic_files = FileList[
  "#{time2_dir}*.c",
  "#{time2_dir}*.h",
  "#{time2_dir}extconf.rb",
  "#{time2_dir}ZMakefile",
  "lib"
]
makes = FileList[
   "#{time2_dir}Makefile",
   "#{time2_dir}ZMakefile"]

task :zic => ["#{time2_dir}ZMakefile", zic_so]
task :time2 => ["#{time2_dir}Makefile", time2_so]

file zic_so => zic_files do
  Dir.chdir(time2_dir) do
    sh((RUBY_PLATFORM =~ /mswin/ ? "nmake /f" : 'make -f')+'ZMakefile')
  end
  cp zic_so, "lib"
end

file time2_so => time2_files do
  Dir.chdir(time2_dir) do
    sh(RUBY_PLATFORM =~ /mswin/ ? "nmake" : 'make')
  end
  cp time2_so, "lib"
end

makes.each do |makefile|
  file makefile => "#{time2_dir}extconf.rb" do
    Dir.chdir(time2_dir) do
      sh "#{Config::CONFIG['prefix']}/bin/#{Config::CONFIG['ruby_install_name']} extconf.rb"
    end
  end
end

file "#{time2_dir}tz_countries.h" => "tzdata/zone.tab" do
  countries = {}

  IO.foreach("tzdata/zone.tab") do |line|
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

  puts "generating tz_countries.h, #{countries.size} entries"
  File.open("#{time2_dir}tz_countries.h", "w") {|src|
    countries.each{|k,zones|
      ary = "rb_ary_new3(#{zones.size},\n\t\t#{zones.map{|z| %Q[rb_str_new("#{z}", #{z.length})] }.join(",\n\t\t")})"
      src.puts "rb_hash_aset(countries, rb_str_new(\"#{k}\", #{k.size}), \n\t#{ary});\n\n"
    }
  }
end

