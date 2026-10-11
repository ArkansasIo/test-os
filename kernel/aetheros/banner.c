// SPDX-License-Identifier: GPL-2.0
#include <linux/kernel.h>
#include <linux/utsname.h>
#include "aetheros_internal.h"

void aetheros_log_banner(void)
{
	pr_info("========================================\n");
	pr_info("        A E T H E R O S  N E X U S      \n");
	pr_info("          NEXUS OPERATING SYSTEM       \n");
	pr_info("========================================\n");
	pr_info("AetherOS %s initializing on Linux %s (%s)\n",
		AETHEROS_RELEASE, init_uts_ns.name.release,
		init_uts_ns.name.machine);
	pr_info("AetherOS identity module: hardware POST is not performed here\n");
}

void aetheros_log_shutdown(void)
{
	pr_info("AetherOS Nexus identity module unloaded\n");
}
