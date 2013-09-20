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
** test_ops.cpp
**
**        Created on: Jun 12, 2013
**   Original Author: Sylvain Fargier <sylvain.fargier@somfy.com>
**
*/

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include <exception>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "bpk.h"
#include "bpk_priv.h"

#define SZ_1K (1024)
#define SZ_512 (512)

class opsTest : public CppUnit::TestFixture
{
    CPPUNIT_TEST_SUITE(opsTest);
    CPPUNIT_TEST(create);
    CPPUNIT_TEST(read);
    CPPUNIT_TEST(find);
    CPPUNIT_TEST(crc);
    CPPUNIT_TEST(append);
    CPPUNIT_TEST(bigfile);
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
    }

    void tearDown()
    {
        unlink(m_data);
        unlink(m_file);
        free(m_data);
        free(m_file);

        if (m_bpk)
            bpk_close(m_bpk);
    }

protected:
    char *m_file;
    char *m_data;
    bpk *m_bpk;

    void create()
    {
        m_bpk = bpk_create(m_file);
        CPPUNIT_ASSERT(m_bpk);

        CPPUNIT_ASSERT_EQUAL(0,
                bpk_write(m_bpk, BPK_TYPE_BL, 0, m_data));
        CPPUNIT_ASSERT_EQUAL(0,
                bpk_write(m_bpk, BPK_TYPE_BLV, 0xFFFFFFFF, m_data));
        CPPUNIT_ASSERT_EQUAL(0,
                bpk_write(m_bpk, BPK_TYPE_KER, 0, m_data));
        CPPUNIT_ASSERT_EQUAL(0,
                bpk_write(m_bpk, BPK_TYPE_RFS, 0, m_data));
        CPPUNIT_ASSERT_EQUAL(0,
                bpk_write(m_bpk, 42, 0, m_data));
        bpk_close(m_bpk);
        m_bpk = NULL;
    }

    void read()
    {
        bpk_size size;
        uint32_t hw_id = 1;
        create();

        m_bpk = bpk_open(m_file, 0);
        CPPUNIT_ASSERT(m_bpk);

        CPPUNIT_ASSERT_EQUAL(0,
                bpk_find(m_bpk, BPK_TYPE_BL, 0, NULL, NULL));
        CPPUNIT_ASSERT_EQUAL(0,
                bpk_read_file(m_bpk, m_data));

        CPPUNIT_ASSERT_EQUAL((bpk_type) BPK_TYPE_BLV,
                bpk_next(m_bpk, &size, NULL, &hw_id));
        CPPUNIT_ASSERT_EQUAL(hw_id, (uint32_t) 0xFFFFFFFF);
        CPPUNIT_ASSERT_EQUAL((bpk_size) SZ_1K * 2, size);
        char *buf = (char *) malloc(SZ_1K);

        try
        {
            CPPUNIT_ASSERT_EQUAL((bpk_size) SZ_1K,
                    bpk_read(m_bpk, buf, SZ_1K));
            CPPUNIT_ASSERT_EQUAL((bpk_size) SZ_512,
                    bpk_read(m_bpk, buf, SZ_512));

            CPPUNIT_ASSERT_EQUAL((bpk_size) SZ_512,
                    bpk_read(m_bpk, buf, SZ_1K));
        }
        catch (std::exception &e)
        {
            free(buf);
            throw;
        }
        free(buf);

        bpk_close(m_bpk);
        m_bpk = NULL;
    }

    void find()
    {
        bpk_size size;

        create();

        m_bpk = bpk_open(m_file, 0);
        CPPUNIT_ASSERT(m_bpk);

        CPPUNIT_ASSERT_EQUAL(0,
                bpk_find(m_bpk, BPK_TYPE_BL, 0, NULL, NULL));
        CPPUNIT_ASSERT_EQUAL(0,
                bpk_find(m_bpk, BPK_TYPE_RFS, 0, &size, NULL));
        CPPUNIT_ASSERT_EQUAL(0,
                bpk_find(m_bpk, BPK_TYPE_BL, 0, &size, NULL));
        CPPUNIT_ASSERT(bpk_find(m_bpk, BPK_TYPE_BLV, 0, &size, NULL) != 0);
        bpk_close(m_bpk);
        m_bpk = NULL;
    }

    void crc()
    {
        create();

        m_bpk = bpk_open(m_file, 0);
        CPPUNIT_ASSERT_EQUAL(0, bpk_check_crc(m_bpk));
        bpk_close(m_bpk);

        FILE *fd = fopen(m_file, "r+");
        CPPUNIT_ASSERT(fd);
        fseek(fd, sizeof (bpk_header), SEEK_SET);
        fwrite("test", 4, 1, fd);
        fclose(fd);

        m_bpk = bpk_open(m_file, 0);
        CPPUNIT_ASSERT(m_bpk);
        CPPUNIT_ASSERT(bpk_check_crc(m_bpk) != 0);
        bpk_close(m_bpk);
        m_bpk = NULL;
    }

    void append()
    {
        m_bpk = bpk_create(m_file);
        CPPUNIT_ASSERT(m_bpk);

        CPPUNIT_ASSERT_EQUAL(0,
                bpk_write(m_bpk, BPK_TYPE_BL, 0, m_data));
        CPPUNIT_ASSERT_EQUAL(0,
                bpk_write(m_bpk, BPK_TYPE_BLV, 0, m_data));
        bpk_close(m_bpk);

        m_bpk = bpk_open(m_file, 1);
        CPPUNIT_ASSERT_EQUAL(0,
                bpk_write(m_bpk, BPK_TYPE_RFS, 0, m_data));
        bpk_close(m_bpk);

        m_bpk = bpk_open(m_file, 1);
        CPPUNIT_ASSERT_EQUAL(0, bpk_check_crc(m_bpk));
        CPPUNIT_ASSERT_EQUAL(0,
                bpk_find(m_bpk, BPK_TYPE_RFS, 0, NULL, NULL));
        bpk_close(m_bpk);
        m_bpk = NULL;
    }

    void bigfile()
    {
        create();

        FILE *fd = fopen(m_file, "a");
        CPPUNIT_ASSERT(fd);
        for (int i = 0; i < 1000; ++i)
            fwrite("test", 4, 1, fd);
        fclose(fd);

        m_bpk = bpk_open(m_file, 0);
        CPPUNIT_ASSERT(m_bpk);
        CPPUNIT_ASSERT(bpk_check_crc(m_bpk) == 0);

        int count = 0;
        while (bpk_next(m_bpk, NULL, NULL, NULL) != BPK_TYPE_INVALID)
            ++count;

        CPPUNIT_ASSERT_EQUAL(5, count); /* must match create() */

        bpk_close(m_bpk);
        m_bpk = NULL;
    }
};
CPPUNIT_TEST_SUITE_REGISTRATION(opsTest);

