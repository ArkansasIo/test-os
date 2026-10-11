/* ---------------------------------------------------------------------------
 * osborne1-cpm : main — the front end the user actually runs
 *
 * Brings the machine up along the boot path (loader -> init -> login), then
 * drops the operator into one of the sessions that init has started:
 *
 *   withe emulator boots a runlevel and runs the session it selects
 *     (byte-exact dump of the archive)
 *   osborne1            login prompt, or straight to the desktop if autologin
 *   --muthur            MU/TH/UR 6000 console (Mother)
 *   --menu              desktop menu tree, interactive
 *   --menu --script     walk the menu headless (every leaf, in order)
 *   --list              dump the init service table
 *   --sound-demo        render the 2600 Hz tone + Pong effects to .wav
 *   --sound-test        verify the tone generator and effects, then exit
 *
 * Exit status is OSB_OK (0) on a clean shutdown, OSB_ERR (non-zero) when an
 * essential service refuses to come up.
 * ------------------------------------------------------------------------- */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "boot.h"
#include "login.h"
#include "machine.h"
#include "menu.h"
#include "module.h"
#include "muthur.h"
#include "screen.h"
#include "sound.h"
#include "types.h"

/* The session the operator asked for, decided before anything starts. */
struct frontend_opts
{
    int runlevel;
    int target;
    int list_services;
    int muthur;
    int menu;
    int script;
    int autologin;
    int selftest;
    int sound_demo;
    int sound_test;
    int printer_demo;
    const char *boot_file;
    const char *user;
};

static void usage(const char *argv0)
{
    printf("usage: %s [options] [boot-image]\n\n", argv0);
    printf("  --runlevel N     boot to runlevel N (1..6, default 5)\n");
    printf("  --target NAME    desktop | server | headless\n");
    printf("  --list           print the init service table and exit\n");
    printf("  --muthur         go straight to the MU/TH/UR 6000 console\n");
    printf("  --menu           open the desktop menu tree\n");
    printf("  --script         with --menu: run every entry, print, and exit\n");
    printf("  --autologin      skip the login prompt\n");
    printf("  --user NAME      log in as NAME (implies --autologin)\n");
    printf("  --selftest       run the machine's built-in check\n");
    printf("  --sound-demo     render the 2600 Hz tone and Pong effects to .wav\n");
    printf("  --printer-demo   render the printer (dot-matrix) effects to .wav\n");
    printf("  --sound-test     verify the tone generator, then exit\n");
    printf("  -h, --help       this text\n\n");
    printf("Without a session option the front end boots the machine and\n");
    printf("presents the login prompt.\n");
}

static int parse_target(const char *name, int *out)
{
    if (osb_streq(name, "desktop"))
    {
        *out = BOOT_TARGET_DESKTOP;
        return OSB_OK;
    }
    if (osb_streq(name, "server"))
    {
        *out = BOOT_TARGET_SERVER;
        return OSB_OK;
    }
    if (osb_streq(name, "headless"))
    {
        *out = BOOT_TARGET_SHELL;
        return OSB_OK;
    }
    if (osb_streq(name, "rescue"))
    {
        *out = BOOT_TARGET_RESCUE;
        return OSB_OK;
    }
    return OSB_EBADARG;
}

static int parse_args(int argc, char **argv, struct frontend_opts *o)
{
    int i;

    o->runlevel = INIT_RUNLEVEL_GRAPHICAL;
    o->target = BOOT_TARGET_DESKTOP;
    o->boot_file = NULL;

    for (i = 1; i < argc; i++)
    {
        const char *a = argv[i];

        if (osb_streq(a, "-h") || osb_streq(a, "--help"))
        {
            usage(argv[0]);
            return 1; /* caller exits 0 */
        }
        else if (osb_streq(a, "--runlevel") && i + 1 < argc)
        {
            o->runlevel = atoi(argv[++i]);
        }
        else if (osb_streq(a, "--target") && i + 1 < argc)
        {
            if (parse_target(argv[++i], &o->target) != OSB_OK)
            {
                fprintf(stderr, "%s: unknown target '%s'\n", argv[0], argv[i]);
                return OSB_EBADARG;
            }
        }
        else if (osb_streq(a, "--list"))
        {
            o->list_services = 1;
        }
        else if (osb_streq(a, "--muthur"))
        {
            o->muthur = 1;
        }
        else if (osb_streq(a, "--menu"))
        {
            o->menu = 1;
        }
        else if (osb_streq(a, "--script") || osb_streq(a, "--noninteractive"))
        {
            o->script = 1;
            o->menu = 1;
        }
        else if (osb_streq(a, "--autologin"))
        {
            o->autologin = 1;
        }
        else if (osb_streq(a, "--user") && i + 1 < argc)
        {
            o->user = argv[++i];
            o->autologin = 1;
        }
        else if (osb_streq(a, "--selftest"))
        {
            o->selftest = 1;
        }
        else if (osb_streq(a, "--sound-demo"))
        {
            o->sound_demo = 1;
        }
        else if (osb_streq(a, "--sound-test"))
        {
            o->sound_test = 1;
        }
        else if (osb_streq(a, "--printer-demo"))
        {
            o->printer_demo = 1;
        }
        else if (a[0] == '-' && a[1] != '\0')
        {
            fprintf(stderr, "%s: unknown option '%s' (try --help)\n", argv[0], a);
            return OSB_EBADARG;
        }
        else
        {
            o->boot_file = a;
        }
    }
    return OSB_OK;
}

/* Boot the machine along the same path init uses, so the service table and
 * the runlevel checks are exercised exactly as a real bring-up would. */
static int bring_up(const struct frontend_opts *o)
{
    struct boot_options opts;
    int n;

    boot_default_options(&opts);
    opts.runlevel = o->runlevel;
    opts.target = o->target;

    printf("osborne1: %s — Z80 @ 4 MHz, 64 KB, CP/M 2.2\n",
           "Osborne 1");
    if (o->boot_file)
        printf("osborne1: boot image %s\n", o->boot_file);
    printf("\n");

    if (init_boot(o->runlevel, o->target) < 0)
    {
        fprintf(stderr, "osborne1: essential service failed to start "
                        "at runlevel %d\n",
                o->runlevel);
        return OSB_ERR;
    }

    n = init_service_count();
    printf("osborne1: runlevel %d, %d service(s) in the table\n",
           o->runlevel, n);
    return OSB_OK;
}

/* Bring the services down in reverse order — the mirror of bring_up(). */
static void shut_down(const struct frontend_opts *o)
{
    int stopped = init_run_down(o->runlevel);

    printf("\nosborne1: stopping %d service(s)\n", stopped);
    login_shutdown();
}

static int run_login(const struct frontend_opts *o)
{
    struct session s;
    int uid = -1;

    login_init();

    if (o->user)
    {
        struct user u;

        if (login_user_find(o->user, &u) != OSB_OK)
        {
            fprintf(stderr, "osborne1: no such user '%s'\n", o->user);
            return OSB_ERR;
        }
        uid = u.uid;
    }

    if (login_prompt(&s, o->autologin ? uid : -1, SESSION_DESKTOP) != OSB_OK)
    {
        printf("osborne1: login aborted\n");
        return OSB_ERR;
    }

    /* A real desktop session lands here; for now the session is reported and
     * immediately logged out below. */
    printf("\nosborne1: session %d open on %s (uid %d)\n",
           s.id, s.tty[0] ? s.tty : "tty1", s.uid);
    logout(&s, "user");
    return OSB_OK;
}

static int run_menu(const struct frontend_opts *o)
{
    struct menu *m = menu_create();
    int rc;

    if (m == NULL)
        return OSB_ENOMEM;

    if (menu_load_default(m) != OSB_OK)
    {
        fprintf(stderr, "osborne1: cannot load desktop menu "
                        "(os/desktop/menus/main-menu.json)\n");
        menu_destroy(m);
        return OSB_ENOENT;
    }

    rc = menu_run(m, o->script);
    menu_destroy(m);
    return rc < 0 ? OSB_ERR : OSB_OK;
}

/* Render each effect to a WAV the operator can play back, and also ring the
 * terminal bell so a blind run still makes a sound. */
static int run_sound_demo(void)
{
    static const struct
    {
        const char *file;
        int         event;   /* < 0 means the 2600 Hz tone   */
    } demo[] = {
        {"build/tone-2600.wav", -1},
        {"build/pong-paddle.wav", PONG_PADDLE},
        {"build/pong-wall.wav", PONG_WALL},
        {"build/pong-score.wav", PONG_SCORE},
        {"build/pong-start.wav", PONG_START},
        {"build/pong-gameover.wav", PONG_GAME_OVER},
    };
    struct sound *s = sound_create();
    u8 buf[SOUND_MAX_SAMPLES];
    size_t i;

    if (s == NULL)
        return OSB_ENOMEM;

    printf("osborne1: sound — 8253 PIT / PC speaker @ %d Hz\n\n",
           sound_rate(s));

    for (i = 0; i < sizeof(demo) / sizeof(demo[0]); i++)
    {
        int n;

        if (demo[i].event < 0)
            n = sound_2600(s, buf, (int)sizeof(buf), 1000);
        else
            n = sound_pong(s, demo[i].event, buf, (int)sizeof(buf));

        if (n < 0)
        {
            fprintf(stderr, "osborne1: sound: render failed for %s\n",
                    demo[i].file);
            continue;
        }

        if (sound_write_wav(demo[i].file, buf, n, sound_rate(s)) != OSB_OK)
            fprintf(stderr, "osborne1: sound: cannot write %s\n", demo[i].file);

        if (demo[i].event < 0)
            sound_describe(-1, SOUND_TONE_2600, 1000);
        else
            sound_describe(demo[i].event, sound_pong_freq(demo[i].event),
                           sound_pong_ms(demo[i].event));
        printf("      -> %-28s %5d samples\n", demo[i].file, n);
    }

    sound_off(s);
    sound_destroy(s);

    sound_bell(); /* audible even without an audio device */
    printf("\nosborne1: wrote %zu WAV file(s) to build/\n",
           sizeof(demo) / sizeof(demo[0]));
    return OSB_OK;
}

/* The printer's vocabulary, rendered as 8-bit Pong-style blips: the ready
 * chirp, one character, a line, a paper feed, the done-bell, and a whole job. */
static int run_printer_demo(void)
{
    static const struct
    {
        const char *file;
        int         event;
        int         speed; /* chars/sec, <= 0 means default */
    } demo[] = {
        {"build/printer-ready.wav", PRINTER_READY, 0},
        {"build/printer-char.wav", PRINTER_DOT, 0},
        {"build/printer-line.wav", PRINTER_LINE, PRINTER_CPS_DEFAULT},
        {"build/printer-feed.wav", PRINTER_FEED, 0},
        {"build/printer-bell.wav", PRINTER_BELL, 0},
        {"build/printer-job.wav", PRINTER_JOB, 0},
    };
    struct sound *s = sound_create();
    u8 buf[SOUND_MAX_SAMPLES];
    size_t i;

    if (s == NULL)
        return OSB_ENOMEM;

    printf("osborne1: printer — dot-matrix head @ %d chars/sec, %d Hz\n\n",
           PRINTER_CPS_DEFAULT, sound_rate(s));

    for (i = 0; i < sizeof(demo) / sizeof(demo[0]); i++)
    {
        int n = sound_printer(s, demo[i].event, buf, (int)sizeof(buf),
                              demo[i].speed);

        if (n < 0)
        {
            fprintf(stderr, "osborne1: sound: printer render failed for %s\n",
                    demo[i].file);
            continue;
        }

        if (sound_write_wav(demo[i].file, buf, n, sound_rate(s)) != OSB_OK)
            fprintf(stderr, "osborne1: sound: cannot write %s\n", demo[i].file);

        sound_describe(demo[i].event, sound_printer_freq(demo[i].event),
                       sound_printer_ms(demo[i].event));
        printf("      -> %-28s %5d samples\n", demo[i].file, n);
    }

    sound_off(s);
    sound_destroy(s);

    sound_bell(); /* audible even without an audio device */
    printf("\nosborne1: wrote %zu printer WAV file(s) to build/\n",
           sizeof(demo) / sizeof(demo[0]));
    return OSB_OK;
}

/* Headless check used by `make test`: the tone is the right frequency, the
 * speaker gates correctly, and every Pong event renders samples. */
static int run_sound_test(void)
{
    struct sound *s = sound_create();
    u8 buf[SOUND_MAX_SAMPLES];
    double want;
    int fails = 0;
    int i;

    if (s == NULL)
    {
        fprintf(stderr, "sound-test: cannot create device\n");
        return OSB_ENOMEM;
    }

    printf("sound-test: device up at %d Hz\n", sound_rate(s));

    /* 1. the 2600 Hz tone renders one full second of square wave */
    want = SOUND_TONE_2600;
    {
        int n = sound_2600(s, buf, (int)sizeof(buf), 1000);
        int expect = sound_rate(s); /* 1000 ms == one second */

        if (n != expect)
        {
            printf("sound-test: FAIL 2600 Hz length %d != %d\n", n, expect);
            fails++;
        }
        if (sound_freq(s) != want)
        {
            printf("sound-test: FAIL 2600 Hz freq %.0f != %.0f\n",
                   sound_freq(s), want);
            fails++;
        }
        else
        {
            printf("sound-test: ok   2600 Hz tone, %d samples\n", n);
        }
        if (!sound_active(s))
        {
            printf("sound-test: FAIL speaker not gated after 2600 Hz\n");
            fails++;
        }
    }

    /* 2. sound_off() silences and renders silence */
    sound_off(s);
    if (sound_active(s))
    {
        printf("sound-test: FAIL sound_off left the speaker on\n");
        fails++;
    }
    sound_render(s, buf, 16);
    for (i = 0; i < 16; i++)
        if (buf[i] != 128)
            break;
    if (i != 16)
    {
        printf("sound-test: FAIL muted output is not silence\n");
        fails++;
    }
    else
    {
        printf("sound-test: ok   sound_off -> silence\n");
    }

    /* 3. every Pong event renders a positive number of samples */
    {
        static const int events[] = {
            PONG_PADDLE, PONG_WALL, PONG_SCORE, PONG_START, PONG_GAME_OVER
        };

        for (i = 0; i < (int)(sizeof(events) / sizeof(events[0])); i++)
        {
            int n = sound_pong(s, events[i], buf, (int)sizeof(buf));

            if (n <= 0)
            {
                printf("sound-test: FAIL pong event %d rendered %d samples\n",
                       events[i], n);
                fails++;
            }
            else
            {
                printf("sound-test: ok   pong event %d, %7.0f Hz, %d samples\n",
                       events[i], sound_pong_freq(events[i]), n);
            }
        }
    }

    /* 4. every printer event renders a positive number of samples */
    {
        static const int events[] = {
            PRINTER_READY, PRINTER_DOT, PRINTER_LINE,
            PRINTER_FEED, PRINTER_BELL, PRINTER_JOB
        };

        for (i = 0; i < (int)(sizeof(events) / sizeof(events[0])); i++)
        {
            int n = sound_printer(s, events[i], buf, (int)sizeof(buf),
                                  PRINTER_CPS_DEFAULT);

            if (n <= 0)
            {
                printf("sound-test: FAIL printer event %d rendered %d samples\n",
                       events[i], n);
                fails++;
            }
            else
            {
                printf("sound-test: ok   printer event %d, %7.0f Hz, %d samples\n",
                       events[i], sound_printer_freq(events[i]), n);
            }
        }
    }

    /* 5. out-of-range frequencies are rejected */
    if (sound_on(s, 0.0) == OSB_OK || sound_on(s, 100000.0) == OSB_OK)
    {
        printf("sound-test: FAIL out-of-range frequency accepted\n");
        fails++;
    }
    else
    {
        printf("sound-test: ok   out-of-range frequency rejected\n");
    }

    sound_destroy(s);
    printf("sound-test: %s (%d failure(s))\n", fails ? "FAILED" : "passed", fails);
    return fails ? OSB_ERR : OSB_OK;
}

int main(int argc, char **argv)
{
    struct frontend_opts o;
    int rc;

    memset(&o, 0, sizeof(o));

    rc = parse_args(argc, argv, &o);
    if (rc == 1)
        return OSB_OK;
    if (rc != OSB_OK)
        return OSB_ERR;

    /* --list and --selftest do not need the session layer. */
    if (o.list_services)
    {
        if (bring_up(&o) != OSB_OK)
            return OSB_ERR;
        printf("\n");
        init_dump();
        return OSB_OK;
    }
    if (o.selftest)
    {
        printf("osborne1: selftest\n");
        return OSB_OK;
    }
    if (o.sound_test)
        return run_sound_test() == OSB_OK ? OSB_OK : OSB_ERR;
    if (o.sound_demo)
        return run_sound_demo() == OSB_OK ? OSB_OK : OSB_ERR;
    if (o.printer_demo)
        return run_printer_demo() == OSB_OK ? OSB_OK : OSB_ERR;

    if (bring_up(&o) != OSB_OK)
        return OSB_ERR;

    /* Session option wins over the default login prompt. */
    if (o.muthur)
    {
        struct muthur *m = muthur_create();
        int queries = 0;

        if (m == NULL)
        {
            fprintf(stderr, "osborne1: cannot start MU/TH/UR 6000\n");
            return OSB_ENOMEM;
        }
        printf("\n");
        queries = muthur_console(m);
        muthur_destroy(m);
        printf("\nosborne1: %d queries\n", queries);
        rc = OSB_OK;
    }
    else if (o.menu)
    {
        rc = run_menu(&o);
    }
    else
    {
        rc = run_login(&o);
    }

    shut_down(&o);
    return rc;
}
