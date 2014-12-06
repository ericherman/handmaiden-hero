#include <stdio.h>

int main(int argc, char *argv[])
{
	char *name;

	if (argc > 1) {
		name = argv[1];
	} else {
		name = "world";
	}
	printf("Hello, %s.\n", name);
	return 0;
}
