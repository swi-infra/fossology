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

#include "CompatibilityAgent.hpp"

using namespace std;

/**
 * Default constructor for CompatibilityAgent.
 */
CompatibilityAgent::CompatibilityAgent()
{
}

/**
 * @brief find the compatibility between the licenses using scheduler mode
 * @param licId license id
 * @param pFileId file id
 * @param databaseHandler Database handler to be used
 * @param a_id agent id
 * @return boolean
 */
bool CompatibilityAgent::compatibilityFunc(vector<unsigned long> &licId, unsigned long &pFileId,
  CompatibilityDatabaseHandler &databaseHandler, int a_id)
{
	vector<tuple<unsigned long, string>> myVec;
	string def= "f";
	string compatible= "t";
	bool res, checking;
	string notCompatible= "f";
	myVec= databaseHandler.queryLicDetails(licId);
	int length = myVec.size();
	for(int main=0; main<(length-1); ++main)
	{
		int rule1, rule2, rule3;
		for(int sub=(main+1); sub<length; ++sub)
		{
			rule1= databaseHandler.queryRule1(myVec[main], myVec[sub]);
			checking= databaseHandler.check(get<0>(myVec[main]), get<0>(myVec[sub]), pFileId);
			if(checking == false)
			{
			if(rule1 == 1 )
			{
				cout<<"rule1 "<<pFileId <<endl;
				res = databaseHandler.queryInsertResult(pFileId, a_id, get<0>(myVec[main]),get<0>(myVec[sub]),compatible);
				if(res == false)
				{
					return res;
				}
				continue;
			}
			else if(rule1 == 0 )
			{
				cout<<"rule1 "<<pFileId<<endl;
				res = databaseHandler.queryInsertResult(pFileId, a_id, get<0>(myVec[main]),get<0>(myVec[sub]),notCompatible);
				if(res == false)
				{
					return res;
				}
				continue;
			}

			rule2= databaseHandler.queryRule2(myVec[main], myVec[sub]);
			if(rule2 == 1 )
			{
				cout<<"rule2 "<<pFileId<<endl;
				res = databaseHandler.queryInsertResult(pFileId, a_id, get<0>(myVec[main]),get<0>(myVec[sub]),compatible);
				if(res == false)
				{
					return res;
				}
				continue;
			}
			else if(rule2 == 0 )
			{
				cout<<"rule2 "<<pFileId<<endl;
				res = databaseHandler.queryInsertResult(pFileId, a_id, get<0>(myVec[main]),get<0>(myVec[sub]),notCompatible);
				if(res == false)
				{
					return res;
				}
				continue;
			}

			rule3= databaseHandler.queryRule3(myVec[main], myVec[sub]);
			if(rule3 == 1 )
			{
				cout<<"rule3 "<<pFileId<<" 1"<<endl;
				res = databaseHandler.queryInsertResult(pFileId, a_id, get<0>(myVec[main]),get<0>(myVec[sub]),compatible);
				if(res == false)
				{
					return res;
				}
				continue;
			}
			else if(rule3 == 0 )
			{
				cout<<"rule3 "<<pFileId<<" 0"<<endl;
				res = databaseHandler.queryInsertResult(pFileId, a_id, get<0>(myVec[main]),get<0>(myVec[sub]),notCompatible);
				if(res == false)
				{
					return res;
				}
				continue;
			}
			res = databaseHandler.queryInsertResult(pFileId, a_id, get<0>(myVec[main]),get<0>(myVec[sub]),def);
			cout<<"def"<<endl;
			}
		}
	}
	return true;
}

