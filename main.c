#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "parseh264.h"
#include "rgb2yuv.h"

/*
	this is a great dream
*/
int main(int argc,char *argv[])
{
	char *path = "../src13.h264";
	long max_nalu = 100000;

	simplest_h264_parser(path, max_nalu);

	return 0;
}



