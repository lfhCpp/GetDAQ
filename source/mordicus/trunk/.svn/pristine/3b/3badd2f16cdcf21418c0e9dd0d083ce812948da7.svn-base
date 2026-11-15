/*
 * LoadConfig.cpp
 *
 * Created on: July, 2010
 * Author: pzhao
 */
#include "ECC.h"
#include "ElectronicsControlCoreImpl.h"
#include "FullConfigEcc.h"
#include "HardwareNodeImpl.h"
#include <iostream>
#include <stdexcept>
#include <unistd.h>
#include <stdlib.h>

using namespace mdaq::hw::server;
using namespace ecc;
using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::runtime_error;


int main(int argc, char **argv)
{

	std::string sfile_cfg(getenv("HOME"));
	sfile_cfg.append("/cfg/");
	sfile_cfg.append("CCfg_file.xml");

	/****************************************
	 * One describes a structure FullConfig : fullCfg.
	 * Nothing is loaded to the remote target so far
	 ***************************************/

	FullConfigEcc fullCfgEcc;

	try
	{
		CCfg::Io::Document doc(sfile_cfg);
		CCfg::CConfig cfg = doc.getConfig();
		fullCfgEcc.fromCCfg(cfg);
	}
	catch (CCfg::Io::Document::IoError& err)
	{
		cout << err.what() << endl;
		return 1;
	}
	catch (std::runtime_error& err)
	{
		cout << "E: " << err.what() << endl;
		return 1;
	}
	catch (const char *msg)
	{
		cout << msg << endl;
		return 1;
	}

	/****************************************
	 * Ice stuff
	 ***************************************/

	Ice::CommunicatorPtr ic;
	ElectronicsControlCorePrx eccPrx;
	ecc::NodeConfigList::iterator itNodeBeg = fullCfgEcc.nodes.begin();
	ecc::NodeConfigList::iterator itNodeEnd = fullCfgEcc.nodes.end();

	try
	{
		ic = Ice::initialize(argc, argv);
		Ice::ObjectPrx base = ic->stringToProxy("ElectronicsControlCore:default -p 10000");
		eccPrx = ElectronicsControlCorePrx::checkedCast(base);
		if(not eccPrx) throw std::runtime_error("Invalid proxy");
		eccPrx->test("Hello World!");
		for(itNodeBeg; itNodeBeg != itNodeEnd; ++itNodeBeg)
		{
			eccPrx->createNode(*itNodeBeg);
		}
		NodeConfigList nodes;
		eccPrx->getConfig(nodes);
		NodeConfigList::iterator itNodes = nodes.begin();
		cout << "The first nodes: " << itNodes->id.ip << ":" << itNodes->id.port << endl;
	}
	catch (const Ice::Exception& ex)
	{
		cerr << "error: " << ex.what() << endl;
		return 1;
	}
	catch (const char* msg)
	{
		cerr << msg << endl;
		return 1;
	}

	return 0;
}
