/**************************************************************************

Copyright © 2007 by Alexander Larsson

All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sub license, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice (including the
next paragraph) shall be included in all copies or substantial portions
of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
IN NO EVENT SHALL THE COPYRIGHT HOLDERS AND/OR THEIR SUPPLIERS BE LIABLE FOR
ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/

#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE

#include <stdio.h>
#include <string.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_ERRNO_H
#include <errno.h>
#endif
#include <fcntl.h>
#include <time.h>
#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif

#include <ext2fs/ext2_fs.h>
#include <ext2fs/ext2fs.h>

#define EXT2_ET_MAGIC_MEM_IO_CHANNEL 0x52849745
/*
 * For checking structure magic numbers...
 */

#define EXT2_CHECK_MAGIC(struct, code) \
	  if ((struct)->magic != (code)) return (code)

struct mem_private_data {
	int	magic;
	void   *data;
	size_t  data_size;
};

static errcode_t mem_open(const char *name, int flags, io_channel *channel);
static errcode_t mem_close(io_channel channel);
static errcode_t mem_set_blksize(io_channel channel, int blksize);
static errcode_t mem_read_blk(io_channel channel, unsigned long block,
			       int count, void *data);
static errcode_t mem_write_blk(io_channel channel, unsigned long block,
				int count, const void *data);
static errcode_t mem_flush(io_channel channel);
static errcode_t mem_set_option(io_channel channel, const char *option, 
				 const char *arg);

static struct struct_io_manager struct_mem_manager = {
	EXT2_ET_MAGIC_IO_MANAGER,
	"Memory I/O Manager",
	mem_open,
	mem_close,
	mem_set_blksize,
	mem_read_blk,
	mem_write_blk,
	mem_flush,
	0,
	mem_set_option
};

io_manager mem_io_manager = &struct_mem_manager;

extern char _binary_image_start[];
extern char _binary_image_end[];

static errcode_t mem_open(const char *name, int flags, io_channel *channel)
{
	io_channel	io = NULL;
	struct mem_private_data *data = NULL;
	errcode_t	retval;

	if (name == 0)
		name = "mem";
	
	retval = ext2fs_get_mem(sizeof(struct struct_io_channel), &io);
	if (retval)
		return retval;
	memset(io, 0, sizeof(struct struct_io_channel));
	io->magic = EXT2_ET_MAGIC_IO_CHANNEL;
	retval = ext2fs_get_mem(sizeof(struct mem_private_data), &data);
	if (retval)
		goto cleanup;

	io->manager = mem_io_manager;
	retval = ext2fs_get_mem(strlen(name)+1, &io->name);
	if (retval)
		goto cleanup;

	strcpy(io->name, name);
	io->private_data = data;
	io->block_size = 1024;
	io->read_error = 0;
	io->write_error = 0;
	io->refcount = 1;

	memset(data, 0, sizeof(struct mem_private_data));
	data->magic = EXT2_ET_MAGIC_MEM_IO_CHANNEL;

	data->data = _binary_image_start;
	data->data_size = _binary_image_end - _binary_image_start;
	
	*channel = io;
	return 0;

cleanup:
	if (data) {
		ext2fs_free_mem(&data);
	}
	if (io)
		ext2fs_free_mem(&io);
	return retval;
}

static errcode_t mem_close(io_channel channel)
{
	struct mem_private_data *data;
	errcode_t	retval = 0;

	EXT2_CHECK_MAGIC(channel, EXT2_ET_MAGIC_IO_CHANNEL);
	data = (struct mem_private_data *) channel->private_data;
	EXT2_CHECK_MAGIC(data, EXT2_ET_MAGIC_MEM_IO_CHANNEL);

	if (--channel->refcount > 0)
		return 0;

	ext2fs_free_mem(&channel->private_data);
	if (channel->name)
		ext2fs_free_mem(&channel->name);
	ext2fs_free_mem(&channel);
	return retval;
}

static errcode_t mem_set_blksize(io_channel channel, int blksize)
{
	struct mem_private_data *data;

	EXT2_CHECK_MAGIC(channel, EXT2_ET_MAGIC_IO_CHANNEL);
	data = (struct mem_private_data *) channel->private_data;
	EXT2_CHECK_MAGIC(data, EXT2_ET_MAGIC_MEM_IO_CHANNEL);

	if (channel->block_size != blksize) {
		channel->block_size = blksize;
	}
	return 0;
}


static errcode_t mem_read_blk(io_channel channel, unsigned long block,
			       int count, void *buf)
{
	struct mem_private_data *data;
	errcode_t	retval;
	ssize_t		size, actual;
	ext2_loff_t	location;

	EXT2_CHECK_MAGIC(channel, EXT2_ET_MAGIC_IO_CHANNEL);
	data = (struct mem_private_data *) channel->private_data;
	EXT2_CHECK_MAGIC(data, EXT2_ET_MAGIC_MEM_IO_CHANNEL);

	size = (count < 0) ? -count : count * channel->block_size;
	location = ((ext2_loff_t) block * channel->block_size);

	actual = size;
	if (location + size >= data->data_size) {
		actual = data->data_size - location;
	}
	
	memcpy (buf, data->data + location, actual);

	retval = 0;
	if (size != actual) {
		memset((char *) buf+actual, 0, size-actual);
		retval = EXT2_ET_SHORT_READ;
		if (channel->read_error)
			retval = (channel->read_error)(channel, block, count, buf,
						       size, actual, retval);
	}
	
	return retval;
}

static errcode_t mem_write_blk(io_channel channel, unsigned long block,
				int count, const void *buf)
{
	return EXT2_ET_CALLBACK_NOTHANDLED;
}

/*
 * Flush data buffers to disk.  
 */
static errcode_t mem_flush(io_channel channel)
{
	return 0;
}

static errcode_t mem_set_option(io_channel channel, const char *option, 
				 const char *arg)
{
	struct mem_private_data *data;

	EXT2_CHECK_MAGIC(channel, EXT2_ET_MAGIC_IO_CHANNEL);
	data = (struct mem_private_data *) channel->private_data;
	EXT2_CHECK_MAGIC(data, EXT2_ET_MAGIC_MEM_IO_CHANNEL);

	return EXT2_ET_INVALID_ARGUMENT;
}
