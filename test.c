#include <assert.h>
#include "ext_args.h"

int eargs(int argc, char *argv[], char *fmt, char **oerr, ...) {
  va_list ap;
  va_start(ap, oerr);
  int res = ext_args(argc, argv, fmt, ap, oerr);
  va_end(ap);
  return res;
}

int main() {
   // Schema Lexing errors
  {
    {
      char *err = NULL;
      int res = eargs(1, (char *[]){""}, "a ..", &err, NULL);
      assert(res == EXT_ARGS_SCHEMA_ERR);
      assert(!strcmp(err, "Schema lexing error, starting from \"..\""));
      free(err);
    }

    {
      char *err = NULL;
      int res = eargs(1, (char *[]){""}, "1a", &err, NULL);
      assert(res == EXT_ARGS_SCHEMA_ERR);
      assert(!strcmp(err, "Schema lexing error, starting from \"1a\""));
      free(err);
    }

    {
      char *err = NULL;
      int res = eargs(1, (char *[]){""}, "-a=1", &err, NULL);
      assert(res == EXT_ARGS_SCHEMA_ERR);
      assert(!strcmp(err, "Schema lexing error, starting from \"1\""));
      free(err);
    }
  }

  // Schema Parsing errors
  {
    {
      char *err = NULL;
      int res = eargs(1, (char *[]){""}, "[a", &err, NULL);
      assert(res == EXT_ARGS_SCHEMA_ERR);
      assert(!strcmp(err, "Schema parsing error. Expected EOI but received LBRAK, starting from \"[a\""));
      free(err);
    }

    {
      char *err = NULL;
      int res = eargs(1, (char *[]){""}, "a]", &err, NULL);
      assert(res == EXT_ARGS_SCHEMA_ERR);
      assert(!strcmp(err, "Schema parsing error. Expected EOI but received RBRAK, starting from \"]\""));
      free(err);
    }

    {
      char *err = NULL;
      int res = eargs(1, (char *[]){""}, "... a", &err, NULL);
      assert(res == EXT_ARGS_SCHEMA_ERR);
      assert(!strcmp(err, "Schema parsing error. Expected EOI but received NAME, starting from \" a\""));
      free(err);
    }

    {
      char *err = NULL;
      int res = eargs(1, (char *[]){""}, "-a=", &err, NULL);
      assert(res == EXT_ARGS_SCHEMA_ERR);
      assert(!strcmp(err, "Schema parsing error. Expected EOI but received EQL, starting from \"=\""));
      free(err);
    }

    {
      char *err = NULL;
      int res = eargs(1, (char *[]){""}, "-a[=val...]", &err, NULL);
      assert(res == EXT_ARGS_SCHEMA_ERR);
      assert(!strcmp(err, "Schema parsing error. Expected EOI but received LBRAK, starting from \"[=val...]\""));
      free(err);
    }

    {
      char *err = NULL;
      int res = eargs(1, (char *[]){""}, "[[-a]]", &err, NULL);
      assert(res == EXT_ARGS_SCHEMA_ERR);
      assert(!strcmp(err, "Schema parsing error. Expected EOI but received LBRAK, starting from \"[[-a]]\""));
      free(err);
    }

    {
      char *err = NULL;
      int res = eargs(1, (char *[]){""}, "[a] b", &err, NULL);
      assert(res == EXT_ARGS_SCHEMA_ERR);
      assert(!strcmp(err, "All optional non-flag arguments must be chained on the schema's right side"));
      free(err);
    }
  }

  // User Input Lexing errors
  {
    {
      char *err = NULL;
      int res = eargs(2, (char *[]){"", "-a.one"}, "-a=val", &err, NULL);
      assert(res == EXT_ARGS_INPUT_ERR);
      assert(!strcmp(err, "Ambiguous argument \"-a.one\""));
      free(err);
    }
  }

  // Using Input Parsing error
  {
    char *err = NULL;
    int res = eargs(2, (char *[]){"", "-a="}, "-a=val", &err, NULL);
    assert(res == EXT_ARGS_INPUT_ERR);
    assert(!strcmp(err, "A value expected \"-a=\""));
    free(err);
  }

  {
    char *err = NULL;
    int res = eargs(3, (char *[]){"", "-a=1" ,"=b"}, "-a=val", &err, NULL);
    assert(res == EXT_ARGS_INPUT_ERR);
    assert(!strcmp(err, "Unexpected input \"=b\""));
    free(err);
  }

  // Lexed/parsed successfully

  // Validation/match errors
  {
    {
      char *err = NULL;
      int res = eargs(3, (char *[]){"", "-a", "-a"}, "[-a]", &err, NULL);
      assert(res == EXT_ARGS_INPUT_ERR);
      assert(!strcmp(err, "Same arguments provided multiple times: -a"));
      free(err);
    }

    {
      char *err = NULL;
      int res = eargs(2, (char *[]){"", "-a"}, "-a=val", &err, NULL);
      assert(res == EXT_ARGS_INPUT_ERR);
      assert(!strcmp(err, "\"-a\" argument requires a value"));
      free(err);
    }

    {
      char *err = NULL;
      int res = eargs(3, (char *[]){"", "-D=1", "-D"}, "-D=val...", &err, NULL);
      assert(res == EXT_ARGS_INPUT_ERR);
      assert(!strcmp(err, "\"-D\" argument requires a value"));
      free(err);
    }

    {
      char *err = NULL;
      int res = eargs(2, (char *[]){"", "-a=1"}, "[-a]", &err, NULL);
      assert(res == EXT_ARGS_INPUT_ERR);
      assert(!strcmp(err, "\"-a\" argument does not require a value"));
      free(err);
    }

    {
      char *err = NULL;
      int res = eargs(2, (char *[]){"", "-b"}, "[-a]", &err, NULL);
      assert(res == EXT_ARGS_INPUT_ERR);
      assert(!strcmp(err, "Ambiguous argument \"-b\" provided"));
      free(err);
    }

    {
      char *err = NULL;
      int res = eargs(1, (char *[]){""}, "-a|-b=val", &err, NULL);
      assert(res == EXT_ARGS_INPUT_ERR);
      assert(!strcmp(err, "\"-a\" argument (or alias) required but not provided"));
      free(err);
    }

    {
      char *err = NULL;
      int res = eargs(2, (char *[]){"", "1"}, "a b [c]", &err, NULL);
      assert(res == EXT_ARGS_INPUT_ERR);
      assert(!strcmp(err, "Not enough positional arguments provided"));
      free(err);
    }

    {
      char *err = NULL;
      int res = eargs(3, (char *[]){"", "1", "2"}, "a", &err, NULL);
      assert(res == EXT_ARGS_INPUT_ERR);
      assert(!strcmp(err, "Too many positional arguments provided"));
      free(err);
    }
  }

  // Validated successfully

  // Floating args
  {
    // Booleans
    {
      {
        bool a = false, b = false;
        char *err = NULL;
        int res = eargs(2, (char *[]){"", "-b"}, "[-a] [-b]", &err, &a, &b);
        assert(res == EXT_ARGS_NO_ERR);
        assert(err == NULL);
        assert(a == false);
        assert(b == true);
      }

      // Aliasses, multiple dashes
      {
        char *err = NULL;
        bool a = false;
        int res = eargs(2, (char *[]){"", "---c"}, "-a|--b|---c", &err, &a);
        assert(res == EXT_ARGS_NO_ERR);
        assert(err == NULL);
        assert(a == true);
      }

      // Gieven NULL as pointer to the receiver variable
      {
        char *err = NULL;
        int res = eargs(2, (char *[]){"", "-a"}, "-a", &err, NULL);
        assert(res == EXT_ARGS_NO_ERR);
        assert(err == NULL);
      }

      // Sets receiver var to `false` if optional flag not set
      {
        char *err = NULL;
        bool a = true;
        int res = eargs(1, (char *[]){""}, "[-a]", &err, &a);
        assert(res == EXT_ARGS_NO_ERR);
        assert(err == NULL);
        assert(a == false);
      }
    }
    // With value
    {
      {
        char *err = NULL;
        char *a = NULL, *b = NULL;
        int res = eargs(3, (char *[]){"", "-a=one", "-b=two"}, "-a=val -b=val", &err, &a, &b);
        assert(res == EXT_ARGS_NO_ERR);
        assert(err == NULL);
        assert(!strcmp(a, "one"));
        assert(!strcmp(b, "two"));
      }

      // One flag, multiple values
      {
        // Non-optional
        {
          char *err = NULL;
          char **a = NULL, **p = NULL;
          int res = eargs(3, (char *[]){"", "-a=1", "-a=2"}, "-a=val...", &err, &a);
          assert(res == EXT_ARGS_NO_ERR);
          assert(err == NULL);
          p = a;
          assert(!strcmp(*p++, "1"));
          assert(!strcmp(*p++, "2"));
          assert(*p == NULL);
          free(a);
        }

        // Optional, nothing provided by User
        {
          char *err = NULL;
          char **a = NULL;
          int res = eargs(1, (char *[]){""}, "[-a=val...]", &err, &a);
          assert(res == EXT_ARGS_NO_ERR);
          assert(err == NULL);
          assert(*a == NULL);
          free(a);
        }
      }

      // Sets receiver var to NULL if optional flag not set
      {
        char *err = NULL;
        char *a = (char *)123;
        int res = eargs(1, (char *[]){""}, "[-a=val]", &err, &a);
        assert(res == EXT_ARGS_NO_ERR);
        assert(err == NULL);
        assert(a == NULL);
      }

      // Optional value with optional value not provided
      {
        char *err = NULL;
        char *a = (char *)123;
        int res = eargs(1, (char *[]){""}, "[-a[=val]]", &err, &a);
        assert(res == EXT_ARGS_NO_ERR);
        assert(err == NULL);
        assert(a == NULL);
      }

      // Non-optional flag provided, optional value not
      {
        char *err = NULL;
        char *a = NULL;
        int res = eargs(2, (char *[]){"", "-a"}, "-a[=val]", &err, &a);
        assert(res == EXT_ARGS_NO_ERR);
        assert(err == NULL);
        assert(a == ext_args_no_value);
      }

      // Optional flag provided but optional value not
      {
        char *err = NULL;
        char *a = (char *)123;
        int res = eargs(2, (char *[]){"", "-a"}, "[-a[=val]]", &err, &a);
        assert(res == EXT_ARGS_NO_ERR);
        assert(err == NULL);
        assert(a == ext_args_no_value);
      }
    }
  }

  // Static (non-floating) args
  {
    {
      char *err = NULL;
      char *a = NULL, *b = NULL, *c = NULL;
      int res = eargs(3, (char *[]){"", "one", "two"}, "a b [c]", &err, &a, &b, &c);
      assert(res == EXT_ARGS_NO_ERR);
      assert(err == NULL);
      assert(!strcmp(a, "one"));
      assert(!strcmp(b, "two"));
      assert(c == NULL);
    }

    // Sets receiver var to NULL if optional arg no provided
    {
      char *err = NULL;
      char *a = (char *)123;
      int res = eargs(1, (char *[]){""}, "[a]", &err, &a);
      assert(res == EXT_ARGS_NO_ERR);
      assert(err == NULL);
      assert(a == NULL);
    }

    // Variadic arguments
    {
      {
        char *err = NULL;
        char *a = NULL, *b = NULL;
        char **opts = NULL;
        int res = eargs(2, (char *[]){"", "one"}, "a [b] ...", &err, &a, &b, &opts);
        assert(res == EXT_ARGS_NO_ERR);
        assert(err == NULL);
        assert(!strcmp(a, "one"));
        assert(b == NULL);
        assert(*opts == NULL);
        free(opts);
      }

      {
        char *err = NULL;
        char *a = NULL, *b = NULL;
        char **opts = NULL;
        int res = eargs(3, (char *[]){"", "1", "2"}, "a [b] ...", &err, &a, &b, &opts);
        assert(res == EXT_ARGS_NO_ERR);
        assert(err == NULL);
        assert(!strcmp(a, "1"));
        assert(!strcmp(b, "2"));
        assert(*opts == NULL);
        free(opts);
      }

      {
        char *err = NULL;
        char *a = NULL, *b = NULL;
        char **opts = NULL;
        int res = eargs(4, (char *[]){"", "1", "2", "3"}, "a [b] ...", &err, &a, &b, &opts);
        assert(res == EXT_ARGS_NO_ERR);
        assert(err == NULL);
        assert(!strcmp(a, "1"));
        assert(!strcmp(b, "2"));
        assert(!strcmp(*opts, "3"));
        assert(*(opts + 1) == NULL);
        free(opts);
      }

      {
        char *err = NULL;
        char **opts = NULL;
        int res = eargs(3, (char *[]){"", "1", "2"}, "...", &err, &opts);
        assert(res == EXT_ARGS_NO_ERR);
        assert(err == NULL);
        assert(!strcmp(*opts, "1"));
        assert(!strcmp(*(opts + 1), "2"));
        assert(*(opts + 2) == NULL);
        free(opts);
      }
    }
  }
}
