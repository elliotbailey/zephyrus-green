#include "main_functions.h"

int main(int argc, char *argv[])
{
	setup();
	while(1) {
		loop();
	}
	return 0;
}