// SPDX-License-Identifier: GPL-2.0
#include <linux/kernel.h>
#include <linux/utsname.h>
#include <linux/string.h>
#include "aetheros_internal.h"

void aetheros_get_identity(char *buffer, size_t capacity)
{
	if (!buffer || capacity == 0)
		return;
	scnprintf(buffer, capacity,
		  "name=%s\nrelease=%s\nkernel=%s\narchitecture=%s\n"
		  "identity_source=kernel-module\n"
		  "hardware_post=not-performed-by-this-module\n",
		  AETHEROS_NAME, AETHEROS_RELEASE,
		  init_uts_ns.name.release, init_uts_ns.name.machine);
}
