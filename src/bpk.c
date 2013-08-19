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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>

#include "bpk.h"
#include "bpk_priv.h"
#include "crc32.h"
#include "compat/endian.h"

static int bpk_init_header(FILE *fd)
{
    bpk_header hdr;
    hdr.magic = htobe32(BPK_MAGIC);
    hdr.version = htobe32(BPK_VERSION);
    hdr.size = htobe64(sizeof (bpk_header));
    hdr.crc = 0;
    hdr.spare = 0;

    if (fwrite(&hdr, sizeof (bpk_header), 1, fd) != 1)
        return -1;
    else
        return 0;
}

static int bpk_check_header(FILE *fd, uint64_t *size)
{
    bpk_header hdr;

    if (fread(&hdr, sizeof (bpk_header), 1, fd) != 1)
        return -1;

    hdr.magic = be32toh(hdr.magic);
    hdr.version = be32toh(hdr.version);
    if (size != NULL)
        *size = be64toh(hdr.size);

    if (hdr.magic != BPK_MAGIC ||
            BPK_MAJOR(hdr.version) > BPK_MAJOR(BPK_VERSION))
        return -1;
    return 0;
}

bpk *bpk_create(const char *file)
{
    bpk *ret;
    FILE *fd = fopen(file, "w+");

    if (fd == NULL)
        return NULL;

    if (bpk_init_header(fd) != 0)
    {
        fclose(fd);
        return NULL;
    }

    ret = malloc(sizeof (bpk));
    if (ret == NULL)
    {
        fclose(fd);
        return NULL;
    }

    ret->fd = fd;
    ret->ppos = ret->psize = 0;
    ret->size = sizeof (bpk_header);
    ret->flags = FLAG_CRC;

    return ret;
}

bpk *bpk_open(const char *file, int append)
{
    bpk *ret;
    FILE *fd;
    uint64_t size = 0;

    if (append)
    {
        fd = fopen(file, "r+");
        if (fd == NULL)
            fd = fopen(file, "w+");

        if (fd != NULL)
        {
            fseek(fd, 0, SEEK_END);
            if ((ftell(fd) < (long) sizeof (bpk_header)) &&
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
    else if (bpk_check_header(fd, &size) != 0)
    {
        fclose(fd);
        return NULL;
    }

    ret = malloc(sizeof (bpk));
    if (ret == NULL)
        return NULL;

    ret->fd = fd;
    ret->ppos = ret->psize = 0;
    ret->flags = (append) ? FLAG_CRC : 0;
    ret->size = size;

    return ret;
}

uint32_t bpk_compute_crc(bpk *bpk, uint32_t *file_crc)
{
    long pos;
    bpk_part part;
    bpk_header hdr;

    pos = ftell(bpk->fd);
    fseek(bpk->fd, 0, SEEK_SET);

    if (fread(&hdr, sizeof (bpk_header), 1, bpk->fd) != 1)
        return 0xFFFFFFFF;

    if (file_crc != NULL)
        *file_crc = be32toh(hdr.crc);
    hdr.crc = 0;

    hdr.crc = crc32((unsigned char const *) &hdr, sizeof (bpk_header),
            BPK_CRC_SEED);
    hdr.size = be64toh(hdr.size) - sizeof (bpk_header);

    while (hdr.size != 0)
    {
        if (hdr.size < sizeof (bpk_part))
            break;

        if (fread(&part, sizeof (bpk_part), 1, bpk->fd) != 1)
            break;

        hdr.size -= sizeof (bpk_part);
        hdr.crc = crc32(&part, sizeof (bpk_part), hdr.crc);

        part.size = be64toh(part.size);
        if (fseek(bpk->fd, part.size, SEEK_CUR) != 0)
            break;
        hdr.size -= part.size;
    }

    fseek(bpk->fd, pos, SEEK_SET);
    return (hdr.size == 0) ? hdr.crc : 0xFFFFFFFF;
}

void bpk_close(bpk *bpk)
{
    uint32_t crc;
    uint64_t size;

    if (bpk == NULL)
        return;

    if (bpk->flags & FLAG_CRC)
    {
        size = htobe64(bpk->size);
        fseek(bpk->fd, offsetof (bpk_header, size), SEEK_SET);
        fwrite(&size, 1, sizeof (uint64_t), bpk->fd);

        crc = htobe32(bpk_compute_crc(bpk, NULL));
        fseek(bpk->fd, offsetof (bpk_header, crc), SEEK_SET);
        fwrite(&crc, 1, sizeof (uint32_t), bpk->fd);
    }
    fflush(bpk->fd);
    fclose(bpk->fd);
    free(bpk);
}

int bpk_check_crc(bpk *bpk)
{
    uint32_t crc;
    uint32_t ref_crc;

    crc = bpk_compute_crc(bpk, &ref_crc);

    return ((crc != 0xFFFFFFFF) && (crc == ref_crc)) ? 0 : -1;
}

int bpk_write(bpk *bpk, bpk_type type, const char *file)
{
    uint32_t crc = BPK_CRC_SEED;
    struct stat st;
    char *buff;
    size_t len;
    FILE *fd_in;
    bpk_part part;

    if (stat(file, &st) != 0)
        return -1;

    part.type = htobe32(type);
    part.spare = 0;
    part.size = htobe64(st.st_size);
    part.crc = 0;

    fd_in = fopen(file, "r");
    if (fd_in == NULL)
        return -1;

    fseek(bpk->fd, bpk->size, SEEK_SET);
    if (fwrite(&part, sizeof (bpk_part), 1, bpk->fd) != 1)
    {
        fclose(fd_in);
        return -2;
    }
    bpk->size += sizeof (bpk_part);

    buff = malloc(2048);
    if (buff == NULL)
    {
        fclose(fd_in);
        return -5;
    }

    while ((len = fread(buff, 1, 2048, fd_in)) > 0)
    {
        crc = crc32(buff, len, crc);

        if (fwrite(buff, len, 1, bpk->fd) != 1)
        {
            fclose(fd_in);
            free(buff);
            return -3;
        }
        bpk->size += len;
    }
    free(buff);
    if (ferror(fd_in) != 0)
    {
        fclose(fd_in);
        return -4;
    }
    fclose(fd_in);

    crc = htobe32(crc);
    fseek(bpk->fd, - st.st_size - sizeof (bpk_part) + offsetof(bpk_part, crc),
            SEEK_CUR);
    fwrite(&crc, sizeof (uint32_t), 1, bpk->fd);
    fseek(bpk->fd, bpk->size, SEEK_SET);

    bpk->ppos = bpk->psize = 0;
    return 0;
}

static int bpk_read_part(bpk *bpk, bpk_part *part)
{
    if (ftell(bpk->fd) >= bpk->size)
        return -1;

    if (fread(part, sizeof (bpk_part), 1, bpk->fd) != 1)
        return -1;

    part->type = be32toh(part->type);
    part->size = be64toh(part->size);
    part->crc = be32toh(part->crc);
    return 0;
}

int bpk_find(
        bpk *bpk, bpk_type type,
        bpk_size *size,
        uint32_t *crc)
{
    bpk_part part;

    bpk_rewind(bpk);

    while (bpk_read_part(bpk, &part) == 0)
    {
        if (part.type == type)
        {
            if (size != NULL)
                *size = part.size;
            if (crc != NULL)
                *crc = part.crc;
            bpk->ppos = 0;
            bpk->psize = part.size;
            return 0;
        }
        fseek(bpk->fd, part.size, SEEK_CUR);
    }
    return -1;
}

bpk_type bpk_next(
        bpk *bpk,
        bpk_size *size,
        uint32_t *crc)
{
    bpk_part part;

    if (bpk->psize != 0)
        fseek(bpk->fd, bpk->psize - bpk->ppos, SEEK_CUR);

    if (bpk_read_part(bpk, &part) == 0)
    {
        if (size != NULL)
            *size = part.size;
        if (crc != NULL)
            *crc = part.crc;
        bpk->ppos = 0;
        bpk->psize = part.size;
        return part.type;
    }
    return BPK_TYPE_INVALID;
}

void bpk_rewind(bpk *bpk)
{
    fseek(bpk->fd, sizeof (bpk_header), SEEK_SET);
    bpk->ppos = bpk->psize = 0;
}

uint32_t bpk_compute_data_crc(bpk *bpk)
{
    bpk_size size;
    ssize_t len;
    char *buff;
    uint32_t crc = BPK_CRC_SEED;

    buff = malloc(2048);
    if (buff == NULL)
        return 0xFFFFFFFF;

    if (bpk->ppos != 0)
        fseek(bpk->fd, -bpk->ppos, SEEK_CUR);

    for (size = bpk->psize; size != 0; )
    {
        len = (size > 2048) ? 2048 : size;

        len = fread(buff, 1, len, bpk->fd);
        if (len <= 0)
            break;

        size -= len;
        crc = crc32(buff, len, crc);
    }
    free(buff);

    fseek(bpk->fd, bpk->ppos - bpk->psize + size, SEEK_CUR);
    return (size == 0) ? crc : 0xFFFFFFFF;
}

bpk_size bpk_read(bpk *bpk, void *buf, bpk_size size)
{
    if (size > (bpk_size) (bpk->psize - bpk->ppos))
        size = bpk->psize - bpk->ppos;

    if (size <= 0)
        return 0;

    size = fread(buf, 1, size, bpk->fd);
    bpk->ppos += size;
    return size;
}

int bpk_read_file(bpk *bpk, const char *file)
{
    FILE *fd_out;
    char *buff;
    size_t len;
    bpk_size size = bpk->psize - bpk->ppos;

    fd_out = fopen(file, "w");
    if (fd_out == NULL)
        return -1;

    buff = malloc(2048);
    if (buff == NULL)
    {
        fclose(fd_out);
        return -2;
    }

    while (size != 0)
    {
        len = (size > 2048) ? 2048 : size;

        len = fread(buff, 1, len, bpk->fd);
        if (len <= 0)
            break;

        size -= len;
        bpk->ppos += len;

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
    bpk->ppos = bpk->psize = 0;
    return 0;
}

