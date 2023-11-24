/*
    hgrep - simple html searching tool
    Copyright (C) 2020-2023 Dominik Stanisław Suchora <suchora.dominik7@gmail.com>

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
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <stdarg.h>
#include <sys/mman.h>
#include <limits.h>
#include <regex.h>
#include <ftw.h>
#include <err.h>

#ifdef LINKED
#include <ctype.h>
#else
#include "ctype.h"
#endif

#include "flexarr.h"
#include "hgrep.h"

#define F_RECURSIVE 0x1
#define F_FAST 0x2

#define BUFF_INC_VALUE (1<<23)

typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;
typedef unsigned long int ulong;

#define PATTERN_SIZE_INC (1<<8)
#define PASSED_INC (1<<10)

#define while_is(w,x,y,z) while ((y) < (z) && w((x)[(y)])) {(y)++;}
#define LENGHT(x) (sizeof(x)/(sizeof(*x)))

typedef struct {
  void *p;
  uchar istable;
} auxiliary_pattern;

#pragma pack(push, 1)
typedef struct {
  hgrep_pattern *pattern;
  size_t id;
  ushort lvl;
} hgrep_x;
#pragma pack(pop)

char *argv0;
flexarr *patterns = NULL;

uint settings = 0;
uchar hflags = 0;
int nftwflags = FTW_PHYS;
FILE* outfile;

static int nftw_func(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf);

static void
die(const char *s, ...)
{
  va_list ap;
  va_start(ap,s);
  vfprintf(stderr,s,ap);
  va_end(ap);
  fputc('\n',stderr);
  exit(1);
}

static void
usage()
{
  die("Usage: %s [OPTION]... PATTERNS [FILE]...\n"\
      "Search for PATTERNS in each html FILE.\n"\
      "Example: %s -i 'div +id; a +href=\".*\\.org\"' index.html\n\n"\
      "Options:\n"\
      "  -i\t\t\tignore case distinctions in patterns and data\n"\
      "  -l\t\t\tlist structure of FILE\n"\
      "  -o FILE\t\tchange output to a FILE instead of stdout\n"\
      "  -f FILE\t\tobtain PATTERNS from FILE\n"\
      "  -E\t\t\tuse extended regular expressions\n"\
      "  -H\t\t\tfollow symlinks\n"\
      "  -r\t\t\tread all files under each directory, recursively\n"\
      "  -R\t\t\tlikewise but follow all symlinks\n"\
      "  -F\t\t\tenter fast and low memory consumption mode\n"\
      "  -h\t\t\tshow help\n"\
      "  -v\t\t\tshow version\n\n"\
      "When FILE isn't specified, FILE will become standard input.",argv0,argv0,argv0);
}

static char *
delchar(char *src, const size_t pos, size_t *size)
{
  size_t s = *size-1;
  for (size_t i = pos; i < s; i++)
    src[i] = src[i+1];
  src[s] = 0;
  *size = s;
  return src;
}

/*void
apatterns_print(flexarr *apatterns, size_t tab)
{
  auxiliary_pattern *a = (auxiliary_pattern*)apatterns->v;
  for (size_t j = 0; j < tab; j++)
    fputc('\t',stderr);
  fprintf(stderr,"%% %lu",apatterns->size);
  fputc('\n',stderr);
  tab++;
  for (size_t i = 0; i < apatterns->size; i++) {
    if (a[i].istable&1) {
      fprintf(stderr,"%%x %d\n",a[i].istable);
      apatterns_print((flexarr*)a[i].p,tab);
    } else {
      for (size_t j = 0; j < tab; j++)
        fputc('\t',stderr);
      fprintf(stderr,"%%j\n");
    }
  }
}*/

static flexarr *
patterns_split(char *src, size_t *pos, size_t s, const uchar flags)
{
  if (s == 0)
    return NULL;
  size_t tpos = 0;
  if (pos == NULL)
    pos = &tpos;
  flexarr *ret = flexarr_init(sizeof(auxiliary_pattern),PATTERN_SIZE_INC);
  auxiliary_pattern *acurrent = (auxiliary_pattern*)flexarr_inc(ret);
  acurrent->p = flexarr_init(sizeof(auxiliary_pattern),PATTERN_SIZE_INC);
  acurrent->istable = 1|4;
  auxiliary_pattern apattern;
  size_t patternl=0;
  size_t i = *pos;
  uchar next = 0;
  while_is(isspace,src,*pos,s);
  for (size_t j; i < s; i++) {
    j = i;

    while (i < s) {
      if (src[i] == '\\' && src[i+1] == '\\') {
        i += 2;
        continue;
      }
      if (src[i] == '\\' && (src[i+1] == ',' || src[i+1] == ';' || src[i+1] == '"' || src[i+1] == '\'' || src[i+1] == '{' || src[i+1] == '}')) {
        delchar(src,i++,&s);
        patternl = i-j;
        continue;
      }
      if (src[i] == '"' || src[i] == '\'') {
        char tf = src[i];
        i++;
        while (i < s && src[i] != tf) {
          if (src[i] == '\\' && (src[i+1] == '\\' || src[i+1] == tf))
            i++;
          i++;
        }
        if (src[i] == tf)
          i++;
      }
      if (src[i] == '[') {
        i++;
        while (i < s && src[i] != ']')
          i++;
        if (src[i] == ']')
          i++;
      }

      if (src[i] == ',') {
        next = 1;
        patternl = i-j;
        i++;
        break;
      }
      if (src[i] == ';') {
        patternl = i-j;
        i++;
        break;
      }
      if (src[i] == '{') {
        i++;
        next = 2;
        break;
      }
      if (src[i] == '}') {
        i++;
        next = 3;
        break;
      }
      i++;
      patternl = i-j;
    }

    if (j+patternl > s)
      patternl = s-j;

    apattern.p = malloc(sizeof(hgrep_pattern));
    hgrep_pcomp(src+j,patternl,(hgrep_pattern*)apattern.p,flags);

    auxiliary_pattern *new = (auxiliary_pattern*)flexarr_inc(acurrent->p);
    new->p = apattern.p;

    if (next == 2) {
      next = 0;
      new->istable = 1|2;
      *pos = i;
      new->p = patterns_split(src,pos,s,flags);
      i = *pos;
      while_is(isspace,src,i,s);
      if (i < s) {
        if (src[i] == ',') {
          i++;
          goto NEXT_NODE;
        } else if (src[i] == '}') {
          i++;
          goto END_BRACKET;
        } else if (src[i] == ';')
          i++;
      }
      //apatterns_print(new->p,0);
    } else
      new->istable = 0;

    if (next == 1) {
      NEXT_NODE: ;
      next = 0;
      acurrent = (auxiliary_pattern*)flexarr_inc(ret);
      acurrent->p = flexarr_init(sizeof(auxiliary_pattern),PATTERN_SIZE_INC);
      acurrent->istable = 1|4;
    }
    if (next == 3) {
      END_BRACKET:
      *pos = i;
      flexarr_clearb(ret);
      return ret;
    }

    while_is(isspace,src,i,s);
    i--;
  }
  *pos = i;
  flexarr_clearb(ret);
  return ret;
}

/*#define print_or_add(x,y) { \
  if (hgrep_match(&nodes[x],y)) { \
  if (dest) { *(size_t*)flexarr_inc(dest) = x; } \
  else if ((y)->format.b) hgrep_printf(outfile,(y)->format.b,(y)->format.s,&nodes[x],hg->data); \
  else hgrep_print(outfile,&nodes[x]); }}*/

static void
first_match(FILE *outfile, hgrep *hg, hgrep_pattern *pattern, flexarr *dest)
{
  hgrep_node *nodes = hg->nodes;
  size_t nodesl = hg->nodesl;
  for (size_t i = 0; i < nodesl; i++) {
    //print_or_add(i,pattern);
    if (hgrep_match(&nodes[i],pattern)) {
      hgrep_x *x = (hgrep_x*)flexarr_inc(dest);
      x->pattern = pattern;
      x->lvl = 0;
      x->id = i;
      //*(size_t*)flexarr_inc(dest) = i;
    }
  }
}


static void
pattern_exec(FILE *outfile, hgrep *hg, hgrep_pattern *pattern, flexarr *source, flexarr *dest)
{
  if (source->size == 0) {
    first_match(outfile,hg,pattern,dest);
    return;
  }
  
  ushort lvl;
  hgrep_node *nodes = hg->nodes;
  size_t current,n;
  for (size_t i = 0; i < source->size; i++) {
    current = ((hgrep_x*)source->v)[i].id;
    lvl = nodes[current].lvl;
    for (size_t j = 0; j <= nodes[current].child_count; j++) {
      n = current+j;
      nodes[n].lvl -= lvl;
      if (hgrep_match(&nodes[n],pattern)) {
        hgrep_x *x = (hgrep_x*)flexarr_inc(dest);
        x->pattern = pattern;
        x->lvl = lvl;
        x->id = n;
      }
      nodes[n].lvl += lvl;
    }
  }
}

static size_t
patterns_recursive_exec(hgrep *hg, auxiliary_pattern *patterns, const size_t size, flexarr *source, flexarr *dest, const size_t lvl)
{
  flexarr *buf[3];

  buf[0] = flexarr_init(sizeof(hgrep_x),PASSED_INC);
  if (source) {
    buf[0]->asize = source->asize;
    buf[0]->size = source->size;
    buf[0]->v = memcpy(malloc(source->asize*sizeof(hgrep_x)),source->v,source->asize*sizeof(hgrep_x));
  }

  buf[1] = flexarr_init(sizeof(hgrep_x),PASSED_INC);
  buf[2] = dest ? dest : flexarr_init(sizeof(hgrep_x),PASSED_INC);
  flexarr *buf_unchanged[2];
  buf_unchanged[0] = buf[0];
  buf_unchanged[1] = buf[1];

  for (size_t i = 0; i < size; i++) {
    //flexarr *out = buf[1];
    //flexarr *out = (source == NULL || (dest == NULL && i == size-1)) ? NULL : buf[1];
    if (patterns[i].istable&1) {
      /*flexarr *copy = flexarr_init(sizeof(hgrep_x),PASSED_INC);
      copy->asize = buf[0]->asize;
      copy->size = buf[0]->size;
      copy->v = memcpy(malloc(buf[0]->asize*sizeof(hgrep_x)),buf[0]->v,buf[0]->asize*sizeof(hgrep_x));*/
      patterns_recursive_exec(hg,(auxiliary_pattern*)((flexarr*)patterns[i].p)->v,((flexarr*)patterns[i].p)->size,buf[0],buf[1],lvl+1);
      //fprintf(stderr,"pase %p %p %lu\n",copy,buf[1],lvl);
      //flexarr_free(copy);
    } else
      pattern_exec(outfile,hg,(hgrep_pattern*)patterns[i].p,buf[0],buf[1]);

    if ((patterns[i].istable&1 && !(patterns[i].istable&2)) || i == size-1) {
      for (size_t j = 0; j < buf[1]->size; j++)
        memcpy(flexarr_inc(buf[2]),&((hgrep_x*)buf[1]->v)[j],sizeof(hgrep_x));
      buf[1]->size = 0;
      continue;
    }
    if (!buf[1]->size)
      break;

    buf[0]->size = 0;
    flexarr *tmp = buf[0];
    buf[0] = buf[1];
    buf[1] = tmp;
  }

  if (!dest) {
    for (size_t j = 0; j < buf[2]->size; j++) {
      hgrep_x *x = &((hgrep_x*)buf[2]->v)[j];
      hg->nodes[x->id].lvl -= x->lvl;
      if (x->pattern->format.b) {
        hgrep_printf(outfile,x->pattern->format.b,x->pattern->format.s,&hg->nodes[x->id],hg->data); \
      } else
        hgrep_print(outfile,&hg->nodes[x->id]);
      hg->nodes[x->id].lvl += x->lvl;
    }
    flexarr_free(buf[2]);
  }

  flexarr_free(buf_unchanged[0]);
  flexarr_free(buf_unchanged[1]);

  return 0;
}


static void
patterns_exec_fast(char *f, size_t s, const uchar inpipe)
{
  if (patterns->size == 0)
    return;
  if (patterns->size > 1)
    die("fast mode cannot run in non linear mode");

  FILE *t = outfile;
  char *ptr;
  size_t fsize;

  flexarr *fpatterns = (flexarr*)((auxiliary_pattern*)patterns->v)[0].p;
  for (size_t i = 0; i < fpatterns->size; i++) {
    outfile = (i == fpatterns->size-1) ? t : open_memstream(&ptr,&fsize);

    if (((auxiliary_pattern*)fpatterns->v)[i].istable&1)
      die("fast mode cannot run in non linear mode");
    hgrep_init(NULL,f,s,outfile,(hgrep_pattern*)((auxiliary_pattern*)fpatterns->v)[i].p);
    fflush(outfile);

    if (!inpipe && i == 0) {
      munmap(f,s);
    } else {
      free(f);
    }
    if (i != fpatterns->size-1)
      fclose(outfile);

    f = ptr;
    s = fsize;
  }
}

static void
patterns_exec(char *f, size_t s, const uchar inpipe)
{
  if (f == NULL || s == 0)
    return;

  if (settings&F_FAST) {
    patterns_exec_fast(f,s,inpipe);
    return;
  }

  hgrep hg;
  hgrep_init(&hg,f,s,NULL,NULL);
  patterns_recursive_exec(&hg,patterns->v,patterns->size,NULL,NULL,0);

  hgrep_free(&hg);
}

static void
pipe_to_str(int fd, char **file, size_t *size)
{
  *file = NULL;
  register size_t readbytes = 0;
  *size = 0;

  do {
    *size += readbytes;
    *file = realloc(*file,*size+BUFF_INC_VALUE);
    readbytes = read(fd,*file+*size,BUFF_INC_VALUE);
  } while (readbytes > 0);
  *file = realloc(*file,*size);
}

void
file_handle(const char *f)
{
  int fd;
  struct stat st;
  char *file;

  if (f == NULL) {
    size_t size;
    pipe_to_str(0,&file,&size);
    patterns_exec(file,size,1);
    return;
  }

  if ((fd = open(f,O_RDONLY)) == -1) {
    warn("%s",f);
    return;
  }

  if (fstat(fd,&st) == -1) {
    warn("%s",f);
    close(fd);
    return;
  }

  if ((st.st_mode&S_IFMT) == S_IFDIR) {
    close(fd);
    if (settings&F_RECURSIVE) {
      if (nftw(f,nftw_func,16,nftwflags) == -1)
        warn("%s",f);
    } else
      fprintf(stderr,"%s: -R not specified: omitting directory '%s'\n",argv0,f);
    return;
  }

  file = mmap(NULL,st.st_size,PROT_READ|PROT_WRITE,MAP_PRIVATE,fd,0);
  if (file == NULL) {
    warn("%s",f);
    close(fd);
  } else {
    close(fd);
    patterns_exec(file,st.st_size,0);
  }
}

int
nftw_func(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf)
{
  if (typeflag == FTW_F || typeflag == FTW_SL)
    file_handle(fpath);

  return 0;
}

void
apatterns_free(flexarr *apatterns)
{
  auxiliary_pattern *a = (auxiliary_pattern*)apatterns->v;
  for (size_t i = 0; i < apatterns->size; i++) {
    if (a[i].istable&1) {
      apatterns_free((flexarr*)a[i].p);
    } else {
      hgrep_pfree((hgrep_pattern*)a[i].p);
    }
  }
  flexarr_free(apatterns);
}

int
main(int argc, char **argv)
{
  argv0 = argv[0];
  if (argc < 2)
    usage();

  int opt;
  outfile = stdout;

  while ((opt = getopt(argc,argv,"Eilo:f:HrRFvh")) != -1) {
    switch (opt) {
      case 'E': hflags |= HGREP_EREGEX; break;
      case 'i': hflags |= HGREP_ICASE; break;
      case 'l': patterns = patterns_split("@p\"%t%I - %c/%l/%s/%p\n\"",NULL,23,hflags); break;
      case 'o': {
        outfile = fopen(optarg,"w");
        if (outfile == NULL)
          err(1,"%s",optarg);
        }
        break;
      case 'f': {
        int fd = open(optarg,O_RDONLY);
        if (fd == -1)
          err(1,"%s",optarg);
        size_t s;
        char *p;
        pipe_to_str(fd,&p,&s);
        close(fd);
        patterns = patterns_split(p,NULL,s,hflags);
        //apatterns_print(patterns,0);
        free(p);
        }
        break;
      case 'H': nftwflags &= ~FTW_PHYS; break;
      case 'r': settings |= F_RECURSIVE; break;
      case 'R': settings |= F_RECURSIVE; nftwflags &= ~FTW_PHYS; break;
      case 'F': settings |= F_FAST; break;
      case 'v': die(VERSION); break;
      case 'h': usage(); break;
      default: exit(1);
    }
  }

  if (!patterns && optind < argc) {
    patterns = patterns_split(argv[optind],NULL,strlen(argv[optind]),hflags);
    //apatterns_print(patterns,0);
    optind++;
  }
  if (!patterns)
      return -1;
  int g = optind;
  for (; g < argc; g++)
    file_handle(argv[g]);
  if (g-optind == 0)
    file_handle(NULL);

  fclose(outfile);
  apatterns_free(patterns);

  return 0;
}
