#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include "ssopc.h"
#include "opcerror.h"
#include "opccomn.h"


using namespace std;

//----------------------------------------------------------

SSOPC_Exception::SSOPC_Exception(string errTxt)
{
	m_errTxt = errTxt;
}

string SSOPC_Exception::ErrDesc()
{
	return m_errTxt;
}

//----------------------------------------------------------

SSOPC_COMLib::SSOPC_COMLib()
{
	//	Initialize the COM library
	HRESULT hr = CoInitialize(NULL);
	if(FAILED(hr))
		cout << "Initialisation of COM Library failed, hr = " << hr << endl;	
}

SSOPC_COMLib::~SSOPC_COMLib()
{
	//	Uninit COM
	CoUninitialize();
}

//----------------------------------------------------------

SSOPC_Group::SSOPC_Group(SSOPC_Server *ownedOpcSvr)
{
	m_ownedServer = ownedOpcSvr;
	m_IOPCItemMgt = NULL;
	m_hServerGroup = NULL;
	m_IOPCSyncIO = NULL;
	m_hServerItems = NULL;
	m_numOfItems = 0;
	m_itemValues = NULL;
}

SSOPC_Group::~SSOPC_Group()
{
	if(m_IOPCSyncIO != NULL) m_IOPCSyncIO->Release();
	if(m_IOPCItemMgt != NULL) m_IOPCItemMgt->Release();

	if(m_hServerGroup != NULL)
	{
		//	Remove Group
		IOPCServer *pIOPCServer = m_ownedServer->GetIOPCServer();
		
		HRESULT hr = pIOPCServer->RemoveGroup(m_hServerGroup, FALSE);
		if (hr != S_OK)
			cout << "RemoveGroup failed, hr = " << hr << endl;		
	}

	if(m_hServerItems != NULL) delete [] m_hServerItems;
	if(m_itemValues != NULL) delete [] m_itemValues; 
}

void SSOPC_Group::InitGroup()
{
	//	Add a group object with a unique name
	HRESULT hr;

	OPCHANDLE 
		clientHandle = 1;
	
	DWORD 
		reqUptRate = 1000, 
		revisedUptRate;
	
	DWORD 
		lcid = 0x409; // Code 0x409 = ENGLISH

	IOPCItemMgt *itemMgt = NULL;
	OPCHANDLE grpHdl = NULL;
	IOPCServer *pIOPCServer = m_ownedServer->GetIOPCServer();

	hr = pIOPCServer->AddGroup(L"",						
						FALSE,							
						reqUptRate,						
						clientHandle,					
						NULL,							
						NULL,							
						lcid,							
						&m_hServerGroup,				
						&revisedUptRate,				
					    IID_IOPCItemMgt,				
						(LPUNKNOWN*)&m_IOPCItemMgt);	
	if(FAILED(hr))
	{
		cout << "Can't add Group to Server, hr = " << hr << endl;
	}
	else
	{
		//	query interface for sync calls on group object
		hr = m_IOPCItemMgt->QueryInterface(IID_IOPCSyncIO, (void**)&m_IOPCSyncIO);
	}
}

void SSOPC_Group::AddItems(_bstr_t itemNames[], int num)
{

	OPCITEMDEF *itemDefs = NULL;
	OPCITEMRESULT *pAddResult = NULL;
	HRESULT *pErrors = NULL;
	HRESULT hr;
	stringstream ss;
	
	m_numOfItems = num;
	itemDefs = new OPCITEMDEF[m_numOfItems];
	m_itemValues = new _variant_t[m_numOfItems];

	for(int i=0; i<m_numOfItems; i++)
	{
		//	define an item table with one item as in-paramter for AddItem 
		itemDefs[i].szAccessPath		  = _bstr_t("");			//	Accesspath not needed
		itemDefs[i].szItemID			  = itemNames[i];		//	ItemID
		itemDefs[i].bActive			  = FALSE;			
		itemDefs[i].hClient			  = 1;
		itemDefs[i].dwBlobSize		  = 0;
		itemDefs[i].pBlob			  = NULL;
		itemDefs[i].vtRequestedDataType = 0;				//	return values in native (cannonical) datatype 
														//	defined by the item itself
	}

	hr = m_IOPCItemMgt->AddItems(m_numOfItems, itemDefs, &pAddResult, &pErrors);	
	if(hr != S_OK)
	{
		
		ss << "AddItems failed, hr = " << hr;
		
		if( hr == S_FALSE)
		{
			for(int i=0; i<m_numOfItems; i++)
			{
				if(pErrors[i] != S_OK)
					ss << "sub hr[" << i << "] = " << pErrors[i];
				cout << "sub hr[" << i << "] = " << pErrors[i] << endl;
			}
		}
	}
	else
	{
		m_hServerItems = new OPCHANDLE[m_numOfItems];

		for(int i=0; i<num; i++)
			m_hServerItems[i] = pAddResult[i].hServer;

	}

	if(pAddResult != NULL)
	{
		for(int i=0; i<m_numOfItems; i++)
		{
			if(pAddResult[i].pBlob != NULL) 
			{
				CoTaskMemFree(pAddResult->pBlob);
			}
		}

		CoTaskMemFree(pAddResult);
	}

	if(pErrors != NULL) CoTaskMemFree(pErrors);
	if(itemDefs != NULL) delete [] itemDefs;	
}

_variant_t *SSOPC_Group::ReadAllItems()
{
	HRESULT hr; 
	OPCITEMSTATE *pItemValues = NULL;
	HRESULT *pErrors = NULL;
	stringstream ss;
	
	hr = m_IOPCSyncIO->Read(OPC_DS_DEVICE, m_numOfItems, m_hServerItems, &pItemValues,&pErrors);
	if(hr != S_OK) 
	{
		ss << "ReadItems failed, hr = " << hr;
		if( hr == S_FALSE)
		{
			
			for(int i=0; i<m_numOfItems; i++)
			{
				if(pErrors[i] != S_OK)
					ss << "sub hr[" << i << "] = " << pErrors[i];
				
			}
		}
	}
	else
	{
		for(int i=0; i<m_numOfItems; i++)
		{
			m_itemValues[i] = pItemValues[i].vDataValue;			
		}
	}
	if(hr == S_OK){
		for(int i=0; i<m_numOfItems; i++)
		{
			VariantClear(&pItemValues[i].vDataValue);
		}
	}

	if(pItemValues != NULL) CoTaskMemFree(pItemValues);
	if(pErrors != NULL) CoTaskMemFree(pErrors);
	//if(hr != S_OK) throw SSOPC_Exception(ss.str());

	return m_itemValues;
}

void SSOPC_Group::WriteItems(int itemIndex[], _variant_t values[], int num)
{

	HRESULT *pErrors = NULL; 
	HRESULT hr; 
	OPCHANDLE *phItems = NULL;
	stringstream ss;

	phItems = new OPCHANDLE[num];

	for(int i=0; i<num; i++)
	{
		phItems[i] = m_hServerItems[ itemIndex[i] ];
	}

	hr = m_IOPCSyncIO->Write(num, phItems, values, &pErrors);

	if(hr != S_OK) 
	{	
		ss << "WriteItems failed, hr = " << hr;
		
		if( hr == S_FALSE)
		{
			for(int i=0; i<m_numOfItems; i++)
			{
				if(pErrors[i] != S_OK)
					ss << "sub hr[" << i << "] = " << pErrors[i];
			}
		}			
	}

	if(pErrors != NULL) CoTaskMemFree(pErrors);
	if(phItems != NULL) delete [] phItems;
	//if(hr != S_OK) throw SSOPC_Exception(ss.str());
}

//----------------------------------------------------------

SSOPC_Server::SSOPC_Server()
{
	m_IOPCServer = NULL;
	m_pOPCGroup = NULL;
}

SSOPC_Server::~SSOPC_Server()
{
	if(m_pOPCGroup != NULL) delete m_pOPCGroup;
	if(m_IOPCServer != NULL) m_IOPCServer->Release();
}

SSOPC_Group * SSOPC_Server::InitServer(bstr_t opcSvrName, _bstr_t opcSrvAddress)
{
	//the first part of an OPC client is to connect to the OPCEnum service on the remote machine so we can look up the clsid of the OPC Server (given as a string).
	//This code should get a list of OPC servers on a remote or local machine assuming that OPCEnum is running.
	const CLSID CLSID_OpcServerList = {0x13486D51,0x4821,0x11D2, {0xA4,0x94,0x3C, 0xB3,0x06,0xC1,0x0,0x0}}; //{ 0x50fa5e8c, 0xdfae, 0x4ba7, { 0xb6, 0x9a, 0x8f, 0x38, 0xc2, 0xfd, 0x6c, 0x27 } }; //{0x13486D50,0x4821,0x11D2, {0xA4,0x94,0x3C, 0xB3,0x06,0xC1,0x0,0x0}};
	const IID IID_IOPCServerList = {0x13486D50,0x4821,0x11D2, {0xA4,0x94,0x3C, 0xB3,0x06,0xC1,0x0,0x0}}; //for some reason the interface IID is the same as the CLSID.
	const IID IID_IOPCServerList2 = {0x9DD0B56C,0xAD9E,0x43EE, {0x83,0x05,0x48, 0x7F,0x31,0x88,0xBF,0x7A}};
	
	IOPCServerList *m_spServerList = NULL;
	IOPCServerList2 *m_spServerList2 = NULL;

	COSERVERINFO ServerInfo = {0};
	_bstr_t ServerName = opcSrvAddress;
	ServerInfo.pwszName = ServerName;  
	ServerInfo.pAuthInfo = NULL;

	MULTI_QI MultiQI [2] = {0};

	MultiQI [0].pIID = &IID_IOPCServerList;
	MultiQI [0].pItf = NULL;
	MultiQI [0].hr = S_OK;

	MultiQI [1].pIID = &IID_IOPCServerList2;
	MultiQI [1].pItf = NULL;
	MultiQI [1].hr = S_OK;
	
	HRESULT hr_Sec = CoInitializeSecurity(NULL,-1,NULL,NULL, RPC_C_AUTHN_LEVEL_NONE,
										  RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE, NULL);

	//  Create the OPC server object and query for the IOPCServer interface of the object
	HRESULT hr = CoCreateInstanceEx (CLSID_OpcServerList, NULL, CLSCTX_LOCAL_SERVER | CLSCTX_REMOTE_SERVER, &ServerInfo, 1, MultiQI); // ,IID_IOPCServer, (void**)&m_IOPCServer);
	//HRESULT hr = CoCreateInstance (CLSID_OpcServerList, NULL, CLSCTX_LOCAL_SERVER ,IID_IOPCServerList, (void**)&m_spServerList);
	if (hr == S_OK)
	{
		m_spServerList = (IOPCServerList*)MultiQI[0].pItf;
		//m_spServerList2 = (IOPCServerList2*)MultiQI[1].pItf;
	}
	else
	{
		printf("Co create returned: %p\n",(void *)hr);
		m_spServerList = NULL;
		//qDebug() << (void *)REGDB_E_CLASSNOTREG;
	}

	//try and get the class id of the OPC server on the remote host

	CLSID opcServerId;
	CLSID clsid;

	if (m_spServerList)
	{
		hr=m_spServerList->CLSIDFromProgID(opcSvrName, &opcServerId);
		m_spServerList->Release();
	}
	else
	{
		cout << "Server list FAILED." << endl;
		hr = S_FALSE;
		opcServerId.Data1 = 0;
		clsid.Data1 = 0;
	}

	//try to attach to an existing OPC Server (so our OPC server is a proxy)

	if (hr != S_OK)
	{   
		wprintf(L"Couldn't get class id for %s\n Return value: %p", opcSvrName, (void *)hr);
	}
	else
	{
		printf("OPC Server clsid: %p %p %p %p%p%p%p%p%p%p%p\n", (void*)opcServerId.Data1, (void*)opcServerId.Data2, (void*)opcServerId.Data3, (void*)opcServerId.Data4[0], (void*)opcServerId.Data4[1], (void*)opcServerId.Data4[2], (void*)opcServerId.Data4[3], (void*)opcServerId.Data4[4], (void*)opcServerId.Data4[5], (void*)opcServerId.Data4[6], (void*)opcServerId.Data4[7]);
	}

	//  Create the OPC server object and query for the IOPCServer interface of the object.
	//Do it on the remote computer.

	MultiQI [0].pIID = &IID_IOPCServer;
	MultiQI [0].pItf = NULL;
	MultiQI [0].hr = S_OK;

	if (opcServerId.Data1)
	{
		hr = CoCreateInstanceEx (opcServerId, NULL, CLSCTX_LOCAL_SERVER | CLSCTX_REMOTE_SERVER, &ServerInfo, 1, MultiQI);
	}
	else
	{
		hr = S_FALSE;
	}

	if (hr != S_OK)
	{
		m_IOPCServer = NULL;
		printf("Couldn't create server.\n");
	}
	else
	{
		//CoCreateInstanceEx should have returned an array of pointers to interfaces. Since we only asked for 1, lets just get it.
		m_IOPCServer = (IOPCServer*) MultiQI[0].pItf;
		printf("Created remote OPC server.\n");

		m_pOPCGroup = new SSOPC_Group(this);
		m_pOPCGroup->InitGroup();

	}

	return m_pOPCGroup;
}

IOPCServer *SSOPC_Server::GetIOPCServer()
{
	return m_IOPCServer;
}

//----------------------------------------------------------
