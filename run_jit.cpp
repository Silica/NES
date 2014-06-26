#include "include/nes.h"

#include <stdio.h>

int main(void)
{
	using namespace NES;
	Native n = nes::compile(
		"def add(a:int,b:int):int{return a + b;}"
		"def fib(n : int):int{"
		"if (n < 2) return 1;"
		"return fib(n-1) + fib(n-2);"
	"}"
	);

	if (!n)
	{
		printf("compile fail\n");
		return 0;
	}

	typedef int (*func)(int);
	func f = (func)n->get("fib");
	int x = f(30);
	printf("%d\n", x);

	typedef int (*add)(int,int);
	add a = (add)n->get("add");
	x = a(10, 20);
	printf("%d\n", x);

	return 0;
}
