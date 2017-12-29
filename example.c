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
      fprintf(stderr, "ExtArg - incorrect schema. Error: %s file: %s, line: %d\n",
        err, __FILE__, __LINE__);
      exit(EXIT_FAILURE);

    case EXT_ARGS_INPUT_ERR:
      return err;
  }
  return NULL;
}

int main(int argc, char *argv[]) {
  char *fmt = "-f|--flag1=val [--flag2] [-f3[=val]] [-D=val...] fname lname [mname] ...";

  if(!eargs(argc, argv, "-h", NULL)) {
    printf("Usage: %s\n", fmt);
    return 0;
  }

  char *f, *f3, *fname, *lname, *mname;
  bool flag2;
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

  puts("D = [");
  for(int i = 0; d[i]; i++) {
    printf("  %s\n", d[i]);
  }
  puts("]");

  puts("files = [");
  for(int i = 0; files[i]; i++) {
    printf("  %s\n", files[i]);
  }
  puts("]");

  free(d);
  free(files);
}
