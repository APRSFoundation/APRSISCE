#ifndef GOT_FILTER_H
#define GOT_FILTER_H
#ifdef __cplusplus
extern "C"
{
#endif

typedef enum FILTER_TYPE_V
{	FILTER_RANGE = 'r',		/* r/lat/lon/dist (max:9) */
	FILTER_PREFIX = 'p',	/* p/aa/bb/cc... */
	FILTER_BUDLIST = 'b',	/* b/call1/call2... *able */
	FILTER_MSG_GROUP = 'g',	/* g/call1/call2... *able */
	FILTER_OBJECT = 'o',	/* o/obj1/obj2... *able */
	FILTER_TYPE = 't',		/* t/poimqstunw */
	FILTER_TYPE_FRIEND = 'T',	/* t/poimqstu/call/km */
	FILTER_SYMBOL = 's',	/* s/pri/alt/over */
	FILTER_DIGI = 'd',		/* d/digi1/digi2... *able */
	FILTER_DIGI_ANY = 'D',		/* D/digi1/digi2... *able */
	FILTER_AREA = 'a',		/* a/latN/lonW/latS/lonE (max:9) */
	FILTER_ENTRY = 'e',		/* e/call1/call1/... *able */
	FILTER_UNPROTO = 'u',	/* u/unproto1/unproto2/... *able */
	FILTER_Q = 'q',			/* q/con/ana */
	FILTER_MY = 'm',		/* m/dist */
	FILTER_FRIEND = 'f'		/* f/call/dist (max:9)*/
} FILTER_TYPE_V;

typedef struct FILTER_COMPONENT_S
{	FILTER_TYPE_V Type;
	BOOL Negative;
	BOOL Positive;
	double f1, f2, f3, f4;	/* Lat/Lon/Dist/corners */
	unsigned int ElementCount;
	char **Elements;
	char *EWild;	/* TRUE if wild in element */
	char *ELen;		/* Wildcard (*) NOT included */
	unsigned int LastHit;	/* BOOL-ized (index+1) */
	char HitNixDetail[256];
} FILTER_COMPONENT_S;

typedef struct FILTER_INFO_S
{	char *FilterText;	/* For change detection */
	BOOL HasNegative;
	BOOL HasPositive;
	BOOL HasErrors;
	unsigned int Count;
	FILTER_COMPONENT_S *Pieces;
	unsigned int LastHit, LastNix;	/* BOOL-ized (index+1) */
	char HitNixDetail[256];
} FILTER_INFO_S;

#include "parsedef.h"

void FreeFilter(FILTER_INFO_S *Filter);
BOOL OptimizeFilter(char *FilterText, FILTER_INFO_S *Filter);
BOOL CheckOptimizedFilter(char *FilterText, FILTER_INFO_S *Filter);
int FilterPacket(FILTER_INFO_S *Filter, APRS_PARSED_INFO_S *Packet);
char *GetHitNixDetail(FILTER_INFO_S *Filter);
#endif	/* GOT_FILTER_H */

#ifdef __cplusplus
}
#endif
