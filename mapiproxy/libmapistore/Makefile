# simple makefile wrapper to run waf

WAF=WAF_MAKE=1 PATH=buildtools:../../buildtools:$$PATH waf

all:
	$(WAF) build

install:
	$(WAF) install

uninstall:
	$(WAF) uninstall

dist:
	touch .tmplock
	WAFLOCK=.tmplock $(WAF) dist

distcheck:
	touch .tmplock
	WAFLOCK=.tmplock $(WAF) distcheck

clean:
	$(WAF) clean

distclean:
	$(WAF) distclean

reconfigure: configure
	$(WAF) reconfigure

show_waf_options:
	$(WAF) --help

pydoctor:
	$(WAF) pydoctor

bin/%:: FORCE
	$(WAF) --targets=`basename $@`
FORCE:
