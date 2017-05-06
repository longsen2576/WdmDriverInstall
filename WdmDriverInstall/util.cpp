
#include <iostream>
#include <string>

#include "Util.h"

using namespace std;

int PrintUsage()
{
	int ret_value = 0;
	string str_input = "";

	cout << "WdmInstall usage:" << endl;
	cout << "For install:WdmInstall -install" << endl;
	cout << "For unload:WdmInstall -unload" << endl;
	cout << "input g for continue" << endl;
	cin >> str_input;

	return ret_value;
}