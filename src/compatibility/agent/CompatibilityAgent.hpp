/*
 * Copyright (C) 2020, Siemens AG
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
/**
 * @file
 */
#ifndef SRC_COMPATIBILITY_AGENT_COMPATIBILITYAGENT_HPP_
#define SRC_COMPATIBILITY_AGENT_COMPATIBILITYAGENT_HPP_

#include <fstream>
#include <vector>
#include <string>
#include <iostream>
#include <tuple>

#include "CompatibilityDatabaseHandler.hpp"

/**
 * @class CompatibilityAgent
 * The CompatibilityAgent class with various functions to scan a file.
 */
class CompatibilityAgent
{
  public:
	CompatibilityAgent();
    bool compatibilityFunc(std::vector<unsigned long> &licId,unsigned long &pFileId,
    CompatibilityDatabaseHandler &databaseHandler, int a_id);
};

#endif /* SRC_COMPATIBILITY_AGENT_COMPATIBILITYAGENT_HPP_ */
