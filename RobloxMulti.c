/*
Creates ROBLOX's "ROBLOX_singletonMutex" and takes 
ownership of it so that future ROBLOX clients will 
enter a loop waiting for ownership, never getting
to the code which detects multiple clients.
*/

#include <Windows.h>
#include <stdio.h>

int main()
{
	CreateMutex(NULL, TRUE, "ROBLOX_singletonMutex"); //bInitialOwner is TRUE, takes ownership and holds it until this process exits

	printf("You can now use multiple ROBLOX clients. Closing this will cause all ROBLOX clients to terminate except for one.\n");
	printf("Press any key to close");
	getch();
	return 0;
}