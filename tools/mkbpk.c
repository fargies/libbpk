/*
** This document and/or file is SOMFY's property. All information it
** contains is strictly confidential. This document and/or file shall
** not be used, reproduced or passed on in any way, in full or in part
** without SOMFY's prior written approval. All rights reserved.
** Ce document et/ou fichier est la propritye SOMFY. Les informations
** quil contient sont strictement confidentielles. Toute reproduction,
** utilisation, transmission de ce document et/ou fichier, partielle ou
** intégrale, non autorisée préalablement par SOMFY par écrit est
** interdite. Tous droits réservés.
** 
** Copyright © (2009-2012), Somfy SAS. All rights reserved.
** All reproduction, use or distribution of this software, in whole or
** in part, by any means, without Somfy SAS prior written approval, is
** strictly forbidden.
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

static const struct {
    const char *str;
    bpk_type type;
} bpk_types_str[] = {
    { "version", BPK_TYPE_FWV },
    { "bootloader", BPK_TYPE_BL },
    { "bootloader_version", BPK_TYPE_BLV },
    { "kernel", BPK_TYPE_KER },
    { "rootfs", BPK_TYPE_RFS },
};
#define bpk_types_str_size (sizeof (bpk_types_str) / sizeof (bpk_types_str[0]))

static void usage(FILE *out, const char *name)
{
    fprintf(out, "Usage: %s [options] [-c|-x] [-f] file [-p] type:[hw_id:]file ...\n", name);
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
    fputs("  mkbpk -c test.bpk rootfs:root.img kernel:uImage version:version.txt\n", out);
    fputs("  mkbpk -x test.bpk 0xFEETFEET:12:version.txt\n", out);
    fputs("\n", out);
}

struct part
{
    bpk_type type;
    char *file;
    uint32_t hw_id;
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

static int splitargs(const char *arg, char **type, char **hw_id, char **file)
{
    *type = strdup(arg);

    *hw_id = strchr(*type, ':');
    if (hw_id == NULL)
    {
        free(*type);
        return -1;
    }
    *(*hw_id)++ = '\0';

    if ((*file = strchr(*hw_id, ':')) == NULL)
    {
        *file = *hw_id;
        *hw_id = NULL;
    }
    else
        *(*file)++ = '\0';
    return 0;
}

static struct part *create_part(const char *arg)
{
    struct part *p;
    char *type, *hw_id, *file;
    long int l_type, l_hw_id;
    int ret = 0;

    if (splitargs(arg, &type, &hw_id, &file) != 0)
        return NULL;

    p = malloc(sizeof (struct part));
    p->type = get_bpk_type(type);
    p->hw_id = 0;

    if (p->type == BPK_TYPE_INVALID)
    {
        if (sscanf(type, "%li", &l_type) != 1 ||
                l_type > UINT32_MAX)
            ret = -1;
        else
            p->type = l_type;
    }
    if (ret == 0 && hw_id)
    {
        if (sscanf(hw_id, "%li", &l_hw_id) != 1 ||
                l_hw_id > UINT32_MAX)
            ret = -1;
        else
            p->hw_id = l_hw_id;
    }
    if (ret == 0)
        p->file = strdup(file);

    free(type);

    if (ret != 0)
    {
        free(p);
        return NULL;
    }
    else
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

                    if (bpk_find(bpk, p->type, p->hw_id, NULL, NULL) != 0)
                    {
                        fprintf(stderr, "Failed to find part: %s\n",
                                get_bpk_str(p->type));
                        ret = EXIT_FAILURE;
                    }
                    else if (bpk_read_file(bpk, p->file) != 0)
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

                if (bpk_write(bpk, p->type, p->hw_id, p->file) != 0)
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

