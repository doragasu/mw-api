#include <stdint.h>

#ifndef NULL
#define NULL (void*)0;
#endif

// main function
extern int main(void);

typedef void(*_voidCallback)(void);

void _start_entry()
{
	// Call main program
	main();
}

void _reset_entry()
{
	main();
}

