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
** bpk.h
**
**        Created on: Aug 08, 2013
**   Original Author: Sylvain Fargier <sylvain.fargier@somfy.com>
**
*/

#ifndef __BPK_H__
#define __BPK_H__

#include <stdint.h>
#include <sys/types.h>

#include "bpk_api.h"

BEGIN_DECLS

/**
 * @defgroup BPK BPK
 * @{
 */

#define BPK_TYPE_BL 0x50424C00 /* PBL */
#define BPK_TYPE_BLV 0x50424C56 /* PBLV */
#define BPK_TYPE_KER 0x504B4552 /* PKER */
#define BPK_TYPE_RFS 0x50524653 /* PRFS */
#define BPK_TYPE_FWV 0x46575600 /* FWV */
#define BPK_TYPE_DEZC 0x44455A43 /* DEZC */
#define BPK_TYPE_INVALID 0xDEADBEEF

typedef struct bpk bpk;

typedef uint32_t bpk_type;
typedef uint64_t bpk_size;

/**
 * @brief create a new bpk package.
 * @param[in] file the file to create.
 * @return
 *  - the newly created bpk file.
 *  - NULL on error (setting errno).
 */
EXPORT bpk *bpk_create(const char *file);

/**
 * @brief open an existing bpk package.
 * @details when append is true the file is opened in RW mode, appending some
 * new parts is then possible.
 *
 * @param[in] file the file to open.
 * @param[in] append opens the file in RW mode.
 * @return
 *  - the opened bpk file.
 *  - NULL on error (setting errno).
 */
EXPORT bpk *bpk_open(const char *file, int append);

/**
 * @brief close a bpk file.
 * @param[in] bpk the file to close.
 */
EXPORT void bpk_close(bpk *bpk);

/**
 * @brief check a bpk file crc.
 * @details this crc only covers the headers.
 *
 * @param[in] bpk the bpk file.
 * @return
 *  - 0 if the crc is correct.
 *  - -1 otherwise.
 */
EXPORT int bpk_check_crc(bpk *bpk);

/**
 * @brief compute the file's crc.
 *
 * @param[in] bpk the bpk file.
 * @param[out] file_crc the crc written in the file's header.
 * @return
 *  - the computed crc.
 *  - 0xFFFFFFFF on error.
 */
EXPORT uint32_t bpk_compute_crc(bpk *bpk, uint32_t *file_crc);

/**
 * @brief write a file in the bpk package.
 * @param[in] bpk the bpk file to edit.
 * @param[in] type the part type.
 * @param[in] hw_id the associated hardware id.
 * @param[in] file the file the file to store in the bpk.
 * @return
 *  - 0 on success.
 *  - < 0 on failure (setting errno).
 */
EXPORT int bpk_write(
        bpk *bpk,
        bpk_type type,
        uint32_t hw_id,
        const char *file);

/**
 * @brief bpk reading function.
 *
 * @return
 *  - 0 on EOF.
 *  - <0 on error.
 */
typedef ssize_t (*bpk_fill_func)(void *buf, size_t count, void *attr);

/**
 * @brief write a file using a custom reading func.
 * @param[in] bpk the bpk file to edit.
 * @param[in] type the part type.
 * @param[in] hw_id the associated hardware id.
 * @param[in] func file reading function.
 * @param[in] func_arg file reading function argument.
 * @return
 *  - 0 on success.
 *  - < 0 on failure (setting errno).
 */
EXPORT int bpk_write_custom(
        bpk *bpk,
        bpk_type type,
        uint32_t hw_id,
        bpk_fill_func func,
        void *func_arg);

/**
 * @brief find a bpk partition.
 * @details the read pointer is moved to the found data section.
 * @param[in] bpk the bpk file to seek.
 * @param[in] type the type to look for.
 * @param[in] hw_id the hardware id to seek.
 * @param[out] size the found partition size.
 * @param[out] crc the found partition crc.
 * @return
 *  - 0 if found.
 *  - < 0 if not found.
 */
EXPORT int bpk_find(
        bpk *bpk,
        bpk_type type,
        uint32_t hw_id,
        bpk_size *size,
        uint32_t *crc);

/**
 * @brief get the next bpk partition.
 * @details the read pointer is moved to the next data section.
 * @param[in] bpk the bpk file to seek.
 * @param[out] size the found partition size.
 * @param[out] crc the found partition crc.
 * @param[out] hw_id the found hardware id.
 * @return
 *  - the found partition type.
 *  - BPK_TYPE_INVALID on eof.
 */
EXPORT bpk_type bpk_next(
        bpk *bpk,
        bpk_size *size,
        uint32_t *crc,
        uint32_t *hw_id);

/**
 * @brief compute current partition data crc.
 * @param[in] bpk the bpk file.
 * @return
 *  - the computed crc.
 *  - 0xFFFFFFFF on error.
 */
EXPORT uint32_t bpk_compute_data_crc(bpk *bpk);

/**
 * @brief move back to the first partition.
 * @param[in] bpk the bpk file to seek.
 */
EXPORT void bpk_rewind(bpk *bpk);

/**
 * @brief reads some part of a bpk file.
 * @param[in] bpk the bpk to read.
 * @param[out] buf the buffer to fill.
 * @param[in] size the buffer size to read.
 * @return
 *  - the number of bytes read is returned.
 *  - 0 on error (setting errno).
 */
EXPORT bpk_size bpk_read(bpk *bpk, void *buf, bpk_size size);

/**
 * @brief saves current bpk partition in a file.
 * @param[in] bpk the bpk to read.
 * @param[in] file the file where to store the data.
 * @return
 *  - 0 on success.
 *  - < 0 on error (setting errno).
 */
EXPORT int bpk_read_file(bpk *bpk, const char *file);

/**
 * @}
 */

END_DECLS

#endif

