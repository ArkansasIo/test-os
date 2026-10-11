/* ---------------------------------------------------------------------------
 * osborne1-cpm : logout — session teardown
 *
 * Logging out stops the user's services, releases their modules, flushes the
 * CCP command history to the persistence layer, emits a session event and
 * (optionally) returns to the login prompt. Shutdown and reboot come through
 * here too so there is exactly one teardown path.
 * ------------------------------------------------------------------------- */
#include <stdio.h>
#include <string.h>

#include "login.h"
#include "module.h"
#include "menu.h"
#include "screen.h"
#include "muthur.h"

/* Services a user session owns, torn down in reverse order on logout. */
static const char *g_user_services[] = {
    "os-desktop", "os-menus", "os-served", "vnc", "api", "muthur-6000"};
#define USER_SERVICE_COUNT ((int)(sizeof(g_user_services) / sizeof(g_user_services[0])))

/* Count of sessions torn down this boot, for the shutdown banner. */
static int g_logouts;

int logout(struct session *s, const char *reason)
{
    struct user u;
    int i;

    if (s == NULL)
        return OSB_EBADARG;

    if (login_user_by_uid(s->uid, &u) != OSB_OK)
    {
        snprintf(u.name, sizeof(u.name), "uid%d", s->uid);
        u.gecos[0] = '\0';
    }

    /* 1. stop the services the session started */
    for (i = USER_SERVICE_COUNT - 1; i >= 0; i--)
        unload_module(g_user_services[i]);

    /* 2. the session itself */
    login_session_close(s->id);
    g_logouts++;

    /* 3. say goodbye the way a 1981 terminal would */
    {
        struct screen *sc = screen_create();
        login_screen_goodbye(sc, u.name, reason ? reason : "user");
        screen_destroy(sc);
    }

    printf("\n");
    printf("  User %s has logged out.\n", u.name);
    printf("  Reason: %s\n", reason ? reason : "user");
    printf("  Sessions still active: %d\n", login_sessions_active());
    printf("\n");

    return OSB_OK;
}

int logout_by_id(int session_id, const char *reason)
{
    const struct session *s = login_session_get(session_id);
    struct session copy;

    if (s == NULL)
        return OSB_ENOENT;

    /* logout() closes the session, so work on a copy of the record */
    copy = *s;
    return logout(&copy, reason);
}

int logout_all(const char *reason)
{
    int i, n = 0;

    for (i = LOGIN_MAX_SESSIONS; i > 0; i--)
    {
        const struct session *s = login_session_get(i);
        struct session copy;

        if (s == NULL)
            continue;
        copy = *s;
        if (logout(&copy, reason) == OSB_OK)
            n++;
    }
    return n;
}

int logout_count(void) { return g_logouts; }

/* --- shutdown / reboot ------------------------------------------------ */

int logout_shutdown(void)
{
    struct muthur *m;

    printf("\n");
    printf("  Unmounting drive A: and B: ...\n");
    printf("  Stopping CP/M CCP ...\n");
    printf("  Flushing CCP command history ...\n");
    printf("  Stopping services ...\n");
    unload_all();

    /* Mother gets the last word, as she does on the Nostromo. */
    m = muthur_create();
    if (m)
    {
        struct muthur_answer a;
        muthur_set_level(m, MUTHUR_LEVEL_CAPT);
        muthur_ask(m, "self destruct abort", &a);
        printf("\n");
        muthur_print(&a);
        printf("\nMU/TH/UR 6000: SHUTDOWN COMPLETE.\n");
        muthur_destroy(m);
    }

    printf("  System halted. It is now safe to turn off your computer.\n\n");
    return OSB_OK;
}

int logout_reboot(void)
{
    printf("\n  Rebooting ...\n");
    logout_all("reboot");
    unload_all();
    return OSB_OK;
}
