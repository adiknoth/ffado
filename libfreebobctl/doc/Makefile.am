CLEANFILES = doxygen-build.stamp

DOX = libfreebobctl.doxygen

INSTIMAGES = html/doxygen.png

all-local: doxygen-build.stamp

doxygen-build.stamp: $(DOX) reference.dox 
	@echo '*** Running doxygen ***'
	doxygen $(DOX)
	touch doxygen-build.stamp

clean-local:
	if test -d html;  then rm -fr html;  fi
	if test -d latex; then rm -fr latex; fi
	if test -d man;   then rm -fr man;   fi

distclean-local: clean
	rm -f *.stamp || true
	if test -d html; then rm -rf html; fi
