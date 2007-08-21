all: glick-shell header.a glick.spec

PREFIX=/usr
LIBDIR=${PREFIX}/lib
BINDIR=${PREFIX}/bin
CFLAGS=-O2 -Wall -g

VERSION=0.1

TEST_FILES=test.sh data.txt
HEADER_SOURCES=ext2fs.c mem_io_manager.c glick.c

dist:	glick.spec
	rm -rf glick-${VERSION}
	mkdir glick-${VERSION}
	cp README AUTHORS glick-${VERSION}
	cp Makefile glick-${VERSION}
	cp ${TEST_FILES} glick-${VERSION}
	cp glick-shell.c ${HEADER_SOURCES} glick-${VERSION}
	cp mkglick glick-mkext2 glick-extract glick-${VERSION}
	cp test-icon.png test.desktop glick-${VERSION}
	cp glick.spec glick.spec.in glick-${VERSION}
	tar czvf glick-${VERSION}.tar.gz glick-${VERSION}
	rm -rf glick-${VERSION}

glick.spec: glick.spec.in
	sed s/\@VERSION\@/${VERSION}/ glick.spec.in > glick.spec 

install: header.a glick-shell mkglick glick-mkext2
	mkdir -p ${LIBDIR}/glick/
	install header.a ${LIBDIR}/glick/

	mkdir -p ${BINDIR}/
	install glick-shell glick-mkext2 ${BINDIR}/
	sed s#LIBDIR=.#LIBDIR=${LIBDIR}/glick# mkglick > ${BINDIR}/mkglick
	chmod a+x ${BINDIR}/mkglick

glick-shell: glick-shell.c
	gcc ${CFLAGS} -o glick-shell glick-shell.c

header.a: ${HEADER_SOURCES}
	gcc -c ${CFLAGS} ${HEADER_SOURCES} `pkg-config --cflags fuse ext2fs`
	rm -f header.a
	ar r header.a ext2fs.o mem_io_manager.o glick.o

clean:
	rm -f header.a *.o test test.ext2 glick-shell

test:	header.a mkglick test.ext2 test-icon.png test.desktop
	./mkglick test test.ext2 --icon test-icon.png --desktop-file test.desktop

test.ext2: test.sh data.txt
	rm -rf test_ext2_dir
	mkdir test_ext2_dir
	cp ${TEST_FILES} test_ext2_dir
	ln -s test.sh test_ext2_dir/start
	./glick-mkext2 test.ext2 test_ext2_dir
	rm -rf test_ext2_dir
