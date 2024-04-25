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

#ifndef HGREP_H
#define HGREP_H

#define HGREP_SAVE 0x8

typedef struct {
  void *arg[4];
  unsigned char flags;
} hgrep_format_func;

typedef struct {
  char *b;
  unsigned char s;
} hgrep_str8;

typedef struct {
  char *b;
  size_t s;
} hgrep_str;

typedef struct {
  char const *b;
  size_t s;
} hgrep_cstr;

typedef struct {
  hgrep_cstr f;
  hgrep_cstr s;
} hgrep_cstr_pair;

typedef struct {
  char msg[512];
  int code;
} hgrep_error;

typedef struct {
  hgrep_cstr all;
  hgrep_cstr tag;
  hgrep_cstr insides;
  hgrep_cstr_pair *attribs;
  unsigned int child_count;
  unsigned short attribsl;
  unsigned short lvl;
} hgrep_node;

struct hgrep_range {
  unsigned int v[3];
  unsigned char flags;
};

typedef struct {
  union {
    hgrep_str str;
    regex_t reg;
  } match;
  struct hgrep_range *ranges;
  size_t rangesl;
  unsigned short flags;
} hgrep_regex;

typedef struct {
  struct hgrep_range *b;
  size_t s;
} hgrep_list;

struct hgrep_pattrib {
  hgrep_regex r[2];
  hgrep_list position;
  unsigned char flags;
};

typedef struct {
  union {
    hgrep_regex r;
    hgrep_list l;
  } match;
  unsigned short flags;
} hgrep_hook;

typedef struct {
  hgrep_regex tag;
  struct hgrep_pattrib *attribs;
  size_t attribsl;
  hgrep_hook *hooks;
  size_t hooksl;
  hgrep_list position;
  unsigned char flags;
} hgrep_pattern;

typedef struct {
  void *p;
  #ifdef HGREP_EDITING
  hgrep_format_func *nodef;
  hgrep_format_func *exprf;
  #else
  char *nodef;
  #endif
  size_t nodefl;
  #ifdef HGREP_EDITING
  size_t exprfl;
  #endif
  unsigned char istable;
} hgrep_epattern;

typedef struct {
  hgrep_epattern *b;
  size_t s;
} hgrep_epatterns;

#pragma pack(push, 1)
typedef struct {
  size_t id;
  unsigned short lvl;
} hgrep_compressed;
#pragma pack(pop)

typedef struct {
  char const *data;
  FILE *output;
  hgrep_node *nodes;
  hgrep_pattern const *pattern;
  #ifdef HGREP_EDITING
  hgrep_format_func *nodef;
  #else
  char *nodef;
  #endif
  size_t nodefl;
  void *attrib_buffer;
  size_t size;
  size_t nodesl;
  unsigned char flags;
} hgrep;

hgrep hgrep_init(const char *ptr, const size_t size, FILE *output);
hgrep_error *hgrep_fmatch(const char *ptr, const size_t size, FILE *output, const hgrep_pattern *pattern,
#ifdef HGREP_EDITING
  hgrep_format_func *nodef,
#else
  char *nodef,
#endif
  size_t nodefl);
hgrep_error *hgrep_efmatch(char *ptr, const size_t size, FILE *output, const hgrep_epatterns *epatterns, int (*freeptr)(void *ptr, size_t size));
hgrep_error *hgrep_pcomp(const char *pattern, size_t size, hgrep_pattern *p);
hgrep_error *hgrep_epcomp(const char *src, size_t size, hgrep_epatterns *epatterns, const unsigned char flags);
int hgrep_match(const hgrep_node *hgn, const hgrep_pattern *p);
hgrep_error *hgrep_ematch(hgrep *hg, const hgrep_epatterns *patterns, hgrep_compressed *source, size_t sourcel, hgrep_compressed *dest, size_t destl);
void hgrep_printf(FILE *outfile, const char *format, const size_t formatl, const hgrep_node *hgn, const char *reference);
void hgrep_print(FILE *outfile, const hgrep_node *hg);
void hgrep_pfree(hgrep_pattern *p);
void hgrep_epatterns_free(hgrep_epatterns *epatterns);
void hgrep_free(hgrep *hg);
hgrep_error *hgrep_set_error(const int code, const char *fmt, ...);

#endif
