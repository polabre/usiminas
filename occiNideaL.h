#include <iostream>
#include <occi.h>
#include <vector>

using namespace oracle::occi;
using namespace std;

struct oracleResultsMatrix
{
	string** stringMatrix;
	int rows;
	int columns;

	//oracleResultsMatrix()
	//{
	//
	//}

	//oracleResultsMatrix(int iRows, int iCols)
	//{
	//	rows = iRows;
	//	columns = iCols;

	//	if (iRows > 0)
	//	{
	//		stringMatrix = new string*[rows];
	//		for(int i = 0; i < rows; i++)
	//		{
	//			stringMatrix[i] = new string[columns];
	//		}
	//	}
	//}

	//oracleResultsMatrix(const oracleResultsMatrix &obj)
	//{
	//	rows = obj.rows;
	//	columns = obj.columns;

	//	stringMatrix = new string*[rows];
	//	for(int i = 0; i < rows; i++)
	//	{
	//		stringMatrix[i] = new string[columns];
	//	}

	//	for(int i = 0; i < rows; i++)
	//		for(int j = 0; j < columns; j++)
	//			stringMatrix[i][j] = obj.stringMatrix[i][j];
	//}

	//~oracleResultsMatrix()
	//{
	//	if (rows > 0)
	//	{
	//		for (int i = 0; i < columns; i++)
	//			delete[] stringMatrix[i];
	//		delete[] stringMatrix;
	//	}
	//}
};

class occiNideaL
{
private:
	Environment* env;
	Connection* conn;

public:
	string sqlStmt;

	occiNideaL(string name, string ip, string port, string user, string password)
	{
		string sdb = "//" + ip + ":" + port + "/" + name;
		env = Environment::createEnvironment(Environment::DEFAULT);
		conn = env->createConnection(user, password, sdb);
	};

	~occiNideaL()
	{
		env->terminateConnection(conn);
		Environment::terminateEnvironment(env);
	};

	void DisplayAllRows()
	{
		Statement* stmt;
		stmt = conn->createStatement(sqlStmt);
		ResultSet* rset = stmt->executeQuery();

		// get number of columns of ResultSet
		vector<MetaData>* metaData  =  new vector<MetaData>(rset->getColumnListMetaData());
		int numCols = metaData->size();

		try
		{
			while (rset->next())
			{
				for (int i = 0; i < numCols; i++)
				{
					string* val = new string(rset->getString(i + 1));
					cout << *val << " ";
				}
				cout << endl;
			}
		}

		catch(SQLException ex)
		{
			cout<<"Exception thrown for displayAllRows"<<endl;
			cout<<"Error number: "<< ex.getErrorCode() << endl;
			cout<<ex.getMessage() << endl;
		}

		stmt->closeResultSet(rset);
		conn->terminateStatement(stmt);
	};

	oracleResultsMatrix GetQueryResults()
	{
		Statement* stmt;
		stmt = conn->createStatement(sqlStmt);
		ResultSet* rset = stmt->executeQuery();

		// get number of columns of ResultSet
		vector<MetaData>* metaData  =  new vector<MetaData>(rset->getColumnListMetaData());
		int numCols = metaData->size();

		// get number of rows of ResultSet
		int numRows = 0;
		try
		{
			while (rset->next())
			{
				numRows++;
			}
		}
		catch(SQLException ex)
		{
			cout<<"Exception thrown for GetQueryResults"<<endl;
			cout<<"Error number: "<< ex.getErrorCode() << endl;
			cout<<ex.getMessage() << endl;
		}

		rset->cancel();

		rset = stmt->executeQuery();

		oracleResultsMatrix finalResult;

		// prepare finalResult
		finalResult.stringMatrix = new string*[numRows];
		for(int i = 0; i < numRows; i++)
		{
			finalResult.stringMatrix[i] = new string[numCols];
		}

		try
		{
			int i = 0;
			while (rset->next())
			{
				for (int j = 0; j < numCols; j++)
				{
					string* val = new string(rset->getString(j + 1));

					finalResult.stringMatrix[i][j] = *val;

					// put dot instead comma returned
					if (finalResult.stringMatrix[i][j].find(",") != string::npos)
						if (finalResult.stringMatrix[i][j].find(",") == 0)
							finalResult.stringMatrix[i][j].replace(finalResult.stringMatrix[i][j].find(","), 1, "0.");
						else
							finalResult.stringMatrix[i][j].replace(finalResult.stringMatrix[i][j].find(","), 1, ".");
				}
				i++;
			}
		}
		catch(SQLException ex)
		{
			cout<<"Exception thrown for GetQueryResults"<<endl;
			cout<<"Error number: "<< ex.getErrorCode() << endl;
			cout<<ex.getMessage() << endl;
		}

		stmt->closeResultSet(rset);
		conn->terminateStatement(stmt);

		return finalResult;
	};
}; // end of class occiNideaL
