/* unicode-utils.c
 * Unicode utility routines
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 2006 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include "unicode-utils.h"

int ws_utf8_seqlen[256] = {
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,  /* 0x00...0x0f */
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,  /* 0x10...0x1f */
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,  /* 0x20...0x2f */
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,  /* 0x30...0x3f */
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,  /* 0x40...0x4f */
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,  /* 0x50...0x5f */
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,  /* 0x60...0x6f */
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,  /* 0x70...0x7f */
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* 0x80...0x8f */
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* 0x90...0x9f */
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* 0xa0...0xaf */
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* 0xb0...0xbf */
  0,0,2,2,2,2,2,2,2,2,2,2,2,2,2,2,  /* 0xc0...0xcf */
  2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,  /* 0xd0...0xdf */
  3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,  /* 0xe0...0xef */
  4,4,4,4,4,0,0,0,0,0,0,0,0,0,0,0,  /* 0xf0...0xff */
};

#ifdef _WIN32

#include <strsafe.h>

/** @file
 * Unicode utilities (internal interface)
 *
 * We define UNICODE and _UNICODE under Windows.  This means that
 * Windows SDK routines expect UTF-16 strings, in contrast to newer
 * versions of Glib and GTK+ which expect UTF-8.  This module provides
 * convenience routines for converting between UTF-8 and UTF-16.
 */

#define INITIAL_UTFBUF_SIZE 128

/*
 * XXX - Should we use g_utf8_to_utf16() and g_utf16_to_utf8()
 * instead?  The goal of the functions below was to provide simple
 * wrappers for UTF-8 <-> UTF-16 conversion without making the
 * caller worry about freeing up memory afterward.
 */

/* Convert from UTF-8 to UTF-16. */
const wchar_t *
utf_8to16(const char *utf8str)
{
  static wchar_t *utf16buf[3];
  static int utf16buf_len[3];
  static int idx;

  if (utf8str == NULL)
    return NULL;

  idx = (idx + 1) % 3;

  /*
   * Allocate the buffer if it's not already allocated.
   */
  if (utf16buf[idx] == NULL) {
    utf16buf_len[idx] = INITIAL_UTFBUF_SIZE;
    utf16buf[idx] = g_malloc(utf16buf_len[idx] * sizeof(wchar_t));
  }

  while (MultiByteToWideChar(CP_UTF8, 0, utf8str,
      -1, NULL, 0) >= utf16buf_len[idx]) {
    /*
     * Double the buffer's size if it's not big enough.
     * The size of the buffer starts at 128, so doubling its size
     * adds at least another 128 bytes, which is more than enough
     * for one more character plus a terminating '\0'.
     */
    utf16buf_len[idx] *= 2;
    utf16buf[idx] = g_realloc(utf16buf[idx], utf16buf_len[idx] * sizeof(wchar_t));
  }

  if (MultiByteToWideChar(CP_UTF8, 0, utf8str,
      -1, utf16buf[idx], utf16buf_len[idx]) == 0)
    return NULL;

  return utf16buf[idx];
}

void
utf_8to16_snprintf(TCHAR *utf16buf, gint utf16buf_len, const gchar* fmt, ...)
{
  va_list ap;
  gchar* dst;

  va_start(ap,fmt);
  dst = ws_strdup_vprintf(fmt, ap);
  va_end(ap);

  StringCchPrintf(utf16buf, utf16buf_len, _T("%s"), utf_8to16(dst));

  g_free(dst);
}

/* Convert from UTF-16 to UTF-8. */
gchar *
utf_16to8(const wchar_t *utf16str)
{
  static gchar *utf8buf[3];
  static int utf8buf_len[3];
  static int idx;

  if (utf16str == NULL)
    return NULL;

  idx = (idx + 1) % 3;

  /*
   * Allocate the buffer if it's not already allocated.
   */
  if (utf8buf[idx] == NULL) {
    utf8buf_len[idx] = INITIAL_UTFBUF_SIZE;
    utf8buf[idx] = g_malloc(utf8buf_len[idx]);
  }

  while (WideCharToMultiByte(CP_UTF8, 0, utf16str, -1,
      NULL, 0, NULL, NULL) >= utf8buf_len[idx]) {
    /*
     * Double the buffer's size if it's not big enough.
     * The size of the buffer starts at 128, so doubling its size
     * adds at least another 128 bytes, which is more than enough
     * for one more character plus a terminating '\0'.
     */
    utf8buf_len[idx] *= 2;
    utf8buf[idx] = g_realloc(utf8buf[idx], utf8buf_len[idx]);
  }

  if (WideCharToMultiByte(CP_UTF8, 0, utf16str, -1,
      utf8buf[idx], utf8buf_len[idx], NULL, NULL) == 0)
    return NULL;

  return utf8buf[idx];
}

/* Convert our argument list from UTF-16 to UTF-8. */
char **
arg_list_utf_16to8(int argc, wchar_t *wc_argv[]) {
  char               **argv;
  int                  i;

  argv = (char **) g_malloc((argc + 1) * sizeof(char *));
  for (i = 0; i < argc; i++) {
    argv[i] = g_utf16_to_utf8(wc_argv[i], -1, NULL, NULL, NULL);
  }
  argv[argc] = NULL;
  return argv;
}

#endif

/*
 * Editor modelines  -  https://www.wireshark.org/tools/modelines.html
 *
 * Local Variables:
 * c-basic-offset: 2
 * tab-width: 8
 * indent-tabs-mode: nil
 * End:
 *
 * ex: set shiftwidth=2 tabstop=8 expandtab:
 * :indentSize=2:tabSize=8:noTabs=true:
 */
