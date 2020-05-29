#ifndef SSOPC_H
#define SSOPC_H

#include <string>
#include <comdef.h>
#include "opc.h"

using namespace std;

//----------------------------------------------------------

class SSOPC_Server;

class SSOPC_Exception
{
public:
	SSOPC_Exception(string errTxt);
	string ErrDesc();
private:
	string m_errTxt;
};

//----------------------------------------------------------

class SSOPC_COMLib
{
public:
	SSOPC_COMLib();
	~SSOPC_COMLib();
};

//----------------------------------------------------------

class SSOPC_Group
{
public:
	SSOPC_Group(SSOPC_Server *ownedOpcSvr);
	SSOPC_Group(){};
	~SSOPC_Group();
	void InitGroup();
	void AddItems(_bstr_t itemNames[], int num);
	_variant_t *ReadAllItems();
	void WriteItems(int itemIndex[], _variant_t values[], int num);
private:
	SSOPC_Server *m_ownedServer;
	IOPCItemMgt	 *m_IOPCItemMgt;
	IOPCSyncIO	 *m_IOPCSyncIO;
	OPCHANDLE	 m_hServerGroup;
	OPCHANDLE    *m_hServerItems;
	int			 m_numOfItems;
	_variant_t	 *m_itemValues;
};

//----------------------------------------------------------

class SSOPC_Server
{
public:
	SSOPC_Server();
	~SSOPC_Server();
	SSOPC_Group *InitServer(bstr_t opcSvrName, _bstr_t opcSrvAddress);	
	IOPCServer *GetIOPCServer();
private:
	IOPCServer  *m_IOPCServer;
	SSOPC_Group *m_pOPCGroup;
};

//----------------------------------------------------------

#endif
