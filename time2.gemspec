
# -*- encoding: utf-8 -*-

Gem::Specification.new do |s|
  s.name = %q{time2}
  s.version = "0.0.1"

  s.required_rubygems_version = Gem::Requirement.new(">= 0") if s.respond_to? :required_rubygems_version=
    s.authors = ["Pavel Valodzka"]
  s.date = %q{2009-03-31}
  s.description = %q{Extensions of ruby Time class}
  s.email = %q{pavel@valodzka.name}
  #s.extra_rdoc_files = ["README.md", "LICENSE"]
  s.extensions = ['ext/time2/extconf.rb']
  s.files = %w{
ext/time2/asctime.c	ext/time2/ialloc.c	ext/time2/pgtz.h	ext/time2/strftime.c	ext/time2/zic.c
ext/time2/c.h		ext/time2/localtime.c	ext/time2/private.h	ext/time2/strptime.c
ext/time2/extconf.rb	ext/time2/pgtime.h	ext/time2/ruby_zic.c	ext/time2/time2.c
ext/time2/getopt.c	ext/time2/pgtz.c	ext/time2/scheck.c	ext/time2/tzfile.h}
  s.has_rdoc = true
  s.homepage = %q{http://github.com/valodzka/time2}
  s.rdoc_options = ["--inline-source", "--charset=UTF-8"]
  s.require_paths = ["lib"]
  #s.rubyforge_project = %q{}
  s.rubygems_version = %q{1.3.0}
  s.summary = s.description

  if s.respond_to? :specification_version then
    current_version = Gem::Specification::CURRENT_SPECIFICATION_VERSION
    s.specification_version = 2
  end
end
