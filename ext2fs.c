
#define FUSE_USE_VERSION 26

#include <fuse_lowlevel.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <ext2fs/ext2fs.h>

static const char *program_name = "ext2fs";

static struct ext_priv {
	char		*name;
	ext2_filsys	fs;
} ep;

struct dirbuf {
	char		*p;
	size_t		size;
        fuse_req_t      req;
};

static void fixup_inode (fuse_ino_t *ino)
{
  /* ext2 root seems to be 2, not 1 */
  if (*ino == 1)
    *ino = 2;
}

static void fill_statbuf(fuse_ino_t ino, struct ext2_inode *inode,
			 struct stat *st)
{
	memset(st, 0, sizeof(*st));
	/* st_dev */
	st->st_ino = ino;
	st->st_mode = inode->i_mode;
	st->st_nlink = inode->i_links_count;
	st->st_uid = inode->i_uid;	/* add in uid_high */
	st->st_gid = inode->i_gid;	/* add in gid_high */
	/* st_rdev */
	st->st_size = inode->i_size;
	st->st_blksize = 4096;		/* FIXME */
	st->st_blocks = inode->i_blocks;
	st->st_atime = inode->i_atime;
	st->st_mtime = inode->i_mtime;
	st->st_ctime = inode->i_ctime;
}

static void dirbuf_add(struct dirbuf *b, const char *name, fuse_ino_t ino)
{
	struct stat stbuf;
	size_t oldsize;

	oldsize = b->size;
	b->size += fuse_add_direntry(b->req, NULL, 0, name, NULL, 0);
	b->p = (char *) realloc(b->p, b->size);
	memset(&stbuf, 0, sizeof(stbuf));
	stbuf.st_ino = ino;
	fuse_add_direntry(b->req, b->p + oldsize, b->size - oldsize, name, &stbuf,
			  b->size);
}

#define min(x, y) ((x) < (y) ? (x) : (y))

static int reply_buf_limited(fuse_req_t req, const char *buf, size_t bufsize,
                             off_t off, size_t maxsize)
{
	if (off < bufsize)
		return fuse_reply_buf(req, buf + off, min(bufsize - off, maxsize));
	else
		return fuse_reply_buf(req, NULL, 0);
}

extern io_manager mem_io_manager;

static void op_init(void *userdata, struct fuse_conn_info *conn)
{
        struct ext_priv *priv = userdata;
	errcode_t rc;

	rc = ext2fs_open("memory", 0, 0, 0,
			 mem_io_manager, &priv->fs);
	if (rc) {
		com_err(program_name, rc, "while trying to open %s",
			priv->name);
		exit(1);
	}
}

static void op_destroy(void *userdata)
{
	struct ext_priv *priv = userdata;
	errcode_t rc;

	rc = ext2fs_close(priv->fs);
	if (rc) {
		com_err(program_name, rc, "while trying to close %s",
			priv->name);
		exit(1);
	}
}

static void op_lookup(fuse_req_t req, fuse_ino_t parent, const char *name)
{
	struct fuse_entry_param fe;
	errcode_t rc;
	ext2_ino_t ino;
	struct ext2_inode inode;

	fixup_inode (&parent);

	rc = ext2fs_lookup(ep.fs, parent, name, strlen(name),
			   NULL, &ino);
	if (rc) {
		fuse_reply_err(req, ENOENT);
		return;
	}

	rc = ext2fs_read_inode(ep.fs, ino, &inode);
	if (rc) {
		fuse_reply_err(req, EIO);
		return;
	}

	fe.ino = ino;
	fe.generation = inode.i_generation;
	fill_statbuf(ino, &inode, &fe.attr);
	fe.attr_timeout = 2.0;
	fe.entry_timeout = 2.0;

	fuse_reply_entry(req, &fe);
}

static void op_getattr(fuse_req_t req, fuse_ino_t ino,
		       struct fuse_file_info *fi)
{
	errcode_t rc;
	struct ext2_inode inode;
	struct stat st;
	
	fixup_inode (&ino);
	
	rc = ext2fs_read_inode(ep.fs, ino, &inode);
	if (rc) {
		fuse_reply_err(req, EIO);
		return;
	}

	fill_statbuf(ino, &inode, &st);

	fuse_reply_attr(req, &st, 2.0);
}

static void op_readlink (fuse_req_t req, fuse_ino_t ino)
{
	errcode_t rc;
	struct ext2_inode inode;
	char *buffer;
	errcode_t retval;
	char *link;
	
	fixup_inode (&ino);
	
	rc = ext2fs_read_inode(ep.fs, ino, &inode);
	if (rc) {
		fuse_reply_err(req, EIO);
		return;
	}
	if (!LINUX_S_ISLNK (inode.i_mode)) {
		fuse_reply_err(req, EINVAL);
		return;
	}

	link = NULL;
	buffer = NULL;
	if (ext2fs_inode_data_blocks (ep.fs, &inode)) {
		retval = ext2fs_get_mem (ep.fs->blocksize, &buffer);
		if (retval == 0) {
			retval = io_channel_read_blk (ep.fs->io, inode.i_block[0], 1, buffer);
			if (retval == 0)
				link = buffer;
		}
	} else {
		link = (char *)&(inode.i_block[0]);
	}

	if (link)
		fuse_reply_readlink(req, link);
	else
		fuse_reply_err(req, EIO);
	
	if (buffer)
		ext2fs_free_mem(&buffer);
}


static void op_open(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
	errcode_t rc;
	ext2_file_t efile;

	fixup_inode (&ino);
	
	rc = ext2fs_file_open(ep.fs, ino, 0, &efile);
	if (rc) {
		fuse_reply_err(req, EIO);
		return;
	}

	fi->fh = (unsigned long) efile;
	fuse_reply_open(req, fi);
}

static void op_read(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off,
		    struct fuse_file_info *fi)
{
	errcode_t rc;
	ext2_file_t efile = (void *)(unsigned long)fi->fh;
	__u64 pos;
	unsigned int bytes;
	void *buf;

	rc = ext2fs_file_llseek(efile, off, SEEK_SET, &pos);
	if (rc) {
		fuse_reply_err(req, EINVAL);
		return;
	}

	buf = malloc(size);
	if (!buf) {
		fuse_reply_err(req, ENOMEM);
		return;
	}

	rc = ext2fs_file_read(efile, buf, size, &bytes);
	if (rc)
		fuse_reply_err(req, EIO);
	else
		fuse_reply_buf(req, buf, bytes);

	free(buf);
}

static void op_release(fuse_req_t req, fuse_ino_t ino,
		       struct fuse_file_info *fi)
{
	errcode_t rc;
	ext2_file_t efile = (void *)(unsigned long)fi->fh;

	rc = ext2fs_file_close(efile);
	if (rc)
		fuse_reply_err(req, EIO);
}

static int walk_dir(struct ext2_dir_entry *de, int   offset, int blocksize,
		    char *buf, void *priv_data)
{
	struct dirbuf *b = priv_data;
	char *s;

	s = malloc(de->name_len + 1);
	if (!s)
		return -ENOMEM;

	memcpy(s, de->name, de->name_len);
	s[de->name_len] = 0;

	dirbuf_add(b, s, de->inode);

	free(s);

	return 0;
}

static void op_readdir(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off,
		       struct fuse_file_info *fi)
{
	errcode_t rc;
	struct dirbuf b;

	fixup_inode (&ino);
	
	memset(&b, 0, sizeof(b));
	b.req = req;

	rc = ext2fs_dir_iterate(ep.fs, ino, 0, NULL, walk_dir, &b);
	if (rc) {
		fuse_reply_err(req, EIO);
		return;
	}

	reply_buf_limited(req, b.p, b.size, off, size);
	free(b.p);
}

static struct fuse_lowlevel_ops ext2fs_ops = {
	.init		= op_init,
	.destroy	= op_destroy,
	.lookup		= op_lookup,
	.forget		= NULL,
	.getattr	= op_getattr,
	.setattr	= NULL,
	.readlink	= op_readlink,
	.mknod		= NULL,
	.mkdir		= NULL,
	.unlink		= NULL,
	.rmdir		= NULL,
	.symlink	= NULL,
	.rename		= NULL,
	.link		= NULL,
	.open		= op_open,
	.read		= op_read,
	.write		= NULL,
	.flush		= NULL,
	.release	= op_release,
	.fsync		= NULL,
	.opendir	= NULL,
	.readdir	= op_readdir,
	.releasedir	= NULL,
	.fsyncdir	= NULL,
	.statfs		= NULL,
	.setxattr	= NULL,
	.getxattr	= NULL,
	.listxattr	= NULL,
	.removexattr	= NULL,
	.access		= NULL,
	.create		= NULL,
};

/* stock main() from FUSE example */
int ext2_main(int argc, char *argv[], void (*mounted) (void))
{
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	struct fuse_chan *ch;
	char *mountpoint;
	int err = -1;
	
	if (fuse_parse_cmdline(&args, &mountpoint, NULL, NULL) != -1 &&
	    (ch = fuse_mount(mountpoint, &args)) != NULL) {
		struct fuse_session *se;
		
		se = fuse_lowlevel_new(&args, &ext2fs_ops, sizeof(ext2fs_ops),
				       &ep);
		if (se != NULL) {
			if (fuse_set_signal_handlers(se) != -1) {
				fuse_session_add_chan(se, ch);
				if (mounted)
				  mounted ();
				err = fuse_session_loop(se);
				fuse_remove_signal_handlers(se);
				fuse_session_remove_chan(ch);
			}
			fuse_session_destroy(se);
		}
		fuse_unmount(mountpoint, ch);
	}
	fuse_opt_free_args(&args);

	return err ? 1 : 0;
}
