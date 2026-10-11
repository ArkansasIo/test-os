// SPDX-License-Identifier: GPL-2.0
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include "aetheros_internal.h"

static struct proc_dir_entry *identity_entry;

static int identity_show(struct seq_file *seq, void *unused)
{
	char identity[512];

	(void)unused;
	aetheros_get_identity(identity, sizeof(identity));
	seq_puts(seq, identity);
	return 0;
}

static int identity_open(struct inode *inode, struct file *file)
{
	return single_open(file, identity_show, NULL);
}

static const struct proc_ops identity_ops = {
	.proc_open = identity_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

int aetheros_proc_register(void)
{
	identity_entry = proc_create(AETHEROS_PROC_NAME, 0444, NULL,
				     &identity_ops);
	return identity_entry ? 0 : -ENOMEM;
}

void aetheros_proc_unregister(void)
{
	if (identity_entry) {
		proc_remove(identity_entry);
		identity_entry = NULL;
	}
}
