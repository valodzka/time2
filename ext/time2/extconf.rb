require 'mkmf'

def try_code(code, expr, headers=nil, opt=nil,&b)
  includes = cpp_include(headers)
  src = %$#{includes}
#include <stdio.h>
int main() { #{code} {
    int conftest_const = (int)(#{expr});
    printf("%d\\n", conftest_const);
    return 0;
  } } $
  if try_link0(src, opt, &b)
    xpopen("./conftest") do |f|
      return Integer(f.gets)
    end
  end
  nil
end

def check_leap_seconds()
  checking_for checking_message("for leap seconds") do
    # 1136073622 is 2005 Dec 31, 23:59:60 if system support leap seconds and 2006 Jan 01 00:00:22 otherwise
    if try_code('time_t t = 1136073622;', 'gmtime(&t)->tm_sec', 'time.h') == 60
      $defs << "-DHAVE_LEAP_SECONDS"
    end
  end
end

# allow use old ruby time in parallel with new
# adds aditional checks and convertation
$defs << "-DOLD_TIME_COMPAT"
# check if OS use leap seconds
check_leap_seconds()
# Check for new hash API (MurmurHash)
have_func('rb_hash_start', 'ruby.h')
# Different versions compatibility
case RUBY_VERSION
when /^1.9/
  $defs << '-DRUBY_TIME_19_COMPAT'
when /^1.8/
  $defs << '-DRUBY_TIME_18_COMPAT'
else
  raise 'You use unsupported ruby version'
end
# Disable use zic.c and others
$objs = %w{asctime.o  localtime.o  pgtz.o strftime.o  strptime.o  time.o}
create_makefile("time2")

# $objs = %w{localtime.o  zic.o scheck.o}
#create_makefile("zic")

