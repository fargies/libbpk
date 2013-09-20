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
** test_crc.cpp
**
**        Created on: Aug 19, 2013
**   Original Author: Sylvain Fargier <sylvain.fargier@somfy.com>
**
*/

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "bpk.h"
#include "bpk_priv.h"
#include "test_helpers.hpp"

#define SZ_1K (1024)
#define SZ_512 (512)

class crcTest : public CppUnit::TestFixture
{
    CPPUNIT_TEST_SUITE(crcTest);
    CPPUNIT_TEST(simple);
    CPPUNIT_TEST(data_crc);
    CPPUNIT_TEST_SUITE_END();

public:
    void setUp()
    {
        m_bpk = NULL;

        m_file = strdup("/tmp/testbpkXXXXXX");
        mktemp(m_file);

        m_data = strdup("/tmp/testbpkdataXXXXXX");
        mktemp(m_data);
        int fd = creat(m_data, S_IRWXU);
        CPPUNIT_ASSERT(fd >= 0);
        ftruncate(fd, SZ_1K * 2);
        close(fd);

        m_sfv = strdup("/tmp/sfvXXXXXX");
    }

    void tearDown()
    {
        unlink(m_data);
        unlink(m_file);
        free(m_data);
        free(m_file);

        unlink(m_sfv);
        free(m_sfv);

        if (m_bpk)
            bpk_close(m_bpk);
    }

protected:
    char *m_file;
    char *m_data;
    char *m_sfv;
    bpk *m_bpk;

    void create()
    {
        m_bpk = bpk_create(m_file);
        CPPUNIT_ASSERT(m_bpk);

        CPPUNIT_ASSERT_EQUAL(0,
                bpk_write(m_bpk, BPK_TYPE_BL, 0, m_data));
        bpk_close(m_bpk);
        m_bpk = NULL;
    }

    void simple()
    {
        create();

        m_bpk = bpk_open(m_file, 0);

        uint32_t file_crc;
        uint32_t crc = bpk_compute_crc(m_bpk, &file_crc);
        CPPUNIT_ASSERT_EQUAL(crc, file_crc);

        bpk_close(m_bpk);
        m_bpk = NULL;

        /* uncomment to display cksfv crc */
        /*
        truncate(m_file, sizeof (bpk_header) + sizeof (bpk_part));
        int fd = open(m_file, O_RDWR);
        CPPUNIT_ASSERT(fd >= 0);
        lseek(fd, offsetof (bpk_header, crc), SEEK_SET);
        file_crc = 0;
        write(fd, &file_crc, sizeof (file_crc));
        close(fd);


        fd = mkstemp(m_sfv);
        CPPUNIT_ASSERT(fd >= 0);
        dprintf(fd, "%s %.8x\n", m_file, crc);
        close(fd);

        const char *argv[] = {
            "/usr/bin/cksfv",
            m_file,
            "-f",
            m_sfv,
            NULL
        };
        CPPUNIT_ASSERT_EQUAL(0, spawn(argv, NULL));
        */
        CPPUNIT_ASSERT_EQUAL((uint32_t) 0x93806d14, crc);

    }

    void data_crc()
    {
        uint32_t crc;
        uint32_t crc2;
        create();

        m_bpk = bpk_open(m_file, 0);

        CPPUNIT_ASSERT(bpk_find(m_bpk, BPK_TYPE_BL, 0, NULL, &crc) == 0);
        bpk_rewind(m_bpk);
        CPPUNIT_ASSERT(bpk_next(m_bpk, NULL, &crc2, NULL) != BPK_TYPE_INVALID);

        CPPUNIT_ASSERT_EQUAL(crc, crc2);
        CPPUNIT_ASSERT_EQUAL(crc, bpk_compute_data_crc(m_bpk));

        bpk_close(m_bpk);
        m_bpk = NULL;

        /* uncomment to display cksfv crc */
        /*
        int fd = mkstemp(m_sfv);
        CPPUNIT_ASSERT(fd >= 0);
        dprintf(fd, "%s %.8x\n", m_data, crc);
        close(fd);

        const char *argv[] = {
            "/usr/bin/cksfv",
            m_data,
            "-f",
            m_sfv,
            NULL
        };
        CPPUNIT_ASSERT_EQUAL(0, spawn(argv, NULL));
        */
        CPPUNIT_ASSERT_EQUAL((uint32_t) 0xf1e8ba9e, crc);
    }
};
CPPUNIT_TEST_SUITE_REGISTRATION(crcTest);

