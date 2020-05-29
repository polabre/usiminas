#include "NideaL_DataCollector.h"
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

int main(int argc, char* argv[])
{
	// get interfaces from nideal (mysql)
	mysqlResultsMatrix interfaces = GetQueryResultsFromMySQL("Call InterfacesEnum();");

	// create datacollectors array
	DataCollector* groups = new DataCollector[interfaces.rows];

	// create items for each group
	for (int i = 0; i < interfaces.rows; i++)
	{
		char* interfaceAlias = (char*)interfaces.stringMatrix[i][0].c_str(); // interface alias
		char* interfaceName = (char*)interfaces.stringMatrix[i][2].c_str(); // interface name
		char* interfaceIP = (char*)interfaces.stringMatrix[i][3].c_str(); // interface host ip address
		char* interfaceType =  (char*)interfaces.stringMatrix[i][1].c_str(); // interface type
		char* interfacePort = (char*)interfaces.stringMatrix[i][4].c_str(); // interface host port
		char* interfaceUser = (char*)interfaces.stringMatrix[i][5].c_str(); // interface host login user
		char* interfacePassword = (char*)interfaces.stringMatrix[i][6].c_str(); // interface host login password

		// get item tags matrix from mysql
		string query = "Call TagsEnum('" + interfaces.stringMatrix[i][0] + "');";
		mysqlResultsMatrix* tags = new mysqlResultsMatrix;
		*tags = GetQueryResultsFromMySQL((char*)query.c_str());

		// re-create interface group (efective constructor)
		groups[i].~DataCollector();
		new(&groups[i]) DataCollector(interfaceAlias, interfaceName, interfaceIP, interfaceType, interfacePort, interfaceUser, interfacePassword);

		// start data collector thread
		groups[i].Run(*tags);
	}

	// previous watchdog initialization
	int* previousWatchdog = new int[interfaces.rows];
	for (int i = 0; i < interfaces.rows; i++)
		previousWatchdog[i]= 39000; // random arbitrary value

	// timeoutCounter initialization
	int* timeoutCounter = new int[interfaces.rows];
	for (int i = 0; i < interfaces.rows; i++)
		timeoutCounter[i] = 0;

	// infinite loop
	while(true)
	{
		for (int i = 0; i < interfaces.rows; i++)
		{
			if (groups[i].threadWatchdog == previousWatchdog[i]) // frozen threadWatchdog test
			{
				if(timeoutCounter[i] < THREAD_TIMEOUT) // while counter < timeout
					timeoutCounter[i]++; // add timeout counter
			}
			else
			{
				timeoutCounter[i] = 0; // timeout counter reset
			}

			if (timeoutCounter[i] >= THREAD_TIMEOUT) // case timeout
			{
				if (timeoutCounter[i] == THREAD_TIMEOUT)
				{
					printf("Working thread %i timeout.\n",i);
					groups[i].~DataCollector(); // destroy object
					printf("Object %i deleted. Thread killed.\n",i);
				}

				timeoutCounter[i]++;

				if (timeoutCounter[i] >= (THREAD_TIMEOUT + RETRY_TIME)) // wait RETRY TIME
				{
					// get opc server info again
					mysqlResultsMatrix srvs = GetQueryResultsFromMySQL("Call InterfacesEnum();");

					char* iAlias = (char*)srvs.stringMatrix[i][0].c_str(); // interface alias
					char* iName = (char*)srvs.stringMatrix[i][2].c_str(); // interface name
					char* ipAdd = (char*)srvs.stringMatrix[i][3].c_str(); // interface ip address
					char* itype = (char*)srvs.stringMatrix[i][1].c_str(); // interface type
					char* iPort = (char*)srvs.stringMatrix[i][4].c_str(); // interface host port
					char* iUser = (char*)srvs.stringMatrix[i][5].c_str(); // interface host login user
					char* iPswd = (char*)srvs.stringMatrix[i][6].c_str(); // interface host login password

					// get item tags matrix from mysql
					string q = "Call TagsEnum('" + srvs.stringMatrix[i][0] + "');";
					mysqlResultsMatrix* tags = new mysqlResultsMatrix;
					*tags = GetQueryResultsFromMySQL((char*)q.c_str());

					// re-create opc group (efective constructor)
					new(&groups[i]) DataCollector(iAlias, iName, ipAdd, itype, iPort, iUser, iPswd);

					// start data collector thread
					groups[i].Run(*tags);

					timeoutCounter[i] = 0;
				}
			}

			previousWatchdog[i] = groups[i].threadWatchdog; // previous watchdog update
		}
		Sleep(1000); // wait time (update scan) (1s)
	}
	return 0;
};
