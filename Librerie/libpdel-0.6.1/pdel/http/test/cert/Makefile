# $Id: Makefile 317 2002-05-03 21:39:04Z archie $

certs:
	@for DIR in ca ca/certs; do \
		if [ ! -d $${DIR} ]; then \
			echo mkdir $${DIR}; \
			mkdir $${DIR}; \
		fi; \
	done
	@sh rc.sslkey ../demo.key ../demo.crt `hostname`

clean cleandir:
	rm -rf ca

