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
** bpk_mount.c
**
**        Created on: Aug 14, 2013
**   Original Author: Sylvain Fargier <sylvain.fargier@somfy.com>
**
*/

#define FUSE_USE_VERSION 26

#pragma GCC diagnostic ignored "-pedantic"
#include <fuse.h>
#pragma GCC diagnostic warning "-pedantic"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "bpk.h"
#include "bpk_priv.h"
#include "bpkfs.h"
#include "compat/queue.h"

#define SFV_SUFF ".sfv"
#define SFV_SUFF_LEN 4

/* ' ' + crc + '\n' */
#define SFV_CRC_LEN (1 + 8 + 1)

struct part
{
    bpk_type type;
    const char *file;
    uint32_t crc;
    bpk_size size;
    off_t offset;
    STAILQ_ENTRY(part) parts;
};
STAILQ_HEAD(parthead, part);

struct bpk_config {
     char *path;
     bpk *bpk;
     struct parthead parts;
};

static struct bpk_config config;

static int bpkfs_init(struct bpk_config *conf)
{
    bpk_type type;
    bpk_size size;
    uint32_t crc;
    struct part *p;

    if (conf == NULL || conf->path == NULL)
        return -1;

    conf->bpk = bpk_open(conf->path, 0);
    if (conf->bpk == NULL)
    {
        fprintf(stderr, "Failed to open bpk file: %s\n", conf->path);
        return -1;
    }
    else if (bpk_check_crc(conf->bpk) != 0)
    {
        bpk_close(conf->bpk);
        fprintf(stderr, "Failed to open bpk file: %s (header corruption)\n",
                conf->path);
        return -1;
    }
    STAILQ_INIT(&conf->parts);

    while ((type = bpk_next(conf->bpk, &size, &crc)) != BPK_TYPE_INVALID)
    {
        p = malloc(sizeof (struct part));
        if (p == NULL)
            return -1;

        p->type = type;
        p->crc = crc;

        switch (type)
        {
            case BPK_TYPE_FWV:
                p->file = BPK_FILE_FWV; break;
            case BPK_TYPE_PBL:
                p->file = BPK_FILE_PBL; break;
            case BPK_TYPE_PBLV:
                p->file = BPK_FILE_PBLV; break;
            case BPK_TYPE_PKER:
                p->file = BPK_FILE_PKER; break;
            case BPK_TYPE_PRFS:
                p->file = BPK_FILE_PRFS; break;
            default:
                free(p);
                continue;
        }
        p->size = size;
        p->offset = ftell(conf->bpk->fd);
        STAILQ_INSERT_TAIL(&conf->parts, p, parts);
    }

    return 0;
}

static void bpkfs_cleanup(struct bpk_config *conf)
{
    struct part *p;

    while (!STAILQ_EMPTY(&conf->parts))
    {
        p = STAILQ_FIRST(&conf->parts);
        STAILQ_REMOVE_HEAD(&conf->parts, parts);
        free(p);
    }
}

static int is_sfv(const char *path)
{
    const char *ext;

    ext = strrchr(path, '.');
    if (ext != NULL && strcmp(ext, SFV_SUFF) == 0)
        return 1;
    else
        return 0;
}

static int bpkfs_getattr(const char *path, struct stat *stbuf)
{
    int res = 0;
    struct part *part;
    int sfv;
    size_t len;

    memset(stbuf, 0, sizeof(struct stat));
    if (strcmp(path, "/") == 0)
    {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
    }
    else
    {
        sfv = is_sfv(path);
        if (sfv)
            len = strlen(path) - 4;
        else
            len = strlen(path);

        res = -ENOENT;
        STAILQ_FOREACH(part, &config.parts, parts)
        {
            if (strncmp(path, part->file, len) == 0)
            {
                stbuf->st_mode = S_IFREG | 0444;
                stbuf->st_nlink = 1;
                if (sfv)
                    stbuf->st_size = strlen(part->file) + SFV_CRC_LEN;
                else
                    stbuf->st_size = part->size;
                res = 0;
                break;
            }
        }
    }
    return res;
}

static int bpkfs_readdir(
        const char *path,
        void *buf,
        fuse_fill_dir_t filler,
        off_t offset,
        struct fuse_file_info *fi)
{
    struct part *part;
    char *sfv;
    (void) offset;
    (void) fi;

    if (strcmp(path, "/") != 0)
        return -ENOENT;

    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    STAILQ_FOREACH(part, &config.parts, parts)
    {
        filler(buf, part->file + 1, NULL, 0);

        sfv = malloc(strlen(part->file + 1) + 1 + SFV_SUFF_LEN);
        if (sfv)
        {
            strcpy(sfv, part->file + 1);
            strcat(sfv, SFV_SUFF);
            filler(buf, sfv, NULL, 0);
            free(sfv);
        }
    }

    return 0;
}

static int bpkfs_open(const char *path, struct fuse_file_info *fi)
{
    struct part *part;
    int ret = -ENOENT;
    size_t len;

    if (is_sfv(path))
        len = strlen(path) - 4;
    else
        len = strlen(path);

    STAILQ_FOREACH(part, &config.parts, parts)
    {
        if (strncmp(path, part->file, len) == 0)
        {
            ret = 0;
            break;
        }
    }
    if (ret != 0)
        return ret;

    if ((fi->flags & 3) != O_RDONLY)
        return -EACCES;

    return 0;
}

static int dump_file(
        bpk *bpk,
        const struct part *part,
        char *buf,
        size_t size,
        off_t offset)
{
    if (offset < 0)
        return -EINVAL;

    if ((bpk_size) offset >= part->size)
        return 0;
    else if ((bpk_size) (offset + size) > part->size)
        size = part->size - offset;

    return pread(fileno(bpk->fd), buf, size, offset +
            part->offset);
}

static int dump_sfv(
        const struct part *part,
        char *buf,
        size_t size,
        off_t offset)
{
    size_t len;
    char *int_buf;

    if (offset < 0)
        return -EINVAL;

    len = strlen(part->file + 1) + SFV_CRC_LEN + 1;
    if ((size_t) offset >= len)
        return 0;

    int_buf = malloc(len);
    if (!int_buf)
        return -EINVAL;

    snprintf(int_buf, len, "%s %.8X\n", part->file + 1, part->crc);
    if (size + offset > --len)
        size = len - offset;
    strncpy(buf, &int_buf[offset], size);

    free(int_buf);

    return size;
}

static int bpkfs_read(
        const char *path,
        char *buf,
        size_t size,
        off_t offset,
        struct fuse_file_info *fi)
{
    struct part *part;
    int sfv;
    size_t len;
    (void) fi;

    sfv = is_sfv(path);
    if (sfv)
        len = strlen(path) - 4;
    else
        len = strlen(path);

    STAILQ_FOREACH(part, &config.parts, parts)
    {
        if (strncmp(path, part->file, len) == 0)
        {
            if (sfv)
                return dump_sfv(part, buf, size, offset);
            else
                return dump_file(config.bpk, part, buf, size, offset);
        }
    }
    return -ENOENT;
}

#define BPK_OPT(t, p, v) { t, offsetof(struct myfs_config, p), v }

static struct fuse_opt bpk_opts[] = {
     FUSE_OPT_END
};

static int bpk_opt_proc(void *data, const char *arg, int key, struct fuse_args *outargs)
{
    (void) data;
    (void) outargs;

    switch (key)
    {
        case FUSE_OPT_KEY_NONOPT:
            if (!config.path)
            {
                config.path = strdup(arg);
                return 0;
            }
            return 1;
    }
    return 1;
}

#pragma GCC diagnostic ignored "-pedantic"
static struct fuse_operations bpk_oper = {
    .getattr    = bpkfs_getattr,
    .readdir    = bpkfs_readdir,
    .open       = bpkfs_open,
    .read       = bpkfs_read,
};
#pragma GCC diagnostic warning "-pedantic"

int main(int argc, char *argv[])
{
    int ret;
    struct fuse_args args;

    args.argc = argc;
    args.argv = argv;
    args.allocated = 0;

    memset(&config, 0, sizeof(config));

    fuse_opt_parse(&args, &config, bpk_opts, bpk_opt_proc);

    if (bpkfs_init(&config) != 0)
        ret = 1;
    else
        ret = fuse_main(args.argc, args.argv, &bpk_oper, NULL);

    bpkfs_cleanup(&config);
    return ret;
}

