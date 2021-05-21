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
 * The utility functions for OJO agent
 */

#include <iostream>

#include "CompatibilityAgent.hpp"
#include "CompatibilityUtils.hpp"

using namespace fo;

/**
 * @brief Create a new state for the current agent based on CliOptions.
 *
 * Called during instantiation of agent.
 * @param cliOptions CLI options passed to the agent
 * @return New CompatibilityState object for the agent
 */
CompatibilityState getState(DbManager &dbManager, CompatibilityCliOptions &&cliOptions)
{
  int agentId = queryAgentId(dbManager);
  return CompatibilityState(agentId, std::move(cliOptions));
}

/**
 * @brief Create a new state for the agent without DB manager
 * @param cliOptions CLI options passed
 * @return New CompatibilityState object
 */
CompatibilityState getState(CompatibilityCliOptions &&cliOptions)
{
  return CompatibilityState(-1, std::move(cliOptions));
}

/**
 * Query the agent ID from the DB.
 * @param dbManager DbManager to be used
 * @return The agent if found, bail otherwise.
 */
int queryAgentId(DbManager &dbManager)
{
  char* COMMIT_HASH = fo_sysconfig(AGENT_NAME, "COMMIT_HASH");
  char* VERSION = fo_sysconfig(AGENT_NAME, "VERSION");
  char *agentRevision;

  if (!asprintf(&agentRevision, "%s.%s", VERSION, COMMIT_HASH))
    bail(-1);

  int agentId = fo_GetAgentKey(dbManager.getConnection(), AGENT_NAME, 0,
      agentRevision, AGENT_DESC);
  free(agentRevision);

  if (agentId <= 0)
    bail(1);

  return agentId;
}

/**
 * Write ARS to the agent's ars table
 * @param state     State of the agent
 * @param arsId     ARS id (0 for new entry)
 * @param uploadId  Upload ID
 * @param success   Success status
 * @param dbManager DbManager to use
 * @return ARS ID.
 */
int writeARS(const CompatibilityState &state, int arsId, int uploadId, int success,
    DbManager &dbManager)
{
  PGconn *connection = dbManager.getConnection();
  int agentId = state.getAgentId();

  return fo_WriteARS(connection, arsId, uploadId, agentId, AGENT_ARS, NULL,
    success);
}

/**
 * Disconnect scheduler and exit in case of failure.
 * @param exitval Exit code to be sent to scheduler and returned by program
 */
void bail(int exitval)
{
  fo_scheduler_disconnect(exitval);
  exit(exitval);
}

/**
 * Process a given upload id
 * @param state           State of the agent
 * @param uploadId        Upload ID to be scanned
 * @param databaseHandler Database handler to be used
 * @return True in case of successful scan, false otherwise.
 */
bool processUploadId(const CompatibilityState &state, int uploadId,
    CompatibilityDatabaseHandler &databaseHandler)
{
  vector<unsigned long> fileIds = databaseHandler.queryFileIdsForScan(
      uploadId, state.getAgentId());
  char const *repoArea = "files";

  bool errors = false;
#pragma omp parallel
  {
    CompatibilityDatabaseHandler threadLocalDatabaseHandler(databaseHandler.spawn());

    size_t pFileCount = fileIds.size();
    CompatibilityAgent agentObj = state.getCompatibilityAgent();
#pragma omp for
    for (size_t it = 0; it < pFileCount; ++it)
    {
      if (errors)
        continue;

      unsigned long pFileId = fileIds[it];

      if (pFileId == 0)
        continue;

      vector<unsigned long> licId = threadLocalDatabaseHandler.queryLicIdsFromPfile(
    	        pFileId);

      char *fileName = threadLocalDatabaseHandler.getPFileNameForFileId(
        pFileId);
      char *filePath = NULL;
#pragma omp critical (repo_mk_path)
      filePath = fo_RepMkPath(repoArea, fileName);

      if (licId. size() < 2)
      {
    	  continue;
      }
      if (!filePath)
      {
        LOG_FATAL(
          AGENT_NAME" was unable to derive a file path for pfile %ld.  Check your HOSTS configuration.",
          pFileId);
        errors = true;
      }

      bool identified;
      try
      {
    	identified = agentObj.compatibilityFunc(licId, pFileId, threadLocalDatabaseHandler, state.getAgentId()); //pass licID vector, return the result(true or false)
      }
      catch (std::runtime_error &e)
      {
        LOG_FATAL("Unable to read %s.", e.what());
        continue;
      }

      if(identified == false)
      {
    	  LOG_FATAL("Unable to store results in database for pfile %ld.",
    	            pFileId);
    	          bail(-20);
      }
      fo_scheduler_heart(1);
    }
  }

  return !errors;
}

/**
 * @brief Parse the options sent by CLI to CliOptions object
 * @param[in]  argc
 * @param[in]  argv
 * @param[out] dest      The parsed OjoCliOptions object
 * @param[out] types Path of the csv file to be scanned
 * @param[out] rules Path of the yaml file to be scanned
 * @param[out] jFile Path of the json file to be scanned
 * @return True if success, false otherwise
 */
bool parseCliOptions(int argc, char **argv, CompatibilityCliOptions &dest,
     std::string &types, std::string &rules, string &jFile)
{
  boost::program_options::options_description desc(
    AGENT_NAME ": recognized options");
  desc.add_options()
    (
      "help,h", "shows help"
    )
    (
      "verbose,v", "increase verbosity"
    )
	(
	  "file,f",
	  boost::program_options::value<string>(),
	  "json file, containing fileNames and licenses within that fileNames"
		)
    (
      "json,J", "output JSON"
    )
    (
      "config,c",
      boost::program_options::value<string>(),
      "path to the sysconfigdir"
    )
    (
      "scheduler_start",
      "specifies, that the command was called by the scheduler"
    )
    (
      "userID",
      boost::program_options::value<int>(),
      "the id of the user that created the job (only in combination with --scheduler_start)"
    )
    (
      "groupID",
      boost::program_options::value<int>(),
      "the id of the group of the user that created the job (only in combination with --scheduler_start)"
    )
    (
      "jobId",
      boost::program_options::value<int>(),
      "the id of the job (only in combination with --scheduler_start)"
    )
	(
	  "types,t",
	  boost::program_options::value<string>(),
	  "license types for compatibility rules"
	)
	(
	  "rules,r",
	  boost::program_options::value<string>(),
	  "license compatibility rules"
	)
    ;

  boost::program_options::positional_options_description p;

  boost::program_options::variables_map vm;

  try
  {
    boost::program_options::store(
      boost::program_options::command_line_parser(argc, argv).options(desc).positional(
        p).run(), vm);

    if (vm.count("help") > 0)
    {
      cout << desc << endl;
      exit(0);
    }

    if (vm.count("rules"))
    {
      rules = vm["rules"].as<std::string>();
    }

    if (vm.count("types"))
    {
      types = vm["types"].as<std::string>();
    }

    if (vm.count("file"))
    {
      jFile = vm["file"].as<std::string>();

    }

    unsigned long verbosity = vm.count("verbose");
    bool json = vm.count("json") > 0 ? true : false;

    dest = CompatibilityCliOptions(verbosity, json);

    return true;
  }
  catch (boost::bad_any_cast&)
  {
    cout << "wrong parameter type" << endl;
    cout << desc << endl;
    return false;
  }
  catch (boost::program_options::error&)
  {
    cout << "wrong command line arguments" << endl;
    cout << desc << endl;
    return false;
  }
}

/**
 * Append a new result from scanner to STDOUT
 * @param fileName   File which was scanned
 * @param resultPair Contains the first license name, second license name and their compatibility result
 * @param printComma Set true to print comma. Will be set true after first
 *                   data is printed
 */
void appendToJson(
    const std::vector<tuple<string,string,string>> resultPair, std::string fileName,
    bool &printComma)
{
  Json::Value result;
#if JSONCPP_VERSION_HEXA < ((1 << 24) | (4 << 16))
  // Use FastWriter for versions below 1.4.0
  Json::FastWriter jsonBuilder;
#else
  // Since version 1.4.0, FastWriter is deprecated and replaced with
  // StreamWriterBuilder
  Json::StreamWriterBuilder jsonBuilder;
  jsonBuilder["commentStyle"] = "None";
  jsonBuilder["indentation"] = "";
#endif
  //jsonBuilder.omitEndingLineFeed();
  Json::Value licenses;
  if(fileName != "null")
  {
    result["file"]= fileName;
    Json::Value res(Json::arrayValue);
    for(unsigned int i=0;i<resultPair.size();i++)
    {
      Json::Value license(Json::arrayValue);
      Json::Value comp;
      Json::Value final;
      license.append(get<0>(resultPair[i]));
      license.append(get<1>(resultPair[i]));
      comp = get<2>(resultPair[i]);
      final["license"]=license;
      final["compatibility"]=comp;
      res.append(final);
    }
    result["results"]=res;
  }
  else
  {
	Json::Value res(Json::arrayValue);
	for(unsigned int i=0;i<resultPair.size();i++)
	{
		Json::Value license(Json::arrayValue);
		Json::Value comp;
		Json::Value final;
		license.append(get<0>(resultPair[i]));
		license.append(get<1>(resultPair[i]));
		comp = get<2>(resultPair[i]);
		final["license"]=license;
		final["compatibility"]=comp;
		res.append(final);
	}
	result["package-level-result"]=res;
  }


  // Thread-Safety: output all matches JSON at once to STDOUT
#pragma omp critical (jsonPrinter)
  {
    if (printComma)
    {
      cout << "," << endl;
    }
    else
    {
      printComma = true;
    }
    string jsonString;
#if JSONCPP_VERSION_HEXA < ((1 << 24) | (4 << 16))
    // For version below 1.4.0, every writer append `\n` at end.
    // Find and replace it.
    jsonString = jsonBuilder.write(result);
    jsonString.replace(jsonString.find("\n"), string("\n").length(), "");
#else
    // For version >= 1.4.0, \n is not appended.
    jsonString = Json::writeString(jsonBuilder, result);
#endif
    cout << "  " << jsonString << flush;

    //cout << "  " << jsonBuilder.write(result) << flush;
  }
}

/**
 * Print the result of current scan to stdout
 * @param fileName   File which was scanned
 * @param resultPair Contains the first license name, second license name and their compatibility result
 */
void printResultToStdout(
    const std::vector<tuple<string,string,string>> resultPair, std::string fileName)
{
  stringstream ss;
  if(fileName != "null")
  {
	cout<<"----"<<fileName<<"----"<<endl;
    for(unsigned int i=0;i<resultPair.size();i++)
    {
      cout<< get<0>(resultPair[i]) <<"," << get<1>(resultPair[i]) <<" ::"<< get<2>(resultPair[i]) << endl;
    }
  }
  else
  {
	  cout<<"----all licenses with their compatibility----"<<endl;
	  for(unsigned int i=0;i<resultPair.size();i++)
	  {
		cout<< get<0>(resultPair[i]) <<"," << get<1>(resultPair[i]) <<" ::"<< get<2>(resultPair[i]) << endl;
	  }
  }

}
