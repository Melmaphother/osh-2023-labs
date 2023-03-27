#define _GNU_SOURCE
#define TRANSFER 548
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

int main() {
	char		 buf1[10];
	char		 buf2[100];
	int 		 len1 = 9, len2 = 99;
	long		 ret1 = syscall(TRANSFER, buf1, len1);
	long		 ret2 = syscall(TRANSFER, buf2, len2);
	if (ret1 != -1)
		printf("test1:buffer size: %d, and it's enough, return value:%ld, buffer content:%s\n",
			   len1 + 1, ret1, buf1);
	else
		printf("test1:buffer size:%d, but it's not enough, return value:-1\n",
			   len1 + 1);
	if (ret2 != -1)
		printf("test2:buffer size: %d, and it's enough, return value:%ld, buffer content:%s\n",
			   len2 + 1, ret2, buf2);
	else
		printf("test2:buffer size:%d, but it's not enough, return value:-1\n",
			   len2 + 1);
	while (1) {};
}
