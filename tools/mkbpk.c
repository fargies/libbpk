/*
** Copyright © (2013-2014), Somfy SAS. All rights reserved.
**
** This library is free software; you can redistribute it and/or
** modify it under the terms of the GNU Lesser General Public
** License as published by the Free Software Foundation; either
** version 2.1 of the License, or (at your option) any later version.
**
** This library is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Lesser General Public License for more details.
**
** You should have received a copy of the GNU Lesser General Public
** License along with this library; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
** MA 02110-1301 USA
**
** mkbpk.c
**
**        Created on: Aug 09, 2013
**   Original Author: Sylvain Fargier <sylvain.fargier@somfy.com>
**
*/

#include <stdio.h>
#include <getopt.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "bpk.h"
#include "compat/queue.h"
#include "zio.h"

static const struct {
    const char *str;
    bpk_type type;
} bpk_types_str[] = {
    { "version", BPK_TYPE_FWV },
    { "bootloader", BPK_TYPE_BL },
    { "bootloader_version", BPK_TYPE_BLV },
    { "kernel", BPK_TYPE_KER },
    { "rootfs", BPK_TYPE_RFS },
    { "description", BPK_TYPE_DEZC },
};
#define bpk_types_str_size (sizeof (bpk_types_str) / sizeof (bpk_types_str[0]))

static void usage(FILE *out, const char *name)
{
    fprintf(out, "Usage: %s [options] [-c|-x] [-f] file [-p] type:[hw_id:][z:]file ...\n", name);
    fputs("\nOptions:\n", out);
    fputs("  -h, --help        Show this help message and exit\n", out);
    fputs("  -f, --file=<f>    Set the file to work on\n", out);
    fputs("  -p, --part=<f>    Partition to create or extract\n", out);
    fputs("  -c, --create      Creation mode\n", out);
    fputs("  -x, --extract     Extraction mode\n", out);
    fputs("  -l, --list        Partition listing mode\n", out);
    fputs("  -t, --list-types  List supported partition types\n", out);
    fputs("  -k, --check       Check a bpk CRC\n", out);
    fputs("\nExamples:\n", out);
    fputs("  mkbpk -c test.bpk rootfs:root.img kernel:uImage version:z:version.txt\n", out);
    fputs("  mkbpk -x test.bpk 0xFEETFEET:12:version.txt\n", out);
    fputs("\n", out);
}

struct part
{
    bpk_type type;
    char *file;
    uint32_t hw_id;
    int comp;
    STAILQ_ENTRY(part) parts;
};
STAILQ_HEAD(parthead, part);

static bpk_type get_bpk_type(const char *type_str)
{
    unsigned int i;

    for (i = 0; i < bpk_types_str_size; ++i)
    {
        if (strcmp(bpk_types_str[i].str, type_str) == 0)
            return bpk_types_str[i].type;
    }
    return BPK_TYPE_INVALID;
}


static const char *get_bpk_str(bpk_type type)
{
    unsigned int i;
    static char unknown_buff[17];

    for (i = 0; i < bpk_types_str_size; ++i)
    {
        if (type == bpk_types_str[i].type)
            return bpk_types_str[i].str;
    }
    snprintf(unknown_buff, 17, "unknown_%.8x", type);
    return unknown_buff;
}

static int parse_uint32(const char *str, uint32_t *ret)
{
    long int i;

    if (sscanf(str, "%li", &i) != 1 ||
            i > (long int) UINT32_MAX)
        return -1;
    else
    {
        *ret = (uint32_t) i;
        return 0;
    }
}

#define MAX_ARGS_PART 4

static struct part *create_part(const char *arg)
{
    char *args[MAX_ARGS_PART];
    char *saveptr, *split = strdup(arg);
    size_t i, args_count;
    int ret = -1;
    struct part *p;

    p = calloc(1, sizeof (struct part));

    for (i = 0; i < MAX_ARGS_PART; ++i)
    {
        args[i] = strtok_r((i == 0) ? split : NULL, ":", &saveptr);
        if (args[i] == NULL)
            break;
    }

    args_count = i;
    if (args_count < 2)
        goto splitargs_err;

    p->type = get_bpk_type(args[0]);
    if (p->type == BPK_TYPE_INVALID &&
            parse_uint32(args[0], &p->type) != 0)
        goto splitargs_err;

    for (i = 1; i < args_count - 1; ++i)
    {
        if (strcmp(args[i], "z") == 0)
            p->comp = 1;
        else if (parse_uint32(args[i], &p->hw_id) != 0)
            goto splitargs_err;
    }

    p->file = strdup(args[args_count - 1]);

    ret = 0;

splitargs_err:
    free(split);
    if (ret != 0)
    {
        free(p);
        p = NULL;
    }

    return p;
}

static void free_part(struct part *p)
{
    if (p != NULL)
    {
        free(p->file);
        free(p);
    }
}

static int write_part(struct bpk *bpk, const struct part *p)
{
    int ret;
    zctrl *ctrl;

    if (p->comp)
    {
        ctrl = zopen(p->file, 1);
        if (ctrl == NULL)
            return -1;
        ret = bpk_write_custom(bpk, p->type, p->hw_id,
                (bpk_fill_func) zfill, ctrl);
        zclose(ctrl);
        return ret;
    }
    else
        return bpk_write(bpk, p->type, p->hw_id, p->file);
}

static int read_part(struct bpk *bpk, bpk_size size, const struct part *p)
{
    if (p->comp)
        return bpk_zread_file(bpk, size, p->file);
    else
        return bpk_read_file(bpk, p->file);

}

int main(int argc, char **argv)
{
    char mode = 0;
    const char *file = NULL;
    struct parthead parts;
    int c;
    struct option long_options[] = {
        { "help", 0, 0, 'h' },
        { "file", 1, 0, 'f' },
        { "part", 1, 0, 'p' },
        { "create", 0, 0, 'c' },
        { "extract", 0, 0, 'x' },
        { "list", 0, 0, 'l' },
        { "list-types", 0, 0, 't' },
        { "check", 0, 0, 'k' },
        { 0, 0, 0, 0 }
    };
    uint32_t crc;
    bpk *bpk;
    bpk_size size;
    bpk_type type;
    uint32_t hw_id;
    int ret;
    int index = 0;

    STAILQ_INIT(&parts);

    while ((c = getopt_long(argc, argv, "-hf:p:cxltk", long_options, &index)) != -1)
    {
        if (c == 1)
            c = (strchr(optarg, ':') != NULL) ? 'p' : 'f';

        switch (c)
        {
            case 'h':
                usage(stdout, argv[0]);
                exit(EXIT_SUCCESS);
            case 'f':
                if (file != NULL)
                {
                    fprintf(stderr, "Too many file arguments\n");
                    exit(EXIT_FAILURE);
                }
                file = optarg;
                break;
            case 'p':
                {
                    struct part *p = create_part(optarg);
                    if (p == NULL)
                    {
                        fprintf(stderr, "Invalid part argument: %s\n", optarg);
                        exit(EXIT_FAILURE);
                    }
                    STAILQ_INSERT_TAIL(&parts, p, parts);
                }
                break;
            case 'x':
            case 'l':
            case 'c':
            case 't':
            case 'k':
                if (mode != 0)
                {
                    fprintf(stderr, "Too many mode arguments\n");
                    exit(EXIT_FAILURE);
                }
                mode = c;
                break;
            case ':':
                usage(stderr, argv[0]);
                exit(EXIT_FAILURE);
            case '?':
            default:
                fprintf(stderr, "%c\n", optopt);
                usage(stderr, argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    ret = 0;

    switch (mode)
    {
        case 't':
            {
                unsigned int i;

                fprintf(stdout, "Supported partitions types: \n");
                for (i = 0; i < bpk_types_str_size; ++i)
                {
                    fputs("  ", stdout);
                    fputs(bpk_types_str[i].str, stdout);
                    fputs("\n", stdout);
                }
            }
            break;
        case 'x':
        case 'l':
        case 'k':
            if (file == NULL)
            {
                fputs("File argument required\n", stderr);
                exit(EXIT_FAILURE);
            }
            bpk = bpk_open(file, 0);
            if (bpk == NULL)
            {
                fprintf(stderr, "Failed to open file: %s\n", file);
                exit(EXIT_FAILURE);
            }

            if (mode == 'x')
            {
                while (!STAILQ_EMPTY(&parts))
                {
                    struct part *p = STAILQ_FIRST(&parts);
                    STAILQ_REMOVE_HEAD(&parts, parts);

                    if (bpk_find(bpk, p->type, p->hw_id, &size, NULL) != 0)
                    {
                        fprintf(stderr, "Failed to find part: %s\n",
                                get_bpk_str(p->type));
                        ret = EXIT_FAILURE;
                    }
                    else if (read_part(bpk, size, p) != 0)
                    {
                        fprintf(stderr, "Failed to read part: %s:%s\n",
                                get_bpk_str(p->type), p->file);
                        ret = EXIT_FAILURE;
                    }
                    free_part(p);
                }
            }
            else if (mode == 'l')
            {
                fputs("Bpk partitions:\n", stdout);
                while ((type = bpk_next(bpk, &size, NULL, &hw_id)) != BPK_TYPE_INVALID)
                    fprintf(stdout, "  %s (size: %lu, hw_id=%.8X)\n", get_bpk_str(type),
                            (unsigned long) size, hw_id);
            }
            else if (mode == 'k')
            {
                ret = 0;
                if (bpk_check_crc(bpk) != 0)
                {
                    fputs("KO: header crc mismatch\n", stdout);
                    ret = EXIT_FAILURE;
                }
                else
                {

                    while ((type = bpk_next(bpk, NULL, &crc, NULL)) != BPK_TYPE_INVALID)
                    {
                        if (bpk_compute_data_crc(bpk) != crc)
                        {
                            fputs("KO: crc mismatch on ", stdout);
                            fputs(get_bpk_str(type), stdout);
                            ret = EXIT_FAILURE;
                        }
                    }
                }
                if (ret == 0)
                {
                    fputs("OK\n", stdout);
                }
            }

            bpk_close(bpk);
            break;
        case 'c':
            if (file == NULL)
            {
                fputs("File argument required\n", stderr);
                exit(EXIT_FAILURE);
            }
            bpk = bpk_create(file);
            if (bpk == NULL)
            {
                fprintf(stderr, "Failed to create file: %s\n", file);
                exit(EXIT_FAILURE);
            }

            while (!STAILQ_EMPTY(&parts))
            {
                struct part *p = STAILQ_FIRST(&parts);
                STAILQ_REMOVE_HEAD(&parts, parts);

                if (write_part(bpk, p) != 0)
                {
                    fprintf(stderr, "Failed to write part: %s:%s\n",
                            get_bpk_str(p->type), p->file);
                    ret = EXIT_FAILURE;
                }
                free_part(p);
            }
            bpk_close(bpk);
            break;
        default:
            usage(stderr, argv[0]);
            exit(EXIT_FAILURE);
    }

    while (!STAILQ_EMPTY(&parts))
    {
        struct part *p = STAILQ_FIRST(&parts);
        STAILQ_REMOVE_HEAD(&parts, parts);
        free_part(p);
    }

    return ret;
}

