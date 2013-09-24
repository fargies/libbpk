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
** zio.c
**
**        Created on: Sep 20, 2013
**   Original Author: Sylvain Fargier <sylvain.fargier@somfy.com>
**
*/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <zlib.h>
#include <time.h>
#include <string.h>

#include "zio.h"

#define CHUNK 2048

struct zctrl
{
    FILE *file;
    z_stream strm;
    int deflate;
    uint8_t in[CHUNK];
};

zctrl *zopen(const char *file, int deflate)
{
    FILE *f = fopen(file, deflate ? "r" : "w");
    zctrl *ctrl;
    int ret;

    if (f == NULL)
        return NULL;

    ctrl = malloc(sizeof (zctrl));
    ctrl->file = f;
    ctrl->strm.zalloc = Z_NULL;
    ctrl->strm.zfree = Z_NULL;
    ctrl->strm.opaque = Z_NULL;
    ctrl->strm.avail_in = 0;
    ctrl->strm.next_in = Z_NULL;
    ctrl->deflate = deflate;

    if (deflate)
        ret = (deflateInit2(&ctrl->strm, Z_BEST_COMPRESSION, Z_DEFLATED,
                    15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK) ? -1 : 0;
    else
        ret = (inflateInit2(&ctrl->strm, 15 + 16) != Z_OK) ? -1 : 0;

    if (ret != 0)
    {
        free(ctrl);
        fclose(f);
        return NULL;
    }
    return ctrl;
}

void zclose(zctrl *ctrl)
{
    fclose(ctrl->file);
    if (ctrl->deflate)
        deflateEnd(&ctrl->strm);
    else
        inflateEnd(&ctrl->strm);
    free(ctrl);
}

/**
 * @brief read data from opened zctrl and return compressed data.
 */
ssize_t zfill(unsigned char *buf, size_t count, void *attr)
{
    zctrl *ctrl = (zctrl *) attr;
    size_t rem = count, readed;
    int ret, eof = 0;

    do
    {
        if (ctrl->strm.avail_in == 0)
        {
            ctrl->strm.avail_in = fread(ctrl->in, 1,
                    CHUNK, ctrl->file);
            if (ferror(ctrl->file))
                return -1;

            if (ctrl->strm.avail_in == 0)
                eof = 1;

            ctrl->strm.next_in = ctrl->in;
        }

        ctrl->strm.avail_out = rem;
        ctrl->strm.next_out = buf;
        ret = deflate(&ctrl->strm, eof ? Z_FINISH : Z_NO_FLUSH);
        if (ret == Z_NEED_DICT ||
                ret == Z_DATA_ERROR ||
                ret == Z_MEM_ERROR)
            return -1;

        readed = rem - ctrl->strm.avail_out;
        buf += readed;
        rem -= readed;
    }
    while (rem && !eof);

    return count - rem;
}

int bpk_zread_file(bpk *bpk, bpk_size size, const char *file)
{
    zctrl *ctrl;
    unsigned char *buff;
    int ret;
    size_t len;

    ctrl = zopen(file, 0);
    if (ctrl == NULL)
        return -1;

    buff = malloc(CHUNK * 4);
    if (buff == NULL)
    {
        zclose(ctrl);
        return -2;
    }

    while (size != 0)
    {
        if (ctrl->strm.avail_in == 0)
        {
            if ((ctrl->strm.avail_in = bpk_read(bpk, ctrl->in, CHUNK)) == 0)
                goto bpk_zread_err;
            ctrl->strm.next_in = ctrl->in;
            size -= ctrl->strm.avail_in;
        }

        do
        {
            ctrl->strm.avail_out = CHUNK * 4;
            ctrl->strm.next_out = buff;
            ret = inflate(&ctrl->strm, (size) ? Z_NO_FLUSH : Z_FINISH);
            if (ret == Z_NEED_DICT ||
                    ret == Z_DATA_ERROR ||
                    ret == Z_MEM_ERROR)
                goto bpk_zread_err;

            len = (CHUNK * 4) - ctrl->strm.avail_out;
            if (fwrite(buff, len, 1, ctrl->file) != 1)
                goto bpk_zread_err;
        }
        while (size && ctrl->strm.avail_in);
    }

bpk_zread_err:
    free(buff);
    zclose(ctrl);
    return (size != 0) ? -1 : 0;
}

