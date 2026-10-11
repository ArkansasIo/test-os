/* ---------------------------------------------------------------------------
   * osborne1-cpm : session — accounts, authentication and login
   *
   * Users and groups are seeded from os/seed/users and os/seed/groups, in the
   * /etc/passwd and /etc/group shape so the files look familiar:
   *
   *   users :  name:hash:uid:gid:gecos:home:shell
   *   groups:  name:gid:member,member,...
   *
   * Passwords are stored as a deterministic hash (login_hash) so the seed files
   * carry real hashes rather than plaintext.
   * ------------------------------------------------------------------------- */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "login.h"
#include "screen.h"

    static struct user g_users[LOGIN_MAX_USERS];
static int g_user_count;
static struct group g_groups[LOGIN_MAX_GROUPS];
static int g_group_count;
static struct session g_sessions[LOGIN_MAX_SESSIONS];
static int g_session_seq;
static int g_attempts[LOGIN_MAX_USERS]; /* failed count per user */
static struct screen *g_login_screen;

int login_init(void)
{
    memset(g_users, 0, sizeof(g_users));
    memset(g_groups, 0, sizeof(g_groups));
    memset(g_sessions, 0, sizeof(g_sessions));
    memset(g_attempts, 0, sizeof(g_attempts));
    g_user_count = 0;
    g_group_count = 0;
    g_session_seq = 0;
    return OSB_OK;
}

void login_shutdown(void) { g_login_screen = NULL; }

/* --- small parsing helpers ------------------------------------------- */

static int str_split(char *line, char sep, char **fields, int max)
{
    int n = 0;
    char *p = line;

    if (line == NULL || max <= 0)
        return 0;
    fields[n++] = p;
    while (*p && n < max)
    {
        if (*p == sep)
        {
            *p = '\0';
            fields[n++] = p + 1;
        }
        p++;
    }
    return n;
}

static void rstrip(char *s)
{
    size_t n;

    if (s == NULL)
        return;
    n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r' ||
                     s[n - 1] == ' ' || s[n - 1] == '\t'))
        s[--n] = '\0';
}

/* --- hashing --------------------------------------------------------- */

/* FNV-1a over "salt:password", printed as 16 hex digits. Not a cryptographic
 * KDF — it exists so the seed files never hold a plaintext password and a
 * session check is a string compare. */
void login_hash(const char *password, const char *salt, char *out, size_t outlen)
{
    u64 h = 1469598103934665603ULL;
    const char *p;

    if (salt == NULL)
        salt = "os1";

    for (p = salt; *p; p++)
    {
        h ^= (u8)*p;
        h *= 1099511628211ULL;
    }
    h ^= (u8)':';
    h *= 1099511628211ULL;
    if (password)
    {
        for (p = password; *p; p++)
        {
            h ^= (u8)*p;
            h *= 1099511628211ULL;
        }
    }
    snprintf(out, outlen, "os1$%016llx", (unsigned long long)h);
}

int login_verify(const char *password, const char *hash)
{
    char want[40];

    if (hash == NULL)
        return OSB_EACCES;
    login_hash(password, "os1", want, sizeof(want));
    if (osb_streq(want, hash))
        return OSB_OK;
    /* also accept a literal hash given in the seed */
    if (osb_streq(password, hash))
        return OSB_OK;
    return OSB_EACCES;
}

/* --- seeding --------------------------------------------------------- */

int login_seed_users(const char *path)
{
    FILE *f;
    char line[256];

    if (path == NULL)
        path = "os/seed/users";

    f = fopen(path, "r");
    if (f == NULL)
        return OSB_ENOENT;

    while (fgets(line, sizeof(line), f))
    {
        char *fields[8];
        int n;
        struct user *u;

        rstrip(line);
        if (line[0] == '\0' || line[0] == '#')
            continue;
        if (g_user_count >= LOGIN_MAX_USERS)
            break;

        n = str_split(line, ':', fields, 8);
        if (n < 3)
            continue;

        u = &g_users[g_user_count];
        memset(u, 0, sizeof(*u));
        snprintf(u->name, sizeof(u->name), "%-31s", fields[0]);
        rstrip(u->name);
        snprintf(u->gecos, sizeof(u->gecos), "%s", n > 4 ? fields[4] : u->name);
        snprintf(u->home, sizeof(u->home), "%s", n > 5 ? fields[5] : "/");
        snprintf(u->shell, sizeof(u->shell), "%s", n > 6 ? fields[6] : "/bin/sh");
        u->uid = n > 2 ? atoi(fields[2]) : 1000;
        u->gid = n > 3 ? atoi(fields[3]) : u->uid;
        /* a "!" prefixed hash or the literal "*" marks the account disabled */
        u->disabled = (n > 1 && (fields[1][0] == '!' || strcmp(fields[1], "*") == 0));
        if (!u->disabled && n > 1)
            snprintf(u->shell, sizeof(u->shell), "%s",
                     u->shell[0] ? u->shell : "/bin/sh");
        g_user_count++;
    }
    fclose(f);
    return g_user_count > 0 ? OSB_OK : OSB_ENOENT;
}

int login_seed_groups(const char *path)
{
    FILE *f;
    char line[256];

    if (path == NULL)
        path = "os/seed/groups";

    f = fopen(path, "r");
    if (f == NULL)
        return OSB_ENOENT;

    while (fgets(line, sizeof(line), f))
    {
        char *fields[4];
        struct group *g;
        char *m;
        int n;

        rstrip(line);
        if (line[0] == '\0' || line[0] == '#')
            continue;
        if (g_group_count >= LOGIN_MAX_GROUPS)
            break;

        n = str_split(line, ':', fields, 4);
        if (n < 2)
            continue;

        g = &g_groups[g_group_count];
        memset(g, 0, sizeof(*g));
        snprintf(g->name, sizeof(g->name), "%s", fields[0]);
        g->gid = atoi(fields[1]);

        if (n > 2 && fields[2][0])
        {
            char *save = fields[2];
            while (*save && g->member_count < 16)
            {
                char *comma = strchr(save, ',');
                if (comma)
                    *comma = '\0';
                g->members[g->member_count++] = atoi(save);
                if (!comma)
                    break;
                save = comma + 1;
            }
        }
        (void)m;
        g_group_count++;
    }
    fclose(f);
    return g_group_count > 0 ? OSB_OK : OSB_ENOENT;
}

/* --- lookup ---------------------------------------------------------- */

int login_user_find(const char *name, struct user *out)
{
    int i;

    if (name == NULL)
        return OSB_ENOENT;
    for (i = 0; i < g_user_count; i++)
    {
        if (osb_streq(g_users[i].name, name))
        {
            if (out)
                *out = g_users[i];
            return OSB_OK;
        }
    }
    return OSB_ENOENT;
}

int login_user_by_uid(int uid, struct user *out)
{
    int i;

    for (i = 0; i < g_user_count; i++)
    {
        if (g_users[i].uid == uid)
        {
            if (out)
                *out = g_users[i];
            return OSB_OK;
        }
    }
    return OSB_ENOENT;
}

int login_user_count(void) { return g_user_count; }

const struct user *login_users(void) { return g_users; }

int login_group_find(const char *name, struct group *out)
{
    int i;

    for (i = 0; i < g_group_count; i++)
    {
        if (osb_streq(g_groups[i].name, name))
        {
            if (out)
                *out = g_groups[i];
            return OSB_OK;
        }
    }
    return OSB_ENOENT;
}

int login_user_in_group(int uid, const char *group)
{
    struct group g;
    int i;

    if (login_group_find(group, &g) != OSB_OK)
        return 0;
    for (i = 0; i < g.member_count; i++)
        if (g.members[i] == uid)
            return 1;
    return 0;
}

/* --- authentication -------------------------------------------------- */

int login_failed_attempts(const char *name)
{
    struct user u;
    int i;

    if (login_user_find(name, &u) != OSB_OK)
        return 0;
    for (i = 0; i < g_user_count; i++)
        if (g_users[i].uid == u.uid)
            return g_attempts[i];
    return 0;
}

void login_reset_attempts(const char *name)
{
    struct user u;
    int i;

    if (login_user_find(name, &u) != OSB_OK)
        return;
    for (i = 0; i < g_user_count; i++)
        if (g_users[i].uid == u.uid)
            g_attempts[i] = 0;
}

/* The seed file's password field; kept out of struct user on purpose. */
static const char *user_hash(const char *name, struct user *out, int *slot)
{
    static char hash[40];
    FILE *f;
    char line[256];

    f = fopen("os/seed/users", "r");
    if (f == NULL)
        return NULL;

    while (fgets(line, sizeof(line), f))
    {
        char *fields[8];
        int i, n;

        rstrip(line);
        if (line[0] == '\0' || line[0] == '#')
            continue;
        n = str_split(line, ':', fields, 8);
        if (n < 3 || !osb_streq(fields[0], name))
            continue;

        *out = g_users[0];
        for (i = 0; i < g_user_count; i++)
        {
            if (osb_streq(g_users[i].name, name))
            {
                *out = g_users[i];
                if (slot)
                    *slot = i;
                break;
            }
        }
        snprintf(hash, sizeof(hash), "%s", n > 1 ? fields[1] : "");
        fclose(f);
        return hash;
    }
    fclose(f);
    return NULL;
}

int login_authenticate(const char *name, const char *password, struct user *out)
{
    struct user u;
    int slot = -1;
    const char *hash;

    if (name == NULL || password == NULL)
        return OSB_EACCES;

    if (login_user_find(name, &u) != OSB_OK)
        return OSB_EACCES;

    if (u.disabled)
        return OSB_ELOCKED;

    if (login_failed_attempts(name) >= LOGIN_MAX_TRIES)
        return OSB_ELOCKED;

    hash = user_hash(name, &u, &slot);
    if (hash == NULL)
        return OSB_EACCES;

    if (login_verify(password, hash) != OSB_OK)
    {
        if (slot >= 0)
            g_attempts[slot]++;
        return OSB_EACCES;
    }

    if (slot >= 0)
        g_attempts[slot] = 0;
    if (out)
        *out = u;
    return OSB_OK;
}

/* --- sessions -------------------------------------------------------- */

const char *login_session_type_name(enum session_type t)
{
    switch (t)
    {
    case SESSION_DESKTOP:
        return "desktop";
    case SESSION_SERVER:
        return "server";
    case SESSION_SHELL:
        return "shell";
    case SESSION_RESCUE:
        return "rescue";
    case SESSION_NONE:
        break;
    }
    return "none";
}

int login_session_open(const struct user *u, enum session_type type,
                       const char *tty, struct session *out)
{
    struct session *s;
    int i;

    if (u == NULL)
        return OSB_EBADARG;

    for (i = 0; i < LOGIN_MAX_SESSIONS; i++)
    {
        if (g_sessions[i].state == SESSION_FREE)
            break;
    }
    if (i >= LOGIN_MAX_SESSIONS)
        return OSB_ENOMEM;

    s = &g_sessions[i];
    memset(s, 0, sizeof(*s));
    s->id = ++g_session_seq;
    s->uid = u->uid;
    s->type = type;
    s->state = SESSION_ACTIVE;
    s->boot_serial = 1;
    snprintf(s->tty, sizeof(s->tty), "%s", tty ? tty : "tty1");
    snprintf(s->display, sizeof(s->display), "%s",
             type == SESSION_SERVER ? "vnc:0" : ":0");
    s->started_ms = (u64)time(NULL) * 1000;

    if (out)
        *out = *s;
    return OSB_OK;
}

int login_session_close(int session_id)
{
    int i;

    for (i = 0; i < LOGIN_MAX_SESSIONS; i++)
    {
        if (g_sessions[i].id == session_id &&
            g_sessions[i].state != SESSION_FREE)
        {
            g_sessions[i].state = SESSION_FREE;
            g_sessions[i].ended_ms = (u64)time(NULL) * 1000;
            return OSB_OK;
        }
    }
    return OSB_ENOENT;
}

int login_session_lock(int session_id)
{
    int i;

    for (i = 0; i < LOGIN_MAX_SESSIONS; i++)
    {
        if (g_sessions[i].id == session_id &&
            g_sessions[i].state == SESSION_ACTIVE)
        {
            g_sessions[i].state = SESSION_LOCKED;
            return OSB_OK;
        }
    }
    return OSB_ENOENT;
}

int login_session_unlock(int session_id, const char *password)
{
    struct user u;
    int i;

    for (i = 0; i < LOGIN_MAX_SESSIONS; i++)
    {
        if (g_sessions[i].id != session_id ||
            g_sessions[i].state != SESSION_LOCKED)
            continue;
        if (login_user_by_uid(g_sessions[i].uid, &u) != OSB_OK)
            return OSB_ENOENT;
        if (login_authenticate(u.name, password, NULL) != OSB_OK)
            return OSB_EACCES;
        g_sessions[i].state = SESSION_ACTIVE;
        return OSB_OK;
    }
    return OSB_ENOENT;
}

const struct session *login_session_get(int session_id)
{
    int i;

    for (i = 0; i < LOGIN_MAX_SESSIONS; i++)
        if (g_sessions[i].id == session_id &&
            g_sessions[i].state != SESSION_FREE)
            return &g_sessions[i];
    return NULL;
}

int login_sessions_active(void)
{
    int i, n = 0;

    for (i = 0; i < LOGIN_MAX_SESSIONS; i++)
        if (g_sessions[i].state != SESSION_FREE)
            n++;
    return n;
}

/* Interactive login. Returns OSB_OK and fills `out`. */
int login_prompt(struct session *out, int autologin, enum session_type type)
{
    char name[LOGIN_NAME_MAX];
    char pass[64];
    struct user u;
    int tries;

    if (g_login_screen == NULL)
        g_login_screen = screen_create();

    if (autologin != 0)
    {
        if (login_user_by_uid(autologin, &u) != OSB_OK)
            return OSB_ENOENT;
        if (u.disabled)
            return OSB_ELOCKED;
        return login_session_open(&u, type, type == SESSION_SERVER ? "vnc0" : "tty1", out);
    }

    login_screen_show(g_login_screen, "osborne1");

    for (tries = 0; tries < LOGIN_MAX_TRIES; tries++)
    {
        size_t n;

        login_screen_prompt(g_login_screen, "login:", name, 0);
        printf("\n");
        if (fgets(name, sizeof(name), stdin) == NULL)
        {
            login_screen_message(g_login_screen, A_YELLOW, "No input: aborting login.");
            return OSB_ERR;
        }
        n = strlen(name);
        while (n > 0 && (name[n - 1] == '\n' || name[n - 1] == '\r'))
            name[--n] = '\0';
        if (name[0] == '\0')
            continue;

        login_screen_prompt(g_login_screen, "Password:", pass, 1);
        printf("\n");
        if (fgets(pass, sizeof(pass), stdin) == NULL)
            return OSB_ERR;
        n = strlen(pass);
        while (n > 0 && (pass[n - 1] == '\n' || pass[n - 1] == '\r'))
            pass[--n] = '\0';

        if (login_authenticate(name, pass, &u) == OSB_OK)
        {
            login_screen_message(g_login_screen, A_GREEN,
                                 "Welcome, %s.", u.gecos[0] ? u.gecos : u.name);
            return login_session_open(&u, type,
                                      type == SESSION_SERVER ? "vnc0" : "tty1", out);
        }
        login_screen_message(g_login_screen, A_RED,
                             "Login incorrect. Attempt %d of %d.",
                             tries + 1, LOGIN_MAX_TRIES);
    }
    login_screen_message(g_login_screen, A_BOLD | A_RED, "Too many failed logins.");
    return OSB_EACCES;
}
