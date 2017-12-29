/* ext_args.h - public domain Vadzim Rachko 2017
                no warranty implied; use at your own risk

                single-file unix command line options parser
                https://github.com/vadimr/ext_args
  LICENSE

    See end of file for license information

  REQUIREMENTS

    libc and c99

  USAGE

  First create your wrapper function where you can specify how you want to
  handle errors:

    #include <stdio.h>
    #include "ext_args.h"

    char *eargs(int argc, char *argv[], char *fmt, ...) {
      char *err;
      va_list ap;
      va_start(ap, fmt);
      int res = ext_args(argc, argv, fmt, ap, &err);
      va_end(ap);

      switch(res) {
        case EXT_ARGS_NO_MEM_ERR:
          fprintf(stderr, "Can't allocated memory");
          exit(EXIT_FAILURE);

        case EXT_ARGS_SCHEMA_ERR:
          fprintf(stderr, "ExtArg - incorrect schema. Error: %s\n", err);
          exit(EXIT_FAILURE);

        case EXT_ARGS_INPUT_ERR:
          return err;
      }
      return NULL;
    }

  Then you can use it like this:

    int main(int argc, char *argv[]) {
      char *fmt = "-f|--flag1=val [--flag2] [-f3[=val]] [-D=val...] fname lname [mname] ...";

      if(!eargs(argc, argv, "-h", NULL)) {
        printf("Usage: %s\n", fmt);
        return EXIT_SUCCESS;
      }

      char *f, *f3, *fname, *lname, *mname;
      bool *flag2;
      char **d, **files;

      char *err = eargs(argc, argv, fmt,
                        &f, &flag2, &f3, &d, &fname, &lname, &mname, &files);
      if(err) {
        puts(err);
        free(err);
        printf("Usage: %s\n", fmt);
        return EXIT_FAILURE;
      }

      printf("-f = %s\n", f);
      printf("--flag2 = %s\n", flag2 ? "YES" : "NO");

      char *f3val = "NOT PROVIDED";
      if(f3) { f3val = f3 == ext_args_no_value ? "IS SET" : f3; }
      printf("-f3 = %s\n", f3val);

      printf("fname = %s\n", fname);
      printf("lname = %s\n", lname);
      printf("mname = %s\n", mname ? mname : "NOT PROVIDED");

      puts("D = ");
      for(int i = 0; d[i]; i++) {
        printf("  %s\n", d[i]);
      }

      puts("files = ");
      for(int i = 0; files[i]; i++) {
        printf("  %s\n", files[i]);
      }

      free(d);
      free(files);
    }

  Examples of definitions with usage:

    {
      // In this case the user is forced to provide the flag, `flag` var always equals to 1
      bool flag;
      char *err = eargs(argc, argv, "-f|--flag", &flag);
      if(!err) {
        printf("flag: %s\n", flag ? "YES" : "NO");
      }
    }

    {
      char *flag;
      char *err = eargs(argc, argv, "-f|--flag=val", &flag);
      if(!err) {
        printf("flag value: %s\n", flag);
      }
    }

    {
      bool flag;
      char *err = eargs(argc, argv, "[-f|--flag|---flaaag]", &flag);
      if(!err) {
        printf("flag: %s\n", flag ? "YES" : "NO");
      }
    }

    {
      bool flag;
      char *err = eargs(argc, argv, "[-f|--flag]", &flag);
      if(!err) {
        printf("flag: %s\n", flag ? "YES" : "NO");
      }
    }

    {
      char *flag;
      char *err = eargs(argc, argv, "[-f|--flag=val]", &flag);
      if(!err) {
        if(flag) {
          printf("flag value: %s\n", flag);
        }
      }
    }

    {
      // The value is optional
      char *flag;
      char *err = eargs(argc, argv, "[-f|--flag[=val]]", &flag);
      if(!err) {
        if(flag) {
          if(flag == ext_args_no_value) {
            puts("flag is set");
          } else {
            printf("flag: %s\n", flag);
          }
        }
      }
    }

    {
      // Use null if you don't want to save the input
      char *err = eargs(argc, argv, "-f|--flag=val", NULL);
      if(!err) {
        // ...
      }
    }

    {
      // Same flag multiple times
      char **d;
      char *err = eargs(argc, argv, "-d=val...", &d);
      if(!err) {
        for(int i = 0; d[i]; i++) {
          printf("%s\n", d[i]);
        }
        free(d);
      }
    }

    {
      // Positional arguments
      char *a, *b;
      char *err = eargs(argc, argv, "a [b]", &a, &b);
      if(!err) {
        printf("a: %s\n", a);
        printf("b: %s\n", b ? b : "NOT PROVIDED");
      }
    }

    {
      // Variadic positional arguments
      char **a;
      char *err = eargs(argc, argv, "...", &a);
      if(!err) {
        for(int i = 0; a[i]; i++) {
          printf("%s\n", a[i]);
        }
        free(a);
      }
    }

  NOTES

  `ext_args` function doesn't mutate `argc` or `argv` arguments. It's safe to call
  it multiple times on the same input.

  If you want to totally free all allocated memory, `free` memory allocated for
  errors, variadic positional and floating arguments, nothing else. Like this:

    char **a, **b;
    char *err = eargs(argc, argv, "-a=val... ...", &a, &b);
    if(err) {
      free(err);
      ...
    } else {
      ...
      free(a);
      free(b);
    }
*/

#ifndef INCLUDE_EXT_ARGS_H
#define INCLUDE_EXT_ARGS_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <setjmp.h>
#include <stdarg.h>

enum {
  EXT_ARGS_ERR_LEX = 1,
  EXT_ARGS_ERR_PARSE,
  EXT_ARGS_ERR_MEM
};

enum {
  EXT_ARGS_TOK_EOI,
  EXT_ARGS_TOK_LBRAK,
  EXT_ARGS_TOK_RBRAK,
  EXT_ARGS_TOK_PIPE,
  EXT_ARGS_TOK_EQL,
  EXT_ARGS_TOK_NAME,
  EXT_ARGS_TOK_FLOAT_ARG,
  EXT_ARGS_TOK_DOTS
};

#define EXT_ARGS_CAT_(a, b) a ## b
#define EXT_ARGS_CAT(a, b) EXT_ARGS_CAT_(a, b)

#define EXT_ARGS_DYN_ARY_FIELDS(type, fname) \
  type *fname; \
  int EXT_ARGS_CAT(fname, Count); \
  int EXT_ARGS_CAT(fname, Allocated)

#define EXT_ARGS_DYN_ARY_SAVE(obj, fname, capacity, buf, val, jbuf) \
  do { \
    if(!obj->fname) { \
      obj->EXT_ARGS_CAT(fname, Allocated) = capacity; \
      obj->fname = malloc(sizeof(*obj->fname) * obj->EXT_ARGS_CAT(fname, Allocated)); \
      if(!obj->fname) { \
        longjmp(jbuf, EXT_ARGS_ERR_MEM); \
      } \
    } \
    \
    if(obj->EXT_ARGS_CAT(fname, Allocated) - obj->EXT_ARGS_CAT(fname, Count) == buf) { \
      obj->EXT_ARGS_CAT(fname, Allocated) += capacity; \
      obj->fname = realloc(obj->fname, sizeof(*obj->fname) * obj->EXT_ARGS_CAT(fname, Allocated)); \
      if(!obj->fname) { \
        longjmp(jbuf, EXT_ARGS_ERR_MEM); \
      } \
    } \
    obj->fname[obj->EXT_ARGS_CAT(fname, Count)++] = val; \
  } while(0)

#define EXT_ARGS_PREALLOC 10

static char *ext_args_no_value = "(NO VALUE)";

// Positional argument
typedef struct {
  bool isOptional;
  char *str;
  int len;
  void *varPtr;
} EXT_ARGS_PosArg;

// Floating argument
typedef struct {
  char *str;
  int len;
  int groupIdx;
} EXT_ARGS_FloatArg;

// Floating arguments group
typedef struct {
  bool isOptional;
  bool hasAssign;
  bool isAssignOptional;
  bool isRepeating;
  int aliasCount;
  void *varPtr;
  bool isUsed;
  EXT_ARGS_DYN_ARY_FIELDS(char *, ary);
} EXT_ARGS_FloatArgsGroup;

typedef struct {
  int type;
  char *str;
  int len;
} EXT_ARGS_Tok;

typedef struct {
  int type;
  int idx;
} EXT_ARGS_SequenceElement;

// Schema parsing stuff
typedef struct {
  // Parsing stuff
  char *str;
  jmp_buf jbuf;
  char *errStart;
  int expTokType;
  int unexpTokType;

  struct {
    bool isOptional;
  } parsingStates;

  EXT_ARGS_Tok lastMatchTok;

  // Structure
  EXT_ARGS_DYN_ARY_FIELDS(EXT_ARGS_PosArg, posArgs);
  EXT_ARGS_DYN_ARY_FIELDS(EXT_ARGS_FloatArgsGroup, groups);
  EXT_ARGS_DYN_ARY_FIELDS(EXT_ARGS_FloatArg, floats);
  EXT_ARGS_DYN_ARY_FIELDS(EXT_ARGS_SequenceElement, sequence);

  bool varPosArgsEnabled; // Variadic positional arguments. Indicated with "..." in the schema at the end
  void *varPosrArgsVarPtr;
} EXT_ARGS_Parser;

static char *EXT_ARGS_CurrentPos(EXT_ARGS_Parser *prs) {
  return prs->str;
}

static void EXT_ARGS_Rollback(EXT_ARGS_Parser *prs, char *str) {
  prs->str = str;
}

static void EXT_ARGS_Consume(EXT_ARGS_Parser *prs) {
  prs->str++;
}

static int EXT_ARGS_Distance(EXT_ARGS_Parser *prs, char *str) {
  return abs(str - prs->str);
}

static char *EXT_ARGS_TokTypeToName(int type) {
  switch(type) {
    case EXT_ARGS_TOK_EOI: return "EOI";
    case EXT_ARGS_TOK_LBRAK: return "LBRAK";
    case EXT_ARGS_TOK_RBRAK: return "RBRAK";
    case EXT_ARGS_TOK_PIPE: return "PIPE";
    case EXT_ARGS_TOK_EQL: return "EQL";
    case EXT_ARGS_TOK_NAME: return "NAME";
    case EXT_ARGS_TOK_FLOAT_ARG: return "FLOAT_ARG";
    case EXT_ARGS_TOK_DOTS: return "DOTS";
  }
  return "UNKNOWN";
}

static bool EXT_ARGS_Char(char c, EXT_ARGS_Parser *prs) {
  if(*prs->str == c) {
    EXT_ARGS_Consume(prs);
    return true;
  }
  return false;
}

static bool EXT_ARGS_Name(EXT_ARGS_Parser *prs) {
  char *c = EXT_ARGS_CurrentPos(prs);
  if(!(*c >= 'a' && *c <= 'z') && !(*c >= 'A' && *c <= 'Z')) {
    return false;
  }
  c++;

  while((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9') || (*c == '_' || *c == '-')) {
    c++;
  }

  if(c == prs->str) {
    return false;
  }

  if(*(c - 1) == '-') { // last can't be '-'
    return false;
  }

  EXT_ARGS_Rollback(prs, c);
  return true;
}

static bool EXT_ARGS_ArgFloat(EXT_ARGS_Parser *prs) {
  char *bk = EXT_ARGS_CurrentPos(prs);

  while(EXT_ARGS_Char('-', prs));

  int d = EXT_ARGS_Distance(prs, bk);
  if(d < 1) {
    EXT_ARGS_Rollback(prs, bk);
    return false;
  }

  if(!EXT_ARGS_Name(prs)) {
    EXT_ARGS_Rollback(prs, bk);
    return false;
  }

  return true;
}

static bool EXT_ARGS_Dots(EXT_ARGS_Parser *prs) {
  char *bk = EXT_ARGS_CurrentPos(prs);

  while(EXT_ARGS_Char('.', prs));

  if(EXT_ARGS_Distance(prs, bk) != 3) {
    EXT_ARGS_Rollback(prs, bk);
    return false;
  }

  return true;
}

static bool EXT_ARGS_SkipSpaces(EXT_ARGS_Parser *prs) {
  char *bk = EXT_ARGS_CurrentPos(prs);

  while(EXT_ARGS_Char(' ', prs) || EXT_ARGS_Char('\n', prs) || EXT_ARGS_Char('\t', prs) || EXT_ARGS_Char('\r', prs));

  if(EXT_ARGS_Distance(prs, bk) == 0) {
    return false;
  }

  return true;
}

static EXT_ARGS_Tok EXT_ARGS_NextTok(EXT_ARGS_Parser *prs) {
  while(*prs->str != '\0') {
    if(EXT_ARGS_SkipSpaces(prs)) {
      continue;
    }

    char *bk = EXT_ARGS_CurrentPos(prs);

    if(EXT_ARGS_Char('[', prs)) {
      return (EXT_ARGS_Tok){EXT_ARGS_TOK_LBRAK, bk, EXT_ARGS_Distance(prs, bk)};
    }

    if(EXT_ARGS_Char(']', prs)) {
      return (EXT_ARGS_Tok){EXT_ARGS_TOK_RBRAK, bk, EXT_ARGS_Distance(prs, bk)};
    }

    if(EXT_ARGS_Char('|', prs)) {
      return (EXT_ARGS_Tok){EXT_ARGS_TOK_PIPE, bk, EXT_ARGS_Distance(prs, bk)};
    }

    if(EXT_ARGS_Char('=', prs)) {
      return (EXT_ARGS_Tok){EXT_ARGS_TOK_EQL, bk, EXT_ARGS_Distance(prs, bk)};
    }

    if(EXT_ARGS_Name(prs)) {
      return (EXT_ARGS_Tok){EXT_ARGS_TOK_NAME, bk, EXT_ARGS_Distance(prs, bk)};
    }

    if(EXT_ARGS_ArgFloat(prs)) {
      return (EXT_ARGS_Tok){EXT_ARGS_TOK_FLOAT_ARG, bk, EXT_ARGS_Distance(prs, bk)};
    }

    if(EXT_ARGS_Dots(prs)) {
      return (EXT_ARGS_Tok){EXT_ARGS_TOK_DOTS, bk, EXT_ARGS_Distance(prs, bk)};
    }

    prs->errStart = bk;
    longjmp(prs->jbuf, EXT_ARGS_ERR_LEX);
  }

  return (EXT_ARGS_Tok){EXT_ARGS_TOK_EOI, NULL, 0};
}

// Parsing

// Grammar

// synopsis: decl+ DOTS? EOI
// decl: arg | LBR arg RBR
// arg: NAME | FLOAT_ARG (PIPE FLOAT_ARG)* (assignval DOTS? | LBR assignval RBR)?
// assignval: EQL NAME

static bool EXT_ARGS_Match(int tokType, EXT_ARGS_Parser *prs, bool isMandatory) {
  char *bk = EXT_ARGS_CurrentPos(prs);

  EXT_ARGS_Tok t = EXT_ARGS_NextTok(prs);
  if(t.type == tokType) {
    prs->lastMatchTok = t;
    return true;
  }

  if(isMandatory) {
    prs->unexpTokType = t.type;
    prs->expTokType = tokType;
    prs->errStart = bk;
    longjmp(prs->jbuf, EXT_ARGS_ERR_PARSE);
  } else {
    EXT_ARGS_Rollback(prs, bk);
    return false;
  }
}

static bool EXT_ARGS_AssignVal(EXT_ARGS_Parser *prs, bool isMandatory) {
  char *bk = EXT_ARGS_CurrentPos(prs);

  if(!EXT_ARGS_Match(EXT_ARGS_TOK_EQL, prs, isMandatory)) {
    return false;
  }

  if(!EXT_ARGS_Match(EXT_ARGS_TOK_NAME, prs, isMandatory)) {
    EXT_ARGS_Rollback(prs, bk);
    return false;
  }

  return true;
}

enum {
  EXT_ARGS_ARG_POS,
  EXT_ARGS_ARG_GROUP
};

static void EXT_ARGS_SavePosArg(EXT_ARGS_Parser *prs) {
  EXT_ARGS_DYN_ARY_SAVE(prs, sequence, EXT_ARGS_PREALLOC, 0, ((EXT_ARGS_SequenceElement){
    .type = EXT_ARGS_ARG_POS,
    .idx = prs->posArgsCount
  }), prs->jbuf);

  EXT_ARGS_DYN_ARY_SAVE(prs, posArgs, EXT_ARGS_PREALLOC, 0, ((EXT_ARGS_PosArg){
    .isOptional = prs->parsingStates.isOptional,
    .str = prs->lastMatchTok.str,
    .len = prs->lastMatchTok.len
  }), prs->jbuf);
}

static int EXT_ARGS_SaveFloatArg(EXT_ARGS_Parser *prs, int groupIdx) {
  int bkIdx = prs->floatsCount;

  EXT_ARGS_DYN_ARY_SAVE(prs, floats, EXT_ARGS_PREALLOC, 0, ((EXT_ARGS_FloatArg){
    .str = prs->lastMatchTok.str,
    .len = prs->lastMatchTok.len,
    .groupIdx = groupIdx
  }), prs->jbuf);

  return bkIdx;
}

static void EXT_ARGS_SaveGroup(EXT_ARGS_Parser *prs, bool hasAssign, bool isRepeating, int aliasCount) {
  EXT_ARGS_DYN_ARY_SAVE(prs, sequence, EXT_ARGS_PREALLOC, 0, ((EXT_ARGS_SequenceElement){
    .type = EXT_ARGS_ARG_GROUP,
    .idx = prs->groupsCount
  }), prs->jbuf);

  EXT_ARGS_DYN_ARY_SAVE(prs, groups, EXT_ARGS_PREALLOC, 0, ((EXT_ARGS_FloatArgsGroup){
    .isOptional = prs->parsingStates.isOptional,
    .hasAssign = hasAssign,
    .isRepeating = isRepeating,
    .aliasCount = aliasCount
  }), prs->jbuf);
}

static bool EXT_ARGS_Arg(EXT_ARGS_Parser *prs, bool isMandatory) {
  if(EXT_ARGS_Match(EXT_ARGS_TOK_NAME, prs, false)) {
    EXT_ARGS_SavePosArg(prs);
    return true;
  }

  if(!EXT_ARGS_Match(EXT_ARGS_TOK_FLOAT_ARG, prs, isMandatory)) {
    return false;
  }

  int groupIdx = prs->groupsCount;
  int floatBk = EXT_ARGS_SaveFloatArg(prs, groupIdx);

  char *bk = EXT_ARGS_CurrentPos(prs);
  int aliasCount = 1;

  while(EXT_ARGS_Match(EXT_ARGS_TOK_PIPE, prs, false)) {
    if(!EXT_ARGS_Match(EXT_ARGS_TOK_FLOAT_ARG, prs, isMandatory)) {
      EXT_ARGS_Rollback(prs, bk);
      prs->floatsCount = floatBk;
      return false;
    }
    EXT_ARGS_SaveFloatArg(prs, groupIdx);
    aliasCount++;
  }

  bool hasAssign = false;
  bool isRepeating = false;

  if(EXT_ARGS_AssignVal(prs, false)) {
    hasAssign = true;
    if(EXT_ARGS_Match(EXT_ARGS_TOK_DOTS, prs, false)) {
      isRepeating = true;
    }
    EXT_ARGS_SaveGroup(prs, hasAssign, isRepeating, aliasCount);
    return true;
  }

  EXT_ARGS_SaveGroup(prs, hasAssign, isRepeating, aliasCount);

  bk = EXT_ARGS_CurrentPos(prs); // new backup point

  if(!EXT_ARGS_Match(EXT_ARGS_TOK_LBRAK, prs, false)) {
    EXT_ARGS_Rollback(prs, bk);
    return true;
  }

  if(!EXT_ARGS_AssignVal(prs, false)) {
    EXT_ARGS_Rollback(prs, bk);
    return true;
  }

  if(!EXT_ARGS_Match(EXT_ARGS_TOK_RBRAK, prs, false)) {
    EXT_ARGS_Rollback(prs, bk);
    return true;
  }

  prs->groups[groupIdx].hasAssign = true;
  prs->groups[groupIdx].isAssignOptional = true;

  return true;
}

static bool EXT_ARGS_Decl(EXT_ARGS_Parser *prs, bool isMandatory) {
  prs->parsingStates.isOptional = false;
  if(EXT_ARGS_Arg(prs, false)) {
    return true;
  }

  char *bk = EXT_ARGS_CurrentPos(prs);

  if(!EXT_ARGS_Match(EXT_ARGS_TOK_LBRAK, prs, isMandatory)) {
    return false;
  }

  prs->parsingStates.isOptional = true;
  if(!EXT_ARGS_Arg(prs, isMandatory)) {
    EXT_ARGS_Rollback(prs, bk);
    return false;
  }

  if(!EXT_ARGS_Match(EXT_ARGS_TOK_RBRAK, prs, isMandatory)) {
    EXT_ARGS_Rollback(prs, bk);
    return false;
  }

  return true;
}

static void EXT_ARGS_Synopsis(EXT_ARGS_Parser *prs) {
  while(EXT_ARGS_Decl(prs, false));
  if(EXT_ARGS_Match(EXT_ARGS_TOK_DOTS, prs, false)) {
    prs->varPosArgsEnabled = true;
  }
  EXT_ARGS_Match(EXT_ARGS_TOK_EOI, prs, true);
}


// Prefix "U" means "User Input"

enum {
  EXT_ARGS_UTOK_FLOAT,
  EXT_ARGS_UTOK_EQL,
  EXT_ARGS_UTOK_VAL,
  EXT_ARGS_UTOK_EOI
};

typedef struct {
  int type;
  char *str;
  int len;
} EXT_ARGS_UTok;

typedef struct {
  char *str;
  int len;
  char *assignVal;
} EXT_ARGS_UFloatArg;

// Array of strings
typedef struct {
  EXT_ARGS_DYN_ARY_FIELDS(char *, ary);
} EXT_ARGS_Ary;

typedef struct {
  EXT_ARGS_DYN_ARY_FIELDS(EXT_ARGS_UTok, tokens);
  EXT_ARGS_DYN_ARY_FIELDS(EXT_ARGS_UFloatArg, floats);
  EXT_ARGS_DYN_ARY_FIELDS(char *, posArgs);
} EXT_ARGS_Inp;

static char *EXT_ARGS_FmtErr(jmp_buf jbuf, char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int len = vsnprintf(&(char){0}, 1, fmt, ap);
  va_end(ap);

  char *str = malloc(len + 1);
  if (!str) {
    longjmp(jbuf, EXT_ARGS_ERR_MEM); \
  }

  va_start(ap, fmt);
  vsnprintf(str, len + 1, fmt, ap);
  va_end(ap);
  return str;
}

enum {
  EXT_ARGS_NO_ERR,
  EXT_ARGS_NO_MEM_ERR,
  EXT_ARGS_SCHEMA_ERR,
  EXT_ARGS_INPUT_ERR
};

static int ext_args(int argc, char *argv[], char *fmt, va_list ap, char **oerr) {
  EXT_ARGS_Parser prs = {.str = fmt};
  EXT_ARGS_Inp *inp = &(EXT_ARGS_Inp){0};
  int res = EXT_ARGS_NO_ERR;

  int jval = setjmp(prs.jbuf);
  switch(jval) {
    case 0:
      EXT_ARGS_Synopsis(&prs);
      break;

    case EXT_ARGS_ERR_LEX:
      res = EXT_ARGS_SCHEMA_ERR;
      *oerr = EXT_ARGS_FmtErr(prs.jbuf, "Schema lexing error, starting from \"%s\"", prs.errStart);
      goto done;

    case EXT_ARGS_ERR_PARSE:
      res = EXT_ARGS_SCHEMA_ERR;
      *oerr = EXT_ARGS_FmtErr(prs.jbuf, "Schema parsing error. Expected %s but received %s, starting from \"%s\"",
          EXT_ARGS_TokTypeToName(prs.expTokType), EXT_ARGS_TokTypeToName(prs.unexpTokType), prs.errStart);
      goto done;

    case EXT_ARGS_ERR_MEM:
      res = EXT_ARGS_NO_MEM_ERR;
      *oerr = NULL;
      goto done;
  }

  // Additional schema validations
  if(prs.posArgsCount > 1) {
    for(int i = 0; i < prs.posArgsCount - 1; i++) {
      // Schema like this is not allowed: "[a] b". This is ok: "a [b]"
      if(prs.posArgs[i].isOptional && !prs.posArgs[i + 1].isOptional) {
        res = EXT_ARGS_SCHEMA_ERR;
        *oerr = EXT_ARGS_FmtErr(prs.jbuf, "All optional non-flag arguments must be chained on the schema's right side");
        goto done;
      }
    }
  }

  // Parse User's input
  //
  // Lexing

  for(int i = 1; i < argc; i++) {
    char *bk = argv[i];
    EXT_ARGS_Parser *ipr = &(EXT_ARGS_Parser){.str = bk};

    if(EXT_ARGS_ArgFloat(ipr)) {
      EXT_ARGS_DYN_ARY_SAVE(inp, tokens, EXT_ARGS_PREALLOC, 0, ((EXT_ARGS_UTok){
        .type = EXT_ARGS_UTOK_FLOAT, .str = bk, .len = EXT_ARGS_Distance(ipr, bk)}
      ), prs.jbuf);

      if(EXT_ARGS_Char('=', ipr)) {
        char *str = EXT_ARGS_CurrentPos(ipr);
        EXT_ARGS_DYN_ARY_SAVE(inp, tokens, EXT_ARGS_PREALLOC, 0, ((EXT_ARGS_UTok){.type = EXT_ARGS_UTOK_EQL, .str = str - 1}), prs.jbuf);
        if(*str != '\0') {
          EXT_ARGS_DYN_ARY_SAVE(inp, tokens, EXT_ARGS_PREALLOC, 0, ((EXT_ARGS_UTok){.type = EXT_ARGS_UTOK_VAL, .str = str}), prs.jbuf);
        }
        continue;
      }

      char *str = EXT_ARGS_CurrentPos(ipr);
      if(*str != '\0') {
        res = EXT_ARGS_INPUT_ERR;
        *oerr = EXT_ARGS_FmtErr(prs.jbuf, "Ambiguous argument \"%s\"", bk);
        goto done;
      }
      continue;
    }

    if(EXT_ARGS_Char('=', ipr)) {
      char *str = EXT_ARGS_CurrentPos(ipr);
      EXT_ARGS_DYN_ARY_SAVE(inp, tokens, EXT_ARGS_PREALLOC, 0, ((EXT_ARGS_UTok){.type = EXT_ARGS_UTOK_EQL, .str = str - 1}), prs.jbuf);
      if(*str != '\0') {
        EXT_ARGS_DYN_ARY_SAVE(inp, tokens, EXT_ARGS_PREALLOC, 0, ((EXT_ARGS_UTok){.type = EXT_ARGS_UTOK_VAL, .str = str}), prs.jbuf);
      }
      continue;
    }

    if(EXT_ARGS_Char('-', ipr) && EXT_ARGS_Char('-', ipr)) {
      EXT_ARGS_DYN_ARY_SAVE(inp, tokens, EXT_ARGS_PREALLOC, 0, ((EXT_ARGS_UTok){.type = EXT_ARGS_UTOK_EOI}), prs.jbuf);
      break;
    }

    EXT_ARGS_DYN_ARY_SAVE(inp, tokens, EXT_ARGS_PREALLOC, 0, ((EXT_ARGS_UTok){.type = EXT_ARGS_UTOK_VAL, .str = bk}), prs.jbuf);
  }

  if(inp->tokensCount == 0 || inp->tokens[inp->tokensCount - 1].type != EXT_ARGS_UTOK_EOI) {
    EXT_ARGS_DYN_ARY_SAVE(inp, tokens, EXT_ARGS_PREALLOC, 0, ((EXT_ARGS_UTok){.type = EXT_ARGS_UTOK_EOI}), prs.jbuf);
  }

  // Parsing

  int i = 0;
  for(;;) {
    if(inp->tokens[i].type == EXT_ARGS_UTOK_EOI) {
      break;
    }

    if(inp->tokens[i].type == EXT_ARGS_UTOK_FLOAT) {
      EXT_ARGS_DYN_ARY_SAVE(inp, floats, EXT_ARGS_PREALLOC, 0, ((EXT_ARGS_UFloatArg){
        .str = inp->tokens[i].str,
        .len = inp->tokens[i].len
      }), prs.jbuf);
      EXT_ARGS_UFloatArg *arg = &inp->floats[inp->floatsCount - 1];

      i++;

      if(inp->tokens[i].type == EXT_ARGS_UTOK_EQL) {
        i++;
        if(inp->tokens[i].type != EXT_ARGS_UTOK_VAL) {
          res = EXT_ARGS_INPUT_ERR;
          *oerr = EXT_ARGS_FmtErr(prs.jbuf, "A value expected \"%s\"", inp->tokens[i - 2].str);
          goto done;
        }
        arg->assignVal = inp->tokens[i].str;

        i++;
      }
      continue;
    }

    if(inp->tokens[i].type == EXT_ARGS_UTOK_VAL) {
      EXT_ARGS_DYN_ARY_SAVE(inp, posArgs, EXT_ARGS_PREALLOC, 0, inp->tokens[i].str, prs.jbuf);
      i++;
      continue;
    }

    res = EXT_ARGS_INPUT_ERR;
    *oerr = EXT_ARGS_FmtErr(prs.jbuf, "Unexpected input \"%s\"", inp->tokens[i].str);
    goto done;
  }

  if(inp->tokens[i].type != EXT_ARGS_UTOK_EOI) {
    res = EXT_ARGS_INPUT_ERR;
    *oerr = EXT_ARGS_FmtErr(prs.jbuf, "Unexpected input \"%s\"", inp->tokens[i].str);
    goto done;
  }

  // Validations

  for(int i = 0; i < inp->floatsCount; i++) {
    EXT_ARGS_UFloatArg uf = inp->floats[i];
    bool found = false;

    for(int i = 0; i < prs.floatsCount; i++) {
      EXT_ARGS_FloatArg f = prs.floats[i];
      if(uf.len != f.len) {
        continue;
      }
      if(strncmp(uf.str, f.str, f.len) == 0) {
        EXT_ARGS_FloatArgsGroup *gr = &prs.groups[f.groupIdx];
        if(gr->isUsed) {
          if(!gr->isRepeating) {
            res = EXT_ARGS_INPUT_ERR;
            *oerr = EXT_ARGS_FmtErr(prs.jbuf, "Same arguments provided multiple times: %.*s", uf.len, uf.str);
            goto done;
          }
        }

        if(gr->hasAssign) {
          if(!uf.assignVal && !gr->isAssignOptional) {
            res = EXT_ARGS_INPUT_ERR;
            *oerr = EXT_ARGS_FmtErr(prs.jbuf, "\"%.*s\" argument requires a value", uf.len, uf.str);
            goto done;
          }
        } else { // assign not required
          if(uf.assignVal) {
            res = EXT_ARGS_INPUT_ERR;
            *oerr = EXT_ARGS_FmtErr(prs.jbuf, "\"%.*s\" argument does not require a value", uf.len, uf.str);
            goto done;
          }
        }

        gr->isUsed = true;
        found = true;
        break;
      }
    }

    if(!found) {
      res = EXT_ARGS_INPUT_ERR;
      *oerr = EXT_ARGS_FmtErr(prs.jbuf, "Ambiguous argument \"%.*s\" provided", uf.len, uf.str);
      goto done;
    }
  }

  // Checking floats that specified but not provided
  for(int i = 0; i < prs.groupsCount; i++) {
    EXT_ARGS_FloatArgsGroup *gr = &prs.groups[i];
    if(!gr->isUsed) {
      if(!gr->isOptional) {
        for(int j = 0; j < prs.floatsCount; j++) {
          EXT_ARGS_FloatArg fa = prs.floats[j];
          if(fa.groupIdx == i) {
            res = EXT_ARGS_INPUT_ERR;
            char *s = gr->aliasCount > 1 ? "(or alias) " : "";
            *oerr = EXT_ARGS_FmtErr(prs.jbuf, "\"%.*s\" argument %srequired but not provided", fa.len, fa.str, s);
            goto done;
          }
        }
      }
    }
  }

  int manposArgsCount = 0;
  for(int i = 0; i < prs.posArgsCount; i++) {
    if(!prs.posArgs[i].isOptional) {
      manposArgsCount += 1;
    }
  }

  if(inp->posArgsCount < manposArgsCount) {
    res = EXT_ARGS_INPUT_ERR;
    *oerr = EXT_ARGS_FmtErr(prs.jbuf, "Not enough positional arguments provided");
    goto done;
  }

  if(!prs.varPosArgsEnabled) {
    if(inp->posArgsCount > prs.posArgsCount) {
      res = EXT_ARGS_INPUT_ERR;
      *oerr = EXT_ARGS_FmtErr(prs.jbuf, "Too many positional arguments provided");
      goto done;
    }
  }

  // Get pointers to user passed vars

  for(int i = 0; i < prs.sequenceCount; i++) {
    EXT_ARGS_SequenceElement sq = prs.sequence[i];
    void *p = va_arg(ap, void *);
    if(sq.type == EXT_ARGS_ARG_POS) {
      prs.posArgs[sq.idx].varPtr = p;
    } else {
      prs.groups[sq.idx].varPtr = p;
    }
  }
  if(prs.varPosArgsEnabled) {
    prs.varPosrArgsVarPtr = va_arg(ap, void *);
  }

  // Vars filling, assings

  for(int i = 0; i < inp->floatsCount; i++) {
    EXT_ARGS_UFloatArg uf = inp->floats[i];

    for(int i = 0; i < prs.floatsCount; i++) {
      EXT_ARGS_FloatArg f = prs.floats[i];
      if(uf.len != f.len) {
        continue;
      }
      if(strncmp(uf.str, f.str, f.len) == 0) {
        EXT_ARGS_FloatArgsGroup *gr = &prs.groups[f.groupIdx];

        if(gr->isRepeating) {
          // for repeating assign always exists
          EXT_ARGS_DYN_ARY_SAVE(gr, ary, EXT_ARGS_PREALLOC, 1, uf.assignVal, prs.jbuf);
          gr->ary[gr->aryCount] = NULL;
          if(gr->varPtr) {
            *((char ***)gr->varPtr) = gr->ary;
          }

        } else {
          if(gr->hasAssign) {
            if(uf.assignVal) {
              if(gr->varPtr) {
                *((char **)gr->varPtr) = uf.assignVal;
              }
            } else { // no value provided by user but the flag is set
              if(gr->varPtr) {
                *((char **)gr->varPtr) = ext_args_no_value;
              }
            }
          } else { // assign not required
            if(gr->varPtr) {
              *((bool *)gr->varPtr) = true;
            }
          }
        }

        break;
      }
    }
  }

  // filling floats that specified but not provided
  for(int i = 0; i < prs.groupsCount; i++) {
    EXT_ARGS_FloatArgsGroup *gr = &prs.groups[i];
    if(!gr->isUsed) {
      if(gr->isOptional) {
        if(gr->isRepeating) {
          EXT_ARGS_DYN_ARY_SAVE(gr, ary, 1, 0, NULL, prs.jbuf);
          if(gr->varPtr) {
            *((char ***)gr->varPtr) = gr->ary;
          }
        } else {
          if(gr->varPtr) {
            if(gr->hasAssign) {
              *((char **)gr->varPtr) = NULL;
            } else {
              *((bool *)gr->varPtr) = false;
            }
          }
        }
      }
    }
  }

  // NULLing optionals not provided by user
  for(int i = inp->posArgsCount; i < prs.posArgsCount; i++) {
    char *p = prs.posArgs[i].varPtr;
    if(p) {
      *((char **)p) = NULL;
    }
  }

  { // filling posArgs vars
    EXT_ARGS_Ary *optsVarAry = &(EXT_ARGS_Ary){0};

    for(int i = 0; i < inp->posArgsCount; i++) {
      if(i < prs.posArgsCount) {
        void *p = prs.posArgs[i].varPtr;
        if(p) {
          *((char **)p) = inp->posArgs[i];
        }
      } else {
        // Filling opts

        if(!prs.varPosrArgsVarPtr) {
          // No need to create an array and assign anything
          break;
        }

        EXT_ARGS_DYN_ARY_SAVE(optsVarAry, ary, EXT_ARGS_PREALLOC, 1, inp->posArgs[i], prs.jbuf);
        optsVarAry->ary[optsVarAry->aryCount] = NULL;
        if(prs.varPosrArgsVarPtr) {
          *((char ***)prs.varPosrArgsVarPtr) = optsVarAry->ary;
        }
      }
    }

    // Filling optional Pos args for which values not provided by user,
    // like c and d in this schema: a b [c] [d]
    for(int i = inp->posArgsCount; i < prs.posArgsCount; i++) {
      void *p = prs.posArgs[i].varPtr;
      if(p) {
        *((char **)p) = NULL;
      }
    }

    // No external pos args provided, let's return an empty array
    if(prs.varPosArgsEnabled && !optsVarAry->ary) {
      if(prs.varPosrArgsVarPtr) {
        char ***p = prs.varPosrArgsVarPtr;
        *p = calloc(1, sizeof(*p));
        if(!(*p)) {
          longjmp(prs.jbuf, EXT_ARGS_ERR_MEM);
        }
        **p = NULL;
      }
    }
  }

done:
  free(prs.posArgs);
  free(prs.groups);
  free(prs.floats);
  free(prs.sequence);

  free(inp->tokens);
  free(inp->floats);
  free(inp->posArgs);

  return res;
}

#endif // INCLUDE_EXT_ARGS_H

/*
------------------------------------------------------------------------------
This software is available under 2 licenses -- choose whichever you prefer.
------------------------------------------------------------------------------
ALTERNATIVE A - MIT License
Copyright (c) 2017 Sean Barrett
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
------------------------------------------------------------------------------
ALTERNATIVE B - Public Domain (www.unlicense.org)
This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.
In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
------------------------------------------------------------------------------
*/
