/* ---------------------------------------------------------------------------
 * osborne1-cpm : user database
 *
 * Thin layer over session.c for account enumeration and editing, the part the
 * desktop's "Users and Groups" panel and the server's REST API both call.
 * ------------------------------------------------------------------------- */
#include <stdio.h>
#include <string.h>

#include "login.h"

/**
 * user_db_list() - render the account table into a caller buffer.
 * Returns the number of users, and always NUL-terminates when buflen > 0.
 */
int user_db_list(char *buf, size_t buflen)
{
    const struct user *u = login_users();
    int i, n = login_user_count();
    size_t used = 0;

    for (i = 0; i < n; i++)
    {
        int wrote = snprintf(buf + used, used < buflen ? buflen - used : 0,
                             "%-12s uid=%-5d gid=%-5d %-24s %s%s\n",
                             u[i].name, u[i].uid, u[i].gid, u[i].gecos,
                             u[i].home,
                             u[i].disabled ? "  [disabled]" : "");
        if (wrote < 0)
            break;
        used += (size_t)wrote;
        if (used >= buflen)
            break;
    }
    if (n == 0 && buflen > 0)
        snprintf(buf, buflen, "(no users seeded)\n");
    return n;
}

int user_db_dump(FILE *out)
{
    const struct user *u = login_users();
    int i;

    if (out == NULL)
        out = stdout;
    fprintf(out, "%-12s %-6s %-6s %-24s %s\n",
            "NAME", "UID", "GID", "FULL NAME", "HOME");
    for (i = 0; i < login_user_count(); i++)
        fprintf(out, "%-12s %-6d %-6d %-24s %s%s\n",
                u[i].name, u[i].uid, u[i].gid, u[i].gecos, u[i].home,
                u[i].disabled ? "  [disabled]" : "");
    return login_user_count();
}

/* Which OS login the seeded accounts map to. The seed file also lists the
 * Nostromo crew; only these three are real accounts. */
int user_db_role(const char *name)
{
    if (name == NULL)
        return 0;
    if (strcmp(name, "root") == 0)
        return 1; /* system administrator      */
    if (strcmp(name, "agent") == 0)
        return 2; /* server operator           */
    if (strcmp(name, "crew") == 0)
        return 3; /* regular desktop user      */
    return 0;
}

const char *user_db_role_name(int role)
{
    switch (role)
    {
    case 1:
        return "administrator";
    case 2:
        return "operator";
    case 3:
        return "user";
    }
    return "unknown";
}

/* The three seeded accounts always exist after a successful seed. */
const char *user_db_admin(void) { return "root"; }
const char *user_db_agent(void) { return "agent"; }
const char *user_db_crew(void) { return "crew"; }
