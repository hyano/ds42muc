/*
 * ds42muc: decompiler from Dragon Slayer 4 (MSX) to MUCOM88 MML
 *
 * Copyright (c) 2019 Hirokuni Yano
 *
 * Released under the MIT license.
 * see https://opensource.org/licenses/MIT
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>

/* use macro instead of expanding envelope command. */
#define USE_SSG_ENV_MACRO

/* combine long length tones. */
#define COMBINE_LONG_TONE

/* combine long length rests. doesn't work due to MUCOM88 bug. */
#undef COMBINE_LONG_REST

/* global option(s) */
bool g_opt_verbose = false;
bool g_opt_ignore_warning = false;

#define BUFF_SIZE (16 * 1024)
uint8_t g_data[BUFF_SIZE];
uint8_t g_loop_flag[BUFF_SIZE];
uint8_t g_loop_nest[BUFF_SIZE];
uint32_t g_ssg_tempo_prev = UINT32_MAX;
uint32_t g_ssg_tempo_count = 0;

#ifdef USE_SSG_ENV_MACRO
const char g_ssg_inst[] = 
"# *0{E$ff,$ff,$ff,$ff,$00,$ff}\n"
"# *1{E$ff,$00,$37,$c8,$00,$0a}\n"
"# *2{E$ff,$00,$37,$c8,$01,$0a}\n"
"# *3{E$ff,$00,$37,$be,$0a,$0a}\n"
"# *4{E$78,$00,$0f,$f0,$01,$0a}\n"
"# *5{E$78,$00,$37,$c8,$01,$0a}\n"
"# *6{E$96,$96,$0f,$f0,$01,$28}\n"
"# *7{E$78,$00,$37,$c8,$00,$28}\n"
"# *8{E$78,$00,$37,$c8,$01,$28}\n"
"# *9{E$78,$00,$5f,$a0,$05,$28}\n"
"# *10{E$78,$00,$5f,$a0,$05,$28}\n"
"# *11{E$78,$00,$5f,$a0,$05,$28}\n"
"# *12{E$78,$00,$5f,$a0,$05,$28}\n"
"";
#endif /* USE_SSG_ENV_MACRO */
const uint8_t g_ssg_env[12][6] =
{
    {0xff, 0xff, 0xff, 0xff, 0x00, 0xff},
    {0xff, 0x00, 0x37, 0xc8, 0x00, 0x0a},
    {0xff, 0x00, 0x37, 0xc8, 0x01, 0x0a},
    {0xff, 0x00, 0x37, 0xbe, 0x0a, 0x0a},
    {0x78, 0x00, 0x0f, 0xf0, 0x01, 0x0a},
    {0x78, 0x00, 0x37, 0xc8, 0x01, 0x0a},
    {0x96, 0x96, 0x0f, 0xf0, 0x01, 0x28},
    {0x78, 0x00, 0x37, 0xc8, 0x00, 0x28},
    {0x78, 0x00, 0x37, 0xc8, 0x01, 0x28},
    {0x78, 0x00, 0x5f, 0xa0, 0x05, 0x28},
    {0x78, 0x00, 0x5f, 0xa0, 0x05, 0x28},
    {0x78, 0x00, 0x5f, 0xa0, 0x05, 0x28},
};

int DBG(const char *format, ...)
{
    va_list va;
    int ret = 0;

    va_start(va, format);
    if (g_opt_verbose)
    {
        ret = vprintf(format, va);
    }
    va_end(va);

    return ret;
}

int WARN(const char *format, ...)
{
    va_list va;
    int ret = 0;

    va_start(va, format);
    if (g_opt_verbose || !g_opt_ignore_warning)
    {
        ret = vprintf(format, va);
    }
    va_end(va);

    if (!g_opt_ignore_warning)
    {
        fprintf(stderr, "exit with warning. try -w option to apply workaround.\n");
        exit(1);
    }

    return ret;
}

uint32_t get_word(const uint8_t *p)
{
    return (uint32_t)p[0] + ((uint32_t)p[1] << 8);
}

void detect_clock(const uint32_t len_count[256], uint32_t *clock, uint32_t *deflen)
{
    const struct {
        uint32_t clock;
        uint32_t count;
    } count_table[] = {
        { 192, len_count[192] + len_count[96] + len_count[48] + len_count[24] + len_count[12] + len_count[6] + len_count[3]},
        { 144, len_count[144] + len_count[72] + len_count[36] + len_count[18] + len_count[ 9]},
        { 128, len_count[128] + len_count[64] + len_count[32] + len_count[16] + len_count[ 8] + len_count[4] + len_count[2]},
        { 112, len_count[112] + len_count[56] + len_count[28] + len_count[14] + len_count[7]},
        { 0, 0},
    };
    uint32_t c;
    uint32_t l;

    {
        DBG("----------------\n");
        for (uint32_t i = 0; count_table[i].clock != 0; i++)
        {
            DBG("%3d: %4d\n", count_table[i].clock, count_table[i].count);
        }
        DBG("--------\n");
        for (uint32_t i = 0; i < 20; i++)
        {
            DBG("%3d:", i*10);
            for (uint32_t j = 0; j < 10; j++)
            {
                DBG(" %4d", len_count[i * 10 + j]);
            }
            DBG("\n");
        }
        DBG("----------------\n");
    }

    c = 0;
    for (uint32_t i = 1; count_table[i].clock != 0; i++)
    {
        if (count_table[i].count > count_table[c].count)
        {
            c = i;
        }
    }

    l = 1;
    for (uint32_t i = 1; i < 7; i++)
    {
        if (len_count[count_table[c].clock / (1 << i)] > len_count[count_table[c].clock / l])
        {
            l = 1 << i;
        }
    }

    *clock = count_table[c].clock;
    *deflen = l;
}

void parse_music(
    const uint8_t *data, uint32_t offset, uint8_t *loop_flag, uint8_t *loop_nest,
    uint32_t *end, uint32_t *clock, uint32_t *deflen)
{
    const uint8_t *d = data;
    uint32_t o = offset;
    uint32_t c;
    uint32_t w;
    uint32_t len;
    bool quit = false;
    uint32_t len_count[256];

    memset(len_count, 0, sizeof(len_count));

    while (!quit)
    {
        c = d[o++];
        if (c >= 0xf0)
        {
            switch (c)
            {
            case 0xfb:
            case 0xfc:
                break;
            case 0xf0:
            case 0xf1:
            case 0xf2:
            case 0xf3:
            case 0xf4:
            case 0xf5:
            case 0xfe:
                o++;
                break;
            case 0xf8:
            case 0xfa:
                o += 2;
                break;
            case 0xf7:
                o += 6;
                break;
            case 0xf9:
                o += 6;
                break;
            case 0xf6:
            case 0xfd:
                o++;
                o++;
                w = get_word(&d[o]);
                o += 2;
                loop_nest[o - w]++;
                break;
            case 0xff:
                w = get_word(&d[o]);
                o += 2;
                if (w != 0)
                {
                    loop_flag[o - w] = 1;
                }
                quit = true;
                break;
            }

        }
        else if (c >= 0x80)
        {
            len = c & 0x7f;
#ifdef COMBINE_LONG_REST
            if ((len == 0x6f)							// length
                && (d[o] < 0xf0 && d[o] >= 0x80)		// next command
                )
            {
                len += d[o] & 0x7f;
                o++;
            }
#endif /* COMBINE_LONG_REST */
            len_count[len]++;
        }
        else
        {
            len = c;
#ifdef COMBINE_LONG_TONE
            if ((len == 0x6f)							// length
                && (d[o] & 0x80)						// &
                && (d[o + 1] < 0x80)					// next command
                && ((d[o + 2] & 0x7f) == (d[o] & 0x7f))	// next note
                )
            {
                len += d[o + 1];
                o += 2;
            }
#endif /* COMBINE_LONG_TONE */
            len_count[len]++;
            o++;
        }
    }

    *end = o;

    detect_clock(len_count, clock, deflen);
}

int print_length(FILE *fp, uint32_t clock, uint32_t deflen, uint32_t len)
{
    int ret = 0;

    if (clock % len == 0)
    {
        if (clock / len == deflen)
        {
            /* nothing */
        }
        else
        {
            ret += fprintf(fp, "%u", clock / len);
        }
    }
    else if ((len % 3 == 0) && (clock % (len / 3 * 2) == 0))
    {
        if (clock / (len / 3 * 2) == deflen)
        {
            ret += fprintf(fp, ".");
        }
        else
        {
            ret += fprintf(fp, "%u.", clock / (len / 3 * 2));
        }
    }
    else
    {
        ret += fprintf(fp, "%%%u", len);
    }
    return ret;
}

void convert_music(FILE *fp, uint32_t music, uint32_t ch, const char *chname,
                   const uint8_t *data, uint8_t *loop_flag, uint8_t *loop_nest)
{
    static const char *notestr[16] = {
        "c", "c+", "d", "d+", "e", "f", "f+", "g", "g+", "a", "a+", "b",
        "?", "?", "?", "?"
    };
    const uint8_t *d = data;
    uint32_t mo = get_word(&data[music * 2]) - 0x8000;
    uint32_t o = get_word(&data[mo + ch * 2]) - 0x8000;
    uint32_t end;
    uint32_t c;
    uint32_t prev_oct, oct, note, len;
    uint32_t ssg_mixer;
    uint32_t ssg_noise;
    uint32_t nest;
    uint32_t clock, deflen;
    uint32_t inst;
    uint32_t qcmd, qlen, nqlen;
    bool init = false;
    bool quit = false;
    int ll;

    parse_music(data, o, loop_flag, loop_nest, &end, &clock, &deflen);

    ll = 0;
    prev_oct = 0xff;
    ssg_mixer = 0x02;
    ssg_noise = 0xff;
    inst = 0x00;
    qcmd = 0x10;
    qlen = 0xff;

    while (!quit)
    {
        if (ll <= 0)
        {
            fprintf(fp, "\n");
            ll = 70;
            ll -= fprintf(fp, "%s ", chname);
            if (!init)
            {
                ll -= fprintf(fp, "C%ul%u", clock, deflen);
                init = true;
            }
        }

        if (loop_flag[o] || loop_nest[o])
        {
            ssg_mixer = 0xff;
            ssg_noise = 0xff;
            qlen = 0xff;
        }
        if (loop_flag[o])
        {
            ll -= fprintf(fp, " L ");
        }
        for (nest = 0; nest < loop_nest[o]; nest++)
        {
            ll -= fprintf(fp, "[");
        }
        if (loop_nest[o])
        {
            DBG("{%04x}", o);
        }

        c = d[o++];
        if (c >= 0xf0)
        {
            switch (c)
            {
            case 0xf0:
                c = d[o++];
                inst = c & 0x0f;
#ifdef USE_SSG_ENV_MACRO
                ll -= fprintf(fp, "*%u", (uint32_t)(c & 0x0f));
#else /* USE_SSG_ENV_MACRO */
                ll -= fprintf(fp, "E%d,%d,%d,%d,%d,%d",
                              g_ssg_env[c][0], g_ssg_env[c][1], g_ssg_env[c][2],
                              g_ssg_env[c][3], g_ssg_env[c][4], g_ssg_env[c][5]);
#endif /* USE_SSG_ENV_MACRO */
                c = (c >> 3) & 0x1e;
                if (c == 0)
                {
                    c = 0x80;
                }
                if ((c>>6) != ssg_mixer)
                {
                    ssg_mixer = c >> 6;
                    ll -= fprintf(fp, "P%u", (ssg_mixer & 0x02) ? 1 : 2);
                }
                if ((c&0x1f) != ssg_noise)
                {
                    ssg_noise = c & 0x1f;
                    ll -= fprintf(fp, "w%u", ssg_noise);
                }
                break;
            case 0xf1:
                ll -= fprintf(fp, "v%u", d[o++]);
                break;
            case 0xf2:
                // ll -= fprintf(fp, "q%d", d[o++]);
                qcmd = d[o++];
                break;
            case 0xf3:
                ll -= fprintf(fp, "D%d", (char)d[o++]);
                break;
            case 0xf4:
                c = d[o++];
                if ((c>>6) != ssg_mixer)
                {
                    ssg_mixer = c >> 6;
                    ll -= fprintf(fp, "P%u", (ssg_mixer & 0x02) ? 1 : 2);
                }
                if ((c&0x1f) != ssg_noise)
                {
                    ssg_noise = c & 0x1f;
                    ll -= fprintf(fp, "w%u", ssg_noise);
                }
                break;
            case 0xf5:
                ll -= fprintf(fp, "t%u", (uint32_t)d[o]);
                if (true)
                {
                    if (g_ssg_tempo_prev == UINT32_MAX)
                    {
                        g_ssg_tempo_prev = (uint32_t)d[o];
                    }
                    else if (g_ssg_tempo_prev != (uint32_t)d[o])
                    {
                        g_ssg_tempo_prev = (uint32_t)d[o];
                        g_ssg_tempo_count++;
                    }
                    DBG("{%04x}", o - 2);
                }
                o++;
                break;
            case 0xf6:
            case 0xfd:
                DBG("{%04x:%04x}", o - 1, o + 4 - get_word(&d[o + 2]));
                ll -= fprintf(fp, "]%u", d[o++]);
                ssg_mixer = 0xff;
                ssg_noise = 0xff;
                o++;
                o += 2;
                break;
            case 0xf7:
                ll -= fprintf(fp, "M%u,%u,%d,%u",
                              (uint32_t)d[o], (uint32_t)d[o + 1],
                              -(int8_t)d[o + 2], (uint32_t)d[o + 3]);
                o += 6;
                break;
            case 0xf8:
                if (d[o] == 0x10)
                {
                    ll -= fprintf(fp, "MF%d", (d[o + 1] == 0) ? 0 : 1);
                }
                else
                {
                    /* not supported in MUCOM88 */
                    ll -= fprintf(fp, "??work");
                }
                o += 2;
                break;
            case 0xf9:
                ll -= fprintf(fp, "E%u,%u,%u,%u,%u,%u",
                              (uint32_t)d[o + 0], (uint32_t)d[o + 1], (uint32_t)d[o + 2],
                              (uint32_t)d[o + 3], (uint32_t)d[o + 4], g_ssg_env[inst][5]);
                o += 6;
                break;
            case 0xfa:
                o += 2;
                break;
            case 0xfb:
                /* not compatible with MUCOM88 */
                ll -= fprintf(fp, "(");
                break;
            case 0xfc:
                /* not compatible with MUCOM88 */
                ll -= fprintf(fp, ")");
                break;
            case 0xfe:
                o++;
                break;
            case 0xff:
                quit = true;
                break;
            }

        }
        else if (c >= 0x80)
        {
            len = c & 0x7f;
#ifdef COMBINE_LONG_REST
            if ((len == 0x6f)							// length
                && (d[o] < 0xf0 && d[o] >= 0x80)		// next command
                && (!loop_flag[o] && !loop_nest[o])
                )
            {
                len += d[o] & 0x7f;
                o++;
            }
#endif /* COMBINE_LONG_REST */
            ll -= fprintf(fp, "r");
            ll -= print_length(fp, clock, deflen, len);
        }
        else
        {
            len = c;
#ifdef COMBINE_LONG_TONE
            if ((len == 0x6f)							// length
                && (d[o] & 0x80)						// &
                && (d[o + 1] < 0x80)					// next command
                && ((d[o + 2] & 0x7f) == (d[o] & 0x7f))	// next note
                && (!loop_flag[o + 1] && !loop_nest[o + 1])
                )
            {
                len += d[o+1];
                o += 2;
            }
#endif /* COMBINE_LONG_TONE */
            oct = ((d[o] >> 4) & 0x07) + 1;
            note = d[o] & 0x0f;
            nqlen = len - (len * qcmd / 0x10);
            if (nqlen != qlen)
            {
                ll -= fprintf(fp, "q%d", nqlen);
                qlen = nqlen;
            }
            if (note >= 12)
            {
                oct++;
                note -= 12;
            }
            if (oct != prev_oct)
            {
                if (oct == prev_oct + 1)
                {
                    ll -= fprintf(fp, ">");
                }
                else if (oct == prev_oct - 1)
                {
                    ll -= fprintf(fp, "<");
                }
                else
                {
                    ll -= fprintf(fp, "o%u", oct);
                }
                prev_oct = oct;
            }
            ll -= fprintf(fp, "%s", notestr[note]);
            ll -= print_length(fp, clock, deflen, len);
            if (d[o] & 0x80)
            {
                ll -= fprintf(fp, "&");
            }
            o++;
        }
    }

    fprintf(fp, "\n");

}

void help(void)
{
    fprintf(stderr, "Usage: ds42muc [option(s)] file\n");
    fprintf(stderr, "  -h\t\tprint this help message and exit\n");
    fprintf(stderr, "  -v\t\tverbose (debug info)\n");
    fprintf(stderr, "  -w\t\tapply workaround and ignore warnings\n");
    fprintf(stderr, "  -o FILE\toutput file (default: stdout)\n");
    fprintf(stderr, "  -n BGM\tBGM number\n");
    fprintf(stderr, "  -m VERSION\tMUCOM88 version\n");
    fprintf(stderr, "  -t TITLE\ttitle for tag\n");
    fprintf(stderr, "  -a AUTHOR\tauthor for tag\n");
    fprintf(stderr, "  -c COMPOSER\tcomposer for tag\n");
    fprintf(stderr, "  -d DATE\tdate for tag\n");
    fprintf(stderr, "  -C COMMENT\tcomment for tag\n");
    exit(1);
}

int main(int argc, char *argv[])
{
    int c;
    FILE *fp;
    uint8_t *data = &g_data[0x0000];
    uint32_t music = 0;
    uint32_t ch;
    const char *chname[] = {"D", "E", "F"};
    const char *mucom88ver = NULL;
    const char *title = NULL;
    const char *author = NULL;
    const char *composer = NULL;
    const char *date = NULL;
    const char *comment = NULL;
    const char *outfile = NULL;

    /* command line options */
    while ((c = getopt(argc, argv, "vwo:n:m:t:a:c:d:C:")) != -1)
    {
        switch (c)
        {
        case 'v':
            /* debug option */
            g_opt_verbose = true;
            break;
        case 'w':
            /* apply workaround and ignore warnings */
            g_opt_ignore_warning = true;
            break;
        case 'o':
            outfile = optarg;
            break;
        case 'n':
            music = atoi(optarg);
            break;
        case 'm':
            /* 1.7 is required for using "r%n" */
            mucom88ver = optarg;
            break;
        case 't':
            title = optarg;
            break;
        case 'a':
            author = optarg;
            break;
        case 'c':
            composer = optarg;
            break;
        case 'd':
            date = optarg;
            break;
        case 'C':
            comment = optarg;
            break;
        default:
            help();
            break;
        }
    }

    if (optind != argc - 1)
    {
        help();
    }

    /* read data to buffer */
    fp = fopen(argv[optind], "rb");
    if (fp == NULL)
    {
        fprintf(stderr, "Can't open '%s'\n", argv[optind]);
        exit(1);
    }
    fseek(fp, 8*1024*4, SEEK_SET);
    fread(g_data, sizeof(uint8_t), sizeof(g_data), fp);
    fclose(fp);

    if (outfile != NULL)
    {
        fp = fopen(outfile, "w");
        if (fp == NULL)
        {
            fprintf(stderr, "Can't open '%s'\n", outfile);
            exit(1);
        }
    }
    else
    {
        fp = stdout;
    }

    /* insert tag */
    if (mucom88ver != NULL)
    {
        fprintf(fp, "#mucom88 %s\n", mucom88ver);
    }
    if (title != NULL)
    {
        fprintf(fp, "#title %s\n", title);
    }
    if (author != NULL)
    {
        fprintf(fp, "#author %s\n", author);
    }
    if (composer != NULL)
    {
        fprintf(fp, "#composer %s\n", composer);
    }
    if (date != NULL)
    {
        fprintf(fp, "#date %s\n", date);
    }
    if (comment != NULL)
    {
        fprintf(fp, "#comment %s\n", comment);
    }
    fprintf(fp, "\n");

    /* convert */
    memset(g_loop_flag, 0, sizeof(g_loop_flag));
    memset(g_loop_nest, 0, sizeof(g_loop_nest));

#ifdef USE_SSG_ENV_MACRO
    fprintf(fp, "%s", g_ssg_inst);
#endif /* USE_SSG_ENV_MACRO */

    fprintf(fp, "A t199\n");

    for (ch = 0; ch < 3; ch++)
    {
        convert_music(
            fp,
            music, 
            ch, chname[ch],
            data, g_loop_flag, g_loop_nest);
    }
    fclose(fp);

    if (outfile)
    {
        fclose(fp);
    }

    return 0;
}
