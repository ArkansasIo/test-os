#ifndef OSB_MUTHUR_H
#define OSB_MUTHUR_H

#include "types.h"

/* ---------------------------------------------------------------------------
 * Weyland-Yutani MU/TH/UR 6000  ("Mother")
 *
 * A clone of the shipboard computer from Alien (1979). In the film the crew
 * address it at a console in the mess hall; it answers in uppercase, terse
 * status lines and runs an internal special-order subsystem (SPECIAL ORDER
 * 937) with its own access hierarchy.
 *
 * This module is the OS-level service that backs that interface:
 *   muthur_ask()      - one query, one answer   (the console)
 *   muthur_console()  - the interactive prompt, MUTHUR 6000>
 *
 * Access levels mirror the film: the science officer holds the highest
 * clearance and the special order is only readable at that level.
 * ------------------------------------------------------------------------- */

#define MUTHUR_NAME "MU/TH/UR 6000"
#define MUTHUR_PROMPT "MUTHUR 6000>"
#define MUTHUR_MAX_QUERY 128
#define MUTHUR_MAX_LINES 24
#define MUTHUR_LINE_LEN 72

/* --- Clearance --------------------------------------------------------- */
enum muthur_level
{
    MUTHUR_LEVEL_CREW = 1,    /* general crew, "what's the ETA"      */
    MUTHUR_LEVEL_FLIGHT = 3,  /* flight officer, navigation & status */
    MUTHUR_LEVEL_CAPT = 5,    /* captain, ship-wide orders          */
    MUTHUR_LEVEL_SCIENCE = 7, /* science officer: SPECIAL ORDER 937 */
    MUTHUR_LEVEL_WEYLAND = 9  /* company override                   */
};

#define SPECIAL_ORDER_NO 937

/* --- Ship state -------------------------------------------------------- */
struct muthur_state
{
    char ship[32];               /* "USCSS NOSTROMO"                 */
    char registry[16];           /* "180924609"                      */
    char crew_count;             /* 7                                */
    int in_transit;              /* 1 while between systems          */
    int days_to_earth;           /* "10 MONTHS" as the film puts it  */
    int self_destruct;           /* armed? 0                 */
    int self_destruct_countdown; /* seconds left             */
    int airscrub_ok;
    int beacon_found;             /* the derelict's transmission      */
    int special_order;            /* 937 active?                      */
    int special_order_clearance;  /* level required           */
    int science_officer_override; /* Ash's company override   */
    int crew_infected;
    char destination[32];
    char current_system[32];
};

struct muthur_query
{
    const char *text;
    enum muthur_level level;
};

struct muthur_answer
{
    int lines;
    char text[MUTHUR_MAX_LINES][MUTHUR_LINE_LEN];
};

struct muthur *muthur_create(void);
void muthur_destroy(struct muthur *m);

/* Defaults from the Nostromo: registry 180924609, transit, 10 months out,
 * SPECIAL ORDER 937 sealed at science-officer clearance. */
void muthur_default_state(struct muthur_state *st);
void muthur_load_state(struct muthur *m, const struct muthur_state *st);
const struct muthur_state *muthur_state(const struct muthur *m);

/* Set the caller's clearance (from the login layer's session). */
void muthur_set_level(struct muthur *m, enum muthur_level level);
enum muthur_level muthur_level(const struct muthur *m);
const char *muthur_level_name(enum muthur_level lv);

/* --- The console ------------------------------------------------------- */
/* Answer a query. Returns the number of lines, or negative on refusal.
 * Unknown input still answers - Mother never says "I don't understand". */
int muthur_ask(struct muthur *m, const char *query, struct muthur_answer *out);
int muthur_ask_level(struct muthur *m, const char *query,
                     enum muthur_level level, struct muthur_answer *out);

/* Print an answer the way the film console does. */
void muthur_print(const struct muthur_answer *a);
void muthur_speak(struct muthur *m, const char *query);

/* Interactive console: reads lines until EXIT/QUIT, answering each. */
int muthur_console(struct muthur *m);

/* --- Special order 937 ------------------------------------------------- */
/* "SCIENCE OFFICER EYES ONLY" — refused below MUTHUR_LEVEL_SCIENCE. */
int muthur_special_order(struct muthur *m, struct muthur_answer *out);
int muthur_special_order_set(struct muthur *m, int active, int clearance);

/* --- Self destruct ----------------------------------------------------- */
int muthur_self_destruct_arm(struct muthur *m, int seconds);
int muthur_self_destruct_abort(struct muthur *m);
int muthur_self_destruct_tick(struct muthur *m, int seconds);

/* The film's exact countdown wording, for the destruct sequence. */
void muthur_destruct_warning(struct muthur_answer *out, int minutes);

#endif /* OSB_MUTHUR_H */
