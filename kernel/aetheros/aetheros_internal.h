/* SPDX-License-Identifier: GPL-2.0 */
#ifndef AETHEROS_INTERNAL_H
#define AETHEROS_INTERNAL_H

#include <linux/types.h>

#define AETHEROS_NAME "AetherOS Nexus"
#define AETHEROS_RELEASE "1.0.0"
#define AETHEROS_PROC_NAME "aetheros_identity"

int aetheros_proc_register(void);
void aetheros_proc_unregister(void);
void aetheros_log_banner(void);
void aetheros_log_shutdown(void);
void aetheros_get_identity(char *buffer, size_t capacity);

#endif /* AETHEROS_INTERNAL_H */
