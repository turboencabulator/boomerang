/* Generated by the New Jersey Machine-Code Toolkit, version 0.5a */
/* command line: tools -debug-match dbm -matcher disassembler9.m -decoder dec -encoder enc -verbose hppa9.spec hppadis.spec */
#include <mclib.h>
#include "enc.h"
#define sign_extend(N,SIZE) \
  (((int)((N) << (sizeof(unsigned)*8-(SIZE)))) >> (sizeof(unsigned)*8-(SIZE)))
/**************

fid ,fmt r,t is 
  (fid) op == 12 & class_21 ==  0 & sub_16 ==  0 & r_06 = r & t_27 = t & 
  fmt_19 = fmt

***********/
void fid(int fmt, int r, int t) {
  if (!((unsigned)(fmt) < 0x4)) 
    fail("`fmt' = 0x%x won't fit in 2 unsigned bits.", fmt); 
  else if (!((unsigned)(t) < 0x20)) 
    fail("`t' = 0x%x won't fit in 5 unsigned bits.", t); 
  else if (!((unsigned)(r) < 0x20)) 
    fail("`r' = 0x%x won't fit in 5 unsigned bits.", r); 
  else 
    
    emitm(0 << 21 | 12 | 0 << 16 | (r & 0x1f) << 6 | (t & 0x1f) << 27 | 
      (fmt & 0x3) << 19, 4); 
}
/**************

fcpy ,fmt r,t is 
  (fcpy) op == 12 & class_21 ==  0 & sub_16 ==  2 & r_06 = r & t_27 = t & 
  fmt_19 = fmt

***********/
void fcpy(int fmt, int r, int t) {
  if (!((unsigned)(t) < 0x20)) 
    fail("`t' = 0x%x won't fit in 5 unsigned bits.", t); 
  else if (!((unsigned)(r) < 0x20)) 
    fail("`r' = 0x%x won't fit in 5 unsigned bits.", r); 
  else if (!((unsigned)(fmt) < 0x4)) 
    fail("`fmt' = 0x%x won't fit in 2 unsigned bits.", fmt); 
  else 
    
    emitm(0 << 21 | 12 | 2 << 16 | (r & 0x1f) << 6 | (t & 0x1f) << 27 | 
      (fmt & 0x3) << 19, 4); 
}
/**************

fabs ,fmt r,t is 
  (fabs) op == 12 & class_21 ==  0 & sub_16 ==  3 & r_06 = r & t_27 = t & 
  fmt_19 = fmt

***********/
void fabs(int fmt, int r, int t) {
  if (!((unsigned)(fmt) < 0x4)) 
    fail("`fmt' = 0x%x won't fit in 2 unsigned bits.", fmt); 
  else if (!((unsigned)(t) < 0x20)) 
    fail("`t' = 0x%x won't fit in 5 unsigned bits.", t); 
  else if (!((unsigned)(r) < 0x20)) 
    fail("`r' = 0x%x won't fit in 5 unsigned bits.", r); 
  else 
    
    emitm(0 << 21 | 12 | 3 << 16 | (r & 0x1f) << 6 | (t & 0x1f) << 27 | 
      (fmt & 0x3) << 19, 4); 
}
/**************

fsqrt ,fmt r,t is 
  (fsqrt) op == 12 & class_21 ==  0 & sub_16 ==  4 & r_06 = r & t_27 = t & 
  fmt_19 = fmt

***********/
void fsqrt(int fmt, int r, int t) {
  if (!((unsigned)(r) < 0x20)) 
    fail("`r' = 0x%x won't fit in 5 unsigned bits.", r); 
  else if (!((unsigned)(fmt) < 0x4)) 
    fail("`fmt' = 0x%x won't fit in 2 unsigned bits.", fmt); 
  else if (!((unsigned)(t) < 0x20)) 
    fail("`t' = 0x%x won't fit in 5 unsigned bits.", t); 
  else 
    
    emitm(0 << 21 | 12 | 4 << 16 | (r & 0x1f) << 6 | (t & 0x1f) << 27 | 
      (fmt & 0x3) << 19, 4); 
}
/**************

frnd ,fmt r,t is 
  (frnd) op == 12 & class_21 ==  0 & sub_16 ==  5 & r_06 = r & t_27 = t & 
  fmt_19 = fmt

***********/
void frnd(int fmt, int r, int t) {
  if (!((unsigned)(fmt) < 0x4)) 
    fail("`fmt' = 0x%x won't fit in 2 unsigned bits.", fmt); 
  else if (!((unsigned)(t) < 0x20)) 
    fail("`t' = 0x%x won't fit in 5 unsigned bits.", t); 
  else if (!((unsigned)(r) < 0x20)) 
    fail("`r' = 0x%x won't fit in 5 unsigned bits.", r); 
  else 
    
    emitm(0 << 21 | 12 | 5 << 16 | (r & 0x1f) << 6 | (t & 0x1f) << 27 | 
      (fmt & 0x3) << 19, 4); 
}
/**************

fneg ,fmt r,t is 
  (fneg) op == 12 & class_21 ==  0 & sub_16 ==  6 & r_06 = r & t_27 = t & 
  fmt_19 = fmt

***********/
void fneg(int fmt, int r, int t) {
  if (!((unsigned)(fmt) < 0x4)) 
    fail("`fmt' = 0x%x won't fit in 2 unsigned bits.", fmt); 
  else if (!((unsigned)(t) < 0x20)) 
    fail("`t' = 0x%x won't fit in 5 unsigned bits.", t); 
  else if (!((unsigned)(r) < 0x20)) 
    fail("`r' = 0x%x won't fit in 5 unsigned bits.", r); 
  else 
    
    emitm(0 << 21 | 12 | 6 << 16 | (r & 0x1f) << 6 | (t & 0x1f) << 27 | 
      (fmt & 0x3) << 19, 4); 
}
/**************

fnegabs ,fmt r,t is 
  (fnegabs) op == 12 & class_21 ==  0 & sub_16 ==  7 & r_06 = r & t_27 = t & 
  fmt_19 = fmt

***********/
void fnegabs(int fmt, int r, int t) {
  if (!((unsigned)(t) < 0x20)) 
    fail("`t' = 0x%x won't fit in 5 unsigned bits.", t); 
  else if (!((unsigned)(r) < 0x20)) 
    fail("`r' = 0x%x won't fit in 5 unsigned bits.", r); 
  else if (!((unsigned)(fmt) < 0x4)) 
    fail("`fmt' = 0x%x won't fit in 2 unsigned bits.", fmt); 
  else 
    
    emitm(0 << 21 | 12 | 7 << 16 | (r & 0x1f) << 6 | (t & 0x1f) << 27 | 
      (fmt & 0x3) << 19, 4); 
}
/**************

fcnvff ,sf,df r,t is 
  (fcnvff) op == 12 & class_21 ==  1 & sub_14 ==  0 & r_06 = r & t_27 = t & 
  df_17 = df & sf_19 = sf

***********/
void fcnvff(int sf, int df, int r, int t) {
  if (!((unsigned)(sf) < 0x4)) 
    fail("`sf' = 0x%x won't fit in 2 unsigned bits.", sf); 
  else if (!((unsigned)(df) < 0x4)) 
    fail("`df' = 0x%x won't fit in 2 unsigned bits.", df); 
  else if (!((unsigned)(t) < 0x20)) 
    fail("`t' = 0x%x won't fit in 5 unsigned bits.", t); 
  else if (!((unsigned)(r) < 0x20)) 
    fail("`r' = 0x%x won't fit in 5 unsigned bits.", r); 
  else 
    
    emitm(1 << 21 | 12 | 0 << 14 | (r & 0x1f) << 6 | (t & 0x1f) << 27 | 
      (df & 0x3) << 17 | (sf & 0x3) << 19, 4); 
}
/**************

fcnvxf ,sf,df r,t is 
  (fcnvxf) op == 12 & class_21 ==  1 & sub_14 ==  1 & r_06 = r & t_27 = t & 
  df_17 = df & sf_19 = sf

***********/
void fcnvxf(int sf, int df, int r, int t) {
  if (!((unsigned)(r) < 0x20)) 
    fail("`r' = 0x%x won't fit in 5 unsigned bits.", r); 
  else if (!((unsigned)(sf) < 0x4)) 
    fail("`sf' = 0x%x won't fit in 2 unsigned bits.", sf); 
  else if (!((unsigned)(df) < 0x4)) 
    fail("`df' = 0x%x won't fit in 2 unsigned bits.", df); 
  else if (!((unsigned)(t) < 0x20)) 
    fail("`t' = 0x%x won't fit in 5 unsigned bits.", t); 
  else 
    
    emitm(1 << 21 | 12 | 1 << 14 | (r & 0x1f) << 6 | (t & 0x1f) << 27 | 
      (df & 0x3) << 17 | (sf & 0x3) << 19, 4); 
}
/**************

fcnvfxt ,sf,df r,t is 
  (fcnvfxt) op == 12 & class_21 ==  1 & sub_14 ==  3 & r_06 = r & t_27 = t & 
  df_17 = df & sf_19 = sf

***********/
void fcnvfxt(int sf, int df, int r, int t) {
  if (!((unsigned)(t) < 0x20)) 
    fail("`t' = 0x%x won't fit in 5 unsigned bits.", t); 
  else if (!((unsigned)(r) < 0x20)) 
    fail("`r' = 0x%x won't fit in 5 unsigned bits.", r); 
  else if (!((unsigned)(sf) < 0x4)) 
    fail("`sf' = 0x%x won't fit in 2 unsigned bits.", sf); 
  else if (!((unsigned)(df) < 0x4)) 
    fail("`df' = 0x%x won't fit in 2 unsigned bits.", df); 
  else 
    
    emitm(1 << 21 | 12 | 3 << 14 | (r & 0x1f) << 6 | (t & 0x1f) << 27 | 
      (df & 0x3) << 17 | (sf & 0x3) << 19, 4); 
}
/**************

flt_c2_0c ,fmt,c r1,r2 is 
  (?noname?) op == 12 & class_21 ==  2 & r_06 = r1 & r_11 = r2 & t_27 = c & 
  fmt_19 = fmt

***********/
void flt_c2_0c(int fmt, int c, int r1, int r2) {
  if (!((unsigned)(c) < 0x20)) 
    fail("`c' = 0x%x won't fit in 5 unsigned bits.", c); 
  else if (!((unsigned)(r2) < 0x20)) 
    fail("`r2' = 0x%x won't fit in 5 unsigned bits.", r2); 
  else if (!((unsigned)(r1) < 0x20)) 
    fail("`r1' = 0x%x won't fit in 5 unsigned bits.", r1); 
  else if (!((unsigned)(fmt) < 0x4)) 
    fail("`fmt' = 0x%x won't fit in 2 unsigned bits.", fmt); 
  else 
    
    emitm(2 << 21 | 12 | (r1 & 0x1f) << 6 | (r2 & 0x1f) << 11 | 
      (c & 0x1f) << 27 | (fmt & 0x3) << 19, 4); 
}
/**************

flt_c2_0e ,f_20,c r1,r2 is 
  (?noname?) {r1[6:31] = 0, r2[6:31] = 0} => 
    op == 14 & class_21 ==  2 & f_19 = r2[5:5] & r_06 = r1[0:4] & 
    r_11 = r2[0:4] & r1_24 = r1[5:5] & f_20 = f_20 & t_27 = c

***********/
void flt_c2_0e(unsigned /* [0..1] */ f_20, int c, int r1, int r2) {
  if (!((unsigned)(f_20) < 0x2)) 
    fail("field f_20 does not fit in 1 unsigned bits"); 
  else 
    if ((r1 >> 6 & 0x3ffffff) == 0 && (r2 >> 6 & 0x3ffffff) == 0) 
      if (!((unsigned)(c) < 0x20)) 
        fail("`c' = 0x%x won't fit in 5 unsigned bits.", c); 
      else 
        
        emitm(2 << 21 | 14 | (r2 >> 5 & 0x1) << 19 | (r1 & 0x1f) << 6 | 
          (r2 & 0x1f) << 11 | (r1 >> 5 & 0x1) << 24 | (f_20 & 0x1) << 20 | 
          (c & 0x1f) << 27, 4);  
    else 
      fail("Conditions not satisfied for constructor flt_c2_0e");  
}
/**************

flt_c3_0c ,fmt r1,r2,t is 
  (?noname?) op == 12 & class_21 ==  3 & r_06 = r1 & r_11 = r2 & t_27 = t & 
  fmt_19 = fmt

***********/
void flt_c3_0c(int fmt, int r1, int r2, int t) {
  if (!((unsigned)(fmt) < 0x4)) 
    fail("`fmt' = 0x%x won't fit in 2 unsigned bits.", fmt); 
  else if (!((unsigned)(t) < 0x20)) 
    fail("`t' = 0x%x won't fit in 5 unsigned bits.", t); 
  else if (!((unsigned)(r2) < 0x20)) 
    fail("`r2' = 0x%x won't fit in 5 unsigned bits.", r2); 
  else if (!((unsigned)(r1) < 0x20)) 
    fail("`r1' = 0x%x won't fit in 5 unsigned bits.", r1); 
  else 
    
    emitm(3 << 21 | 12 | (r1 & 0x1f) << 6 | (r2 & 0x1f) << 11 | 
      (t & 0x1f) << 27 | (fmt & 0x3) << 19, 4); 
}
/**************

flt_c3_0e ,fmt r1,r2,t is 
  (?noname?) {r1[6:31] = 0, r2[6:31] = 0, t[6:31] = 0} => 
    op == 14 & class_21 ==  3 & t_27 = t[0:4] & f_19 = r2[5:5] & 
    t_25 = t[5:5] & r_06 = r1[0:4] & r_11 = r2[0:4] & r1_24 = r1[5:5] & 
    f_20 = fmt

***********/
void flt_c3_0e(int fmt, int r1, int r2, int t) {
  if ((r1 >> 6 & 0x3ffffff) == 0 && (r2 >> 6 & 0x3ffffff) == 0 && 
    (t >> 6 & 0x3ffffff) == 0) 
    if (!((unsigned)(fmt) < 0x2)) 
      fail("`fmt' = 0x%x won't fit in 1 unsigned bits.", fmt); 
    else 
      
      emitm(3 << 21 | 14 | (t & 0x1f) << 27 | (r2 >> 5 & 0x1) << 19 | 
        (t >> 5 & 0x1) << 25 | (r1 & 0x1f) << 6 | (r2 & 0x1f) << 11 | 
        (r1 >> 5 & 0x1) << 24 | (fmt & 0x1) << 20, 4);  
  else 
    fail("Conditions not satisfied for constructor flt_c3_0e"); 
}

ClosurePostfix enc_clofuns[] = {
  { (ApplyMethod) 0, (char *) 0 }
};
ClosurePostfix enc_clobytes[] = {
  { (ApplyMethod) 0, (char *) 0 }
};

/* Bytecode total is 0 */
ClosureEmitter enc_cloemitters[] = {
  { (ApplyMethod) 0, (void *) 0 /*type is a lie and a cheat*/ }
};
