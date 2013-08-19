/*
** Copyright (C) 2012 Somfy SAS.
**
** This library is free software; you can redistribute it and/or
** modify it under the terms of the GNU Lesser General Public
** License as published by the Free Software Foundation; either
** version 2.1 of the License, or (at your option) any later version.
**
** This library is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
** Lesser General Public License for more details.
**
** You should have received a copy of the GNU Lesser General Public
** License along with this library; if not, write to the Free Software
** Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
**
** test_helpers.hpp
**
**        Created on: Aug 08, 2012
**   Original Author: Sylvain Fargier <sylvain.fargier@somfy.com>
**
*/

#ifndef __TEST_HELPERS_HPP__
#define __TEST_HELPERS_HPP__

/**
 * @brief execve wrapper
 * @details forks (or vforks if available) and exec the given program.
 *
 * @param[in] argv the program's argv.
 * @param[in] envp the program's envp (set to NULL if no env is required).
 * @return the command return code (or execve error if it occurs).
 */
int spawn(const char * *argv, const char * *envp);

#endif

