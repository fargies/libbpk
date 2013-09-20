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
** bpk_priv.h
**
**        Created on: Aug 08, 2013
**   Original Author: Sylvain Fargier <sylvain.fargier@somfy.com>
**
*/

#ifndef __BPK_PRIV_H__
#define __BPK_PRIV_H__

#include <stdint.h>
#include <stdio.h>

#define BPK_MAJOR(ver) (ver & 0xFFFF0000)
#define BPK_VERSION 0x00010000 /* 1.0 */

#define BPK_MAGIC 0x534F4659 /* SOFY */

#define FLAG_CRC 0x01 /* compute crc and len when closing the file */

#define BPK_CRC_SEED 0x0U

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t version;
    uint64_t size;
    uint32_t crc;
    uint64_t spare;
} bpk_header;

typedef struct __attribute__((packed)) {
    bpk_type type;
    bpk_size size;
    uint32_t crc;
    uint32_t hw_id;
    uint32_t spare;
} bpk_part;

struct bpk {
    FILE *fd; /**! bpk filedescriptor */
    off_t ppos; /**!< position in the current partition */
    off_t psize; /**!< size of the current partition */
    off_t size; /**!< total size of the bpk file */
    uint8_t flags; /**!< internal flags */
};

#endif

