#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "parse.c"
void cdecl TraceLogThread(char *Name, BOOL ForceIt, char *Format, ...) {}

FILE *OpenDated(char *Base, char *Add, char *Vers)
{	char *p, *Name = malloc(strlen(Base)+strlen(Add)+strlen(Vers)+80);
	FILE *Out;

	Vers = strdup(Vers);
	for (p=Vers; *p; )
	{	if (!isdigit(*p))
		{	memmove(p,p+1,strlen(p));
		} else p++;
	}

	sprintf(Name, "%s.%s%s", Base, Vers, Add);
	Out = fopen(Name,"wb");
	if (!Out)
	{	fprintf(stderr,"Failed To Create %s\n", Name);
		exit(0);
	} else printf("Created %s\n", Name);
	free(Name);
	return Out;
}

int main(int argc, char *argv[])
{
	if (argc != 2) fprintf(stderr,"Usage: %s APRSISxx\n");
	else
	{	FILE *In;
		In = fopen(argv[1], "rb");
		if (!In) fprintf(stderr,"Failed To Open %s\n", argv[1]);
		else
		{	long Size;
			char *Buf;
			fseek(In, 0, SEEK_END);
			Size = ftell(In);
			fseek(In, 0, SEEK_SET);
			Buf = malloc(Size);
			if (fread(Buf,1,Size,In) != Size)
				fprintf(stderr,"Failed To Read %ld Bytes From %s\n", (long) Size, argv[1]);
			else
			{	unsigned long CRC = CRC32(Buf,Size);
				char Vers[17] = "0000/00/00 00:00";
				FILE *Out;
				int i;
				for (i=0; i<Size; i++)
				{	if (isdigit(Buf[i]) && isdigit(Buf[i+1]) && isdigit(Buf[i+2]) && isdigit(Buf[i+3])
					&& Buf[i+4] == '/' && isdigit(Buf[i+5]) && isdigit(Buf[i+6])
					&& Buf[i+7] == '/' && isdigit(Buf[i+8]) && isdigit(Buf[i+9])
					&& Buf[i+10] == ' ' && isdigit(Buf[i+11]) && isdigit(Buf[i+12])
					&& Buf[i+13] == ':' && isdigit(Buf[i+14]) && isdigit(Buf[i+15])
					&& Buf[i+16] != ':')	/* Ignore non-version (hh:mm:ss) timestamps */
					{	if (strncmp(Vers,&Buf[i],16) < 0)
						{	printf("Using %.16s\n", &Buf[i]);
							strncpy(Vers,&Buf[i],16);
						} else printf("Found %.16s\n", &Buf[i]);
					}
				}
				printf("Final %.16s\n", Vers);
				Out = OpenDated(argv[1], "", Vers);
				fwrite(Buf, 1, Size, Out);
				fclose(Out);

				Out = OpenDated(argv[1], ".ver", Vers);
				fprintf(Out, "%s", Vers);
				fclose(Out);

				Out = OpenDated(argv[1], ".crc", Vers);
				fprintf(Out, "%lX", (long) CRC);
				fclose(Out);
			}
			free(Buf);
			fclose(In);
		}
	}
	return 0;
}

