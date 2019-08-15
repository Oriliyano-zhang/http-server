#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "server_http.h"

int main(int argc,const char *argv[])
{
	if(argc<3)
	{
		printf("./a.out port path\n");
		return -1;
	}
	int port=atoi(argv[1]);

	int ret=chdir(argv[2]);
	if(ret==-1)
	{
		perror("chdir error");
		exit(1);
	}

	//start epoll
	epoll_run(port);
	return 0;
}

