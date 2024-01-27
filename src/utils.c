/*
    hgrep - html searching tool
    Copyright (C) 2020-2024 Dominik Stanisław Suchora <suchora.dominik7@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#define _GNU_SOURCE
#define __USE_XOPEN
#define __USE_XOPEN_EXTENDED
#define _XOPEN_SOURCE 600

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <regex.h>
#include <stdarg.h>

typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;
typedef unsigned long int ulong;

#include "hgrep.h"
#include "flexarr.h"
#include "ctype.h"
#include "utils.h"

#define RANGES_INC (1<<4)

#define while_is(w,x,y,z) while ((y) < (z) && w((x)[(y)])) {(y)++;}

char
special_character(const char c)
{
  char r;
  switch (c) {
    case '0': r = '\0'; break;
    case 'a': r = '\a'; break;
    case 'b': r = '\b'; break;
    case 't': r = '\t'; break;
    case 'n': r = '\n'; break;
    case 'v': r = '\v'; break;
    case 'f': r = '\f'; break;
    case 'r': r = '\r'; break;
    default: r = c;
  }
  return r;
}

char *
delchar(char *src, const size_t pos, size_t *size)
{
  size_t s = *size-1;
  for (size_t i = pos; i < s; i++)
    src[i] = src[i+1];
  src[s] = 0;
  *size = s;
  return src;
}

unsigned int
get_dec(const char *src, size_t size, size_t *traversed)
{
    size_t pos=0;
    unsigned int r = 0;
    while (pos < size && isdigit(src[pos]))
        r = (r*10)+(src[pos++]-48);
    *traversed = pos;
    return r;
}

unsigned int
number_handle(const char *src, size_t *pos, const size_t size)
{
  size_t s;
  int ret = get_dec(src+*pos,size-*pos,&s);
  if (s == 0)
    return -1;
  *pos += s;
  return ret;
}

hgrep_error *
get_quoted(char *src, size_t *i, size_t *size, const char delim, size_t *start, size_t *len)
{
  *len = 0;
  if (src[*i] == '"' || src[*i] == '\'') {
    char tf = src[*i];
    *start = ++(*i);
    while (*i < *size && src[*i] != tf) {
      if (src[*i] == '\\' && src[*i+1] == '\\') {
        (*i)++;
      } else if (src[*i] == '\\' && src[*i+1] == tf)
        delchar(src,*i,size);
      (*i)++;
    }
    if (src[*i] != tf)
      return hgrep_set_error(1,"pattern: could not find the end of %c quote [%lu:]",tf,*start);
    *len = ((*i)++)-(*start);
  } else {
    *start = *i;
    while (*i < *size && !isspace(src[*i]) && src[*i] != delim) {
      if (src[*i] == '\\' && src[*i+1] == '\\') {
        (*i)++;
      } else if (src[*i] == '\\' && (isspace(src[*i+1]) || src[*i+1] == delim)) {
        delchar(src,*i,size);
      }
      (*i)++;
    }
    *len = *i-*start;
  }
  return NULL;
}

void
conv_special_characters(char *src, size_t *size)
{
  for (size_t i = 0; i < (*size)-1; i++) {
    if (src[i] != '\\')
      continue;
    char c = special_character(src[i+1]);
    if (c != src[i+1]) {
      delchar(src,i,size);
      src[i] = c;
    }
  }
}

static void
range_handle(const char *src, const size_t size, struct hgrep_range *range)
{
  memset(range->v,0,sizeof(struct hgrep_range));
  size_t pos = 0;

  for (int i = 0; i < 3; i++) {
    while_is(isspace,src,pos,size);
    if (i == 1)
      range->flags |= R_RANGE; //is a range
    if (src[pos] == '-') {
      pos++;
      while_is(isspace,src,pos,size);
      range->flags |= 1<<i; //starts from the end
      range->flags |= R_NOTEMPTY; //not empty
    }
    if (isdigit(src[pos])) {
      range->v[i] = number_handle(src,&pos,size);
      while_is(isspace,src,pos,size);
      range->flags |= R_NOTEMPTY; //not empty
    } else if (i == 1)
      range->flags |= 1<<i;

    if (pos >= size || src[pos] != ':')
      break;
    pos++;
  }
}

uchar
ranges_match(const uint matched, const struct hgrep_range *ranges, const size_t rangesl, const size_t last)
{
  if (!rangesl)
    return 1;
  struct hgrep_range const *r;
  uint x,y;
  for (size_t i = 0; i < rangesl; i++) {
    r = &ranges[i];
    x = r->v[0];
    y = r->v[1];
    if (!(r->flags&8)) {
      if (r->flags&1)
        x = ((uint)last < r->v[0]) ? 0 : last-r->v[0];
      if (matched == x)
        return 1;
    } else {
      if (r->flags&1)
        x = ((uint)last < r->v[0]) ? 0 : last-r->v[0];
      if (r->flags&2) {
        if ((uint)last < r->v[1])
          continue;
        y = last-r->v[1];
      }
      if (matched >= x && matched <= y)
        if (r->v[2] < 2 || matched%r->v[2] == 0)
          return 1;
    }
  }
  return 0;
}

hgrep_error *
ranges_handle(const char *src, size_t *pos, const size_t size, struct hgrep_range **ranges, size_t *rangesl)
{
  if (src[*pos] != '[')
    return NULL;
  (*pos)++;
  size_t end;
  struct hgrep_range range, *new_ptr;
  *rangesl = 0;
  *ranges = NULL;
  size_t rangesl_buffer = 0;

  while (*pos < size && src[*pos] != ']') {
    while_is(isspace,src,*pos,size);
    end = *pos;
    while (end < size && (isspace(src[end]) || isdigit(src[end]) || src[end] == ':' || src[end] == '-') && src[end] != ',')
      end++;
    if (src[end] != ',' && src[end] != ']')
      return hgrep_set_error(1,"range: char %u(0x%02x): not a number",end,src[end]);

    range_handle(src+(*pos),end-(*pos),&range);
    if (range.flags&(R_RANGE|R_NOTEMPTY)) {
      (*rangesl)++;
      if (*rangesl > rangesl_buffer)
        rangesl_buffer += RANGES_INC;
      if ((new_ptr = realloc(*ranges,rangesl_buffer*sizeof(struct hgrep_range))) == NULL) {
        if (*rangesl > 0)
          free(*ranges);
        *ranges = NULL;
        *rangesl = 0;
        return NULL;
      }
      *ranges = new_ptr;
      memcpy(&(*ranges)[*rangesl-1],&range,sizeof(struct hgrep_range));
    }
    *pos = end+((src[end] == ',') ? 1 : 0);
  }
  if (src[*pos] != ']')
    return hgrep_set_error(1,"range: char %d: unprecedented end of range",*pos);
  (*pos)++;

  if (*rangesl != rangesl_buffer) {
    if ((new_ptr = realloc(*ranges,(*rangesl)*sizeof(struct hgrep_range))) == NULL) {
      if (*rangesl > 0)
        free(*ranges);
      *ranges = NULL;
      *rangesl = 0;
      return NULL;
    }
    *ranges = new_ptr;
  }
  return NULL;
}
