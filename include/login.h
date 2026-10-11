#ifndef OSB_LOGIN_H
#define OSB_LOGIN_H

#include "types.h"

/* Login / logout / sessions. The OS layer authenticates users before it
 * hands them a desktop or server session. Users and groups are seeded from
 * os/seed/users and os/seed/groups and mirrored into MySQL (see sql/schema.sql). */

#define LOGIN_NAME_MAX 32
#define LOGIN_MAX_TRIES 3
#define LOGIN_MAX_USERS 64
#define LOGIN_MAX_GROUPS 32
#define LOGIN_MAX_SESSIONS 32

/* Human account id: uid 0 = root, uid >= 1000 = regular user */
#define LOGIN_UID_ROOT 0
#define LOGIN_UID_NOBODY 65534

enum session_type
{
    SESSION_NONE = 0,
    SESSION_DESKTOP, /* os-desk   */
    SESSION_SERVER,  /* os-served */
    SESSION_SHELL,   /* text      */
    SESSION_RESCUE
};

enum session_state
{
    SESSION_FREE = 0,
    SESSION_ACTIVE,
    SESSION_LOCKED
};

struct user
{
    int uid;
    int gid; /* primary group              */
    char name[LOGIN_NAME_MAX];
    char gecos[64]; /* full name                  */
    char home[64];
    char shell[64];
    int disabled;
};

struct group
{
    int gid;
    char name[LOGIN_NAME_MAX];
    int members[16]; /* member uids                */
    int member_count;
};

struct session
{
    int id;
    int uid;
    enum session_type type;
    enum session_state state;
    char tty[16]; /* "tty1", ":0", "vnc:1"      */
    char display[32];
    u64 started_ms;
    u64 ended_ms;
    int boot_serial; /* boot_ctx serial it belongs to */
};

/* --- Account database -------------------------------------------------- */
int login_seed_users(const char *path);  /* NULL = os/seed/users    */
int login_seed_groups(const char *path); /* NULL = os/seed/groups   */

int login_user_find(const char *name, struct user *out);
int login_user_by_uid(int uid, struct user *out);
int login_user_count(void);
const struct user *login_users(void);

int login_group_find(const char *name, struct group *out);
int login_user_in_group(int uid, const char *group);

/* --- Authentication ---------------------------------------------------- */
/* Verify name + password. Password is hashed (see login_hash) so a plain
 * password is never compared directly. Returns 0 on success, OSB_EACCES
 * on bad credentials, OSB_ELOCKED on a disabled account. */
int login_authenticate(const char *name, const char *password, struct user *out);

/* Crypt-style one way hash used for the seeded accounts: "os1$" + 16 hex
 * digits. Deterministic, so seed files can carry real hashes. */
void login_hash(const char *password, const char *salt, char *out, size_t outlen);
int login_verify(const char *password, const char *hash);

/* Number of failed attempts for a name; a name is temporarily refused
 * after LOGIN_MAX_TRIES misses. */
int login_failed_attempts(const char *name);
void login_reset_attempts(const char *name);

/* --- Sessions --------------------------------------------------------- */
/* Interactive login: prompts on the tty, authenticates, opens a session.
 * autologin != 0 skips the prompt for that uid. */
int login_prompt(struct session *out, int autologin, enum session_type type);

/* Open/close a session without prompting (used by the OS desktop/server). */
int login_session_open(const struct user *u, enum session_type type,
                       const char *tty, struct session *out);
int login_session_close(int session_id);
int login_session_lock(int session_id);
int login_session_unlock(int session_id, const char *password);

const struct session *login_session_get(int session_id);
int login_sessions_active(void);

const char *login_session_type_name(enum session_type t);

/* --- Logout ------------------------------------------------------------ */
/* Tear a session down: stop user services, flush history, emit an event.
 * `reason` is "user", "timeout", "shutdown" or "reboot". */
int logout(struct session *s, const char *reason);
int logout_by_id(int session_id, const char *reason);
int logout_all(const char *reason);

int login_init(void);
void login_shutdown(void);

#endif /* OSB_LOGIN_H */
