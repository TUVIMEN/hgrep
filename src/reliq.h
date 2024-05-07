/*
    reliq - html searching tool
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

#ifndef RELIQ_H
#define RELIQ_H

#define RELIQ_SAVE 0x8

#define RELIQ_ERROR_MESSAGE_LENGTH 512

typedef struct {
  void *arg[4];
  unsigned char flags;
} reliq_format_func;

typedef struct {
  char *b;
  unsigned char s;
} reliq_str8;

typedef struct {
  char *b;
  size_t s;
} reliq_str;

typedef struct {
  char const *b;
  size_t s;
} reliq_cstr;

typedef struct {
  reliq_cstr f;
  reliq_cstr s;
} reliq_cstr_pair;

typedef struct {
  char msg[RELIQ_ERROR_MESSAGE_LENGTH];
  int code;
} reliq_error;

typedef struct {
  reliq_cstr all;
  reliq_cstr tag;
  reliq_cstr insides;
  reliq_cstr_pair *attribs;
  unsigned int child_count;
  unsigned short attribsl;
  unsigned short lvl;
} reliq_hnode; //html node

struct reliq_range_node {
  unsigned int v[3];
  unsigned char flags;
};

typedef struct {
  struct reliq_range_node *b;
  size_t s;
} reliq_range;

typedef struct {
  union {
    reliq_str str;
    regex_t reg;
  } match;
  reliq_range range;
  unsigned short flags;
} reliq_pattern;

struct reliq_pattrib {
  reliq_pattern r[2];
  reliq_range position;
  unsigned char flags;
};

typedef struct {
  union {
    reliq_pattern pattern;
    reliq_range range;
  } match;
  unsigned short flags;
} reliq_hook;

typedef struct {
  reliq_pattern tag;
  struct reliq_pattrib *attribs;
  size_t attribsl;
  reliq_hook *hooks;
  size_t hooksl;
  reliq_range position;
  unsigned char flags;
} reliq_node;

typedef struct {
  void *e; //either points to reliq_exprs or reliq_node
  #ifdef RELIQ_EDITING
  reliq_format_func *nodef;
  reliq_format_func *exprf;
  #else
  char *nodef;
  #endif
  size_t nodefl;
  #ifdef RELIQ_EDITING
  size_t exprfl;
  #endif
  unsigned char istable;
} reliq_expr;

typedef struct {
  reliq_expr *b;
  size_t s;
} reliq_exprs;

typedef struct {
  size_t id;
  size_t parentid;
} reliq_compressed;

typedef struct {
  char const *data;
  FILE *output;
  reliq_hnode *nodes;
  reliq_node const *expr; //node passed to process at parsing
  #ifdef RELIQ_EDITING
  reliq_format_func *nodef;
  #else
  char *nodef;
  #endif
  size_t nodefl;
  void *attrib_buffer;
  size_t size;
  size_t nodesl;
  unsigned char flags;
} reliq;

reliq reliq_init(const char *ptr, const size_t size, FILE *output);
reliq_error *reliq_fmatch(const char *ptr, const size_t size, FILE *output, const reliq_node *node,
#ifdef RELIQ_EDITING
  reliq_format_func *nodef,
#else
  char *nodef,
#endif
  size_t nodefl);
reliq_error *reliq_efmatch(char *ptr, const size_t size, FILE *output, const reliq_exprs *exprs, int (*freeptr)(void *ptr, size_t size));
reliq_error *reliq_ncomp(const char *script, size_t size, reliq_node *node);
reliq_error *reliq_ecomp(const char *script, size_t size, reliq_exprs *exprs);
int reliq_match(const reliq_hnode *rqn, const reliq_hnode *parent, const reliq_node *node);
reliq_error *reliq_ematch(reliq *rq, const reliq_exprs *expr, reliq_compressed *source, size_t sourcel, reliq_compressed *dest, size_t destl);
void reliq_printf(FILE *outfile, const char *format, const size_t formatl, const reliq_hnode *rqn, const reliq_hnode *parent, const char *reference);
void reliq_print(FILE *outfile, const reliq_hnode *rq);
void reliq_nfree(reliq_node *node);
void reliq_efree(reliq_exprs *expr);
void reliq_free(reliq *rq);
reliq_error *reliq_set_error(const int code, const char *fmt, ...);

#endif
