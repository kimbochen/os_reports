#include "syscall.h"

int
main()
{
	int n, i;
	for (n = 1; n < 10; ++n) {
		PrintInt(1);
		for (i=0; i<100; ++i);
	}
	Exit(0);
}