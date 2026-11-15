/*
 ============================================================================
 Name        : EccServer.cpp
 Author      : 
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C++,
 ============================================================================
 */

#include <iostream>
#include <Ice/Ice.h>
#include "ElectronicsControlCoreImpl.h"

using namespace std;

using namespace ecc;

class EccServer: virtual public Ice::Application {
public:
	virtual int run(int argc, char* argv[]) {

		int status = 0;
		try
		{
			Ice::ObjectAdapterPtr
					adapter = communicator()->createObjectAdapterWithEndpoints(
									"ElectronicsControlCoreAdapter",
									"default -p 10000");
			std::cout << "adapter created" << std::endl;

			Ice::ObjectPtr object = new ElectronicsControlCoreImpl;
			std::cout << "servant created" << std::endl;

			adapter->add(object, communicator()->stringToIdentity(
					"ElectronicsControlCore"));
			std::cout << "servant registered" << std::endl;

			adapter->activate();
			std::cout << "adapter activated: running!" << std::endl;

			communicator()->waitForShutdown();
			std::cout << "shutting down..." << std::endl;
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
	EccServer app;
	return app.main(argc,argv);
}//main


