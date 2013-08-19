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
** bpk.h
**
**        Created on: Aug 08, 2013
**   Original Author: Sylvain Fargier <sylvain.fargier@somfy.com>
**
*/

#ifndef __BPK_H__
#define __BPK_H__

#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @defgroup BPK BPK
 * @{
 */

#define BPK_TYPE_PBL 0x50424C00 /* PBL */
#define BPK_TYPE_PBLV 0x50424C56 /* PBLV */
#define BPK_TYPE_PKER 0x504B4552 /* PKER */
#define BPK_TYPE_PRFS 0x50524653 /* PRFS */
#define BPK_TYPE_FWV 0x46575600 /* FWV */
#define BPK_TYPE_INVALID 0xDEADBEEF

typedef struct bpk bpk;

typedef uint32_t bpk_type;
typedef uint64_t bpk_size;

/**
 * @brief create a new bpk package.
 * @param[in] file the file to create.
 * @return
 *  - the newly created bpk file.
 *  - NULL on error
 */
bpk *bpk_create(const char *file);

/**
 * @brief open an existing bpk package.
 * @details when append is true the file is opened in RW mode, appending some
 * new parts is then possible.
 *
 * @param[in] file the file to open.
 * @param[in] append opens the file in RW mode.
 * @return
 *  - the opened bpk file.
 *  - NULL on error.
 */
bpk *bpk_open(const char *file, int append);

/**
 * @brief close a bpk file.
 * @param[in] bpk the file to close.
 */
void bpk_close(bpk *bpk);

/**
 * @brief check a bpk file crc.
 * @param[in] bpk the bpk file.
 * @return
 *  - 0 if the crc is correct.
 *  - -1 otherwise.
 */
int bpk_check_crc(bpk *bpk);

/**
 * @brief compute the file's crc.
 *
 * @param[in] bpk the bpk file.
 * @param[out] file_crc the crc written in the file's header.
 * @return
 *  - the computed crc.
 *  - 0xFFFFFFFF on error.
 */
uint32_t bpk_compute_crc(bpk *bpk, uint32_t *file_crc);

/**
 * @brief write a file in the bpk package.
 * @param[in] bpk the bpk file to edit.
 * @param[in] type the part type.
 * @param[in] file the file the file to store in the bpk.
 * @return
 *  - 0 on success.
 *  - < 0 on failure.
 */
int bpk_write(bpk *bpk, bpk_type type, const char *file);

/**
 * @brief find a bpk partition.
 * @details the read pointer is moved to the found data section.
 * @param[in] bpk the bpk file to seek.
 * @param[in] type the type to look for.
 * @param[out] size the found partition size.
 * @return
 *  - 0 if found.
 *  - < 0 if not found.
 */
int bpk_find(bpk *bpk, bpk_type type, bpk_size *size);

/**
 * @brief get the next bpk partition.
 * @details the read pointer is moved to the next data section.
 * @param[in] bpk the bpk file to seek.
 * @param[out] size the found partition size.
 * @return
 *  - the found partition type.
 *  - BPK_TYPE_INVALID on eof.
 */
bpk_type bpk_next(bpk *bpk, bpk_size *size);

/**
 * @brief move back to the first partition.
 * @param[in] bpk the bpk file to seek.
 */
void bpk_rewind(bpk *bpk);

/**
 * @brief reads some part of a bpk file.
 * @param[in] bpk the bpk to read.
 * @param[out] buf the buffer to fill.
 * @param[in] size the buffer size to read.
 * @return
 *  - the number of bytes read is returned.
 *  - 0 on error.
 */
bpk_size bpk_read(bpk *bpk, void *buf, bpk_size size);

/**
 * @brief saves current bpk partition in a file.
 * @param[in] bpk the bpk to read.
 * @param[in] file the file where to store the data.
 * @return
 *  - 0 on success.
 *  - < 0 on error.
 */
int bpk_read_file(bpk *bpk, const char *file);

/**
 * @}
 */

#if defined(__cplusplus)
}
#endif

#endif

