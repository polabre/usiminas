#include "stdafx.h"

#include <mysql.h>
#include "ssopc.h"

#include "modelNN.h"

#include "occiNideaL.h"

using namespace std;

// mysql database connection parameters
static char* hostName = "localhost"; // server host (default=localhost)
static char* userName = "root"; // username (default=login name)
static char* password = "sa"; // password (default=none)
static unsigned int portNum = 3306; // port number (use built-in value)
static char* socketName = NULL; // socket name (use built-in value)
static char* schemaName = "nideal"; // database name (default=none)
static unsigned int flags = CLIENT_MULTI_STATEMENTS; // connection flags

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

// update interval (ms)
const DWORD UPD_INTERVAL = 950;

// retry time (s)
const int RETRY_TIME = 180;

// thread timeout (s)
const int THREAD_TIMEOUT = 300;

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

// struct to store mysql query results
struct mysqlResultsMatrix
{
	string** stringMatrix;
	int rows;
	int columns;
};

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

// function to return OPC servers from mysql
mysqlResultsMatrix GetQueryResultsFromMySQL(char* query)
{
	mysqlResultsMatrix finalResult; // return of this function

	MYSQL* conn;
	MYSQL_RES* res;
	MYSQL_ROW row;

	conn = mysql_init (NULL);

	mysql_real_connect (conn, hostName, userName, password, schemaName, portNum, socketName, flags);
	mysql_query(conn, query); // query to enumerate opc servers

	res = mysql_store_result(conn); // get query results

	int field_count = mysql_num_fields(res); // get number of columns
	int row_count = mysql_num_rows(res); // get number of rows

	if ((field_count > 0) && (row_count > 0))
	{
		// create string matrix
		string** resMatrix = new string*[row_count];
		for(int i = 0; i < row_count; i++)
		{
			resMatrix[i] = new string[field_count];
		}

		// copy results to the string matrix
		int i = 0;
		int j = 0;
		while ((row = mysql_fetch_row(res)) != NULL)
		{
			for(j = 0; j < field_count; j++)
			{
				if (row[j] != NULL)
					resMatrix[i][j] = row[j];
				else
					resMatrix[i][j] = "NULL";
			}
			i++;
		}

		// prepare the final variable to be returned
		finalResult.stringMatrix = resMatrix;
		finalResult.rows = i;
		finalResult.columns = j;
	}
	else
	{
		printf("Query %s returned void result. \n", query);
	}

	// clean up the database result set
	mysql_free_result(res);

	// clean up the database link
	mysql_close(conn);

	return finalResult;
};

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

// Data Collector class (OPC Client + historian)
class DataCollector
{
private:
	bool enableUpd; // enable working thread updated

	mysqlResultsMatrix intTagsMatrix; // interface tags matrix

	char* intAlias; // interface alias
	char* intName; //interface name
	char* intIP; // interface ip address
	char* intType; //interface type
	char* intPort; //interface port
	char* intUser; //interface user
	char* intPassword; //interface password

	HANDLE threadHandle; // working thread handle

	//-------------------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------------------

	void SetTagsMatrix(mysqlResultsMatrix matrix) // just copy
	{
		intTagsMatrix = matrix;
	};

	//-------------------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------------------

	// Thread for opc read
	static unsigned __stdcall WorkingThread_opc_read(void* param)
	{
		DataCollector* dc = (DataCollector*)param; // param cast

		// get opcItems to be read
		_bstr_t* opcItems = new _bstr_t[dc->intTagsMatrix.rows];

		for(int n = 0; n < dc->intTagsMatrix.rows; n++)
		{
			opcItems[n] = (char*)dc->intTagsMatrix.stringMatrix[n][2].c_str();
		}

		// opc server connection
		SSOPC_COMLib g_cl;
		SSOPC_Server *pOpcSvr = new SSOPC_Server;
		SSOPC_Group *pOpcGrp = pOpcSvr->InitServer(dc->intName, dc->intIP);

		// store opc values returned
		_variant_t* itemsValues = new _variant_t; // store opc values return

		string prevRolo = " "; // previous value for rolo
		bool bNewData = false; // new data detection
		bool bFirstScan = true; // first scan bit
		bool bStatus[32]; // status word bits

		string insertQueryValues;
		string sRolo;
		string sStatus;
		unsigned int iStatus;
		string query;

		int iDelay = 0; // update delay

		_variant_t* opcWriteValues = new _variant_t[2]; // opc write tags declaration
		int* indexes = new int[2]; indexes[0] = 18; indexes[1] = 19; // opc write indexes
		double force = 0.0;
		long uiword = 0;

		double neuralInputs[67][1]; // neural network inputs

		bool invalidateForce = false;

		mysqlResultsMatrix inputsNN;

		if (pOpcGrp != NULL) // if group exists
		{
			pOpcGrp->AddItems(opcItems, dc->intTagsMatrix.rows);
			while(dc->enableUpd) // loop if update enabled
			{
				itemsValues = pOpcGrp->ReadAllItems(); // opc items reading
				if (itemsValues[0].vt != NULL) // if first value is not null
				{
					insertQueryValues = "("; // string to store insert values

					for (int i = 0; i < dc->intTagsMatrix.rows; i++)
					{
						if (dc->intTagsMatrix.stringMatrix[i][0] == "rolo")
						{
							sRolo = (string)(_bstr_t)itemsValues[i];

							// new data detection
							if (sRolo == prevRolo)
								bNewData = false;
							else
								bNewData = true;
							prevRolo = sRolo;

							// get detailed information from rolo tag
							cout << "rolo = " << sRolo.substr(0,6) << endl;																	insertQueryValues.append("'" + sRolo.substr(0,6) + "', ");
							cout << "divisao = " << sRolo.substr(6,2) << endl;																insertQueryValues.append("'" + sRolo.substr(6,2) + "', ");
						}
						else
						{
							if(dc->intTagsMatrix.stringMatrix[i][0] == "status")
							{
								cout << dc->intTagsMatrix.stringMatrix[i][0] << " = " << (string)(_bstr_t)itemsValues[i] << " >>> ";

								sStatus = (string)(_bstr_t)itemsValues[i];															insertQueryValues.append("'" + sStatus + "', ");
								iStatus = stol(sStatus);

								for (int w = 0; w < 32; w++)
								{
									bStatus[w] = 0 != (iStatus & (1 << w));
								}

								for (int w = 31; w >= 0; w--)
								{
									cout << bStatus[w];
								}

								cout << " <<<" << endl;
							}
							else
							{
								if ((dc->intTagsMatrix.stringMatrix[i][0] == "nr_cil_inf") || (dc->intTagsMatrix.stringMatrix[i][0] == "nr_cil_sup"))
								{
									if ((int)itemsValues[i] < 100)
									{
										cout << dc->intTagsMatrix.stringMatrix[i][0] << " = LET0" << (string)(_bstr_t)itemsValues[i] << endl;		insertQueryValues.append("'LET0" + (string)(_bstr_t)itemsValues[i] + "', ");
									}
									else
									{
										cout << dc->intTagsMatrix.stringMatrix[i][0] << " = LET" << (string)(_bstr_t)itemsValues[i] << endl;		insertQueryValues.append("'LET" + (string)(_bstr_t)itemsValues[i] + "', ");
									}
								}
								else
								{
									cout << dc->intTagsMatrix.stringMatrix[i][0] << " = " << (string)(_bstr_t)itemsValues[i] << endl;			insertQueryValues.append("'" + (string)(_bstr_t)itemsValues[i] + "', ");
								}
							}
						}
					}

					// prepare insertQueryValues tail
					insertQueryValues.pop_back(); insertQueryValues.pop_back(); insertQueryValues.append(");");

					// detect new data
					if ((bNewData) && (!bFirstScan))
					{
						iDelay = 1;
						
					}

					// add time delay
					if (iDelay > 0)
					{
						iDelay++;
					}

					// update new data
					if (iDelay == 5)
					{
						cout << insertQueryValues << endl;

						query = "CALL InsertDB_LE02" + insertQueryValues;
					
						inputsNN = GetQueryResultsFromMySQL((char*)query.c_str());

						for (int i = 0; i < inputsNN.columns; i++)
						{
							neuralInputs[i][0] = atof((char*)inputsNN.stringMatrix[0][i].c_str());
						}

						force = myNeuralNetworkFunction(neuralInputs) * 1379.3; // adjust force

						uiword = 1;
						iDelay = 0; // reset iDelay
					}

					if (bStatus[3])
						invalidateForce = true;

					if (invalidateForce && !bStatus[3])
					{
						uiword = 0;
						invalidateForce = false;
					}

					opcWriteValues[0] = force; // set opc force tag
					opcWriteValues[1] = uiword; // set opc send word tag
					pOpcGrp->WriteItems(indexes, opcWriteValues, 2); // write opc tags

					dc->threadWatchdog++; // thread watchdog
					if (dc->threadWatchdog > 32767) // avoid watchdog overflow
					{
						dc->threadWatchdog = 0;
					}
				}

				bFirstScan = false;
				Sleep(UPD_INTERVAL); // wait time (update scan)
			}
		}

		delete pOpcSvr; // close opc connection

		return 0;
	};

	//-------------------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------------------

	// Thread for oracle read
	static unsigned __stdcall WorkingThread_oracle_rolos(void* param)
	{
		DataCollector* dc = (DataCollector*)param; // param cast

		mysqlResultsMatrix resUpdParam;
		mysqlResultsMatrix resDumb;

		string sName = dc->intName;
		string sIP = dc->intIP;
		string sPort = dc->intPort;
		string sUser = dc->intUser;
		string sPwd = dc->intPassword;

		string sGetUpdateParamsQuery;
		string sMainQuery;
		string mysqlQuery;

		// run query object
		occiNideaL oracleQuery(sName, sIP, sPort, sUser, sPwd);

		if (true) // always true
		{
			while(dc->enableUpd) // loop if update enabled
			{
				// call stored procedure
				sGetUpdateParamsQuery = "Call GetUpdateParams_" + dc->intTagsMatrix.stringMatrix[0][0] + "();";

				// store parameter for main query (oracle database)
				resUpdParam = GetQueryResultsFromMySQL((char*)sGetUpdateParamsQuery.c_str());

				// main query init (oracle database) - copy from tagsMatrix
				sMainQuery = dc->intTagsMatrix.stringMatrix[0][3];

				// insert param into main query
				sMainQuery.replace(sMainQuery.find("#"), 1, resUpdParam.stringMatrix[0][0]);

				oracleQuery.sqlStmt = sMainQuery;
				oracleResultsMatrix res = oracleQuery.GetQueryResults();

				for (int i = 0; i < res.rows; i++)
				{
					mysqlQuery = "CALL Update_"+ dc->intTagsMatrix.stringMatrix[0][0] + "(";

					for (int j = 0; j < res.columns; j++)
					{
						mysqlQuery.append("'" + res.stringMatrix[i][j] + "', ");
					}

					// string end
					mysqlQuery.pop_back(); mysqlQuery.pop_back(); mysqlQuery.append(");");

					// put NULL instead ''
					while (mysqlQuery.find("''") != string::npos)
					{
						mysqlQuery.replace(mysqlQuery.find("''"), 2, "NULL");
					}

					// insert into mysql
					resDumb = GetQueryResultsFromMySQL((char*)mysqlQuery.c_str());// execute MySQL query

				}
				
				for (int i = 0; i < res.columns; i++)
					delete[] res.stringMatrix[i];
				delete[] res.stringMatrix;

				dc->threadWatchdog++; // thread watchdog
				if (dc->threadWatchdog > 32767) // avoid watchdog overflow
				{
					dc->threadWatchdog = 0;
				}
				
				Sleep(UPD_INTERVAL); // wait time (update scan)
			}
		}

		return 0;
	};

	//-------------------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------------------

	// Thread for oracle read
	static unsigned __stdcall WorkingThread_oracle_cilindros_dureza(void* param)
	{
		DataCollector* dc = (DataCollector*)param; // param cast

		mysqlResultsMatrix resUpdParam;
		mysqlResultsMatrix resDumb;

		string sName = dc->intName;
		string sIP = dc->intIP;
		string sPort = dc->intPort;
		string sUser = dc->intUser;
		string sPwd = dc->intPassword;

		string sGetUpdateParamsQuery;
		string sMainQuery;
		string mysqlQuery;
		
		// run query object
		occiNideaL oracleQuery(sName, sIP, sPort, sUser, sPwd);
		
		if (true) // always true
		{
			while(dc->enableUpd) // loop if update enabled
			{
				// call stored procedure
				sGetUpdateParamsQuery = "Call GetUpdateParams_" + dc->intTagsMatrix.stringMatrix[0][0] + "();";

				// store parameter for main query (oracle database)
				resUpdParam = GetQueryResultsFromMySQL((char*)sGetUpdateParamsQuery.c_str());

				// main query init (oracle database) - copy from tagsMatrix
				sMainQuery = dc->intTagsMatrix.stringMatrix[0][3];

				// insert param into main query
				sMainQuery.replace(sMainQuery.find("#"), 1, resUpdParam.stringMatrix[0][0]);

				oracleQuery.sqlStmt = sMainQuery;
				oracleResultsMatrix res = oracleQuery.GetQueryResults();

				for (int i = 0; i < res.rows; i++)
				{
					mysqlQuery = "CALL Update_"+ dc->intTagsMatrix.stringMatrix[0][0] + "(";

					for (int j = 0; j < res.columns; j++)
					{
						mysqlQuery.append("'" + res.stringMatrix[i][j] + "', ");
					}

					// string end
					mysqlQuery.pop_back(); mysqlQuery.pop_back(); mysqlQuery.append(");");

					// put NULL instead ''
					while (mysqlQuery.find("''") != string::npos)
					{
						mysqlQuery.replace(mysqlQuery.find("''"), 2, "NULL");
					}

					// insert into mysql
					resDumb = GetQueryResultsFromMySQL((char*)mysqlQuery.c_str());// execute MySQL query

				}
				
				for (int i = 0; i < res.columns; i++)
					delete[] res.stringMatrix[i];
				delete[] res.stringMatrix;

				dc->threadWatchdog++; // thread watchdog
				if (dc->threadWatchdog > 32767) // avoid watchdog overflow
				{
					dc->threadWatchdog = 0;
				}
				
				Sleep(UPD_INTERVAL); // wait time (update scan)
			}
		}

		return 0;
	};

	//-------------------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------------------

	// Thread for oracle read
	static unsigned __stdcall WorkingThread_oracle_cilindros_rugosidade(void* param)
	{
		DataCollector* dc = (DataCollector*)param; // param cast

		mysqlResultsMatrix resUpdParam;
		mysqlResultsMatrix resDumb;

		string sName = dc->intName;
		string sIP = dc->intIP;
		string sPort = dc->intPort;
		string sUser = dc->intUser;
		string sPwd = dc->intPassword;

		string sGetUpdateParamsQuery;
		string sMainQuery;
		string mysqlQuery;

		
		// run query object
		occiNideaL oracleQuery(sName, sIP, sPort, sUser, sPwd);

		if (true) // always true
		{
			while(dc->enableUpd) // loop if update enabled
			{
				// call stored procedure
				sGetUpdateParamsQuery = "Call GetUpdateParams_" + dc->intTagsMatrix.stringMatrix[0][0] + "();";

				// store parameter for main query (oracle database)
				resUpdParam = GetQueryResultsFromMySQL((char*)sGetUpdateParamsQuery.c_str());

				// main query init (oracle database) - copy from tagsMatrix
				sMainQuery = dc->intTagsMatrix.stringMatrix[0][3];

				// insert param into main query
				sMainQuery.replace(sMainQuery.find("#"), 1, resUpdParam.stringMatrix[0][0]);

				oracleQuery.sqlStmt = sMainQuery;
				oracleResultsMatrix res = oracleQuery.GetQueryResults();

				for (int i = 0; i < res.rows; i++)
				{
					mysqlQuery = "CALL Update_"+ dc->intTagsMatrix.stringMatrix[0][0] + "(";

					for (int j = 0; j < res.columns; j++)
					{
						mysqlQuery.append("'" + res.stringMatrix[i][j] + "', ");
					}

					// string end
					mysqlQuery.pop_back(); mysqlQuery.pop_back(); mysqlQuery.append(");");

					// put NULL instead ''
					while (mysqlQuery.find("''") != string::npos)
					{
						mysqlQuery.replace(mysqlQuery.find("''"), 2, "NULL");
					}

					// insert into mysql
					resDumb = GetQueryResultsFromMySQL((char*)mysqlQuery.c_str());// execute MySQL query

				}

				for (int i = 0; i < res.columns; i++)
					delete[] res.stringMatrix[i];
				delete[] res.stringMatrix;

				dc->threadWatchdog++; // thread watchdog
				if (dc->threadWatchdog > 32767) // avoid watchdog overflow
				{
					dc->threadWatchdog = 0;
				}
				
				Sleep(UPD_INTERVAL); // wait time (update scan)
			}
		}

		return 0;
	};

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

public:
	int threadWatchdog; // working thread watchdog (conn monitoring)

	// void constructor
	DataCollector(){};

	// efective constructor
	DataCollector(char* interfaceAlias, char* interfaceName, char* interfaceIP, char* interfaceType, char* interfacePort, char* interfaceUser, char* interfacePassword)
	{
		threadWatchdog = 0; // watchdog ini
		enableUpd = true; // enable update ini
		intAlias = interfaceAlias; // alias copy
		intName = interfaceName; // name copy
		intIP = interfaceIP; // ip address copy
		intType = interfaceType; // type copy
		intPort = interfacePort; // port copy
		intUser = interfaceUser; // user copy
		intPassword = interfacePassword; // password copy
	};

	// destructor 
	~DataCollector()
	{
		enableUpd = false; // stop working thread update

		WaitForSingleObject(threadHandle, INFINITE); // wait for working thread return
		CloseHandle(threadHandle); // kill the thread
	};

	// run data collector working thread
	void Run(mysqlResultsMatrix matrix)
	{
		SetTagsMatrix(matrix); // set private tags matrix

		// working thread call
		if (strcmp (intType, "opc_read") == 0)
			threadHandle = (HANDLE)_beginthreadex(0, 0, &WorkingThread_opc_read, this, 0, 0);

		//if (strcmp (intAlias, "oracle_rolos") == 0)
		//	threadHandle = (HANDLE)_beginthreadex(0, 0, &WorkingThread_oracle_rolos, this, 0, 0);

		//if (strcmp (intAlias, "oracle_cilindros_dureza") == 0)
		//	threadHandle = (HANDLE)_beginthreadex(0, 0, &WorkingThread_oracle_cilindros_dureza, this, 0, 0);

		//if (strcmp (intAlias, "oracle_cilindros_rugosidade") == 0)
		//	threadHandle = (HANDLE)_beginthreadex(0, 0, &WorkingThread_oracle_cilindros_rugosidade, this, 0, 0);
	};
};
