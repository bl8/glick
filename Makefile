all: glick-shell header.a glick.spec

DESTDIR=
PREFIX=/usr
LIBDIR=${PREFIX}/lib
BINDIR=${PREFIX}/bin
CFLAGS=-O2 -Wall -g

VERSION=0.2

TEST_FILES=test.sh data.txt
HEADER_SOURCES=ext2fs.c mem_io_manager.c glick.c

dist:	glick.spec
	rm -rf glick-${VERSION}
	mkdir glick-${VERSION}
	cp README AUTHORS COPYING NEWS glick-${VERSION}
	cp Makefile glick-${VERSION}
	cp ${TEST_FILES} test-ext2.c glick-${VERSION}
	cp glick-shell.c ${HEADER_SOURCES} glick-${VERSION}
	cp mkglick glick-mkext2 glick-extract glick-${VERSION}
	cp test-icon.png test.desktop glick-${VERSION}
	cp glick.spec glick.spec.in glick-${VERSION}
	tar czvf glick-${VERSION}.tar.gz glick-${VERSION}
	rm -rf glick-${VERSION}

glick.spec: glick.spec.in
	sed s/\@VERSION\@/${VERSION}/ glick.spec.in > glick.spec 

install: header.a glick-shell mkglick glick-mkext2
	mkdir -p ${DESTDIR}${LIBDIR}/glick/
	install header.a ${DESTDIR}${LIBDIR}/glick/

	mkdir -p ${DESTDIR}${BINDIR}/
	install glick-shell glick-mkext2 glick-extract ${DESTDIR}${BINDIR}/
	sed s#LIBDIR=.#LIBDIR=${LIBDIR}/glick# mkglick > ${DESTDIR}${BINDIR}/mkglick
	chmod a+x ${DESTDIR}${BINDIR}/mkglick

glick-shell: glick-shell.c
	gcc ${CFLAGS} -o glick-shell glick-shell.c

header.a: ${HEADER_SOURCES}
	gcc -c ${CFLAGS} ${HEADER_SOURCES} `pkg-config --cflags fuse ext2fs`
	rm -f header.a
	ar r header.a ext2fs.o mem_io_manager.o glick.o

test-ext2: test-ext2.c ext2fs.c mem_io_manager.c test.ext2
	cp test.ext2 image
	ld -r -b binary -o test_image.o image
	rm image
	objcopy --rename-section .data=.glick.image,alloc,load,readonly,data,cont test_image.o
	gcc -o test-ext2 ${CFLAGS} test_image.o test-ext2.c mem_io_manager.c ext2fs.c `pkg-config --cflags --libs fuse ext2fs`

clean:
	rm -f header.a *.o test test.ext2 glick-shell test-ext2

test:	header.a mkglick test.ext2 test-icon.png test.desktop
	./mkglick test test.ext2 --icon test-icon.png --desktop-file test.desktop

test.ext2: test.sh data.txt
	rm -rf test_ext2_dir
	mkdir test_ext2_dir
	cp ${TEST_FILES} test_ext2_dir
	ln -s test.sh test_ext2_dir/start
	./glick-mkext2 test.ext2 test_ext2_dir
	rm -rf test_ext2_dir
