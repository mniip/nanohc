#include "rts/gc.h"

int main(int argc, char **argv)
{
	gc_collect();
	
	(void)argc;
	(void)argv;
	return 0;
}
