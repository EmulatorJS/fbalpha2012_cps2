#include <stdio.h>
#include "burnint.h"
#include "m68000_intf.h"
#include "z80_intf.h"

// A hiscore.dat support module for FBA - written by Treble Winner, Feb 2009
// At some point we really need a CPU interface to track CPU types and numbers,
// to make this module and the cheat engine foolproof

#define MAX_CONFIG_LINE_SIZE 		48

#define HISCORE_MAX_RANGES		20

UINT32 nHiscoreNumRanges;

#define APPLIED_STATE_NONE		0
#define APPLIED_STATE_ATTEMPTED		1
#define APPLIED_STATE_CONFIRMED		2

struct _HiscoreMemRange
{
	UINT32 Loaded, nCpu, Address, NumBytes, StartValue, EndValue, ApplyNextFrame, Applied;
	UINT8 *Data;
};

struct _HiscoreMemRange HiscoreMemRange[HISCORE_MAX_RANGES];

INT32 EnableHiscores;
static INT32 HiscoresInUse;

static INT32 nCpuType;
extern INT32 nSekCount;

static void set_cpu_type(void)
{
	if (nSekCount > -1)
		nCpuType = 1;			// Motorola 68000
	else if (nHasZet > -1)
		nCpuType = 5;			// Zilog Z80
	else
	{
		nCpuType = 0;			// Unknown (don't use cheats)
	}
}

static void cpu_open(INT32 nCpu)
{
	switch (nCpuType)
	{
		case 1:	
			SekOpen(nCpu);
		break;

		case 5:
			ZetOpen(nCpu);
		break;
	}
}

static void cpu_close(void)
{
	switch (nCpuType)
	{
		case 1:
			SekClose();
		break;

		case 5:
			ZetClose();
		break;
	}
}

/*static INT32 cpu_get_active()
{
	switch (nCpuType) {
		case 1: {
			return SekGetActive();
		}
		
		case 2: {
			return VezGetActive();
		}
		
		case 3: {
			return Sh2GetActive();
		}
		
		case 4: {
			return m6502GetActive();
		}
		
		case 5: {
			return ZetGetActive();
		}
		
		case 6: {
			return M6809GetActive();
		}
		
		case 7: {
			return HD6309GetActive();
		}
		
		case 8: {
			return -1;
		}
		
		case 9: {
			return nActiveS2650;
		}
	}
}
*/
static UINT8 cpu_read_byte(UINT32 a)
{
	switch (nCpuType)
	{
		case 1:
			return SekReadByte(a);

		case 5:
			return ZetReadByte(a);
	}

	return 0;
}

static void cpu_write_byte(UINT32 a, UINT8 d)
{
	switch (nCpuType)
	{
		case 1:
			SekWriteByteROM(a, d);
		break;

		case 5:
			ZetWriteByte(a, d);
		break;
	}
}

static UINT32 hexstr2num (const char **pString)
{
	const char *string = *pString;
	UINT32 result = 0;
	if (string)
	{
		for(;;)
		{
			char c = *string++;
			INT32 digit;

			if (c>='0' && c<='9')
			{
				digit = c-'0';
			}
			else if (c>='a' && c<='f')
			{
				digit = 10+c-'a';
			}
			else if (c>='A' && c<='F')
			{
				digit = 10+c-'A';
			}
			else
			{
				if (!c) string = NULL;
				break;
			}
			result = result*16 + digit;
		}
		*pString = string;
	}
	return result;
}

static INT32 is_mem_range (const char *pBuf)
{
	char c;
	for(;;)
	{
		c = *pBuf++;
		if (c == 0) return 0; /* premature EOL */
		if (c == ':') break;
	}
	c = *pBuf; /* character following first ':' */

	return	(c>='0' && c<='9') ||
			(c>='a' && c<='f') ||
			(c>='A' && c<='F');
}

static INT32 matching_game_name (const char *pBuf, const char *name)
{
	while (*name)
	{
		if (*name++ != *pBuf++) return 0;
	}
	return (*pBuf == ':');
}

static INT32 CheckHiscoreAllowed(void)
{
	INT32 Allowed = 1;
	
	if (!EnableHiscores) Allowed = 0;
	if (!(BurnDrvGetFlags() & BDF_HISCORE_SUPPORTED)) Allowed = 0;
	
	return Allowed;
}

void HiscoreInit(void)
{
   FILE *fp;
   INT32 Offset = 0;
#ifdef _WIN32
   char slash = '\\';
#else
   char slash = '/';
#endif
   TCHAR szFilename[MAX_PATH];
   TCHAR szDatFilename[MAX_PATH];

   if (!CheckHiscoreAllowed())
	   return;
	
   HiscoresInUse = 0;
   snprintf(szDatFilename, sizeof(szDatFilename), "%s%cfbalpha2012%chiscore.dat",
		   g_system_dir, slash, slash);

   fp = _tfopen(szDatFilename, _T("r"));

   if (fp)
   {
		char buffer[MAX_CONFIG_LINE_SIZE];
		enum { FIND_NAME, FIND_DATA, FETCH_DATA } mode;
		mode = FIND_NAME;

		while (fgets(buffer, MAX_CONFIG_LINE_SIZE, fp)) {
			if (mode == FIND_NAME) {
				if (matching_game_name(buffer, BurnDrvGetTextA(DRV_NAME))) {
					mode = FIND_DATA;
				}
			} else {
				if (is_mem_range(buffer)) {
					if (nHiscoreNumRanges < HISCORE_MAX_RANGES) {
						const char *pBuf = buffer;
					
						HiscoreMemRange[nHiscoreNumRanges].Loaded = 0;
						HiscoreMemRange[nHiscoreNumRanges].nCpu = hexstr2num(&pBuf);
						HiscoreMemRange[nHiscoreNumRanges].Address = hexstr2num(&pBuf);
						HiscoreMemRange[nHiscoreNumRanges].NumBytes = hexstr2num(&pBuf);
						HiscoreMemRange[nHiscoreNumRanges].StartValue = hexstr2num(&pBuf);
						HiscoreMemRange[nHiscoreNumRanges].EndValue = hexstr2num(&pBuf);
						HiscoreMemRange[nHiscoreNumRanges].ApplyNextFrame = 0;
						HiscoreMemRange[nHiscoreNumRanges].Applied = 0;
						HiscoreMemRange[nHiscoreNumRanges].Data = (UINT8*)malloc(HiscoreMemRange[nHiscoreNumRanges].NumBytes);
						memset(HiscoreMemRange[nHiscoreNumRanges].Data, 0, HiscoreMemRange[nHiscoreNumRanges].NumBytes);
					
						nHiscoreNumRanges++;
					
						mode = FETCH_DATA;
					} else {
						break;
					}
				} else {
					if (mode == FETCH_DATA) break;
				}
			}
		}
		
		fclose(fp);
	}
	
	if (nHiscoreNumRanges) HiscoresInUse = 1;
	
	snprintf(szFilename, sizeof(szFilename), "%s%c%s.hi", g_save_dir, slash, BurnDrvGetText(DRV_NAME));

	fp = _tfopen(szFilename, _T("r"));
	if (fp)
   {
      UINT8 *Buffer;
      UINT32 i, j;
		UINT32 nSize = 0;
		
		while (!feof(fp))
      {
			fgetc(fp);
			nSize++;
		}
		
		Buffer = (UINT8*)malloc(nSize);
		rewind(fp);
		
		fgets((char*)Buffer, nSize, fp);
		
		for (i = 0; i < nHiscoreNumRanges; i++)
      {
			for (j = 0; j < HiscoreMemRange[i].NumBytes; j++)
				HiscoreMemRange[i].Data[j] = Buffer[j + Offset];
			Offset += HiscoreMemRange[i].NumBytes;
			
			HiscoreMemRange[i].Loaded = 1;
		}
		
		if (Buffer)
      {
			free(Buffer);
			Buffer = NULL;
		}

		fclose(fp);
	}
	
	nCpuType = -1;
}

void HiscoreReset(void)
{
   UINT32 i;
	if (!CheckHiscoreAllowed() || !HiscoresInUse)
      return;
	
	if (nCpuType == -1)
      set_cpu_type();
	
	for (i = 0; i < nHiscoreNumRanges; i++)
   {
      HiscoreMemRange[i].ApplyNextFrame = 0;
      HiscoreMemRange[i].Applied = APPLIED_STATE_NONE;

      if (HiscoreMemRange[i].Loaded)
      {
         cpu_open(HiscoreMemRange[i].nCpu);
         cpu_write_byte(HiscoreMemRange[i].Address, (UINT8)~HiscoreMemRange[i].StartValue);
         if (HiscoreMemRange[i].NumBytes > 1) cpu_write_byte(HiscoreMemRange[i].Address + HiscoreMemRange[i].NumBytes - 1, (UINT8)~HiscoreMemRange[i].EndValue);
         cpu_close();

      }
   }
}

void HiscoreApply(void)
{
   UINT32 i;
	if (!CheckHiscoreAllowed() || !HiscoresInUse)
      return;
	
	if (nCpuType == -1) set_cpu_type();
	
	for (i = 0; i < nHiscoreNumRanges; i++)
   {
      if (HiscoreMemRange[i].Loaded && HiscoreMemRange[i].Applied == APPLIED_STATE_ATTEMPTED)
      {
         UINT32 j;
         INT32 Confirmed = 1;
         cpu_open(HiscoreMemRange[i].nCpu);
         for (j = 0; j < HiscoreMemRange[i].NumBytes; j++)
         {
            if (cpu_read_byte(HiscoreMemRange[i].Address + j) != HiscoreMemRange[i].Data[j])
               Confirmed = 0;
         }
         cpu_close();

         if (Confirmed == 1)
         {
            HiscoreMemRange[i].Applied = APPLIED_STATE_CONFIRMED;
         }
         else
         {
            HiscoreMemRange[i].Applied = APPLIED_STATE_NONE;
            HiscoreMemRange[i].ApplyNextFrame = 1;
         }
      }

      if (HiscoreMemRange[i].Loaded && HiscoreMemRange[i].Applied == APPLIED_STATE_NONE && HiscoreMemRange[i].ApplyNextFrame)
      {
         UINT32 j;
         cpu_open(HiscoreMemRange[i].nCpu);
         for (j = 0; j < HiscoreMemRange[i].NumBytes; j++)
            cpu_write_byte(HiscoreMemRange[i].Address + j, HiscoreMemRange[i].Data[j]);				
         cpu_close();

         HiscoreMemRange[i].Applied = APPLIED_STATE_ATTEMPTED;
         HiscoreMemRange[i].ApplyNextFrame = 0;
      }

      if (HiscoreMemRange[i].Loaded && HiscoreMemRange[i].Applied == APPLIED_STATE_NONE) {
         cpu_open(HiscoreMemRange[i].nCpu);
         if (cpu_read_byte(HiscoreMemRange[i].Address) == HiscoreMemRange[i].StartValue && cpu_read_byte(HiscoreMemRange[i].Address + HiscoreMemRange[i].NumBytes - 1) == HiscoreMemRange[i].EndValue)
            HiscoreMemRange[i].ApplyNextFrame = 1;
         cpu_close();
      }
   }
}

void HiscoreExit(void)
{
#ifdef _WIN32
   char slash = '\\';
#else
   char slash = '/';
#endif
   UINT32 i;
   FILE *fp;
   TCHAR szFilename[MAX_PATH];

	if (!CheckHiscoreAllowed() || !HiscoresInUse)
		return;
	
	if (nCpuType == -1)
      set_cpu_type();

	snprintf(szFilename, sizeof(szFilename), "%s%c%s.hi", g_save_dir, slash, BurnDrvGetText(DRV_NAME));

	fp = _tfopen(szFilename, _T("w"));
	if (fp)
   {
      UINT32 i;
		for (i = 0; i < nHiscoreNumRanges; i++)
      {
         UINT32 j;
         UINT8 *Buffer = (UINT8*)malloc(HiscoreMemRange[i].NumBytes);

         cpu_open(HiscoreMemRange[i].nCpu);
         for (j = 0; j < HiscoreMemRange[i].NumBytes; j++)
            Buffer[j] = cpu_read_byte(HiscoreMemRange[i].Address + j);
         cpu_close();

         fwrite(Buffer, 1, HiscoreMemRange[i].NumBytes, fp);

         if (Buffer)
         {
            free(Buffer);
            Buffer = NULL;
         }
      }
	}
	fclose(fp);
	
	nCpuType = -1;
	nHiscoreNumRanges = 0;
	
	for (i = 0; i < HISCORE_MAX_RANGES; i++) {
		HiscoreMemRange[i].Loaded = 0;
		HiscoreMemRange[i].nCpu = 0;
		HiscoreMemRange[i].Address = 0;
		HiscoreMemRange[i].NumBytes = 0;
		HiscoreMemRange[i].StartValue = 0;
		HiscoreMemRange[i].EndValue = 0;
		HiscoreMemRange[i].ApplyNextFrame = 0;
		HiscoreMemRange[i].Applied = 0;
		
		free(HiscoreMemRange[i].Data);
		HiscoreMemRange[i].Data = NULL;
	}
}
