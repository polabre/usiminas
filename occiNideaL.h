#include <iostream>
#include <occi.h>
#include <vector>

using namespace oracle::occi;
using namespace std;

class occiNideaL
{
private:
	Environment* env;
	Connection* conn;
	Statement* stmt;

public:
	occiNideaL(string usr, string psw, string sdb)
	{
		env = Environment::createEnvironment(Environment::DEFAULT);
		conn = env->createConnection(usr, psw, sdb);
	}

	~occiNideaL()
	{
		env->terminateConnection(conn);
		Environment::terminateEnvironment(env);
	}

	void DisplayAllRows()
	{
		string sqlStmt = "SELECT REGEXP_REPLACE(HS_OC.VS_TF_CILINDRO.id_cilindro,'[[:space:]]{2,}', ''), REGEXP_REPLACE(HS_OC.VS_TF_CILINDRO.nm_abrev_tipo_conjunto,'[[:space:]]{2,}', ''), REGEXP_REPLACE(HS_OC.VS_TF_CAMPANHA_CILINDRO.ds_cadeira_laminador,'[[:space:]]{2,}', ''), REGEXP_REPLACE(HS_OC.VS_TF_CILINDRO.cd_laminador,'[[:space:]]{2,}', ''), TO_CHAR(HS_OC.VS_TF_CAMPANHA_CILINDRO.dt_colocacao_laminador,'YYYY-MM-DD HH24:MI:SS'), TO_CHAR(HS_OC.VS_TF_CAMPANHA_CILINDRO.dt_saida_laminador,'YYYY-MM-DD HH24:MI:SS'), TO_CHAR(HS_OC.VS_TF_USINAGEM_CILINDRO.dt_inicio_usinagem,'YYYY-MM-DD HH24:MI:SS'), TO_CHAR(HS_OC.VS_TF_USINAGEM_CILINDRO.dt_fim_usinagem,'YYYY-MM-DD HH24:MI:SS'), HS_OC.VS_TF_USINAGEM_CILINDRO.vl_dureza_cil_cen_fim, HS_OC.VS_TF_USINAGEM_CILINDRO.vl_dureza_cil_lmo_fim, HS_OC.VS_TF_USINAGEM_CILINDRO.vl_dureza_cil_lop_fim, HS_OC.VS_TF_USINAGEM_CILINDRO.dm_inicial_prod_usinagem, HS_OC.VS_TF_USINAGEM_CILINDRO.dm_final_prod_usinagem, (HS_OC.VS_TF_USINAGEM_CILINDRO.dm_inicial_prod_usinagem - HS_OC.VS_TF_USINAGEM_CILINDRO.dm_final_prod_usinagem) FROM HS_OC.VS_TF_CAMPANHA_CILINDRO LEFT JOIN HS_OC.VS_TF_CILINDRO ON HS_OC.VS_TF_CILINDRO.id_cilindro = HS_OC.VS_TF_CAMPANHA_CILINDRO.id_cilindro LEFT JOIN HS_OC.VS_TF_USINAGEM_CILINDRO ON HS_OC.VS_TF_USINAGEM_CILINDRO.id_cilindro = HS_OC.VS_TF_CILINDRO.id_cilindro AND HS_OC.VS_TF_USINAGEM_CILINDRO.dt_colocacao_laminador = HS_OC.VS_TF_CAMPANHA_CILINDRO.dt_colocacao_laminador GROUP BY HS_OC.VS_TF_CILINDRO.id_cilindro, HS_OC.VS_TF_CILINDRO.nm_abrev_tipo_conjunto, HS_OC.VS_TF_CAMPANHA_CILINDRO.ds_cadeira_laminador, HS_OC.VS_TF_CILINDRO.cd_laminador, HS_OC.VS_TF_CAMPANHA_CILINDRO.dt_colocacao_laminador, HS_OC.VS_TF_CAMPANHA_CILINDRO.dt_saida_laminador, HS_OC.VS_TF_USINAGEM_CILINDRO.dt_inicio_usinagem, HS_OC.VS_TF_USINAGEM_CILINDRO.dt_fim_usinagem, HS_OC.VS_TF_USINAGEM_CILINDRO.vl_dureza_cil_cen_fim, HS_OC.VS_TF_USINAGEM_CILINDRO.vl_dureza_cil_lmo_fim, HS_OC.VS_TF_USINAGEM_CILINDRO.vl_dureza_cil_lop_fim, HS_OC.VS_TF_USINAGEM_CILINDRO.dm_inicial_prod_usinagem, HS_OC.VS_TF_USINAGEM_CILINDRO.dm_final_prod_usinagem, (HS_OC.VS_TF_USINAGEM_CILINDRO.dm_inicial_prod_usinagem - HS_OC.VS_TF_USINAGEM_CILINDRO.dm_final_prod_usinagem) HAVING HS_OC.VS_TF_CILINDRO.nm_abrev_tipo_conjunto = 'TRA' AND HS_OC.VS_TF_CILINDRO.cd_laminador = 'LE2' AND HS_OC.VS_TF_CAMPANHA_CILINDRO.dt_colocacao_laminador BETWEEN TO_DATE('2020-04-16 00:00:00','YYYY-MM-DD HH24:MI:SS') AND TO_DATE('2020-10-10 23:00:00','YYYY-MM-DD HH24:MI:SS') ORDER BY HS_OC.VS_TF_CAMPANHA_CILINDRO.dt_colocacao_laminador";

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
	}
}; // end of class occiNideaL
