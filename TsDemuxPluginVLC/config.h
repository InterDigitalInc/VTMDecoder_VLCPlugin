#ifndef _DVBPSI_CONFIG_H_
#define _DVBPSI_CONFIG_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#if defined(_MSC_VER)
#define NOMINMAX
#include <basetsd.h>
#include <string.h>
typedef SSIZE_T ssize_t;
#define strcasecmp(s1, s2) _stricmp(s1, s2)
#define strncasecmp(s1, s2, s3) _strnicmp(s1, s2, s3)
#define asprintf(...) false // TODO
#define timegm( x ) 4 // TODO
#define __attribute__(x)
static char* strndup(char const* s, size_t n)
{
  size_t len = strnlen(s, n);
  char* newChar = (char*)malloc(len + 1);
  if (!newChar)
    return 0;
  newChar[len] = '\0';
  return (char*) memcpy(newChar, s, len);
}
#endif
#define N_(x) x // TODO
#define _(x) x // TODO

#endif