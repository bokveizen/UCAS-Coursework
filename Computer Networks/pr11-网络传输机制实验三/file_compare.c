#include <stdio.h>
#include <stdlib.h>

int main()
{
	FILE * f1 = fopen("1MB.dat","r");
	FILE * f2 = fopen("server-output.dat","r");
	char c1 = fgetc(f1);
	char c2 = fgetc(f2);
	while(!feof(f1) && !feof(f2))
	{
		if(c1 != c2) 
		{
			printf("NO\n");
			return 0;
		}
		c1 = fgetc(f1);
		c2 = fgetc(f2);
	}
	if(c1 == EOF && c2 == EOF) printf("YES\n");
}
