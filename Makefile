
SHELL = /bin/sh

#### Start of system configuration section. ####

srcdir = .
topdir = /usr/include/ruby-1.9.0
hdrdir = /usr/include/ruby-1.9.0
arch_hdrdir = /usr/include/ruby-1.9.0/$(arch)
VPATH = $(srcdir);$(arch_hdrdir)/ruby;$(hdrdir)/ruby
prefix = $(DESTDIR)/usr
exec_prefix = $(prefix)
bindir = $(exec_prefix)/bin
sbindir = $(exec_prefix)/sbin
libexecdir = $(exec_prefix)/libexec
datadir = $(prefix)/share
sysconfdir = $(prefix)/etc
sharedstatedir = $(DESTDIR)/etc
localstatedir = $(DESTDIR)/var
libdir = $(exec_prefix)/lib
includedir = $(prefix)/include
oldincludedir = $(DESTDIR)/usr/include
infodir = $(prefix)/info
mandir = $(prefix)/man
sitedir = $(prefix)/lib/ruby/site_ruby
vendordir = $(prefix)/lib/ruby/vendor_ruby
rubyhdrdir = $(includedir)/ruby-$(ruby_version)
sitehdrdir = $(rubyhdrdir)/site_ruby
vendorhdrdir = $(rubyhdrdir)/vendor_ruby
rubylibdir = $(libdir)/ruby/$(ruby_version)
archdir = $(rubylibdir)/$(arch)
sitelibdir = $(sitedir)/$(ruby_version)
sitearchdir = $(sitelibdir)/$(sitearch)
vendorlibdir = $(vendordir)/$(ruby_version)
vendorarchdir = $(vendorlibdir)/$(sitearch)

CC = cl -nologo
CXX = $(CC)
LIBRUBY = $(RUBY_SO_NAME).lib
LIBRUBY_A = $(RUBY_SO_NAME)-static.lib
LIBRUBYARG_SHARED = $(LIBRUBY)
LIBRUBYARG_STATIC = $(LIBRUBY_A)
OUTFLAG = -Fe
COUTFLAG = -Fo

RUBY_EXTCONF_H = 
cflags   = 
optflags = 
debugflags = 
warnflags = 
CFLAGS   =  -MD -Zi  -O2b2xty-  -Zm200 
INCFLAGS = -I. -I$(arch_hdrdir) -I$(hdrdir)/ruby/backward -I$(hdrdir) -I$(srcdir)
DEFS     = -D_CRT_SECURE_NO_DEPRECATE -D_CRT_NONSTDC_NO_DEPRECATE
CPPFLAGS =  -D_CRT_SECURE_NO_DEPRECATE -D_CRT_NONSTDC_NO_DEPRECATE  
CXXFLAGS = $(CFLAGS) -MD -Zi  -O2b2xty-  -Zm200
ldflags  = 
dldflags = -link -incremental:no -debug -opt:ref -opt:icf -dll $(LIBPATH)
archflag = 
DLDFLAGS = $(ldflags) $(dldflags) $(archflag)
LDSHARED = cl -nologo -LD
LDSHAREDXX = $(LDSHARED)
AR = lib -nologo
EXEEXT = .exe

RUBY_INSTALL_NAME = ruby
RUBY_SO_NAME = msvcr90-ruby190
arch = i386-mswin32_90
sitearch = i386-msvcr90
ruby_version = 1.9.0
ruby = /usr/bin/ruby
RUBY = $(ruby:/=\)
RM = $(COMSPEC) /C $(top_srcdir:/=\)\win32\rm.bat
RM_RF = $(RUBY) -run -e rm -- -rf
MAKEDIRS = @$(RUBY) -run -e mkdir -- -p
INSTALL = @$(RUBY) -run -e install -- -vp
INSTALL_PROG = $(INSTALL) -m 0755
INSTALL_DATA = $(INSTALL) -m 0644
COPY = copy > nul

#### End of system configuration section. ####

preload = 

libpath = . $(libdir)
LIBPATH =  -libpath:"." -libpath:"$(libdir)"
DEFFILE = $(TARGET)-$(arch).def

CLEANFILES = mkmf.log
DISTCLEANFILES = vc*.pdb $(DEFFILE)

extout = 
extout_prefix = 
target_prefix = 
LOCAL_LIBS = 
LIBS = $(LIBRUBYARG_SHARED)  oldnames.lib user32.lib advapi32.lib shell32.lib ws2_32.lib  
SRCS = asctime.c localtime.c pgtz.c test.c time.c
OBJS = asctime.obj localtime.obj pgtz.obj test.obj time.obj
TARGET = time2
DLLIB = $(TARGET).so
EXTSTATIC = 
STATIC_LIB = 

BINDIR        = $(bindir)
RUBYCOMMONDIR = $(sitedir)$(target_prefix)
RUBYLIBDIR    = $(sitelibdir)$(target_prefix)
RUBYARCHDIR   = $(sitearchdir)$(target_prefix)
HDRDIR        = $(rubyhdrdir)/ruby$(target_prefix)
ARCHHDRDIR    = $(rubyhdrdir)/$(arch)/ruby$(target_prefix)

TARGET_SO     = $(DLLIB)
CLEANLIBS     = $(TARGET).so 
CLEANOBJS     = *.obj $(TARGET).exp $(TARGET).lib $(TARGET).pdb *.bak

all:		$(DLLIB)
static:		$(STATIC_LIB)

clean:
		@-$(RM) $(CLEANLIBS:/=\) $(CLEANOBJS:/=\) $(CLEANFILES:/=\)

distclean:	clean
		@-$(RM_RF) conftest.dSYM
		@-$(RM) Makefile $(RUBY_EXTCONF_H) conftest.* mkmf.log
		@-$(RM) core ruby$(EXEEXT) *~ $(DISTCLEANFILES:/=\)

realclean:	distclean
install: install-so install-rb

install-so: $(RUBYARCHDIR)
install-so: $(RUBYARCHDIR)/$(DLLIB)
$(RUBYARCHDIR)/$(DLLIB): $(DLLIB)
	$(INSTALL_PROG) $(DLLIB:/=\) $(RUBYARCHDIR:/=\)
install-rb: pre-install-rb install-rb-default
install-rb-default: pre-install-rb-default
pre-install-rb: Makefile
pre-install-rb-default: Makefile
$(RUBYARCHDIR):
	$(MAKEDIRS) $@

site-install: site-install-so site-install-rb
site-install-so: install-so
site-install-rb: install-rb

.SUFFIXES: .c .m .cc .cxx .cpp .obj

{$(hdrdir)}.cc.obj:
	$(CXX) $(INCFLAGS) $(CXXFLAGS) $(CPPFLAGS) $(COUTFLAG)$(@) -c -Tp$(<:\=/)

{$(topdir)}.cc.obj:
	$(CXX) $(INCFLAGS) $(CXXFLAGS) $(CPPFLAGS) $(COUTFLAG)$(@) -c -Tp$(<:\=/)

{$(srcdir)}.cc.obj:
	$(CXX) $(INCFLAGS) $(CXXFLAGS) $(CPPFLAGS) $(COUTFLAG)$(@) -c -Tp$(<:\=/)

.cc.obj:
	$(CXX) $(INCFLAGS) $(CXXFLAGS) $(CPPFLAGS) $(COUTFLAG)$(@) -c -Tp$(<:\=/)

{$(hdrdir)}.cxx.obj:
	$(CXX) $(INCFLAGS) $(CXXFLAGS) $(CPPFLAGS) $(COUTFLAG)$(@) -c -Tp$(<:\=/)

{$(topdir)}.cxx.obj:
	$(CXX) $(INCFLAGS) $(CXXFLAGS) $(CPPFLAGS) $(COUTFLAG)$(@) -c -Tp$(<:\=/)

{$(srcdir)}.cxx.obj:
	$(CXX) $(INCFLAGS) $(CXXFLAGS) $(CPPFLAGS) $(COUTFLAG)$(@) -c -Tp$(<:\=/)

.cxx.obj:
	$(CXX) $(INCFLAGS) $(CXXFLAGS) $(CPPFLAGS) $(COUTFLAG)$(@) -c -Tp$(<:\=/)

{$(hdrdir)}.cpp.obj:
	$(CXX) $(INCFLAGS) $(CXXFLAGS) $(CPPFLAGS) $(COUTFLAG)$(@) -c -Tp$(<:\=/)

{$(topdir)}.cpp.obj:
	$(CXX) $(INCFLAGS) $(CXXFLAGS) $(CPPFLAGS) $(COUTFLAG)$(@) -c -Tp$(<:\=/)

{$(srcdir)}.cpp.obj:
	$(CXX) $(INCFLAGS) $(CXXFLAGS) $(CPPFLAGS) $(COUTFLAG)$(@) -c -Tp$(<:\=/)

.cpp.obj:
	$(CXX) $(INCFLAGS) $(CXXFLAGS) $(CPPFLAGS) $(COUTFLAG)$(@) -c -Tp$(<:\=/)

{$(hdrdir)}.c.obj:
	$(CC) $(INCFLAGS) $(CFLAGS) $(CPPFLAGS) $(COUTFLAG)$(@) -c -Tc$(<:\=/)

{$(topdir)}.c.obj:
	$(CC) $(INCFLAGS) $(CFLAGS) $(CPPFLAGS) $(COUTFLAG)$(@) -c -Tc$(<:\=/)

{$(srcdir)}.c.obj:
	$(CC) $(INCFLAGS) $(CFLAGS) $(CPPFLAGS) $(COUTFLAG)$(@) -c -Tc$(<:\=/)

.c.obj:
	$(CC) $(INCFLAGS) $(CFLAGS) $(CPPFLAGS) $(COUTFLAG)$(@) -c -Tc$(<:\=/)

$(DLLIB): $(DEFFILE) $(OBJS)
	@-$(RM) $(@:/=\)
	$(LDSHARED) -Fe$(@) $(OBJS) $(LIBS) $(LOCAL_LIBS) $(DLDFLAGS) -implib:$(*F:.so=)-$(arch).lib -pdb:$(*F:.so=)-$(arch).pdb -def:$(DEFFILE)
	@$(RUBY) -run -e wait_writable -- -n 10 $(@)
	mt -nologo -manifest $(@).manifest -outputresource:$(@);2
	@$(RM) $(@:/=\).manifest



$(DEFFILE): 
	$(RUBY) -e "puts 'EXPORTS', 'Init_$(TARGET)'"  > $@

$(OBJS): {.;$(VPATH)}ruby.h {.;$(VPATH)}defines.h
