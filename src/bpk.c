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
#include <errno.h>

#include "bpk.h"
#include "bpk_priv.h"
#include "crc32.h"
#include "compat/endian.h"

/**
 * @brief initialize a bpk header.
 * @param[in] fd the bpk filedescriptor.
 * @return
 *  0 on success.
 *  -1 on error (errno set accordingly).
 */
static int bpk_init_header(FILE *fd)
{
    bpk_header hdr;
    hdr.magic = htobe32(BPK_MAGIC);
    hdr.version = htobe32(BPK_VERSION);
    hdr.size = htobe64(sizeof (bpk_header));
    hdr.crc = 0;
    hdr.spare = 0;

    if (fwrite(&hdr, sizeof (bpk_header), 1, fd) != 1)
    {
        errno = EIO;
        return -1;
    }
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
                errno = EIO;
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
        errno = EILSEQ;
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

    hdr.crc = bpk_crc32((unsigned char const *) &hdr, sizeof (bpk_header),
            BPK_CRC_SEED);
    hdr.size = be64toh(hdr.size) - sizeof (bpk_header);

    while (hdr.size != 0)
    {
        if (hdr.size < sizeof (bpk_part))
            break;

        if (fread(&part, sizeof (bpk_part), 1, bpk->fd) != 1)
            break;

        hdr.size -= sizeof (bpk_part);
        hdr.crc = bpk_crc32(&part, sizeof (bpk_part), hdr.crc);

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

int bpk_write_custom(
        bpk *bpk,
        bpk_type type,
        uint32_t hw_id,
        bpk_fill_func func,
        void *func_arg)
{
    char *buff;
    ssize_t len;
    bpk_part part;

    part.type = htobe32(type);
    part.hw_id = htobe32(hw_id);
    part.spare = 0;
    part.size = 0;
    part.crc = BPK_CRC_SEED;

    fseek(bpk->fd, bpk->size, SEEK_SET);
    if (fwrite(&part, sizeof (bpk_part), 1, bpk->fd) != 1)
    {
        errno = EIO;
        return -2;
    }
    bpk->size += sizeof (bpk_part);

    buff = malloc(2048);
    if (buff == NULL)
        return -5;

    while ((len = func(buff, 2048, func_arg)) > 0)
    {
        part.crc = bpk_crc32(buff, len, part.crc);

        if (fwrite(buff, len, 1, bpk->fd) != 1)
        {
            free(buff);
            errno = EIO;
            return -3;
        }
        part.size += len;
        bpk->size += len;
    }
    free(buff);
    if (len < 0)
    {
        errno = EIO;
        return -4;
    }

    fseek(bpk->fd, - part.size - sizeof (bpk_part) + offsetof(bpk_part, size),
            SEEK_CUR);
    part.size = htobe64(part.size);
    part.crc = htobe32(part.crc);

    fwrite(&part.size, sizeof (bpk_size), 1, bpk->fd);
    fwrite(&part.crc, sizeof (uint32_t), 1, bpk->fd);
    fseek(bpk->fd, bpk->size, SEEK_SET);

    bpk->ppos = bpk->psize = 0;
    return 0;
}

int bpk_write(
        bpk *bpk,
        bpk_type type,
        uint32_t hw_id,
        const char *file)
{
    char *buff;
    size_t len;
    FILE *fd_in;
    bpk_part part;

    part.type = htobe32(type);
    part.hw_id = htobe32(hw_id);
    part.spare = 0;
    part.size = 0;
    part.crc = BPK_CRC_SEED;

    fd_in = fopen(file, "r");
    if (fd_in == NULL)
        return -1;

    fseek(bpk->fd, bpk->size, SEEK_SET);
    if (fwrite(&part, sizeof (bpk_part), 1, bpk->fd) != 1)
    {
        fclose(fd_in);
        errno = EIO;
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
        part.crc = bpk_crc32(buff, len, part.crc);

        if (fwrite(buff, len, 1, bpk->fd) != 1)
        {
            fclose(fd_in);
            free(buff);
            errno = EIO;
            return -3;
        }
        part.size += len;
        bpk->size += len;
    }
    free(buff);
    if (ferror(fd_in) != 0)
    {
        fclose(fd_in);
        errno = EIO;
        return -4;
    }
    fclose(fd_in);

    fseek(bpk->fd, - part.size - sizeof (bpk_part) + offsetof(bpk_part, size),
            SEEK_CUR);
    part.size = htobe64(part.size);
    part.crc = htobe32(part.crc);

    fwrite(&part.size, sizeof (bpk_size), 1, bpk->fd);
    fwrite(&part.crc, sizeof (uint32_t), 1, bpk->fd);
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
    part->hw_id = be32toh(part->hw_id);
    return 0;
}

int bpk_find(
        bpk *bpk,
        bpk_type type,
        uint32_t hw_id,
        bpk_size *size,
        uint32_t *crc)
{
    bpk_part part;

    bpk_rewind(bpk);

    while (bpk_read_part(bpk, &part) == 0)
    {
        if (part.type == type && part.hw_id == hw_id)
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
        uint32_t *crc,
        uint32_t *hw_id)
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
        if (hw_id != NULL)
            *hw_id = part.hw_id;
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
        crc = bpk_crc32(buff, len, crc);
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
    if (ferror(bpk->fd) != 0)
        errno = EIO;

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
            errno = EIO;
            return -3;
        }
    }
    free(buff);
    if (ferror(fd_out) != 0)
    {
        fclose(fd_out);
        errno = EIO;
        return -4;
    }
    fclose(fd_out);
    bpk->ppos = bpk->psize = 0;
    return 0;
}

