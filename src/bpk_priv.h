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
    uint32_t spare;
    bpk_size size;
} bpk_part;

struct bpk {
    FILE *fd; /**! bpk filedescriptor */
    off_t ppos; /**!< position in the current partition */
    off_t psize; /**!< size of the current partition */
    off_t size; /**!< total size of the bpk file */
    uint8_t flags; /**!< internal flags */
};

#endif

