// socket.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
void socket_server();

int _tmain(int argc, _TCHAR* argv[])
{
	char command[100] = "fsutil file createnew store.bin 8388608";
	FILE *file;
	errno_t e = fopen_s(&file, "store.bin", "rb");
	if (e != 0)
	{
		system(command);
	}
	else{
		fclose(file);
	}
	socket_server();
	return 0;
}

