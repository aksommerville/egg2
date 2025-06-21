/* egg_test.h
 */
 
#ifndef EGG_TEST_H
#define EGG_TEST_H

#if 0 /*XXX*/
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <stdlib.h>
#else
#include "opt/stdlib/egg-stdlib.h"
#endif

#define EGG_ITEST(name,...) int name()
#define XXX_EGG_ITEST(name,...) int name()

#define EGG_UTEST(name,...) EGG_UTEST_INNER(name,0,##__VA_ARGS__)
#define XXX_EGG_UTEST(name,...) EGG_UTEST_INNER(name,1,##__VA_ARGS__)
#define EGG_UTEST_INNER(name,ignore,...) { \
  if (!egg_test_filter(#name,#__VA_ARGS__,__FILE__,ignore)) fprintf(stderr,"EGG_TEST SKIP %s %s %s\n",#name,__FILE__,#__VA_ARGS__); \
  else if (name()<0) fprintf(stderr,"EGG_TEST FAIL %s %s %s\n",#name,__FILE__,#__VA_ARGS__); \
  else fprintf(stderr,"EGG_TEST PASS %s %s %s\n",#name,__FILE__,#__VA_ARGS__); \
}

int egg_test_filter(const char *name,const char *tags,const char *file,int ignore);

int egg_string_printable(const char *src,int srcc);

#define EGG_FAILURE_VALUE -1

#define EGG_FAIL_MORE(k,fmt,...) fprintf(stderr,"EGG_TEST DETAIL * %15s "fmt"\n",k,##__VA_ARGS__);

#define EGG_FAIL_END { \
  fprintf(stderr,"EGG_TEST DETAIL ******************************************************\n"); \
  return EGG_FAILURE_VALUE; \
}

#define EGG_FAIL_BEGIN(fmt,...) { \
  fprintf(stderr,"EGG_TEST DETAIL ******************************************************\n"); \
  EGG_FAIL_MORE("Location","%s:%d",__FILE__,__LINE__) \
  if (fmt&&fmt[0]) EGG_FAIL_MORE("Message",fmt,##__VA_ARGS__) \
}

#define EGG_FAIL(...) { \
  EGG_FAIL_BEGIN(""__VA_ARGS__) \
  EGG_FAIL_END \
}

#define EGG_ASSERT(condition,...) { \
  if (!(condition)) { \
    EGG_FAIL_BEGIN(""__VA_ARGS__) \
    EGG_FAIL_MORE("Expected","true") \
    EGG_FAIL_MORE("As written","%s",#condition) \
    EGG_FAIL_END \
  } \
}

#define EGG_ASSERT_NOT(condition,...) { \
  if (condition) { \
    EGG_FAIL_BEGIN(""__VA_ARGS__) \
    EGG_FAIL_MORE("Expected","false") \
    EGG_FAIL_MORE("As written","%s",#condition) \
    EGG_FAIL_END \
  } \
}

#define EGG_ASSERT_CALL(call,...) { \
  int _result=(call); \
  if (_result<0) { \
    EGG_FAIL_BEGIN(""__VA_ARGS__) \
    EGG_FAIL_MORE("Expected","successful call") \
    EGG_FAIL_MORE("Result","%d",_result) \
    EGG_FAIL_MORE("As written","%s",#call) \
    EGG_FAIL_END \
  } \
}

#define EGG_ASSERT_FAILURE(call,...) { \
  int _result=(call); \
  if (_result>=0) { \
    EGG_FAIL_BEGIN(""__VA_ARGS__) \
    EGG_FAIL_MORE("Expected","failed call") \
    EGG_FAIL_MORE("Result","%d",_result) \
    EGG_FAIL_MORE("As written","%s",#call) \
    EGG_FAIL_END \
  } \
}

#define EGG_ASSERT_INTS_OP(a,op,b,...) { \
  int _a=(a),_b=(b); \
  if (!(_a op _b)) { \
    EGG_FAIL_BEGIN(""__VA_ARGS__) \
    EGG_FAIL_MORE("As written","%s %s %s",#a,#op,#b) \
    EGG_FAIL_MORE("Values","%d %s %d",_a,#op,_b) \
    EGG_FAIL_END \
  } \
}

#define EGG_ASSERT_INTS(a,b,...) EGG_ASSERT_INTS_OP(a,==,b,##__VA_ARGS__)

#define EGG_ASSERT_FLOATS(a,b,e,...) { \
  double _a=(a),_b=(b),_e=(e); \
  double _d=_a-_b; if (_d<0.0) _d=-_d; \
  if (_d>_e) { \
    EGG_FAIL_BEGIN(""__VA_ARGS__) \
    EGG_FAIL_MORE("As written","%s == %s within %s",#a,#b,#e) \
    EGG_FAIL_MORE("Values","%f == %f within %f",_a,_b,_e) \
    EGG_FAIL_MORE("Delta","%f",_d) \
    EGG_FAIL_END \
  } \
}

#define EGG_ASSERT_STRINGS(a,ac,b,bc,...) { \
  const char *_a=(a),*_b=(b); \
  int _ac=(ac),_bc=(bc); \
  if (!_a) _ac=0; else if (_ac<0) { _ac=0; while (_a[_ac]) _ac++; } \
  if (!_b) _bc=0; else if (_bc<0) { _bc=0; while (_b[_bc]) _bc++; } \
  if ((_ac!=_bc)||memcmp(_a,_b,_ac)) { \
    EGG_FAIL_BEGIN(""__VA_ARGS__) \
    EGG_FAIL_MORE("(A) As written","%s : %s",#a,#ac) \
    EGG_FAIL_MORE("(B) As written","%s : %s",#b,#bc) \
    if (egg_string_printable(_a,_ac)) EGG_FAIL_MORE("(A) Value","%.*s",_ac,_a) else EGG_FAIL_MORE("(A) Unprintable","") \
    if (egg_string_printable(_b,_bc)) EGG_FAIL_MORE("(B) Value","%.*s",_bc,_b) else EGG_FAIL_MORE("(B) Unprintable","") \
    EGG_FAIL_END \
  } \
}

#endif
