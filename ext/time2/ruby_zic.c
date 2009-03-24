
#include "ruby.h"

extern int zic_main(int argc, char *argv[]);

static VALUE rb_zic(VALUE klass, VALUE args) {
  VALUE* array =  RARRAY_PTR(args);
  int len = RARRAY_LEN(args), i = 1;
  char **argv;

  argv = malloc(sizeof(char*)*(len+1));
  argv[0] = "zic";

  while (i < len + 1) {
    argv[i] = StringValueCStr(array[i-1]);
	++i;
  }

  return INT2FIX(zic_main(len + 1, argv));
}

// May be it is not best way to call zic, by it's defenitly shortest
void Init_zic(void)
{
  rb_define_method(rb_cObject, "zic", rb_zic, 1);
}
