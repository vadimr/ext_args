# ext_args.h
single-file public domain (or MIT licensed) unix command line options parser for C

## What is required?

**libc** and **c99**

## Usage

First create your wrapper function where you can specify how you want to
handle errors:

```c
#include <stdio.h>
#include "extargs.h"

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
```

Then you can use it like this:

```c
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
```

### Examples of definitions with usage:

```c
{
  // In this case the user is forced to provide the flag, `flag` var always equals to 1
  bool flag;
  char *err = eargs(argc, argv, "-f|--flag", &flag);
  if(!err) {
    printf("flag: %s\n", flag ? "YES" : "NO");
  }
}
```

```c
{
  char *flag;
  char *err = eargs(argc, argv, "-f|--flag=val", &flag);
  if(!err) {
    printf("flag value: %s\n", flag);
  }
}
```

```c
{
  bool flag;
  char *err = eargs(argc, argv, "[-f|--flag|---flaaag]", &flag);
  if(!err) {
    printf("flag: %s\n", flag ? "YES" : "NO");
  }
}
```

```c
{
  bool flag;
  char *err = eargs(argc, argv, "[-f|--flag]", &flag);
  if(!err) {
    printf("flag: %s\n", flag ? "YES" : "NO");
  }
}
```

```c
{
  char *flag;
  char *err = eargs(argc, argv, "[-f|--flag=val]", &flag);
  if(!err) {
    if(flag) {
      printf("flag value: %s\n", flag);
    }
  }
}
```

```c
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
```

```c
{
  // Use null if you don't want to save the input
  char *err = eargs(argc, argv, "-f|--flag=val", NULL);
  if(!err) {
    // ...
  }
}
```

```c
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
```

```c
{
  // Positional arguments
  char *a, *b;
  char *err = eargs(argc, argv, "a [b]", &a, &b);
  if(!err) {
    printf("a: %s\n", a);
    printf("b: %s\n", b ? b : "NOT PROVIDED");
  }
}
```

```c
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
```

### Notes

`ext_args` function doesn't mutate `argc` or `argv` arguments. It's safe to call
it multiple times on the same input.

If you want to totally free all allocated memory `free` memory allocated for
errors, variadic positional and floating arguments. Example:

```c
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
```
