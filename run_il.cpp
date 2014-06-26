#include "include/nes.h"

#include <stdio.h>

void printint(int i)
{
	printf("%d\n", i);
}

int main(void)
{
	using namespace NES;
	Environment env = nes::compile_IL(
		"var printint : (int):void;"
		"var initvar : int = fib(10);"

		"def add(a:int,b:int):int{return a + b;}"
		"def fib(n : int):int{"
		"if (n < 2) return 1;"
		"return fib(n-1) + fib(n-2);"
		"}"

		"def embed{printint(123);}"
	);

	if (!env)
	{
		printf("compile fail\n");
		return 0;
	}

	int x = env->call("fib", 20);
	printf("%d\n", x);

	x = env->call("add", 10, 20);
	printf("%d\n", x);

	*env->getGlobal("printint") = (int)printint;
	env->call("embed");

	printf("%d\n", *env->getGlobal("initvar"));

	return 0;
}
