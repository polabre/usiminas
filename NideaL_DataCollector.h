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

// oracle database connection parameters
//string user = "DANIEL";
//string passwd = "sa";
//string db = "//localhost:1522/NIDEAL";

// oracle database connection parameters
const string user = "USER_LF";
const string passwd = "USER_LF";
const string db = "//10.100.180.150:1552/UCBOPH0";

// update interval (ms)
const DWORD UPD_INTERVAL = 950;

// retry time (s)
const int RETRY_TIME = 15;

// thread timeout (s)
const int THREAD_TIMEOUT = 15;

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

// function to replace substring
string FindAndReplaceString(string stToFind, string mainString, string stReplaceFor)
{
	size_t pos = mainString.find(stToFind);
	return mainString.replace(pos, stToFind.length(), stReplaceFor);
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

// Data Collector class (OPC Client + historian)
class DataCollector
{
private:
	bool enableUpd; // enable working thread updated

	mysqlResultsMatrix intTagsMatrix; // interface tags matrix

	char* intName; //interface name
	char* intIP; // interface ip address
	char* intType; //interface type

	HANDLE threadHandle; // working thread handle

	void SetTagsMatrix(mysqlResultsMatrix matrix) // just copy
	{
		intTagsMatrix = matrix;
	};

	// Data collector thread
	static unsigned __stdcall WorkingThread_opc_read(void* param)
	{
		DataCollector* dc = (DataCollector*)param; // param cast

		// mysql database connection
		MYSQL* conn;
		conn = mysql_init (NULL);
		mysql_real_connect (conn, hostName, userName, password, schemaName, portNum, socketName, flags);

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

		if (pOpcGrp != NULL) // if group exists
		{
			pOpcGrp->AddItems(opcItems, dc->intTagsMatrix.rows);
			while(dc->enableUpd) // loop if update enabled
			{
				itemsValues = pOpcGrp->ReadAllItems(); // opc items reading
				if (itemsValues[0].vt != NULL) // if first value is not null
				{
					string insertQueryValues = "("; // string to store insert values

					for (int i = 0; i < dc->intTagsMatrix.rows; i++)
					{
						if (dc->intTagsMatrix.stringMatrix[i][0] == "rolo")
						{
							string sRolo = (string)(_bstr_t)itemsValues[i];

							// new data detection
							if (sRolo == prevRolo)
								bNewData = false;
							else
								bNewData = true;
							prevRolo = sRolo;

							// get detailed information from rolo tag
							cout << "rolo = " << sRolo.substr(0,6) << endl;																	insertQueryValues.append(sRolo.substr(0,6) + ", ");
							cout << "divisao = " << sRolo.substr(6,2) << endl;																insertQueryValues.append(sRolo.substr(6,2) + ", ");
						}
						else
						{
							if(dc->intTagsMatrix.stringMatrix[i][0] == "status")
							{
								cout << dc->intTagsMatrix.stringMatrix[i][0] << " = " << (string)(_bstr_t)itemsValues[i] << " >>> ";

								string sStatus = (string)(_bstr_t)itemsValues[i];															insertQueryValues.append(sStatus + ", ");
								unsigned int iStatus = stol(sStatus);

								bool bStatus[32];
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
								cout << dc->intTagsMatrix.stringMatrix[i][0] << " = " << (string)(_bstr_t)itemsValues[i] << endl;			insertQueryValues.append((string)(_bstr_t)itemsValues[i] + ", ");
							}
						}
					}

					if (bNewData)
					{
						insertQueryValues.pop_back(); insertQueryValues.pop_back(); insertQueryValues.append(");");
						cout << insertQueryValues << endl;

						string query = "CALL InsertDB_LE02" + insertQueryValues;
						mysql_query(conn, (char*)query.c_str()); // execute MySQL query
					}

					dc->threadWatchdog++; // thread watchdog
					if (dc->threadWatchdog > 32767) // avoid watchdog overflow
					{
						dc->threadWatchdog = 0;
					}
				}

				Sleep(UPD_INTERVAL); // wait time (update scan)
			}
		}

		delete pOpcSvr; // close opc connection
		mysql_close(conn); // close mysql connection

		return 0;
	};

public:
	int threadWatchdog; // working thread watchdog (conn monitoring)

	// void constructor
	DataCollector(){};

	// efective constructor
	DataCollector(char* interfaceName, char* interfaceIP, char* interfaceType)
	{
		threadWatchdog = 0; // watchdog ini

		enableUpd = true; // enable update ini

		intName = interfaceName; // name copy

		intIP = interfaceIP; // ip address copy

		intType = interfaceType; // type copy
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
	};
};
