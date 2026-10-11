// SPDX-License-Identifier: GPL-2.0
/*
 * AetherOS Nexus kernel identity module.
 *
 * This module provides actual Linux-kernel integration: a startup banner in
 * the kernel log and a read-only /proc/aetheros_identity identity endpoint.
 * It intentionally does not claim to run firmware POST or control power.
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/utsname.h>

#define AETHEROS_NAME "AetherOS Nexus"
#define AETHEROS_RELEASE "1.0.0"
#define AETHEROS_PROC_NAME "aetheros_identity"

MODULE_AUTHOR("ArkansasIo");
MODULE_DESCRIPTION("AetherOS Nexus Linux kernel identity and branding");
MODULE_LICENSE("GPL");
MODULE_VERSION(AETHEROS_RELEASE);

static struct proc_dir_entry *aetheros_proc_entry;

static int aetheros_identity_show(struct seq_file *m, void *v)
{
	(void)v;
	seq_printf(m, "name=%s\n", AETHEROS_NAME);
	seq_printf(m, "release=%s\n", AETHEROS_RELEASE);
	seq_printf(m, "kernel=%s\n", init_uts_ns.name.release);
	seq_printf(m, "architecture=%s\n", init_uts_ns.name.machine);
	seq_puts(m, "identity_source=kernel-module\n");
	seq_puts(m, "hardware_post=not-performed-by-this-module\n");
	return 0;
}

static int aetheros_identity_open(struct inode *inode, struct file *file)
{
	return single_open(file, aetheros_identity_show, NULL);
}

static const struct proc_ops aetheros_identity_ops = {
	.proc_open = aetheros_identity_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int __init aetheros_init(void)
{
	pr_info("AETHEROS NEXUS %s: kernel integration starting\n",
		AETHEROS_RELEASE);
	pr_info("  /\\      AETHEROS\n");
	pr_info(" /  \\     NEXUS OPERATING SYSTEM\n");
	pr_info("/____\\    Linux kernel %s\n", init_uts_ns.name.release);

	aetheros_proc_entry = proc_create(AETHEROS_PROC_NAME, 0444, NULL,
					  &aetheros_identity_ops);
	if (!aetheros_proc_entry) {
		pr_err("AetherOS: failed to create /proc/%s\n",
		       AETHEROS_PROC_NAME);
		return -ENOMEM;
	}

	pr_info("AetherOS: identity endpoint available at /proc/%s\n",
		AETHEROS_PROC_NAME);
	return 0;
}

static void __exit aetheros_exit(void)
{
	proc_remove(aetheros_proc_entry);
	pr_info("AetherOS Nexus: branding module unloaded\n");
}

module_init(aetheros_init);
module_exit(aetheros_exit);
