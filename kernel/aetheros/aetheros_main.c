// SPDX-License-Identifier: GPL-2.0
/*
 * AetherOS Nexus Linux kernel module entry point.
 * This is a loadable kernel component, not a replacement for upstream Linux.
 */
#include <linux/init.h>
#include <linux/module.h>
#include "aetheros_internal.h"

MODULE_AUTHOR("ArkansasIo");
MODULE_DESCRIPTION("AetherOS Nexus kernel identity and branding");
MODULE_LICENSE("GPL");
MODULE_VERSION(AETHEROS_RELEASE);

static int __init aetheros_init(void)
{
	aetheros_log_banner();
	if (aetheros_proc_register() != 0) {
		pr_err("AetherOS: unable to register identity endpoint\n");
		return -ENOMEM;
	}
	pr_info("AetherOS: kernel identity endpoint ready\n");
	return 0;
}

static void __exit aetheros_exit(void)
{
	aetheros_proc_unregister();
	aetheros_log_shutdown();
}

module_init(aetheros_init);
module_exit(aetheros_exit);
