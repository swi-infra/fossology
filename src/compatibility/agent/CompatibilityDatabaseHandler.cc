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
 * @brief Data base handler for COMPATIBILITY
 */

#include "CompatibilityDatabaseHandler.hpp"


using namespace fo;
using namespace std;

/**
 * Default constructor for CompatibilityDatabaseHandler
 * @param dbManager DBManager to be used
 */
CompatibilityDatabaseHandler::CompatibilityDatabaseHandler(DbManager dbManager) :
    fo::AgentDatabaseHandler(dbManager)
{
}

/**
 * Get a vector of all file id for a given upload id.
 * @param uploadId Upload ID to be queried
 * @return List of all pfiles for the given upload
 */
vector<unsigned long> CompatibilityDatabaseHandler::queryFileIdsForUpload(int uploadId)
{
  return queryFileIdsVectorForUpload(uploadId, false);
}

/**
 * Get a vector of all file id for a given upload id which are not scanned by the given agentId.
 * @param uploadId Upload ID to be queried
 * @param agentId  ID of the agent
 * @return List of all pfiles for the given upload
 */
vector<unsigned long> CompatibilityDatabaseHandler::queryFileIdsForScan(int uploadId, int agentId)
{
  string uploadtreeTableName = queryUploadTreeTableName(uploadId);

  QueryResult queryResult = dbManager.execPrepared(
    fo_dbManager_PrepareStamement(dbManager.getStruct_dbManager(),
      ("pfileForUploadFilterAgent" + uploadtreeTableName).c_str(),
      ("SELECT distinct(ut.pfile_fk) FROM " + uploadtreeTableName + " AS ut "
      "INNER JOIN license_file AS lf ON ut.pfile_fk = lf.pfile_fk "
      "LEFT JOIN comp_result AS cr ON ut.pfile_fk = cr.pfile_fk "
      "AND cr.agent_fk = $2 WHERE cr.pfile_fk IS NULL "
      "AND ut.upload_fk = $1 AND (ut.ufile_mode&x'3C000000'::int)=0;").c_str(),
      int, int),
    uploadId, agentId);

  return queryResult.getSimpleResults(0, fo::stringToUnsignedLong);
}

/**
 * @brief to get the license id from the file id
 * @param pFileId
 * @return List of licenses for the given file id
 */
vector<unsigned long> CompatibilityDatabaseHandler::queryLicIdsFromPfile(unsigned long pFileId)	//to get licId from pfile_id
{
	QueryResult queryResult = dbManager.execPrepared(
		fo_dbManager_PrepareStamement(dbManager.getStruct_dbManager(),
				"myTable",
		      ("select  distinct rf_fk from license_file inner join license_ref on rf_fk=rf_pk and rf_shortname not in('Dual-license', 'No_license_found', 'Void') where pfile_fk= $1 "),
			  unsigned long),
		    pFileId);

	return queryResult.getSimpleResults(0, fo::stringToUnsignedLong);
}

/**
 * @brief store the id and type of that respective license in vector of tuple for all the licenses
 * @param licId
 * @return vector of tuple
 */
vector<tuple<unsigned long, string>> CompatibilityDatabaseHandler::queryLicDetails(vector<unsigned long> licId)
{
	vector<tuple<unsigned long, string>> vec;
	for(vector<unsigned long> ::iterator i=licId.begin(); i!=licId.end(); ++i)
		{
		QueryResult queryResult = dbManager.execPrepared(
			fo_dbManager_PrepareStamement(dbManager.getStruct_dbManager(),
					"myTable2",
				 ("SELECT rf_licensetype FROM license_ref WHERE rf_pk= $1 "),
				 unsigned long),
				 *i);
		vector<string> vec2=queryResult.getRow(0);
		vec.push_back(make_tuple(*i, vec2[0]));
		}
	return vec;
}

/**
 * @brief this rule uses id of both the licenses to find the compatibility
 * @param lic1 holding license 2 information i.e. license id and license type
 * @param lic2 holding license 2 information i.e. license id and license type
 * @return integer if 1 then compatibility is true, 0 then compatibility is false otherwise 2
 */
int CompatibilityDatabaseHandler::queryRule1(tuple<unsigned long, string> lic1,tuple<unsigned long, string> lic2)
{
	QueryResult queryResult = dbManager.execPrepared(
		fo_dbManager_PrepareStamement(dbManager.getStruct_dbManager(),
				"myTable3",
			("SELECT compatibility FROM license_rules WHERE (main_rf_fk = $1 and sub_rf_fk = $2) or (sub_rf_fk = $1 and main_rf_fk = $2)"),
			unsigned long, unsigned long),
			   get<0>(lic1), get<0>(lic2));
	if(queryResult.getRowCount() == 0)
	{
		return 2;
	}
	else
	{
		bool result= queryResult.getSimpleResults(0, fo::stringToBool)[0];
		if(result)
		{
			return 1;
		}
		else
			return 0;
	}

}

/**
 * @brief this rule uses license type of both the licenses to find the compatibility
 * @param lic1 holding license 1 information
 * @param lic2 holding license 2 information
 * @return integer if 1 then compatibility is true, 0 then compatibility is false otherwise 2
 */
int CompatibilityDatabaseHandler::queryRule2(tuple<unsigned long, string> lic1, tuple<unsigned long, string> lic2)
{
	QueryResult queryResult = dbManager.execPrepared(
		fo_dbManager_PrepareStamement(dbManager.getStruct_dbManager(),
				"myTable4",
			("SELECT compatibility FROM license_rules WHERE (main_type = $1 and sub_type = $2) or (sub_type = $1 and main_type = $2)"),
			 char*, char*),
			   get<1>(lic1).c_str(), get<1>(lic2).c_str());
	if(queryResult.getRowCount() == 0)
	{
		return 2;
	}
	else
	{
		bool result= queryResult.getSimpleResults(0, fo::stringToBool)[0];
		if(result)
		{
			return 1;
		}
		else
			return 0;
	}

}

/**
 * @brief this rule uses license id and license type to find the compatibility
 * @param lic1 holding license 1 information
 * @param lic2 holding license 2 information
 * @return integer if 1 then compatibility is true, 0 then compatibility is false otherwise 2
 */
int CompatibilityDatabaseHandler::queryRule3(tuple<unsigned long, string> lic1,tuple<unsigned long, string> lic2)
{
	QueryResult queryResult = dbManager.execPrepared(
			fo_dbManager_PrepareStamement(dbManager.getStruct_dbManager(),
					"myTable5",
				("SELECT compatibility FROM license_rules WHERE (main_rf_fk = $1 and sub_type = $2) or (main_rf_fk = $3 and sub_type = $4)"),
				unsigned long, char*, unsigned long, char*),
				   get<0>(lic1), get<1>(lic2).c_str(), get<0>(lic2), get<1>(lic1).c_str());

	if(queryResult.getRowCount() == 0)
	{
		return 2;
	}
	else
	{
		bool result= queryResult.getSimpleResults(0, fo::stringToBool)[0];
		if(result)
		{
			return 1;
		}
		else
			return 0;
	}

}

/**
 * @brief checking wether the data is already present in database or not, to prevent redundancy
 * @param id1 holding license 1 information
 * @param id2 holding license 2 information
 * @return boolean
 */
bool CompatibilityDatabaseHandler::check(unsigned long id1, unsigned long id2, unsigned long pFileId)
{
	QueryResult queryResult = dbManager.execPrepared(
			fo_dbManager_PrepareStamement(dbManager.getStruct_dbManager(),
					"myTable7",
				("SELECT exists(SELECT 1 FROM comp_result WHERE ((first_rf_fk= $1 AND second_rf_fk= $2) OR (second_rf_fk= $1 AND first_rf_fk= $2)) AND pfile_fk = $3)"),
				   unsigned long, unsigned long,  unsigned long),
				   id1, id2, pFileId);
	bool res= queryResult.getSimpleResults(0, fo::stringToBool)[0];
	return res;
}

/**
 * @brief insert the compatibility result in the comp_result table
 * @param pFileId file id
 * @param a_id agent id
 * @param id1 first license id
 * @param id2 second license id
 * @param comp storing the compatibility result
 * @return boolean
 */
bool CompatibilityDatabaseHandler::queryInsertResult(unsigned long pFileId, int a_id, unsigned long id1, unsigned long id2, string comp)
{
	QueryResult queryResult = dbManager.execPrepared(
	    fo_dbManager_PrepareStamement(dbManager.getStruct_dbManager(),
	    	"CompInsertResult",
	    	"INSERT INTO comp_result"
	    	"(pfile_fk, agent_fk, first_rf_fk, second_rf_fk, result)"
	    	"VALUES($1, $2, $3, $4, $5)",
			unsigned long, int, unsigned long, unsigned long, char*),
			pFileId, a_id, id1, id2, comp.c_str());
	bool res = queryResult.isFailed();
	if(res)
	{
		return false;
	}
	else
	{
		return true;
	}
}
/**
 * Spawn a new DbManager object.
 *
 * Used to create new objects for threads.
 * @return DbManager object for threads.
 */
CompatibilityDatabaseHandler CompatibilityDatabaseHandler::spawn() const
{
  DbManager spawnedDbMan(dbManager.spawn());
  return CompatibilityDatabaseHandler(spawnedDbMan);
}
