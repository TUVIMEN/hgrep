/*
    hgrep - simple html searching tool
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

#define HGREP_EREGEX 0x1
#define HGREP_ICASE 0x2

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
  hgrep_cstr_pair *attrib;
  unsigned int child_count;
  unsigned short attribl;
  unsigned short lvl;
} hgrep_node;

struct hgrep_range {
  unsigned int v[3];
  unsigned char flags;
};

struct hgrep_pattrib {
  regex_t r[2];
  struct hgrep_range *position_r;
  size_t position_rl;
  unsigned char flags;
};

#ifdef HGREP_EDITING
typedef struct {
  void *arg[4];
  unsigned char flags;
} hgrep_format_func;
#endif

typedef struct {
  regex_t tag;
  regex_t insides;
  #ifdef HGREP_EDITING
  hgrep_format_func *format;
  #else
  char *format;
  #endif
  size_t formatl;
  struct hgrep_pattrib *attrib;
  size_t attribl;
  struct hgrep_range *position_r;
  struct hgrep_range *attribute_r;
  struct hgrep_range *size_r;
  struct hgrep_range *child_count_r;
  size_t position_rl;
  size_t attribute_rl;
  size_t size_rl;
  size_t child_count_rl;
  unsigned char flags;
} hgrep_pattern;

typedef struct {
  void *p;
  unsigned char istable;
} hgrep_epattern;

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
  void *attrib_buffer;
  size_t size;
  size_t nodesl;
  unsigned char flags;
} hgrep;

enum hgrep_UnallocMethod {
    HGREP_UNALLOC_NO = 0, //don't unalloc
    HGREP_UNALLOC_FREE, //unalloc with free()
    HGREP_UNALLOC_MUNMAP //unalloc with munmap()
};

hgrep hgrep_init(const char *ptr, const size_t size, FILE *output);
hgrep_error *hgrep_fmatch(const char *ptr, const size_t size, FILE *output, const hgrep_pattern *pattern);
hgrep_error *hgrep_efmatch(char *ptr, const size_t size, FILE *output, const hgrep_epattern *epatterns, const size_t epatternsl, const enum hgrep_UnallocMethod unalloc_method);
hgrep_error *hgrep_pcomp(const char *pattern, size_t size, hgrep_pattern *p, const unsigned char flags);
hgrep_error *hgrep_epcomp(const char *src, size_t size, hgrep_epattern **epatterns, size_t *epatternsl, const unsigned char flags);
int hgrep_match(const hgrep_node *hgn, const hgrep_pattern *p);
hgrep_error *hgrep_ematch(hgrep *hg, const hgrep_epattern *patterns, const size_t size, hgrep_compressed *source, size_t sourcel, hgrep_compressed *dest, size_t destl);
void hgrep_printf(FILE *outfile, const char *format, const size_t formatl, const hgrep_node *hgn, const char *reference);
void hgrep_print(FILE *outfile, const hgrep_node *hg);
void hgrep_pfree(hgrep_pattern *p);
void hgrep_epattern_free(hgrep_epattern *epatterns, const size_t epatternsl);
void hgrep_free(hgrep *hg);

#endif
