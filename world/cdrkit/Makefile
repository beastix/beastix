ifneq ($(CFLAGS),)
CMAKETWEAKS += ( cd build ; cmake .. -DCMAKE_C_FLAGS="$(CFLAGS)" ) || exit 1; 
endif

ifneq ($(LDFLAGS),)
CMAKETWEAKS += (cd build ; cmake .. -DCMAKE_EXE_LINKER_FLAGS:STRING="$(LDFLAGS)" -DCMAKE_MODULE_LINKER_FLAGS:STRING="$(LDFLAGS)" -DCMAKE_SHARED_LINKER_FLAGS:STRING="$(LDFLAGS)" ) || exit 1; 
endif

ifneq ($(PREFIX),)
CMAKETWEAKS += ( cd build ;  cmake .. -DCMAKE_INSTALL_PREFIX="$(PREFIX)") || exit 1; 
endif

ifneq ($(MANSUBDIR),)
CMAKETWEAKS += ( cd build ;  cmake .. -DMANSUBDIR="$(MANSUBDIR)" ) || exit 1; 
endif

default_target: all

DISTNAME=cdrkit-$(shell cat VERSION)
DEBSRCNAME=cdrkit_$(shell cat VERSION | sed -e "s,pre,~pre,").orig.tar.gz

build/Makefile:
	@-mkdir build 2>/dev/null
	cd build && cmake ..

cmakepurge:
	rm -rf install_manifest.txt progress.make CMakeFiles CMakeCache.txt cmake_install.cmake 
	rm -rf */install_manifest.txt */progress.make */CMakeFiles */CMakeCache.txt */cmake_install.cmake 
	rm -rf */*/install_manifest.txt */*/progress.make */*/CMakeFiles */*/CMakeCache.txt */*/cmake_install.cmake 
	rm */Makefile */*/Makefile

clean:
	rm -rf build

tarball:
#	if test "$(shell svn status | grep -v -i make)" ; then echo Uncommited files found. Run \"svn status\" to display them. ; exit 1 ; fi
	@if test -f ../$(DISTNAME).tar.gz ; then echo ../$(DISTNAME).tar.gz exists, not overwritting ; exit 1; fi
	-svn up
	rm -rf tmp
	mkdir tmp
	svn export . tmp/$(DISTNAME)
	rm -rf tmp/$(DISTNAME)/debian
	tar -f - -c -C tmp $(DISTNAME) | gzip -9 > ../$(DISTNAME).tar.gz
	rm -rf tmp
	test -e /etc/debian_version && ln -f ../$(DISTNAME).tar.gz ../$(DEBSRCNAME) || true
	test -e ../tarballs && ln -f ../$(DISTNAME).tar.gz ../tarballs/$(DEBSRCNAME) || true

tarball-remove:
	rm -f ../$(DISTNAME).tar.gz ../tarballs/$(DEBSRCNAME) ../$(DEBSRCNAME)

SVNBASE=$(shell svn info | grep URL: | cut -f2 -d' ' | xargs dirname)
release: tarball
	svn ci
	svn cp $(SVNBASE)/trunk $(SVNBASE)/tags/release_$(shell cat VERSION)

#%::
#	$(MAKE) $(MAKE_FLAGS) build/Makefile
#	$(CMAKETWEAKS)
#	$(MAKE) -C build $(MAKE_FLAGS) $@

# needs to be explicite, for PHONY and install (AKA INSTALL) file on cygwin
install: build/Makefile
	$(CMAKETWEAKS)
	$(MAKE) -C build $(MAKE_FLAGS) $@

all: build/Makefile
	$(CMAKETWEAKS)
	$(MAKE) -C build $(MAKE_FLAGS) $@

.PHONY: install all


