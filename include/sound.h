#ifndef OSB_SOUND_H
#define OSB_SOUND_H

#include "types.h"

/* ---------------------------------------------------------------------------
 * osborne1-cpm : sound — the 8253 PIT / PC-speaker tone generator
 *
 * The Osborne 1 has no dedicated sound chip: like most CP/M machines of the
 * era it drives a one-bit speaker from a counter/timer. That is enough to
 * produce the whole vocabulary of 1980s computing: key clicks, boot beeps,
 * error buzzes, and the beeps that Pong plays when the ball hits a paddle.
 *
 * Model:
 *   - a square wave at `freq_hz`, gated on/off by the speaker port
 *   - one-channel, one-bit output (a PC speaker is either moving or not)
 *   - "rendering" writes samples into a buffer, so a front end can push them
 *     at a real audio device, and a headless tool can dump a .wav
 *
 * The 2600 Hz tone is the classic continuous tone (also the frequency used by
 * phone phreaks); PONG_* beeps are the tinny square-wave blips the arcade game
 * made famous.
 * ------------------------------------------------------------------------- */

/* --- The tone device --------------------------------------------------- */
#define SOUND_TONE_2600 2600.0 /* the requested 2600 Hz tone           */
#define SOUND_TONE_BEEP 880.0  /* ordinary prompt beep                 */
#define SOUND_TONE_ERROR 220.0 /* low error buzz                       */

/* Pong's three sounds, as the 2600-ish arcade beeps.
 *   paddle hit : a short high blip
 *   wall  hit  : a short mid blip
 *   score/miss : a low descending thunk
 */
#define PONG_FREQ_PADDLE 1000.0
#define PONG_FREQ_WALL 450.0
#define PONG_FREQ_SCORE 160.0

/* Printer sounds: the dot-matrix / daisy-wheel vocabulary. A line of text fed
 * through the print head rattles at the character rate, the carriage return
 * sweeps, the paper feed steps, and the end of a job rings a done-bell. */
#define PRINTER_FREQ_TICK 900.0  /* head stepping over a character */
#define PRINTER_FREQ_RETURN 380.0 /* carriage return sweep         */
#define PRINTER_FREQ_FEED 260.0   /* line feed / paper advance     */
#define PRINTER_FREQ_BELL 1200.0  /* job-done bell                 */
#define PRINTER_FREQ_READY 700.0  /* online / ready chirp          */
#define PRINTER_FREQ_READY_LO 520.0 /* second note of the ready chirp */

#define SOUND_MAX_HZ 20000.0
#define SOUND_MIN_HZ 20.0

/* Default sample rate for rendering to PCM (the CP/M machine's DAC saw 8-bit
 * unsigned samples; 22050 is plenty for square waves). */
#define SOUND_RATE_DEFAULT 22050

/* Big enough for the longest rendered tone: the 2600 Hz effect is held for a
 * full second, which is SOUND_RATE_DEFAULT samples at the default rate. */
#define SOUND_MAX_SAMPLES (SOUND_RATE_DEFAULT * 2)

struct sound;

struct sound *sound_create(void);
void sound_destroy(struct sound *s);

void sound_reset(struct sound *s);

/* Gate the speaker. `freq` <= 0 (or out of SOUND_MIN_HZ..SOUND_MAX_HZ) is
 * rejected; use sound_off() to silence. */
int sound_on(struct sound *s, double freq_hz);
void sound_off(struct sound *s);

int sound_active(const struct sound *s);
double sound_freq(const struct sound *s);

/* Sample rate used by sound_render(). */
int sound_set_rate(struct sound *s, int rate);
int sound_rate(const struct sound *s);

/* Render `nsamples` of the current tone as 8-bit unsigned PCM (128 = silence,
 * which is also what sound_off() produces). Returns the number of samples
 * written, or negative on bad arguments. */
int sound_render(struct sound *s, u8 *buf, int nsamples);

/* Append a square-wave tone of `ms` milliseconds at `freq_hz` to `buf`,
 * returning the samples written. Does not change the speaker state. */
int sound_tone_pcm(u8 *buf, int max_samples, int rate, double freq_hz, int ms);

/* --- Effects ----------------------------------------------------------- */

/* The 2600 Hz tone, held for `ms` milliseconds, appended into `buf`. */
int sound_2600(struct sound *s, u8 *buf, int max_samples, int ms);

/* Play a Pong event: renders its tone into `buf` and leaves the speaker
 * gated at that frequency. The caller stops it with sound_off(), or by
 * playing the next sound — exactly how Pong's one-shot beeps behaved. */
int sound_pong(struct sound *s, int event, u8 *buf, int max_samples);

enum sound_pong_event
{
    PONG_PADDLE = 0, /* ball hit a paddle          */
    PONG_WALL,       /* ball hit the top/bottom    */
    PONG_SCORE,      /* a player missed, a point   */
    PONG_START,      /* serve / game start blip     */
    PONG_GAME_OVER   /* the three-note "you lost"  */
};

/* Convenience: the tone + duration for each Pong event. */
double sound_pong_freq(int event);
int sound_pong_ms(int event);

/* --- Printer effect ---------------------------------------------------- */

/* One printer event, rendered as 8-bit Pong-style blips. `speed` is the
 * character rate in characters per second (<= 0 means PRINTER_CPS_DEFAULT).
 * A line print chops into per-character ticks; a page print repeats a line
 * with carriage returns and line feeds; a job repeats a few pages and ends
 * with the done-bell. */
int sound_printer(struct sound *s, int event, u8 *buf, int max_samples,
                  int speed);

enum sound_printer_event
{
    /* Based above the Pong range so a single event code can be handed to
     * sound_describe() without the two vocabularies colliding. */
    PRINTER_READY = 100, /* power-on / online chirp      */
    PRINTER_DOT,       /* a single character printed     */
    PRINTER_LINE,      /* one line, then a carriage ret. */
    PRINTER_FEED,      /* paper advance                  */
    PRINTER_BELL,      /* end of job: the done-bell       */
    PRINTER_JOB        /* a whole several-page job       */
};

#define PRINTER_CPS_DEFAULT 100 /* chars/sec a dot-matrix head ran at */

/* Convenience: tone + duration for one printer event. */
double sound_printer_freq(int event);
int sound_printer_ms(int event);

/* --- Output helpers ---------------------------------------------------- */

/* Write `nsamples` of 8-bit unsigned PCM as a mono WAV file. Returns OSB_OK
 * or a negative error. */
int sound_write_wav(const char *path, const u8 *pcm, int nsamples, int rate);

/* Emit the terminal BEL, for front ends with no audio device. */
void sound_bell(void);

/* "Send `path` to the printer": prints a banner line with the file name, a
 * body of character ticks, and rings the done-bell — the whole job rendered
 * through the printer vocabulary. Returns OSB_OK, or OSB_EBADARG when `path`
 * is empty. This is what a print: menu action calls. */
int osb_print_file(const char *path);

/* Print a description of a rendered effect (used by --sound-demo). */
void sound_describe(int event, double freq_hz, int ms);

#endif /* OSB_SOUND_H */
