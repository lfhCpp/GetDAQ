/*
 ============================================================================
 Name        : EccClientMain.cpp
 Author      : 
 Version     :
 Copyright   : Your copyright notice
 Description :
 ============================================================================
 */

#include <iostream>
#include <Ice/Ice.h>
#include "ElectronicsControlCoreImpl.h"
#include "ECC.h"


using namespace ecc;

class EccClient: virtual public Ice::Application {
public:
	virtual int run(int argc, char* argv[]) {

		int status = 0;
		try
		{
			Ice::ObjectPrx base = communicator()->stringToProxy(
					"ElectronicsControlCore:default -p 10000");
			ElectronicsControlCorePrx eccP = ElectronicsControlCorePrx::checkedCast(base);
			if(!eccP) throw "Invalid proxy";

		}

		catch (const Ice::Exception& e)
		{
			std::cerr << "E: " << e.what() << std::endl;
			status = 1;
		} catch (const char* msg) {
			std::cerr << msg << std::endl;
			status = 1;
		}

		return status;

	};
};

int main(int argc, char* argv[])
{
	EccClient app;
	return app.main(argc,argv);
}//main


