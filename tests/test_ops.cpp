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
                bpk_write(m_bpk, BPK_TYPE_PBL, m_data));
        CPPUNIT_ASSERT_EQUAL(0,
                bpk_write(m_bpk, BPK_TYPE_PBLV, m_data));
        CPPUNIT_ASSERT_EQUAL(0,
                bpk_write(m_bpk, BPK_TYPE_PKER, m_data));
        CPPUNIT_ASSERT_EQUAL(0,
                bpk_write(m_bpk, BPK_TYPE_PRFS, m_data));
        CPPUNIT_ASSERT_EQUAL(0,
                bpk_write(m_bpk, 42, m_data));
        bpk_close(m_bpk);
        m_bpk = NULL;
    }

    void read()
    {
        bpk_size size;
        create();

        m_bpk = bpk_open(m_file, 0);
        CPPUNIT_ASSERT(m_bpk);

        CPPUNIT_ASSERT_EQUAL(0,
                bpk_find(m_bpk, BPK_TYPE_PBL, NULL));
        CPPUNIT_ASSERT_EQUAL(0,
                bpk_read_file(m_bpk, m_data));

        CPPUNIT_ASSERT_EQUAL((bpk_type) BPK_TYPE_PBLV,
                bpk_next(m_bpk, &size));
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
                bpk_find(m_bpk, BPK_TYPE_PBL, NULL));
        CPPUNIT_ASSERT_EQUAL(0,
                bpk_find(m_bpk, BPK_TYPE_PRFS, &size));
        CPPUNIT_ASSERT_EQUAL(0,
                bpk_find(m_bpk, BPK_TYPE_PBL, &size));
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
        fseek(fd, 42, SEEK_SET);
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
                bpk_write(m_bpk, BPK_TYPE_PBL, m_data));
        CPPUNIT_ASSERT_EQUAL(0,
                bpk_write(m_bpk, BPK_TYPE_PBLV, m_data));
        bpk_close(m_bpk);

        m_bpk = bpk_open(m_file, 1);
        CPPUNIT_ASSERT_EQUAL(0,
                bpk_write(m_bpk, BPK_TYPE_PRFS, m_data));
        bpk_close(m_bpk);

        m_bpk = bpk_open(m_file, 1);
        CPPUNIT_ASSERT_EQUAL(0, bpk_check_crc(m_bpk));
        CPPUNIT_ASSERT_EQUAL(0,
                bpk_find(m_bpk, BPK_TYPE_PRFS, NULL));
        bpk_close(m_bpk);
        m_bpk = NULL;
    }
};
CPPUNIT_TEST_SUITE_REGISTRATION(opsTest);

