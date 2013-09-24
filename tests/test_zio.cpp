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
** test_zio.c
**
**        Created on: Sep 22, 2013
**   Original Author: Sylvain Fargier <sylvain.fargier@somfy.com>
**
*/

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include "zio.h"
#include "test_helpers.hpp"
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define ZIO_TEST_DATA "this is a test"
#define ZIO_TEST_DATA_SZ (sizeof (ZIO_TEST_DATA) - 1)

#define TEST_BPK_FILE "/tmp/testbpk"
#define TEST_BPK_DATA "/tmp/testbpkdata"
#define TEST_BPK_DATA2 "/tmp/testbpkdata2"
#define TEST_BPK_DATA2_GZ "/tmp/testbpkdata2.gz"

class zioTest : public CppUnit::TestFixture
{
    CPPUNIT_TEST_SUITE(zioTest);
    CPPUNIT_TEST(compress);
    CPPUNIT_TEST(zbpk);
    CPPUNIT_TEST_SUITE_END();

public:
    void setUp()
    {
        m_zfile = NULL;
        m_file = NULL;
        m_bpk = NULL;

        int fd = creat(TEST_BPK_DATA, S_IRWXU);
        CPPUNIT_ASSERT(fd >= 0);
        write(fd, ZIO_TEST_DATA, ZIO_TEST_DATA_SZ);
        close(fd);
    }

    void tearDown()
    {
        unlink(TEST_BPK_DATA);
        unlink(TEST_BPK_DATA2);
        unlink(TEST_BPK_DATA2_GZ);
        unlink(TEST_BPK_FILE);

        if (m_file)
            fclose(m_file);
        if (m_zfile)
            zclose(m_zfile);
        if (m_bpk)
            bpk_close(m_bpk);
    }

protected:
    FILE *m_file;
    zctrl *m_zfile;
    bpk *m_bpk;

    void compress()
    {
        unsigned char buf[1024];
        ssize_t ret;
        const char *gunzip_args[] = { "/bin/gunzip", TEST_BPK_DATA2_GZ, NULL };
        const char *cmp_args[] = { "/usr/bin/cmp", TEST_BPK_DATA,
            TEST_BPK_DATA2, NULL };

        m_zfile = zopen(TEST_BPK_DATA, 1);
        CPPUNIT_ASSERT(m_zfile != NULL);

        ret = zfill(buf, 1024, m_zfile);
        CPPUNIT_ASSERT(ret >= 0);

        m_file = fopen(TEST_BPK_DATA2_GZ, "w");
        CPPUNIT_ASSERT(fwrite(buf, ret, 1, m_file) == 1);
        fclose(m_file);
        m_file = NULL;

        CPPUNIT_ASSERT_EQUAL(0, spawn(gunzip_args, NULL));
        CPPUNIT_ASSERT_EQUAL(0, spawn(cmp_args, NULL));

        CPPUNIT_ASSERT_EQUAL((ssize_t) 0, zfill(buf, 1024, m_zfile));

        zclose(m_zfile);
        m_zfile = NULL;
    }

    void zbpk()
    {
        bpk_size size;
        const char *cmp_args[] = { "/usr/bin/cmp", TEST_BPK_DATA,
            TEST_BPK_DATA2, NULL };

        m_bpk = bpk_create(TEST_BPK_FILE);
        CPPUNIT_ASSERT(m_bpk);

        m_zfile = zopen(TEST_BPK_DATA, 1);
        CPPUNIT_ASSERT(m_zfile != NULL);

        CPPUNIT_ASSERT_EQUAL(0,
                bpk_write_custom(m_bpk, BPK_TYPE_BL, 0, (bpk_fill_func) zfill,
                    m_zfile));
        bpk_close(m_bpk);


        m_bpk = bpk_open(TEST_BPK_FILE, 0);
        CPPUNIT_ASSERT(m_bpk);

        CPPUNIT_ASSERT_EQUAL(0,
                bpk_find(m_bpk, BPK_TYPE_BL, 0, &size, NULL));

        CPPUNIT_ASSERT_EQUAL(0,
                bpk_zread_file(m_bpk, size, TEST_BPK_DATA2));

        CPPUNIT_ASSERT_EQUAL(0, spawn(cmp_args, NULL));

        bpk_close(m_bpk);
        m_bpk = NULL;
    }
};

CPPUNIT_TEST_SUITE_REGISTRATION(zioTest);
