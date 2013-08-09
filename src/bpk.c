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
** bpk.c
**
**        Created on: Aug 08, 2013
**   Original Author: Sylvain Fargier <sylvain.fargier@somfy.com>
**
*/

#include <stddef.h>
#include <endian.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>

#include "bpk.h"
#include "bpk_priv.h"
#include "crc32.h"

static int bpk_init_header(FILE *fd)
{
    bpk_header hdr;
    hdr.magic = htobe32(BPK_MAGIC);
    hdr.version = htobe32(BPK_VERSION);
    hdr.crc = 0;
    hdr.spare = 0;

    if (fwrite(&hdr, sizeof (bpk_header), 1, fd) != 1)
        return -1;
    else
        return 0;
}

static int bpk_check_header(FILE *fd)
{
    bpk_header hdr;

    if (fread(&hdr, sizeof (bpk_header), 1, fd) != 1)
        return -1;

    hdr.magic = be32toh(hdr.magic);
    hdr.version = be32toh(hdr.version);

    if (hdr.magic != BPK_MAGIC ||
            BPK_MAJOR(hdr.version) > BPK_MAJOR(BPK_VERSION))
        return -1;
    return 0;
}

bpk *bpk_create(const char *file)
{
    bpk *ret;
    FILE *fd = fopen(file, "w");

    if (fd == NULL)
        return NULL;

    ret = malloc(sizeof (bpk));

    ret->fd = fd;
    ret->pos = ret->size = 0;
    ret->flags = FLAG_CRC;

    if (bpk_init_header(fd) != 0)
    {
        fclose(fd);
        free(ret);
        return NULL;
    }

    return ret;
}

bpk *bpk_open(const char *file, int append)
{
    bpk *ret;
    FILE *fd;

    if (append)
    {
        fd = fopen(file, "r+");
        if (fd == NULL)
            fd = fopen(file, "w+");

        if (fd != NULL)
        {
            fseek(fd, 0, SEEK_END);
            if ((ftell(fd) < sizeof (bpk_header)) &&
                    (bpk_init_header(fd) != 0))
            {
                fclose(fd);
                fd = NULL;
            }
            else
                fseek(fd, 0, SEEK_SET);
        }
    }
    else
        fd = fopen(file, "r");

    if (fd == NULL)
        return NULL;
    else if (bpk_check_header(fd) != 0)
    {
        fclose(fd);
        return NULL;
    }

    ret = malloc(sizeof (bpk));
    ret->fd = fd;
    ret->pos = ret->size = 0;
    ret->flags = (append) ? FLAG_CRC : 0;

    return ret;
}

static uint32_t bpk_compute_crc(bpk *bpk)
{
    long pos = ftell(bpk->fd);
    fseek(bpk->fd, 0, SEEK_SET);

    char *buff = malloc(2048);
    ssize_t len;
    uint32_t crc;

    bpk_header hdr;
    len = fread(&hdr, 1, sizeof (bpk_header), bpk->fd);
    hdr.crc = 0;

    crc = crc32((unsigned char const *) &hdr, len, 0);

    while ((len = fread(buff, 1, 2048, bpk->fd)) > 0)
    {
        crc = crc32(buff, len, crc);
    }
    free(buff);

    fseek(bpk->fd, pos, SEEK_SET);
    return crc;
}

void bpk_close(bpk *bpk)
{
    if (bpk == NULL)
        return;

    if (bpk->flags & FLAG_CRC)
    {
        uint32_t crc = htobe32(bpk_compute_crc(bpk));

        fseek(bpk->fd, offsetof (bpk_header, crc), SEEK_SET);
        fwrite(&crc, 1, sizeof (uint32_t), bpk->fd);
    }
    fflush(bpk->fd);
    fclose(bpk->fd);
    free(bpk);
}

int bpk_check_crc(bpk *bpk)
{
    long pos = ftell(bpk->fd);
    fseek(bpk->fd, 0, SEEK_SET);

    char *buff = malloc(2048);
    size_t len;
    uint32_t crc;
    uint32_t ref_crc;

    bpk_header hdr;
    len = fread(&hdr, 1, sizeof (bpk_header), bpk->fd);
    ref_crc = be32toh(hdr.crc);
    hdr.crc = 0;

    crc = crc32((void *) &hdr, sizeof (bpk_header), 0);

    while ((len = fread(buff, 1, 2048, bpk->fd)) > 0)
    {
        crc = crc32(buff, len, crc);
    }
    free(buff);

    fseek(bpk->fd, pos, SEEK_SET);
    return (crc == ref_crc) ? 0 : -1;
}

int bpk_write(bpk *bpk, bpk_type type, const char *file)
{
    struct stat st;

    if (stat(file, &st) != 0)
        return -1;

    bpk_part part;
    part.type = htobe32(type);
    part.spare = 0;
    part.size = htobe64(st.st_size);

    FILE *fd_in = fopen(file, "r");
    if (fd_in == NULL)
        return -1;

    if (fwrite(&part, sizeof (bpk_part), 1, bpk->fd) != 1)
    {
        fclose(fd_in);
        return -2;
    }

    char *buff = malloc(2048);
    size_t len;

    while ((len = fread(buff, 1, 2048, fd_in)) > 0)
    {
        if (fwrite(buff, len, 1, bpk->fd) != 1)
        {
            fclose(fd_in);
            free(buff);
            return -3;
        }
    }
    free(buff);
    if (ferror(fd_in) != 0)
    {
        fclose(fd_in);
        return -4;
    }
    fclose(fd_in);
    bpk->pos = bpk->size = 0;
    return 0;
}

static int bpk_read_part(bpk *bpk, bpk_part *part)
{
    if (fread(part, sizeof (bpk_part), 1, bpk->fd) != 1)
        return -1;

    part->type = be32toh(part->type);
    part->size = be64toh(part->size);
    return 0;
}

int bpk_find(bpk *bpk, bpk_type type, bpk_size *size)
{
    bpk_part part;

    fseek(bpk->fd, sizeof (bpk_header), SEEK_SET);
    bpk->pos = bpk->size = 0;

    while (bpk_read_part(bpk, &part) == 0)
    {
        if (part.type == type)
        {
            if (size != NULL)
                *size = part.size;
            bpk->pos = 0;
            bpk->size = part.size;
            return 0;
        }
        fseek(bpk->fd, part.size, SEEK_CUR);
    }
    return -1;
}

bpk_type bpk_next(bpk *bpk, bpk_size *size)
{
    bpk_part part;

    if (bpk->size != 0)
        fseek(bpk->fd, bpk->size - bpk->pos, SEEK_CUR);

    if (bpk_read_part(bpk, &part) == 0)
    {
        if (size != NULL)
            *size = part.size;
        bpk->pos = 0;
        bpk->size = part.size;
        return part.type;
    }
    return BPK_TYPE_INVALID;
}

bpk_size bpk_read(bpk *bpk, void *buf, bpk_size size)
{
    if (size > (bpk->size - bpk->pos))
        size = bpk->size - bpk->pos;

    if (size <= 0)
        return 0;

    size = fread(buf, 1, size, bpk->fd);
    bpk->pos += size;
    return size;
}

int bpk_read_file(bpk *bpk, const char *file)
{
    FILE *fd_out = fopen(file, "w");
    if (fd_out == NULL)
        return -1;

    char *buff = malloc(2048);
    size_t len;
    bpk_size size = bpk->size - bpk->pos;

    while (size != 0)
    {
        len = (size > 2048) ? 2048 : size;

        len = fread(buff, 1, len, bpk->fd);
        if (len <= 0)
            break;

        size -= len;
        bpk->pos += len;

        if (fwrite(buff, len, 1, fd_out) != 1)
        {
            fclose(fd_out);
            free(buff);
            return -3;
        }
    }
    free(buff);
    if (ferror(fd_out) != 0)
    {
        fclose(fd_out);
        return -4;
    }
    fclose(fd_out);
    bpk->pos = bpk->size = 0;
    return 0;
}

