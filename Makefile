all: glick-shell header.a

CFLAGS=-O -Wall -g

glick-shell: glick-shell.c
	gcc ${CFLAGS} -o glick-shell glick-shell.c

header.a: ext2fs.c mem_io_manager.c glick.c
	gcc -c ${CFLAGS} ext2fs.c mem_io_manager.c glick.c `pkg-config --cflags fuse ext2fs`
	rm -f header.a
	ar r header.a ext2fs.o mem_io_manager.o glick.o

clean:
	rm -f header.a *.o test test.ext2 glick-shell

test:	header.a mkglick test.ext2 test-icon.png test.desktop
	./mkglick test test.ext2 --icon test-icon.png --desktop-file test.desktop

test.ext2: test.sh data.txt
	rm -rf test_ext2_dir
	mkdir test_ext2_dir
	cp test.sh test_ext2_dir
	cp data.txt test_ext2_dir
	ln -s test.sh test_ext2_dir/start
	./glick-mkext2 test.ext2 test_ext2_dir
	rm -rf test_ext2_dir
