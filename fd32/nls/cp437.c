#include <ll/i386/hw-data.h>
#include <ll/i386/stdlib.h>

#include <errors.h>
#include <nls.h>

/* Unicode scalar values for the CP437 character set.                  */
/* Thanks to Roman Czyborra <http://czyborra.com> for this table.      */
/* Thanks to RHIDE macros that helped me very much in writing it in C. */
static const uint16_t Cp437[256] =
{
#if 1 /* Set it if you want to use glyph chars instead of control chars */
/* 00 */ 0x0000, 0x263A, 0x263B, 0x2665, 0x2666, 0x2663, 0x2660, 0x2022,
/* 08 */ 0x25D8, 0x25E6, 0x25D9, 0x2642, 0x2640, 0x266A, 0x266B, 0x263C,
/* 10 */ 0x25B6, 0x25C0, 0x2195, 0x203C, 0x00B6, 0x00A7, 0x25AC, 0x21A8,
/* 18 */ 0x2191, 0x2193, 0x2192, 0x2190, 0x2310, 0x2194, 0x25B2, 0x25BC,
#else
/* 00 */ 0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007,
/* 08 */ 0x0008, 0x0009, 0x000A, 0x000B, 0x000C, 0x000D, 0x000E, 0x000F,
/* 10 */ 0x0010, 0x0011, 0x0012, 0x0013, 0x0014, 0x0015, 0x0016, 0x0017,
/* 18 */ 0x0018, 0x0019, 0x001A, 0x001B, 0x001C, 0x001D, 0x001E, 0x001F,
#endif
/* 20 */ 0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027,
/* 28 */ 0x0028, 0x0029, 0x002A, 0x002B, 0x002C, 0x002D, 0x002E, 0x002F,
/* 30 */ 0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037,
/* 38 */ 0x0038, 0x0039, 0x003A, 0x003B, 0x003C, 0x003D, 0x003E, 0x003F,
/* 40 */ 0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047,
/* 48 */ 0x0048, 0x0049, 0x004A, 0x004B, 0x004C, 0x004D, 0x004E, 0x004F,
/* 50 */ 0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057,
/* 58 */ 0x0058, 0x0059, 0x005A, 0x005B, 0x005C, 0x005D, 0x005E, 0x005F,
/* 60 */ 0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067,
/* 68 */ 0x0068, 0x0069, 0x006A, 0x006B, 0x006C, 0x006D, 0x006E, 0x006F,
/* 70 */ 0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077,
/* 78 */ 0x0078, 0x0079, 0x007A, 0x007B, 0x007C, 0x007D, 0x007E, 0x007F,// ?
/* 80 */ 0x00C7, 0x00FC, 0x00E9, 0x00E2, 0x00E4, 0x00E0, 0x00E5, 0x00E7,
/* 88 */ 0x00EA, 0x00EB, 0x00E8, 0x00EF, 0x00EE, 0x00EC, 0x00C4, 0x00C5,
/* 90 */ 0x00C9, 0x00E6, 0x00C6, 0x00F4, 0x00F6, 0x00F2, 0x00FB, 0x00F9,
/* 98 */ 0x00FF, 0x00D6, 0x00DC, 0x00A2, 0x00A3, 0x00A5, 0x20A7, 0x0192,
/* A0 */ 0x00E1, 0x00ED, 0x00F3, 0x00FA, 0x00F1, 0x00D1, 0x00AA, 0x00BA,
/* A8 */ 0x00BF, 0x2310, 0x00AC, 0x00BD, 0x00BC, 0x00A1, 0x00AB, 0x00BB,
/* B0 */ 0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556,
/* B8 */ 0x2555, 0x2563, 0x2551, 0x2557, 0x255D, 0x255C, 0x255B, 0x2510,
/* C0 */ 0x2514, 0x2534, 0x252C, 0x251C, 0x2500, 0x253C, 0x255E, 0x255F,
/* C8 */ 0x255A, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256C, 0x2567,
/* D0 */ 0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256B,
/* D8 */ 0x256A, 0x2518, 0x250C, 0x2588, 0x2584, 0x258C, 0x2590, 0x2580,
/* E0 */ 0x03B1, 0x03B2, 0x0393, 0x03C0, 0x03A3, 0x03C3, 0x03BC, 0x03C4,
/* E8 */ 0x03A6, 0x0398, 0x03A9, 0x03B4, 0x221E, 0x2205, 0x2208, 0x2229,
/* F0 */ 0x2261, 0x00B1, 0x2265, 0x2264, 0x2320, 0x2321, 0x00F7, 0x2248,
/* F8 */ 0x00B0, 0x2219, 0x00B7, 0x221A, 0x207F, 0x00B2, 0x25A0, 0x00A0
};


/* Converts a Unicode scalar value into a code-page specific value */
/* and append it to the Local string.                              */
/* Returns the number of bytes taken by the converted character.   */
int fd32_unicode_to_oemcp(wchar_t Scalar, char *Dest)
{
  int k;

  if (Scalar < 256)
    if (Scalar == Cp437[Scalar]) { *Dest = Scalar; return 1; }
  for (k = 0; k < 256; k++)
    if (Scalar == Cp437[k]) { *Dest = Cp437[k]; return 1; }
  return 0;
}


int oemcp_to_utf8(char *Source, char *Dest)
{
  wchar_t Ch;
  int   Res;

  for (; *Source; Source++)
  {
    Ch = Cp437[(unsigned) *Source];
    if (Ch < 0x000080) *Dest++ = (char) Ch;
    else
    {
      Res = unicode_wctoutf8(Dest, Ch, 6);
      if (Res < 0) return FD32_EUTF8;
      Dest += Res;
    }
  }
  *Dest = 0;
  return 0;
}


#define OEMCP_UNCONVERTABLE (1 << 0)
#define OEMCP_WAS_LONGER    (1 << 1)


/* Converts a up to SourceSize bytes of the Source UTF-8 string to the */
/* OEM character set, and puts the result in the Dest strings, writing */
/* at most DestSize bytes. If SourceSize is -1 the Source string is    */
/* supposed to be null terminated.                                     */
/* WARNING: Does not append the null terminator to Dest.               */
int utf8_to_oemcp(char *Source, int SourceSize,
                  char *Dest,   int DestSize)
{
  char  *SourceLast = NULL;
  char  *DestLast   = Dest + DestSize - 1;
  int    k, Res = 0;
  wchar_t Scalar;

  if (SourceSize != -1) SourceLast = Source + SourceSize - 1;
  while (*Source)
  {
    if ((SourceLast) && (Source > SourceLast)) break;
    if (Dest > DestLast)
    {
      if (*Source) Res |= OEMCP_WAS_LONGER;
      break;
    }
    /* Get the Unicode scalar value from Source */
    if (!(*Source & 0x80)) Scalar = *Source++;
    else
    {
      int Skip = unicode_utf8towc(&Scalar, Source, 6);
      if (Skip < 0) return FD32_EUTF8;
      Source += Skip;
    }
    /* If the scalar matches with the same code page value we are luky */
    if (Scalar < 256)
      if (Scalar == Cp437[Scalar])
      {
        *Dest++ = Scalar;
        continue;
      }
    /* Otherwise we have to scan the code page to find the scalar */
    for (k = 0; k < 256; k++)
      if (Scalar == Cp437[k])
      {
        *Dest++ = Cp437[k];
        break;
      }
    /* And place an underscore if we can't convert it */
    if (k == 256)
    {
      *Dest++ = '_';
      Res |= OEMCP_UNCONVERTABLE;
    }
  }
  return Res;
}


/* The code page 437 always has 1-byte characters */
int oemcp_skipchar(char *Dest)
{
  return 1;
}


#if 1
#include <kernel.h>
#include <ll/i386/error.h>
static struct { char *Name; DWORD Address; } Symbols[] =
{
  { "oemcp_to_utf8",  (DWORD) oemcp_to_utf8  },
  { "utf8_to_oemcp",  (DWORD) utf8_to_oemcp  },
  { "oemcp_skipchar", (DWORD) oemcp_skipchar },
  { 0, 0 }
};


void cp437_init(void)
{
  int k;

  message("Going to install the old and dirty cp437 NLS driver... ");
  for (k = 0; Symbols[k].Name; k++)
    if (add_call(Symbols[k].Name, Symbols[k].Address, ADD) == -1)
      message("Cannot add %s to the symbol table\n", Symbols[k].Name);
  message("Done\n");
}
#endif
