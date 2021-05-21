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
 * @dir
 * @brief The COMPATIBILITY agent
 * @file
 * @brief Entry point for compatibility agent
 * @page compatibility COMPATIBILITY Agent
 * @tableofcontents
 *
 * Compatibility agent to find the compatibility between licenses
 * present in a file.
 * Can run in scheduler as well as cli mode
 *
 * The agent runs in multi-threaded mode and creates a new thread for
 * every pfile for faster processing.
 *
 * @section compatibilityactions Supported actions
 * | Command line flag | Description |
 * | ---: | :--- |
 * | -h [--help] | Shows help |
 * | -v [--verbose] | Increase verbosity |
 * | --file arg | Json File to scan |
 * | -J [--json] | Output JSON |
 * | --types arg | CSV File to scan |
 * | --rules arg | YAML File to scan |
 * | -c [ --config ] arg | Path to the sysconfigdir |
 * | --scheduler_start | Specifies, that the command was called by the |
 * || scheduler |
 * | --userID arg | The id of the user that created the job (only in |
 * || combination with --scheduler_start) |
 * | --groupID arg | The id of the group of the user that created the job |
 * || (only in combination with --scheduler_start) |
 * | --jobId arg | The id of the job (only in combination with |
 * || --scheduler_start) |
 * @section compatibilitysource Agent source
 *   - @link src/compatibility/agent @endlink
 *   - @link src/compatibility/ui @endlink
 */

#include "compatibilityS.hpp"

using namespace fo;

/**
 * @def return_sched(retval)
 * Send disconnect to scheduler with retval and return function with retval.
 */
#define return_sched(retval) \
  do {\
    fo_scheduler_disconnect((retval));\
    return (retval);\
  } while(0)

int main(int argc, char **argv)
{
  CompatibilityCliOptions cliOptions;
  vector<string> licenseNames;
  string lic_types, rule, fileName;
  string jFile;
  if (!parseCliOptions(argc, argv, cliOptions, lic_types, rule, jFile))
  {
    return_sched(1);
  }

  bool json = cliOptions.doJsonOutput();
  CompatibilityState state = getState(std::move(cliOptions));

  if (!jFile.empty())
  {
    bool fileError = false;
    bool printComma = false;
    //CompatibilityAgent agentObj = state.getCompatibilityAgent();


    string main_name, main_type, sub_name, sub_type, fileName;
    vector<string> allLic;
    if (json)
    {
      cout << "[" << endl;
    }

    Json::Value root;
    std::ifstream ifs(jFile.c_str());
   	ifs >> root;

   	for (Json::Value::const_iterator jfile = root["results"].begin(); jfile != root["results"].end(); ++jfile)  //iterating the json_file
    {
      fileName = (*jfile)["file"].asString();
      unsigned int size = (*jfile)["licenses"].size();
      vector<string> myVec;
      if (size > 1)  //checking if a file contains more than one license
      {
    	for (Json::Value::const_iterator larray = (*jfile)["licenses"].begin(); larray != (*jfile)["licenses"].end(); ++larray) //iterating the license array															   license array
    	{
    	  string str = (*larray).asString();
    	  if(str == "Dual-license")
    	  {
    		  continue;
    	  }
    	  myVec.push_back(str);
    	  allLic.push_back(str);
    	}
      }
      vector<tuple<string,string,string>> result;
      result= compatibilityFunc(myVec, fileName , lic_types, rule);
      if (json)
      {
    	  appendToJson(result, fileName, printComma);
      }
      else
      {
    	  printResultToStdout(result, fileName);
      }
    }
   	vector<string> distinctLic;
   	for(unsigned int i=0;i<allLic.size();i++)	//store all the distinct licenses in a vector
   	{
   		unsigned int j;
   		for(j=0;j<i;j++)
   		{
   			if(allLic[i] == allLic[j])
   				break;
   		}
   		if(i==j)
   		{
   			distinctLic.push_back(allLic[i]);
   		}
   	}
    vector<tuple<string,string,string>> result2;
    string name="null";
    result2= compatibilityFunc(distinctLic, name , lic_types, rule);
    if (json)
    {
  	  appendToJson(result2, name, printComma);
    }
    else
    {
      printResultToStdout(result2, name);
    }


    if (json)
    {
      cout << endl << "]" << endl;
    }
    return fileError ? 1 : 0;
  }

  else
  {
    DbManager dbManager(&argc, argv);
    CompatibilityDatabaseHandler databaseHandler(dbManager);

    state.setAgentId(queryAgentId(dbManager));

    while (fo_scheduler_next() != NULL)
    {
      int uploadId = atoi(fo_scheduler_current());

      if (uploadId == 0)
        continue;

      int arsId = writeARS(state, 0, uploadId, 0, dbManager);

      if (arsId <= 0)
        bail(5);

      if (!processUploadId(state, uploadId, databaseHandler))
        bail(2);

      fo_scheduler_heart(0);
      writeARS(state, arsId, uploadId, 1, dbManager);
    }
    fo_scheduler_heart(0);

    /* do not use bail, as it would prevent the destructors from running */
    fo_scheduler_disconnect(0);
  }
  return 0;
}

/**
 * @brief find the compatibility for cli mode
 * @param myVec containg licenses present in the file
 * @param fileName name of the file
 * @param lic_types containing location of the csv file in which license information is present
 * @param rule containing location of the yaml file in which the rules are defined
 * @return vector of tuple
 */
vector<tuple<string,string,string>> compatibilityFunc(vector<string> myVec, string fileName, string lic_types, string rule)
   	{
	  std::ifstream ip(lic_types.c_str());
	  YAML::Node rules = YAML::LoadFile(rule);
	  string main_name, main_type, sub_name, sub_type;
	  vector<tuple<string,string,string>> res;

	  string line,name,type;
	  vector<string> license_name;
	  vector<string> license_type;

	  while(getline(ip,line))	//parsing the csv file
	  {
		stringstream ss(line);
		getline(ss,name,',');
		getline(ss,type,',');
		license_name.push_back(name);
		license_type.push_back(type);
	  }

      unsigned int vecSize = myVec.size();
      for (unsigned int argn = 0; argn < (vecSize-1); argn++)
      {
          for (unsigned int argn2 = (argn+1); argn2 < vecSize; argn2++)
          {
        	  for(unsigned int i=1;i<license_name.size();i++)
        	  {
        		  if(myVec[argn]==license_name[i])
        		  {
        			  main_name = license_name[i];
        			  main_type = license_type[i];
        		  }
        		  if(myVec[argn2]==license_name[i])
        		  {
        			  sub_name = license_name[i];
        			  sub_type = license_type[i];
        		  }
        	  }

        	  int def = 0, priority = 0;

        	  for (YAML::const_iterator yml_rfile =rules["rules"].begin(); yml_rfile != rules["rules"].end(); yml_rfile++) //iterating the yml.rules
        	  {
        	  	string type = "", type2="", name="", name2="", ans="";
        	  	int rule1 = 0, rule2 = 0, rule3 = 0, rule4 = 0;
        	  	for (YAML::const_iterator tag = yml_rfile->begin(); tag != yml_rfile->end(); tag++)
        	  	{
        	  	  if ((*tag).first.as<string>().compare("text") == 0) {
        	  		continue;
        	      }
        	  	  if ((*tag).first.as<string>().compare("compatibility") == 0) {
        	  	    ans = (*tag).second.as<string>();
        	  	    rule1++;
        	  	    rule4++;
        	  	    rule2++;
        	  	    rule3++;
        	      }
        	  	  if ((*tag).first.as<string>().compare("maintype") == 0 && ((*tag).second.as<string>() != "~")) {
        	  		rule1++;
        	  		rule4++;
        	  		type = (*tag).second.as<string>();

        	      }
        	  	  if ((*tag).first.as<string>().compare("subtype") == 0 && ((*tag).second.as<string>() != "~")) {
        	  		rule1++;
        	  		rule3++;
        	  		type2 = (*tag).second.as<string>();
        	      }
        	  	  if ((*tag).first.as<string>().compare("mainname") == 0 && ((*tag).second.as<string>() != "~")) {
        	  		rule2++;
        	  		rule3++;
        	  		name = (*tag).second.as<string>();
        	  	  }
        	  	  if ((*tag).first.as<string>().compare("subname") == 0 && ((*tag).second.as<string>() != "~")) {
        	  		rule2++;
        	  		rule4++;
        	  		name2 = (*tag).second.as<string>();
        	  	  }
        	  	  if ((rule1 == 3) && (main_type.compare(type) == 0 && sub_type.compare(type2) == 0)) //rule1 for maintype and subtype
        	  	  {
        	  		res.push_back(make_tuple(main_name,sub_name,ans));
        	  		def++;
        	  		continue;
        	  	  }
        	  	  if ((rule2 == 3) && (((main_name.compare(name) == 0) || (main_name.compare(name2) == 0)) && ((sub_name.compare(name) == 0) || (sub_name.compare(name2) == 0)))) //rule2 for mainname and subname
        	  	  {
        	  		res.push_back(make_tuple(main_name,sub_name,ans));
        	  		++priority;
        			def++;
        			continue;
        	  	  }
        	  	  if ((rule3 == 3) && (priority == 0) && ((main_name.compare(name) == 0) || (sub_name.compare(name) == 0)) && ((main_type.compare(type2) == 0) || (sub_type.compare(type2) == 0))) //rule3 for mainname and subtype
        	  	  {
        	  		res.push_back(make_tuple(main_name,sub_name,ans));
        	  		def++;

        	  	  }
        	  	}
        	  }
         	      if (def == 0)
        	      {
         	    	res.push_back(make_tuple(main_name,sub_name,rules["default"].as<string>()));
        	  	  }

          }
      }
      return res;
}
