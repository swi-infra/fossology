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
 * @brief Database handler for COMPATIBILITY
 */

#ifndef COMPATIBILITY_AGENT_DATABASE_HANDLER_HPP
#define COMPATIBILITY_AGENT_DATABASE_HANDLER_HPP

#include <unordered_map>
#include <algorithm>
#include <string>
#include <iostream>
#include <tuple>
#include <vector>

#include "libfossUtils.hpp"
#include "libfossAgentDatabaseHandler.hpp"
#include "libfossdbmanagerclass.hpp"

extern "C" {
#include "libfossology.h"
}
using namespace std;
/**
 * @struct CompatibilityDatabaseEntry
 * Structure to hold entries to be inserted in DB
 */
struct CompatibilityDatabaseEntry
{
  /**
   * @var long int license_fk
   * License ID
   * @var long int agent_fk
   * Agent ID
   * @var long int pfile_fk
   * Pfile ID
   */
  const unsigned long int license_fk, agent_fk, pfile_fk;
  /**
   * Constructor for CompatibilityDatabaseEntry structure
   * @param l License ID
   * @param a Agent ID
   * @param p Pfile ID
   */
  CompatibilityDatabaseEntry(const unsigned long int l, const unsigned long int a,
    const unsigned long int p) :
    license_fk(l), agent_fk(a), pfile_fk(p)
  {
  }
};

/**
 * @class CompatibilityDatabaseHandler
 * Database handler for COMPATIBILITY agent
 */
class CompatibilityDatabaseHandler: public fo::AgentDatabaseHandler
{
  public:
    CompatibilityDatabaseHandler(fo::DbManager dbManager);
    CompatibilityDatabaseHandler(CompatibilityDatabaseHandler &&other) :
      fo::AgentDatabaseHandler(std::move(other))
    {
    }
    ;
    CompatibilityDatabaseHandler spawn() const;

    std::vector<unsigned long> queryFileIdsForUpload(int uploadId);
    std::vector<unsigned long> queryFileIdsForScan(int uploadId, int agentId);

    std::vector<unsigned long> queryLicIdsFromPfile(unsigned long pFileId);
    std::vector<std::tuple<unsigned long, std::string>> queryLicDetails(std::vector<unsigned long> licId);
    int queryRule1(std::tuple<unsigned long, std::string> lic1, std::tuple<unsigned long, std::string> lic2);
    int queryRule2(std::tuple<unsigned long, std::string> lic1, std::tuple<unsigned long, std::string> lic2);
    int queryRule3(std::tuple<unsigned long, std::string> lic1, std::tuple<unsigned long, std::string> lic2);
    bool queryInsertResult(unsigned long pFileId, int a_id, unsigned long id1, unsigned long id2, string comp);
    bool check(unsigned long id1, unsigned long id2, unsigned long pFileId);
};

#endif // COMPATIBILITY_AGENT_DATABASE_HANDLER_HPP
