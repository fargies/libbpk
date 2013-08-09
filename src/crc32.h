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
** crc32.h
**
**        Created on: Aug 08, 2013
**   Original Author: Sylvain Fargier <sylvain.fargier@somfy.com>
**
*/

#ifndef __CRC32_H__
#define __CRC32_H__

#include <stdint.h>

uint32_t crc32(const void *data, size_t len, uint32_t seed); 

#endif

