#include "sysheads.h"

#include <xp/source/xmlparse.h>		/* The raw XML Parser */

#include "LLUtil.h"
#include "OSMUtil.h"
#include "tcputil.h"

#include "Address.h"

// helper macros
#ifndef ARRAYSIZE
#define ARRAYSIZE(a) (UINT_PTR)(sizeof(a)/sizeof((a)[0]))
#endif

typedef struct PARSE_STACK_S
{	long ValueCount;
	long ValueMax;
	char *ValueB;
	char *Name;
	long ElementCount;
	long ElementMax;
} PARSE_STACK_S;

typedef struct GEONAME_S
{	char *name, *fcl, *fcode;
} GEONAME_S;

typedef struct PARSE_INFO_S
{	XML_Parser Parser;
	HWND hwnd;

	char *street, *streetNumber;
	char *placename, *adminName1, *adminName2, *countryCode;

	int nameIndex;
	int nameCount;
	GEONAME_S *names;

	long Depth;
	long MaxDepth;
	PARSE_STACK_S *Stack;
} PARSE_INFO_S;

static void startElement(void *userData, const char *name, const char **atts)
{	PARSE_INFO_S *Info = (PARSE_INFO_S *)userData;

	if (Info->Depth) Info->Stack[Info->Depth].ElementCount++;
	Info->Depth++;
	if (Info->Depth >= Info->MaxDepth)
	{	Info->MaxDepth = Info->Depth+1;
		Info->Stack = (PARSE_STACK_S *)realloc(Info->Stack, sizeof(*Info->Stack)*Info->MaxDepth);
		memset(&Info->Stack[Info->Depth], 0, sizeof(Info->Stack[Info->Depth])/**(Info->MaxDepth-Info->Depth+1)*/);
	}

	Info->Stack[Info->Depth].ValueCount = 0;
	Info->Stack[Info->Depth].ElementCount = 0;
	if (Info->Stack[Info->Depth].ValueB) *Info->Stack[Info->Depth].ValueB = '\0';
	if (Info->Depth > 2)
	{	int Len = strlen(Info->Stack[Info->Depth-1].Name) + 1 + strlen(name) + 1;
		Info->Stack[Info->Depth].Name = (char *) malloc(Len);
		sprintf(Info->Stack[Info->Depth].Name, "%s.%s", Info->Stack[Info->Depth-1].Name, name);
	} else Info->Stack[Info->Depth].Name = _strdup(name);

	if (!strcmp(Info->Stack[Info->Depth].Name,"geoname"))
	{	Info->nameIndex = Info->nameCount++;
		Info->names = (GEONAME_S *) realloc(Info->names, sizeof(*Info->names)*Info->nameCount);
		memset(&Info->names[Info->nameIndex], 0, sizeof(Info->names[Info->nameIndex]));
	}
//	const char **a;
//	for (a=atts; *a; a+=2)
//	{	TraceActivity(Info->hwnd, "[%ld]     (%s) %s=%s\n", (long) Info->Depth, Info->Stack[Info->Depth].Name, a[0], a[1]);
//	}
}

static void endElement(void *userData, const char *name)
{	PARSE_INFO_S *Info = (PARSE_INFO_S *) userData;

//	Info->Stack[Info->Depth].Name has value Info->Stack[Info->Depth].ValueB IF Info->Stack[Info->Depth].ValueCount

TraceActivity(Info->hwnd, "[%ld]endElement(%s) Count %ld Value %.*s\n",
					(long) Info->Depth, Info->Stack[Info->Depth].Name,
					(long) Info->Stack[Info->Depth].ElementCount,
					(int) Info->Stack[Info->Depth].ValueCount,
					Info->Stack[Info->Depth].ValueB);

	if (!Info->Stack[Info->Depth].ElementCount
	&& Info->Stack[Info->Depth].ValueB)
	{	if (!strcmp(Info->Stack[Info->Depth].Name,"address.street"))
		{	if (Info->street) free(Info->street);
			Info->street = _strdup(Info->Stack[Info->Depth].ValueB);
		} else if (!strcmp(Info->Stack[Info->Depth].Name,"address.streetNumber"))
		{	if (Info->streetNumber) free(Info->streetNumber);
			Info->streetNumber = _strdup(Info->Stack[Info->Depth].ValueB);
		} else if (!strcmp(Info->Stack[Info->Depth].Name,"address.placename"))
		{	if (Info->placename) free(Info->placename);
			Info->placename = _strdup(Info->Stack[Info->Depth].ValueB);
		} else if (!strcmp(Info->Stack[Info->Depth].Name,"address.adminName1"))
		{	if (Info->adminName1) free(Info->adminName1);
			Info->adminName1 = _strdup(Info->Stack[Info->Depth].ValueB);
		} else if (!strcmp(Info->Stack[Info->Depth].Name,"address.adminName2"))
		{	if (Info->adminName2) free(Info->adminName2);
			Info->adminName2 = _strdup(Info->Stack[Info->Depth].ValueB);
		} else if (!strcmp(Info->Stack[Info->Depth].Name,"address.countryCode"))
		{	if (Info->countryCode) free(Info->countryCode);
			Info->countryCode = _strdup(Info->Stack[Info->Depth].ValueB);

		} else if (!strcmp(Info->Stack[Info->Depth].Name,"geoname.name"))
		{	if (Info->names[Info->nameIndex].name) free(Info->names[Info->nameIndex].name);
			Info->names[Info->nameIndex].name = _strdup(Info->Stack[Info->Depth].ValueB);
		} else if (!strcmp(Info->Stack[Info->Depth].Name,"geoname.fcl"))
		{	if (Info->names[Info->nameIndex].fcl) free(Info->names[Info->nameIndex].fcl);
			Info->names[Info->nameIndex].fcl = _strdup(Info->Stack[Info->Depth].ValueB);
		} else if (!strcmp(Info->Stack[Info->Depth].Name,"geoname.fcode"))
		{	if (Info->names[Info->nameIndex].fcode) free(Info->names[Info->nameIndex].fcode);
			Info->names[Info->nameIndex].fcode = _strdup(Info->Stack[Info->Depth].ValueB);
		}
	}

	Info->Depth--;
}

static void characterData(void *userData, const char *s, int len)
{	PARSE_INFO_S *Info = (PARSE_INFO_S *) userData;
	char * Value = Info->Stack[Info->Depth].ValueB;
	long Len = Info->Stack[Info->Depth].ValueCount;
	long i;

	for (i=0; i<len; i++)
		if (s[i] != '\n' && s[i] != '\r')
			break;
	if (i && i>=len) return;

	if (Len+len+1 > Info->Stack[Info->Depth].ValueMax)
	{	//Info->Stack[Info->Depth].ValueMax += RtGetGrowthFactor(Info->Stack[Info->Depth].ValueMax);
		if (Len+len+1 > Info->Stack[Info->Depth].ValueMax)
			Info->Stack[Info->Depth].ValueMax = Len+len+1;
		Value = (char *) realloc(Value, Info->Stack[Info->Depth].ValueMax);
	}
	for (i=0; i<len; i++)
		if (s[i] != '\n' && s[i] != '\r')
			Value[Len++] = s[i];
	Value[Len] = '\0';
	Info->Stack[Info->Depth].ValueCount = Len;
	Info->Stack[Info->Depth].ValueB = Value;
}

static void startCdata(void *userData)
{
}

static void endCdata(void *userData)
{
}

TCHAR * FindNearbyAddress(HWND hwnd, double lat, double lon)
{	char URL[80];
	int BufLen;
	char *Buffer;
	TCHAR *Address = NULL;

	sprintf(URL, "/extendedFindNearby?lat=%.8lf&lng=%.8lf", lat, lon);
	Buffer = httpGetBuffer(hwnd, "ws.geonames.org", 80, URL, &BufLen, "APRSISCE/32");
	if (!Buffer) return NULL;

	{	PARSE_INFO_S *Info;
		XML_Parser parser = XML_ParserCreate(NULL);
		Info = (PARSE_INFO_S *) calloc(1,sizeof(*Info));
		Info->Parser = parser;
		Info->hwnd = hwnd;
		XML_SetUserData(parser, Info);
		XML_SetElementHandler(parser, startElement, endElement);
		XML_SetCdataSectionHandler(parser, startCdata, endCdata);
		XML_SetCharacterDataHandler(parser, characterData);

		BOOL Result = TRUE;
		Result = XML_Parse(parser, Buffer, BufLen, FALSE);
		if (Result) Result = XML_Parse(parser, NULL, 0, TRUE);
		if (!Result)
		{static TCHAR Buffer[256];
			StringCbPrintf(Buffer, sizeof(Buffer), TEXT("XML_Parse(Address) FAILED!\n\n%S at line %d\n"),
						XML_ErrorString(XML_GetErrorCode(parser)),
						XML_GetCurrentLineNumber(parser));
			MessageBox(hwnd, Buffer, TEXT("FindNearbyAddress"), MB_OK);
		} else
		{	size_t Remaining = sizeof(TCHAR)*1024;
			TCHAR *Next = Address = (TCHAR*)malloc(Remaining);
			*Next = *TEXT("");

			if (Info->nameCount)
			{	int i;
				for (i=Info->nameCount-1; i>=0; i--)
				{
#define DO_UTF8
#ifdef DO_UTF8
					/* %S (%S.%S) */
					StringCbPrintExUTF8(Next, Remaining, &Next, &Remaining, -1, Info->names[i].name, " (");
					StringCbPrintExUTF8(Next, Remaining, &Next, &Remaining, -1, Info->names[i].fcl, ".");
					StringCbPrintExUTF8(Next, Remaining, &Next, &Remaining, -1, Info->names[i].fcode, ")\n");
#else
					StringCbPrintfEx(Next, Remaining, &Next, &Remaining, STRSAFE_IGNORE_NULLS,
										TEXT("%S (%S.%S)\n"),
										Info->names[i].name?Info->names[i].name:"",
										Info->names[i].fcl?Info->names[i].fcl:"",
										Info->names[i].fcode?Info->names[i].fcode:"");
#endif
				}
			} else
			{
#ifdef DO_UTF8
				if (Info->streetNumber && *Info->streetNumber)
					StringCbPrintExUTF8(Next, Remaining, &Next, &Remaining, -1, Info->streetNumber, " ");
				if (Info->street && *Info->street)
					StringCbPrintExUTF8(Next, Remaining, &Next, &Remaining, -1, Info->street, "\n");
				if (Info->placename && *Info->placename)
					StringCbPrintExUTF8(Next, Remaining, &Next, &Remaining, -1, Info->placename, "\n");
				if (Info->adminName2 && *Info->adminName2)
					StringCbPrintExUTF8(Next, Remaining, &Next, &Remaining, -1, Info->adminName2, "\n");
				if (Info->adminName1 && *Info->adminName1)
					StringCbPrintExUTF8(Next, Remaining, &Next, &Remaining, -1, Info->adminName1, "\n");
				if (Info->countryCode && *Info->countryCode)
					StringCbPrintExUTF8(Next, Remaining, &Next, &Remaining, -1, Info->countryCode, "\n");
#else
				if (Info->streetNumber && *Info->streetNumber)
					StringCbPrintfEx(Next, Remaining, &Next, &Remaining, STRSAFE_IGNORE_NULLS,
									TEXT("%S "), Info->streetNumber);
				if (Info->street && *Info->street)
					StringCbPrintfEx(Next, Remaining, &Next, &Remaining, STRSAFE_IGNORE_NULLS,
									TEXT("%S\n"), Info->street);
				if (Info->placename && *Info->placename)
					StringCbPrintfEx(Next, Remaining, &Next, &Remaining, STRSAFE_IGNORE_NULLS,
									TEXT("%S\n"), Info->placename);
				if (Info->adminName2 && *Info->adminName2)
					StringCbPrintfEx(Next, Remaining, &Next, &Remaining, STRSAFE_IGNORE_NULLS,
									TEXT("%S\n"), Info->adminName2);
				if (Info->adminName1 && *Info->adminName1)
					StringCbPrintfEx(Next, Remaining, &Next, &Remaining, STRSAFE_IGNORE_NULLS,
									TEXT("%S\n"), Info->adminName1);
				if (Info->countryCode && *Info->countryCode)
					StringCbPrintfEx(Next, Remaining, &Next, &Remaining, STRSAFE_IGNORE_NULLS,
									TEXT("%S\n"), Info->countryCode);
#endif
			}
			if (!*Address)
			{	free(Address);
				Address = NULL;
			}
		}
		XML_ParserFree(parser);
		if (Info->street) free(Info->street);
		if (Info->streetNumber) free(Info->streetNumber);
		if (Info->placename) free(Info->placename);
		if (Info->adminName1) free(Info->adminName1);
		if (Info->adminName2) free(Info->adminName2);
		if (Info->countryCode) free(Info->countryCode);
		if (Info->nameCount)
		{	int i;
			for (i=0; i<Info->nameCount; i++)
			{	if (Info->names[i].name) free(Info->names[i].name);
				if (Info->names[i].fcl) free(Info->names[i].fcl);
				if (Info->names[i].fcode) free(Info->names[i].fcode);
			}
			free(Info->names);
		}
#ifdef UNNECESSARY
		for (int i=0; i<Info->MaxDepth; i++)
			if (Info->Stack[i].ValueB) 
				free(Info->Stack[i].ValueB);
		if (Info->Stack) free(Info->Stack);
		free(Info);
#endif
	}
	free(Buffer);
	return Address;
}

