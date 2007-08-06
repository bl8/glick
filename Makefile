all: test

header.a: ext2fs.c mem_io_manager.c glick.c
	gcc -c -O -Wall ext2fs.c mem_io_manager.c glick.c `pkg-config --cflags fuse ext2fs`
	rm -f header.a
	ar r header.a ext2fs.o mem_io_manager.o glick.o

test:	header.a mkglick.sh
	./mkglick.sh test.ext2 test

test.ext2: test.sh data.txt
	dd if=/dev/zero of=test.ext2 bs=100k count=1
	/sbin/mke2fs test.ext2
	mkdir lo_dir
	mount -o loop test.ext2 lo_dir
	cp test.sh lo_dir
	cp data.txt lo_dir
	ln -s test.sh lo_dir/start
	umount lo_dir
	rm -rf lo_dir
