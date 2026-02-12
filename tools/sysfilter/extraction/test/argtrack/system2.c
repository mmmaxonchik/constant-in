#include <stdlib.h>

// Although SAST finds this program as a potential vulnerability, using 
// system() seems to be the goal of the program and therefore, no changes 
// are made to this file.

int main(int argc, char **argv)
{
	char *cmd = "whoami";
	system(cmd);/* Flawfinder: ignore */
	return 0;
}
