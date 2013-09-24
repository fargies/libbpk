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
** bpk_api.h
**
**        Created on: Sep 24, 2013
**   Original Author: Sylvain Fargier <sylvain.fargier@somfy.com>
**
*/

#ifndef __BPK_API_H__
#define __BPK_API_H__

#if defined(__cplusplus)
#define BEGIN_DECLS extern "C" {
#define END_DECLS }
#define EXTERN_C extern "C"
#else
#define BEGIN_DECLS
#define END_DECLS
#define EXTERN_C
#endif

#define __GNUC_REQ(maj, min) \
            defined(__GNUC__) && ((__GNUC__ == maj && __GNUC_MINOR__ >= min) || __GNUC__ > maj)

#if __GNUC_REQ(3, 3)
#define HIDDEN __attribute__ ((visibility("hidden")))
#define EXPORT __attribute__ ((visibility("default")))
#else
#define HIDDEN
#define EXPORT
#endif

#endif

