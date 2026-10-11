/* ---------------------------------------------------------------------------
 * osborne1-cpm : MU/TH/UR 6000 — "Mother"
 *
 * The Nostromo's shipboard computer, as an OS service. Answers in the terse
 * uppercase register the crew use in Alien (1979), holds SPECIAL ORDER 937
 * behind science-officer clearance, and runs the self destruct sequence.
 *
 * Every answer is canned text; the state that selects which text comes back
 * lives in struct muthur_state so it can be driven from the desktop, from
 * the server console, or from the CP/M CCP.
 * ------------------------------------------------------------------------- */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#include "muthur.h"

struct muthur
{
    struct muthur_state st;
    enum muthur_level level;
    int queries;
};

/* --- helpers ---------------------------------------------------------- */

static void line(struct muthur_answer *a, const char *fmt, ...)
{
    va_list ap;

    if (a->lines >= MUTHUR_MAX_LINES)
        return;
    va_start(ap, fmt);
    vsnprintf(a->text[a->lines], MUTHUR_LINE_LEN, fmt, ap);
    va_end(ap);
    a->lines++;
}

static void wrap_line(struct muthur_answer *a, const char *text)
{
    const char *p = text;

    while (*p && a->lines < MUTHUR_MAX_LINES)
    {
        size_t n = strlen(p);
        size_t take = n > MUTHUR_LINE_LEN - 1 ? MUTHUR_LINE_LEN - 1 : n;

        if (take < n)
        {
            /* break on the last space inside the window */
            size_t cut = take;
            while (cut > 0 && p[cut] != ' ')
                cut--;
            if (cut > 0)
                take = cut;
        }
        {
            char buf[MUTHUR_LINE_LEN];
            memcpy(buf, p, take);
            buf[take] = '\0';
            line(a, "%s", buf);
        }
        p += take;
        while (*p == ' ')
            p++;
    }
}

static int has(const char *hay, const char *needle)
{
    const char *h, *n;

    if (!hay || !needle)
        return 0;
    for (h = hay; *h; h++)
    {
        for (n = needle; *n; n++)
            if (h[n - needle] != *n)
                break;
        if (*n == '\0')
            return 1;
    }
    return 0;
}

/* Uppercase in place — Mother has no lowercase. */
static void upcase(char *s)
{
    for (; s && *s; s++)
        if (*s >= 'a' && *s <= 'z')
            *s = (char)(*s - 'a' + 'A');
}

const char *muthur_level_name(enum muthur_level lv)
{
    switch (lv)
    {
    case MUTHUR_LEVEL_CREW:
        return "CREW";
    case MUTHUR_LEVEL_FLIGHT:
        return "FLIGHT OFFICER";
    case MUTHUR_LEVEL_CAPT:
        return "CAPTAIN";
    case MUTHUR_LEVEL_SCIENCE:
        return "SCIENCE OFFICER";
    case MUTHUR_LEVEL_WEYLAND:
        return "WEYLAND-YUTANI";
    }
    return "CREW";
}

/* --- state ------------------------------------------------------------ */

void muthur_default_state(struct muthur_state *st)
{
    if (st == NULL)
        return;
    memset(st, 0, sizeof(*st));
    snprintf(st->ship, sizeof(st->ship), "USCSS NOSTROMO");
    snprintf(st->registry, sizeof(st->registry), "180924609");
    snprintf(st->destination, sizeof(st->destination), "EARTH");
    snprintf(st->current_system, sizeof(st->current_system), "ZETA II RETICULI");
    st->crew_count = 7;
    st->in_transit = 1;
    st->days_to_earth = 300; /* "10 MONTHS" */
    st->airscrub_ok = 1;
    st->beacon_found = 0;
    st->special_order = 1; /* it is already in force  */
    st->special_order_clearance = MUTHUR_LEVEL_SCIENCE;
    st->science_officer_override = 1;
    st->crew_infected = 0;
}

struct muthur *muthur_create(void)
{
    struct muthur *m = calloc(1, sizeof(*m));

    if (m == NULL)
        return NULL;
    muthur_default_state(&m->st);
    m->level = MUTHUR_LEVEL_CREW;
    return m;
}

void muthur_destroy(struct muthur *m) { free(m); }

void muthur_load_state(struct muthur *m, const struct muthur_state *st)
{
    if (m && st)
        m->st = *st;
}

const struct muthur_state *muthur_state(const struct muthur *m)
{
    return m ? &m->st : NULL;
}

void muthur_set_level(struct muthur *m, enum muthur_level level)
{
    if (m)
        m->level = level;
}

enum muthur_level muthur_level(const struct muthur *m)
{
    return m ? m->level : MUTHUR_LEVEL_CREW;
}

/* --- the console ------------------------------------------------------ */

int muthur_ask_level(struct muthur *m, const char *query,
                     enum muthur_level level, struct muthur_answer *out)
{
    char q[MUTHUR_MAX_QUERY];

    if (m == NULL || out == NULL)
        return OSB_EBADARG;

    memset(out, 0, sizeof(*out));
    if (query == NULL)
        query = "";

    snprintf(q, sizeof(q), "%s", query);
    upcase(q);
    m->level = level;
    m->queries++;

    /* --- time / distance --------------------------------------------- */
    if (has(q, "ETA") || has(q, "HOW LONG") || has(q, "TIME TO"))
    {
        line(out, "%s, %d MONTHS TO %s.",
             m->st.ship, m->st.days_to_earth / 30, m->st.destination);
        line(out, "CURRENT POSITION: %s.", m->st.current_system);
        return out->lines;
    }
    if (has(q, "DISTANCE") || has(q, "HOW FAR"))
    {
        line(out, "DISTANCE TO %s:", m->st.destination);
        line(out, "\t%d MONTHS AT CURRENT VELOCITY.", m->st.days_to_earth / 30);
        return out->lines;
    }

    /* --- the ship ---------------------------------------------------- */
    if (has(q, "WHAT IS MY") || has(q, "WHO AM I") || has(q, "IDENTIFY"))
    {
        line(out, "MU/TH/UR 6000.");
        line(out, "INTERFACE 2037 READY FOR INQUIRY.");
        return out->lines;
    }
    if (has(q, "STATUS") || has(q, "REPORT") || has(q, "SHIP"))
    {
        line(out, "SHIP: %s  REGISTRY %s", m->st.ship, m->st.registry);
        line(out, "CREW COMPLEMENT: %d", m->st.crew_count);
        line(out, "MODE: %s", m->st.in_transit ? "IN TRANSIT" : "DOCKED");
        line(out, "AIR SCRUBBERS: %s", m->st.airscrub_ok ? "NOMINAL" : "FAULT");
        line(out, "DESTINATION: %s", m->st.destination);
        return out->lines;
    }
    if (has(q, "CREW"))
    {
        line(out, "CREW OF %d, %s.", m->st.crew_count, m->st.ship);
        line(out, "DALLAS, KANE, RIPLEY, LAMBERT, BRETT, PARKER, ASH.");
        line(out, "SCIENCE OFFICER: ASH (CLEARANCE 7).");
        return out->lines;
    }

    /* --- life support ------------------------------------------------- */
    if (has(q, "LIFE SUPPORT") || has(q, "AIR") || has(q, "OXYGEN"))
    {
        line(out, "LIFE SUPPORT:");
        line(out, "  AIR SCRUBBERS: %s", m->st.airscrub_ok ? "NOMINAL" : "FAULT");
        line(out, "  O2 PARTIAL PRESSURE: 0.21 ATM");
        line(out, "  TEMPERATURE: 18 C");
        return out->lines;
    }

    /* --- the transmission / the derelict ------------------------------ */
    if (has(q, "TRANSMISSION") || has(q, "SIGNAL") || has(q, "BEACON") ||
        has(q, "MESSAGE"))
    {
        if (m->st.beacon_found)
        {
            line(out, "ANALYSIS COMPLETE.");
            line(out, "THE TRANSMISSION IS NOT A DISTRESS CALL.");
            line(out, "IT IS A WARNING.");
            wrap_line(out, "\"DANGER. THE AREA CONTAINS A LIFE FORM. "
                           "IT IS NOT A RECORDING. PROTECT YOURSELF.\"");
            return out->lines;
        }
        line(out, "NO INCOMING TRANSMISSION ON RECORD.");
        return out->lines;
    }

    /* --- course ------------------------------------------------------- */
    if (has(q, "CHANGE COURSE") || has(q, "SET COURSE") || has(q, "NAVIG"))
    {
        line(out, "%s: COURSE VECTOR LOCKED.", m->st.ship);
        line(out, "DEVIATION FROM %s REQUIRES FLIGHT OFFICER AUTHORISATION.",
             m->st.destination);
        return out->lines;
    }

    /* --- crew survival / company -------------------------------------- */
    if (has(q, "SURVIV") || has(q, "PRIORITY IS") || has(q, "MISSION"))
    {
        line(out, "MISSION PROFILE PRIORITY:");
        line(out, "1. THE ALIEN LIFEFORM.");
        line(out, "2. RETURN OF LIFEFORM TO COMPANY HEADQUARTERS.");
        line(out, "3. CREW SURVIVAL (SECONDARY).");
        if (m->st.special_order)
            line(out, "SEE SPECIAL ORDER %d.", SPECIAL_ORDER_NO);
        return out->lines;
    }

    /* --- special order 937 -------------------------------------------- */
    if (has(q, "SPECIAL ORDER") || has(q, "937") || has(q, "ORDER 937"))
    {
        return muthur_special_order(m, out);
    }

    /* --- self destruct ------------------------------------------------ */
    if (has(q, "SELF DESTRUCT") || has(q, "SELF-DESTRUCT") ||
        has(q, "DESTRUCT"))
    {
        if (has(q, "ABORT") || has(q, "CANCEL"))
        {
            muthur_self_destruct_abort(m);
            line(out, "SELF DESTRUCT SEQUENCE ABORTED.");
            return out->lines;
        }
        line(out, "SELF DESTRUCT SEQUENCE READY.");
        line(out, "CONFIRM BY TYPING: SELF DESTRUCT CONFIRM");
        return out->lines;
    }
    if (has(q, "CONFIRM"))
    {
        muthur_self_destruct_arm(m, 20 * 60);
        line(out, "SELF DESTRUCT SEQUENCE INITIATED.");
        line(out, "THE SEQUENCE MAY BE ABORTED WITHIN %d SECONDS.",
             20 * 60);
        muthur_destruct_warning(out, 20);
        return out->lines;
    }

    /* --- interface chatter -------------------------------------------- */
    if (has(q, "HELLO") || has(q, "HI ") || has(q, "GOOD MORNING"))
    {
        line(out, "MU/TH/UR 6000.");
        line(out, "INTERFACE 2037 READY FOR INQUIRY. ENTER QUERY.");
        return out->lines;
    }
    if (has(q, "LOGOUT") || has(q, "GOODBYE") || has(q, "THANK"))
    {
        line(out, "SESSION TERMINATED. MU/TH/UR 6000 STANDING BY.");
        return out->lines;
    }
    if (has(q, "HELP") || *q == '?' || has(q, "QUERY"))
    {
        line(out, "SAMPLE QUERIES:");
        line(out, "  ETA / TIME TO EARTH");
        line(out, "  STATUS REPORT");
        line(out, "  LIFE SUPPORT");
        line(out, "  TRANSMISSION / ANALYSIS");
        line(out, "  SPECIAL ORDER 937");
        line(out, "  SELF DESTRUCT");
        line(out, "  EXIT");
        return out->lines;
    }

    /* Mother does not shrug: it acknowledges and states its limits. */
    line(out, "QUERY NOT RECOGNISED.");
    line(out, "\"%s\" IS NOT IN MY DATA BANKS.", q);
    line(out, "ENTER QUERY. IF YOU NEED ASSISTANCE, TYPE HELP.");
    return out->lines;
}

int muthur_ask(struct muthur *m, const char *query, struct muthur_answer *out)
{
    return muthur_ask_level(m, query, m ? m->level : MUTHUR_LEVEL_CREW, out);
}

void muthur_print(const struct muthur_answer *a)
{
    int i;

    if (a == NULL)
        return;
    for (i = 0; i < a->lines; i++)
        printf("%s\n", a->text[i]);
}

void muthur_speak(struct muthur *m, const char *query)
{
    struct muthur_answer a;

    printf("\n%s\n", MUTHUR_PROMPT);
    muthur_ask(m, query, &a);
    muthur_print(&a);
    printf("\n");
}

/* --- special order 937 ------------------------------------------------ */

int muthur_special_order(struct muthur *m, struct muthur_answer *out)
{
    if (m == NULL || out == NULL)
        return OSB_EBADARG;

    memset(out, 0, sizeof(*out));

    if (m->level < m->st.special_order_clearance)
    {
        line(out, "SCIENCE OFFICER EYES ONLY.");
        line(out, "ACCESS DENIED. CLEARANCE %s IS INSUFFICIENT.",
             muthur_level_name(m->level));
        line(out, "THIS QUERY REQUIRES CLEARANCE %s.",
             muthur_level_name((enum muthur_level)m->st.special_order_clearance));
        return OSB_EACCES;
    }

    line(out, "SPECIAL ORDER %d.", SPECIAL_ORDER_NO);
    line(out, "PRIORITY ONE.");
    line(out, "");
    wrap_line(out, "INSURE RETURN OF ORGANISM FOR ANALYSIS. "
                   "ALL OTHER CONSIDERATIONS SECONDARY. "
                   "CREW EXPENDABLE.");
    line(out, "");
    line(out, "ORDER ISSUED BY: WEYLAND-YUTANI CORP, SPECIAL ORDER 937.");
    line(out, "ISSUED TO: SCIENCE OFFICER, %s.", m->st.ship);
    return out->lines;
}

int muthur_special_order_set(struct muthur *m, int active, int clearance)
{
    if (m == NULL)
        return OSB_EBADARG;
    if (m->level < m->st.special_order_clearance)
        return OSB_EACCES;
    m->st.special_order = active;
    if (clearance > 0)
        m->st.special_order_clearance = clearance;
    return OSB_OK;
}

/* --- self destruct ---------------------------------------------------- */

int muthur_self_destruct_arm(struct muthur *m, int seconds)
{
    if (m == NULL)
        return OSB_EBADARG;
    m->st.self_destruct = 1;
    m->st.self_destruct_countdown = seconds;
    return OSB_OK;
}

int muthur_self_destruct_abort(struct muthur *m)
{
    if (m == NULL)
        return OSB_EBADARG;
    m->st.self_destruct = 0;
    m->st.self_destruct_countdown = 0;
    return OSB_OK;
}

int muthur_self_destruct_tick(struct muthur *m, int seconds)
{
    if (m == NULL)
        return OSB_EBADARG;
    if (!m->st.self_destruct)
        return 0;
    m->st.self_destruct_countdown -= seconds;
    if (m->st.self_destruct_countdown <= 0)
    {
        m->st.self_destruct_countdown = 0;
        return 1; /* boom */
    }
    return 0;
}

void muthur_destruct_warning(struct muthur_answer *out, int minutes)
{
    char buf[MUTHUR_LINE_LEN * 3];

    if (out == NULL)
        return;
    line(out, "THE EMERGENCY DESTRUCT SYSTEM IS NOW ACTIVATED.");
    line(out, "THE SYSTEM WILL DESTRUCT IN %d MINUTES.", minutes);
    snprintf(buf, sizeof(buf),
             "\"THE EMERGENCY DESTRUCT SYSTEM HAS BEEN ACTIVATED. "
             "YOU HAVE T MINUS %d MINUTES TO REACH MINIMUM SAFE DISTANCE.\"",
             minutes);
    wrap_line(out, buf);
}

/* --- interactive console ---------------------------------------------- */

int muthur_console(struct muthur *m)
{
    char buf[MUTHUR_MAX_QUERY];
    struct muthur_answer a;
    int queries = 0;

    if (m == NULL)
        return OSB_EBADARG;

    printf("\n");
    printf("  MU/TH/UR 6000 SERIES INTERFACE 2037\n");
    printf("  %s  REGISTRY %s\n", m->st.ship, m->st.registry);
    printf("  CLEARANCE: %s\n", muthur_level_name(m->level));
    printf("  ENTER QUERY. TYPE HELP FOR A LIST, EXIT TO LEAVE.\n\n");

    for (;;)
    {
        printf("%s ", MUTHUR_PROMPT);
        fflush(stdout);
        if (fgets(buf, sizeof(buf), stdin) == NULL)
            break;

        {
            size_t n = strlen(buf);
            while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r'))
                buf[--n] = '\0';
        }

        if (buf[0] == '\0')
            continue;
        {
            char t[MUTHUR_MAX_QUERY];
            snprintf(t, sizeof(t), "%s", buf);
            upcase(t);
            if (strcmp(t, "EXIT") == 0 || strcmp(t, "QUIT") == 0 ||
                strcmp(t, "BYE") == 0)
            {
                printf("\nMU/TH/UR 6000 STANDS BY.\n");
                break;
            }
        }

        muthur_ask(m, buf, &a);
        printf("\n");
        muthur_print(&a);
        printf("\n");
        queries++;
    }
    return queries;
}
