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

typedef struct partition
{
    bpk_type type;
    const char *file;
    uint32_t crc;
    bpk_size size;
    off_t offset;
    STAILQ_ENTRY(partition) parts;
} partition;
STAILQ_HEAD(parthead, partition);

typedef struct hardware
{
    uint32_t id;
    char *name;
    struct parthead parts;
    STAILQ_ENTRY(hardware) hards;
} hardware;
STAILQ_HEAD(hardhead, hardware);

struct bpk_config {
     char *path;
     bpk *bpk;
     struct hardhead hards;
};

static struct bpk_config config;

/**
 * @brief find an existing hardware in the config.
 *
 * @param[in] config the bpk file.
 * @param[in] id the hardware id to look for.
 * @return
 *  - NULL if the hardware couldn't be found.
 *  - an hardware pointer.
 */
static hardware *bpkfs_find_hw_id(struct bpk_config *config, uint32_t id)
{
    hardware *hard;

    STAILQ_FOREACH(hard, &config->hards, hards)
    {
        if (hard->id == id)
            return hard;
    }
    return NULL;
}

static hardware *bpkfs_find_hw_name(
        struct bpk_config *config,
        const char *name)
{
    hardware *hard;

    STAILQ_FOREACH(hard, &config->hards, hards)
    {
        if (strcmp(name, hard->name) == 0)
            return hard;
    }
    return NULL;
}

static partition *bpkfs_find_part(
        struct bpk_config *config,
        const char *hw_id,
        const char *file)
{
    hardware *h;
    partition *p;

    h = bpkfs_find_hw_name(config, hw_id);
    if (h != NULL)
    {
        STAILQ_FOREACH(p, &h->parts, parts)
        {
            if (strcmp(file, p->file) == 0)
                return p;
        }
    }
    return NULL;
}


/**
 * @brief create a new hardware object.
 *
 * @param[in] id the hardware id.
 * @return
 *  - NULL on malloc error.
 */
static hardware *hardware_new(uint32_t id)
{
    hardware *h = malloc(sizeof (hardware));
    if (h == NULL)
        return NULL;

    h->id = id;
    h->name = malloc(17);
    snprintf(h->name, 17, "hw_id_%x", id);
    STAILQ_INIT(&h->parts);
    return h;
}

/**
 * @brief create a new partition object.
 *
 * @param[in] type the partition type.
 * @param[in] size the partition size.
 * @param[in] crc the partition crc.
 * @param[in] offset the partition offset in the file.
 * @return
 *  - NULL on malloc error.
 */
static partition *partition_new(
        bpk_type type,
        bpk_size size,
        uint32_t crc,
        off_t offset)
{
    partition *p = malloc(sizeof (partition));
    if (p == NULL)
        return NULL;

    p->type = type;
    p->crc = crc;

    switch (type)
    {
        case BPK_TYPE_FWV:
            p->file = BPK_FILE_FWV; break;
        case BPK_TYPE_BL:
            p->file = BPK_FILE_BL; break;
        case BPK_TYPE_BLV:
            p->file = BPK_FILE_BLV; break;
        case BPK_TYPE_KER:
            p->file = BPK_FILE_KER; break;
        case BPK_TYPE_RFS:
            p->file = BPK_FILE_RFS; break;
        default:
            p->file = malloc(17);
            snprintf((char *) p->file, 17, "unknown_%.8x", type);
            break;
    }
    p->size = size;
    p->offset = offset;
    return p;
}

static void partition_delete(partition *p)
{
    if (p == NULL)
        return;

    switch (p->type)
    {
        case BPK_TYPE_FWV:
        case BPK_TYPE_BL:
        case BPK_TYPE_BLV:
        case BPK_TYPE_KER:
        case BPK_TYPE_RFS:
            break;
        default:
            free((char *) p->file);
            break;
    }
    free(p);
}

static void hardware_delete(hardware *h)
{
    partition *p;

    if (h == NULL)
        return;

    while (!STAILQ_EMPTY(&h->parts))
    {
        p = STAILQ_FIRST(&h->parts);
        STAILQ_REMOVE_HEAD(&h->parts, parts);
        partition_delete(p);
    }
    free(h->name);
    free(h);
}

static void bpkfs_cleanup(struct bpk_config *conf)
{
    hardware *h;

    while (!STAILQ_EMPTY(&conf->hards))
    {
        h = STAILQ_FIRST(&conf->hards);
        STAILQ_REMOVE_HEAD(&conf->hards, hards);
        hardware_delete(h);
    }
}


static int bpkfs_init(struct bpk_config *conf)
{
    bpk_type type;
    bpk_size size;
    uint32_t crc;
    uint32_t hw_id;
    partition *p;
    hardware *h;

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
    STAILQ_INIT(&conf->hards);

    while ((type = bpk_next(conf->bpk, &size, &crc, &hw_id)) != BPK_TYPE_INVALID)
    {
        p = partition_new(type, size, crc, ftell(conf->bpk->fd));
        if (p == NULL)
        {
            bpkfs_cleanup(conf);
            return -1;
        }

        h = bpkfs_find_hw_id(conf, hw_id);
        if (h == NULL)
        {
            if ((h = hardware_new(hw_id)) == NULL)
            {
                partition_delete(p);
                bpkfs_cleanup(conf);
                return -1;
            }
            STAILQ_INSERT_TAIL(&conf->hards, h, hards);
        }

        STAILQ_INSERT_TAIL(&h->parts, p, parts);
    }

    return 0;
}

/**
 * @brief test wether the given file is a 'sfv' or not.
 *
 * @return
 *  - != O if the file is an sfv file.
 */
static int is_sfv(const char *path)
{
    const char *ext;

    ext = strrchr(path, '.');
    if (ext != NULL && strcmp(ext, SFV_SUFF) == 0)
        return 1;
    else
        return 0;
}

/**
 * @brief split path in two.
 *
 * @details hw_id and part are parts from the same buffer, only hw_id must be
 * freed.
 */
static void splitpath(const char *path, char **hw_id, char **part)
{
    if (path[0] == '/')
        ++path;

    *hw_id = strdup(path);

    *part = strchr(*hw_id, '/');
    if (*part != NULL)
    {
        *((*part)++) = '\0';
        if (*part == '\0')
            *part = NULL;
    }
}

static int bpkfs_getattr(const char *path, struct stat *stbuf)
{
    int res = 0;
    partition *p;
    int sfv;
    char *hw_id, *part;

    splitpath(path, &hw_id, &part);

    if (part == NULL)
    {
        if (*hw_id != '\0' &&
                bpkfs_find_hw_name(&config, hw_id) == NULL)
            res = -ENOENT;
        else
        {
            memset(stbuf, 0, sizeof(struct stat));
            stbuf->st_mode = S_IFDIR | 0755;
            stbuf->st_nlink = 2;
        }
    }
    else
    {
        if ((sfv = is_sfv(part)) != 0)
            part[strlen(part) - SFV_SUFF_LEN] = '\0';

        p = bpkfs_find_part(&config, hw_id, part);
        if (p == NULL)
            res = -ENOENT;
        else
        {
            memset(stbuf, 0, sizeof(struct stat));
            stbuf->st_mode = S_IFREG | 0444;
            stbuf->st_nlink = 1;
            stbuf->st_size = (sfv) ? (strlen(p->file) + SFV_CRC_LEN) :
                (p->size);
        }
    }
    free(hw_id);
    return res;
}


static int bpkfs_readdir(
        const char *path,
        void *buf,
        fuse_fill_dir_t filler,
        off_t offset,
        struct fuse_file_info *fi)
{
    hardware *h;
    partition *p;
    char *sfv;
    char *hw_id, *part;
    int ret = 0;

    (void) offset;
    (void) fi;

    splitpath(path, &hw_id, &part);

    if (part != NULL)
        ret = -ENOENT;
    else if (*hw_id == '\0')
    {
        filler(buf, ".", NULL, 0);
        filler(buf, "..", NULL, 0);

        STAILQ_FOREACH(h, &config.hards, hards)
        {
            filler(buf, h->name, NULL, 0);
        }
    }
    else
    {
        h = bpkfs_find_hw_name(&config, hw_id);
        if (h == NULL)
            ret = -ENOENT;
        else
        {
            filler(buf, ".", NULL, 0);
            filler(buf, "..", NULL, 0);

            STAILQ_FOREACH(p, &h->parts, parts)
            {
                filler(buf, p->file, NULL, 0);

                sfv = malloc(strlen(p->file) + 1 + SFV_SUFF_LEN);
                if (sfv)
                {
                    strcpy(sfv, p->file);
                    strcat(sfv, SFV_SUFF);
                    filler(buf, sfv, NULL, 0);
                    free(sfv);
                }
            }
        }
    }
    free(hw_id);
    return ret;
}

static int bpkfs_open(const char *path, struct fuse_file_info *fi)
{
    int ret = -ENOENT;
    char *hw_id, *part;

    splitpath(path, &hw_id, &part);

    if (part != NULL)
    {
        if (is_sfv(part))
            part[strlen(part) - SFV_SUFF_LEN] = '\0';

        if (bpkfs_find_part(&config, hw_id, part) != NULL)
            ret = 0;
    }
    free(hw_id);

    if ((ret == 0) && (fi->flags & 3) != O_RDONLY)
        return -EACCES;

    return 0;
}

static int dump_file(
        bpk *bpk,
        const partition *part,
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
        const partition *part,
        char *buf,
        size_t size,
        off_t offset)
{
    size_t len;
    char *int_buf;

    if (offset < 0)
        return -EINVAL;

    len = strlen(part->file) + SFV_CRC_LEN + 1;
    if ((size_t) offset >= len)
        return 0;

    int_buf = malloc(len);
    if (!int_buf)
        return -EINVAL;

    snprintf(int_buf, len, "%s %.8X\n", part->file, part->crc);
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
    partition *p;
    int sfv;
    char *hw_id, *part;
    (void) fi;

    splitpath(path, &hw_id, &part);

    if ((sfv = is_sfv(part)) != 0)
        part[strlen(part) - SFV_SUFF_LEN] = '\0';

    p = bpkfs_find_part(&config, hw_id, part);
    free(hw_id);

    if (p != NULL)
    {
        if (sfv)
            return dump_sfv(p, buf, size, offset);
        else
            return dump_file(config.bpk, p, buf, size, offset);
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

