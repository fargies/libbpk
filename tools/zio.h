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
** zio.h
**
**        Created on: Sep 20, 2013
**   Original Author: Sylvain Fargier <sylvain.fargier@somfy.com>
**
*/

#ifndef __ZIO_H__
#define __ZIO_H__

#include <sys/types.h>
#include "bpk.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct zctrl zctrl;

/**
 * @brief open a file that we will compress when reading.
 */
zctrl *zopen(const char *file, int deflate);

/**
 * @brief close a zctrl opened file.
 */
void zclose(zctrl *ctrl);

/**
 * @brief fill buffer with compressed data.
 * @details to be used with bpk_read_custom.
 */
ssize_t zfill(unsigned char *buf, size_t count, void *attr);

/**
 * @brief write a gzip compressed bpk partition.
 *
 * @param[in] bpk the opened bpk file.
 * @param[in] file the file to read (uncompressed).
 */
int bpk_zwrite_file(bpk *bpk, const char *file);

/**
 * @brief read a gzip compressed partition.
 *
 * @param[in] bpk the opened bpk file.
 * @param[in] size the size to read.
 * @param[in] file the file to write.
 * @return
 *  - 0 on success.
 */
int bpk_zread_file(bpk *bpk, bpk_size size, const char *file);

#if defined(__cplusplus)
}
#endif

#endif
