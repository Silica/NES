#include "include/nes.h"

#include <stdio.h>
using namespace std;

int main(int argc, char **argv)
{
	if (argc < 2)
		return 0;

	FILE *fp = fopen(argv[1], "r");
	if (!fp)
		return 1;

	fseek(fp, 0, SEEK_END);
	int srcsize = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	string src;
	src.resize(srcsize);
	src.resize(fread(&src[0], 1, srcsize, fp));
	fclose(fp);

	using namespace NES;
	Environment env = nes::compile_IL(src);
	if (!env)
		return 0;

	env->run();

	return 0;
}
