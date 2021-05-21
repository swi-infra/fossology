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

#ifndef COMPATIBILITY_AGENT_UTILS_HPP
#define COMPATIBILITY_AGENT_UTILS_HPP

#define AGENT_NAME "compatibility"
#define AGENT_DESC "compatibility agent"
#define AGENT_ARS  "compatibility_ars"

#include <vector>
#include <utility>
#include <json/json.h>
#include <boost/program_options.hpp>

#include "CompatibilityState.hpp"
#include "libfossologyCPP.hpp"
#include "CompatibilityDatabaseHandler.hpp"

extern "C" {
#include "libfossology.h"
}

using namespace std;

CompatibilityState getState(fo::DbManager &dbManager, CompatibilityCliOptions &&cliOptions);
CompatibilityState getState(CompatibilityCliOptions &&cliOptions);
int queryAgentId(fo::DbManager &dbManager);
int writeARS(const CompatibilityState &state, int arsId, int uploadId, int success,
  fo::DbManager &dbManager);
void bail(int exitval);
bool processUploadId(const CompatibilityState &state, int uploadId,
  CompatibilityDatabaseHandler &databaseHandler);
bool parseCliOptions(int argc, char **argv, CompatibilityCliOptions &dest,
   std::string &types, std::string &rules, string &jFile);
void appendToJson(const vector<tuple<string,string,string>> resultPair, std::string fileName, bool &printComma);
void printResultToStdout(const vector<tuple<string,string,string>> resultPair, std::string fileName);

#endif // COMPATIBILITY_AGENT_UTILS_HPP
