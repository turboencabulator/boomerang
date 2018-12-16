#define sign_extend(N,SIZE) (((int)((N) << (sizeof(unsigned)*8-(SIZE)))) >> (sizeof(unsigned)*8-(SIZE)))
#include <assert.h>

#line 1 "machine/st20/decoder.m"
/**
 * \file
 * \brief Contains the high level decoding functionality, for matching ST-20
 *        instructions.
 *
 * \authors
 * Copyright (C) 2005 Mike Van Emmerik
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "st20decoder.h"

#include "BinaryFile.h"
#include "boomerang.h"
#include "exp.h"
#include "rtl.h"
#include "statement.h"

#define fetch8(pc) bf->readNative1(pc)

/**
 * Constructor.  The code won't work without this (not sure why the default
 * constructor won't do...)
 */
ST20Decoder::ST20Decoder(Prog *prog) :
	NJMCDecoder(prog)
{
	std::string file = Boomerang::get()->getProgPath() + "frontend/machine/st20/st20.ssl";
	RTLDict.readSSLFile(file);
}

#if 0 // Cruft?
// For now...
int
ST20Decoder::decodeAssemblyInstruction(ADDRESS, ptrdiff_t)
{
	return 0;
}
#endif

static DecodeResult result;

/**
 * Decodes a machine instruction and returns an RTL instance.  In all cases a
 * single instruction is decoded.
 *
 * \param pc       The native address of the pc.
 * \param delta    The difference between the above address and the host
 *                 address of the pc (i.e. the address that the pc is at in
 *                 the loaded object file).
 * \param RTLDict  The dictionary of RTL templates used to instantiate the RTL
 *                 for the instruction being decoded.
 * \param proc     The enclosing procedure.
 *
 * \returns  A DecodeResult structure containing all the information gathered
 *           during decoding.
 */
DecodeResult &
ST20Decoder::decodeInstruction(ADDRESS pc, const BinaryFile *bf)
{
	result.reset();  // Clear the result structure (numBytes = 0 etc)
	unsigned total = 0;  // Total value from all prefixes

	while (1) {

#line 78 "st20decoder.cpp"

#line 72 "machine/st20/decoder.m"
{ 
  ADDRESS MATCH_p = 
    
#line 72 "machine/st20/decoder.m"
pc + result.numBytes++
#line 86 "st20decoder.cpp"
;
  const char *MATCH_name;
  static const char *MATCH_name_fc_0[] = {
    NULL, "ldlp", NULL, "ldnl", "ldc", "ldnlp", NULL, "ldl", "adc", NULL, 
    NULL, "ajw", "eqc", "stl", "stnl", 
  };
  unsigned /* [0..255] */ MATCH_w_8_0;
  { 
    MATCH_w_8_0 = fetch8(MATCH_p); 
    
      switch((MATCH_w_8_0 >> 4 & 0xf) /* fc at 0 */) {
        case 0: 
          { 
            unsigned oper = (MATCH_w_8_0 & 0xf) /* bot at 0 */;
            
#line 85 "machine/st20/decoder.m"

			result.rtl = unconditionalJump(pc, "j", pc + result.numBytes + total + oper);


#line 107 "st20decoder.cpp"

            
          }
          
          break;
        case 1: case 3: case 4: case 5: case 7: case 8: case 11: case 12: 
        case 13: case 14: 
          MATCH_name = 
            MATCH_name_fc_0[(MATCH_w_8_0 >> 4 & 0xf) /* fc at 0 */]; 
          { 
            const char *name = MATCH_name;
            unsigned oper = (MATCH_w_8_0 & 0xf) /* bot at 0 */;
            
#line 82 "machine/st20/decoder.m"

			result.rtl = instantiate(pc, name, new Const(total + oper));


#line 126 "st20decoder.cpp"

            
          }
          
          break;
        case 2: 
          { 
            unsigned oper = (MATCH_w_8_0 & 0xf) /* bot at 0 */;
            
#line 74 "machine/st20/decoder.m"

			total = (total + oper) << 4;
			continue;


#line 142 "st20decoder.cpp"

            
          }
          
          break;
        case 6: 
          { 
            unsigned oper = (MATCH_w_8_0 & 0xf) /* bot at 0 */;
            
#line 78 "machine/st20/decoder.m"

			total = (total + ~oper) << 4;
			continue;


#line 158 "st20decoder.cpp"

            
          }
          
          break;
        case 9: 
          { 
            unsigned oper = (MATCH_w_8_0 & 0xf) /* bot at 0 */;
            
#line 88 "machine/st20/decoder.m"

			total += oper;
			result.rtl = instantiate(pc, "call", new Const(total));
			auto newCall = new CallStatement;
			newCall->setIsComputed(false);
			newCall->setDest(pc + result.numBytes + total);
			result.rtl->appendStmt(newCall);


#line 178 "st20decoder.cpp"

            
          }
          
          break;
        case 10: 
          { 
            unsigned oper = (MATCH_w_8_0 & 0xf) /* bot at 0 */;
            
#line 96 "machine/st20/decoder.m"

			auto br = new BranchStatement();
			//br->setCondType(BRANCH_JE);
			br->setDest(pc + result.numBytes + total + oper);
			//br->setCondExpr(dis_Reg(0));
			br->setCondExpr(new Binary(opEquals, dis_Reg(0), new Const(0)));
			result.rtl = new RTL(pc);
			result.rtl->appendStmt(br);


#line 199 "st20decoder.cpp"

            
          }
          
          break;
        case 15: 
          { 
            unsigned oper = (MATCH_w_8_0 & 0xf) /* bot at 0 */;
            
#line 105 "machine/st20/decoder.m"

			total |= oper;
			const char *name = nullptr;
			bool isRet = false;
			if (total >= 0) {
				switch (total) {
				case 0x00: name = "rev";           break;
				case 0x01: name = "lb";            break;
				case 0x02: name = "bsub";          break;
				case 0x03: name = "endp";          break;
				case 0x04: name = "diff";          break;
				case 0x05: name = "add";           break;
				case 0x06: name = "gcall";         break;
				case 0x07: name = "in";            break;
				case 0x08: name = "prod";          break;
				case 0x09: name = "gt";            break;
				case 0x0A: name = "wsub";          break;
				case 0x0B: name = "out";           break;
				case 0x0C: name = "sub";           break;
				case 0x0D: name = "startp";        break;
				case 0x0E: name = "outbyte";       break;
				case 0x0F: name = "outword";       break;
				case 0x10: name = "seterr";        break;
				case 0x12: name = "resetch";       break;
				case 0x13: name = "csub0";         break;
				case 0x15: name = "stopp";         break;
				case 0x16: name = "ladd";          break;
				case 0x17: name = "stlb";          break;
				case 0x18: name = "sthf";          break;
				case 0x19: name = "norm";          break;
				case 0x1A: name = "ldiv";          break;
				case 0x1B: name = "ldpi";          break;
				case 0x1C: name = "stlf";          break;
				case 0x1D: name = "xdble";         break;
				case 0x1E: name = "ldpri";         break;
				case 0x1F: name = "rem";           break;
				case 0x20: name = "ret"; isRet = true; break;
				case 0x21: name = "lend";          break;
				case 0x22: name = "ldtimer";       break;
				case 0x29: name = "testerr";       break;
				case 0x2A: name = "testpranal";    break;
				case 0x2B: name = "tin";           break;
				case 0x2C: name = "div";           break;
				case 0x2E: name = "dist";          break;
				case 0x2F: name = "disc";          break;
				case 0x30: name = "diss";          break;
				case 0x31: name = "lmul";          break;
				case 0x32: name = "not";           break;
				case 0x33: name = "xor";           break;
				case 0x34: name = "bcnt";          break;
				case 0x35: name = "lshr";          break;
				case 0x36: name = "lshl";          break;
				case 0x37: name = "lsum";          break;
				case 0x38: name = "lsub";          break;
				case 0x39: name = "runp";          break;
				case 0x3A: name = "xword";         break;
				case 0x3B: name = "sb";            break;
				case 0x3C: name = "gajw";          break;
				case 0x3D: name = "savel";         break;
				case 0x3E: name = "saveh";         break;
				case 0x3F: name = "wcnt";          break;
				case 0x40: name = "shr";           break;
				case 0x41: name = "shl";           break;
				case 0x42: name = "mint";          break;
				case 0x43: name = "alt";           break;
				case 0x44: name = "altwt";         break;
				case 0x45: name = "altend";        break;
				case 0x46: name = "and";           break;
				case 0x47: name = "enbt";          break;
				case 0x48: name = "enbc";          break;
				case 0x49: name = "enbs";          break;
				case 0x4A: name = "move";          break;
				case 0x4B: name = "or";            break;
				case 0x4C: name = "csngl";         break;
				case 0x4D: name = "ccnt1";         break;
				case 0x4E: name = "talt";          break;
				case 0x4F: name = "ldiff";         break;
				case 0x50: name = "sthb";          break;
				case 0x51: name = "taltwt";        break;
				case 0x52: name = "sum";           break;
				case 0x53: name = "mul";           break;
				case 0x54: name = "sttimer";       break;
				case 0x55: name = "stoperr";       break;
				case 0x56: name = "cword";         break;
				case 0x57: name = "clrhalterr";    break;
				case 0x58: name = "sethalterr";    break;
				case 0x59: name = "testhalterr";   break;
				case 0x5A: name = "dup";           break;
				case 0x5B: name = "move2dinit";    break;
				case 0x5C: name = "move2dall";     break;
				case 0x5D: name = "move2dnonzero"; break;
				case 0x5E: name = "move2dzero";    break;
				case 0x5F: name = "gtu";           break;
				case 0x63: name = "unpacksn";      break;
				case 0x64: name = "slmul";         break;
				case 0x65: name = "sulmul";        break;
				case 0x68: name = "satadd";        break;
				case 0x69: name = "satsub";        break;
				case 0x6A: name = "satmul";        break;
				case 0x6C: name = "postnormsn";    break;
				case 0x6D: name = "roundsn";       break;
				case 0x6E: name = "ldtraph";       break;
				case 0x6F: name = "sttraph";       break;
				case 0x71: name = "ldinf";         break;
				case 0x72: name = "fmul";          break;
				case 0x73: name = "cflerr";        break;
				case 0x74: name = "crcword";       break;
				case 0x75: name = "crcbyte";       break;
				case 0x76: name = "bitcnt";        break;
				case 0x77: name = "bitrevword";    break;
				case 0x78: name = "bitrevnbits";   break;
				case 0x79: name = "pop";           break;
				case 0x7E: name = "ldmemstartval"; break;
				case 0x81: name = "wsubdb";        break;
				case 0x9C: name = "fptesterr";     break;
				case 0xB0: name = "settimeslice";  break;
				case 0xB8: name = "xbword";        break;
				case 0xB9: name = "lbx";           break;
				case 0xBA: name = "cb";            break;
				case 0xBB: name = "cbu";           break;
				case 0xC1: name = "ssub";          break;
				case 0xC4: name = "intdis";        break;
				case 0xC5: name = "intenb";        break;
				case 0xC6: name = "ldtrapped";     break;
				case 0xC7: name = "cir";           break;
				case 0xC8: name = "ss";            break;
				case 0xCA: name = "ls";            break;
				case 0xCB: name = "sttrapped";     break;
				case 0xCC: name = "ciru";          break;
				case 0xCD: name = "gintdis";       break;
				case 0xCE: name = "gintenb";       break;
				case 0xF0: name = "devlb";         break;
				case 0xF1: name = "devsb";         break;
				case 0xF2: name = "devls";         break;
				case 0xF3: name = "devss";         break;
				case 0xF4: name = "devlw";         break;
				case 0xF5: name = "devsw";         break;
				case 0xF6: name = "null";          break;
				case 0xF7: name = "null";          break;
				case 0xF8: name = "xsword";        break;
				case 0xF9: name = "lsx";           break;
				case 0xFA: name = "cs";            break;
				case 0xFB: name = "csu";           break;
				case 0x17C:name = "lddevid";       break;
				}
			} else {
				// Total is negative, as a result of nfixes
				total = (~total & ~0xF) | (total & 0xF);  // 1's complement the upper nibbles
				switch (total) {
				case 0x00: name = "swapqueue";     break;
				case 0x01: name = "swaptimer";     break;
				case 0x02: name = "insertqueue";   break;
				case 0x03: name = "timeslice";     break;
				case 0x04: name = "signal";        break;
				case 0x05: name = "wait";          break;
				case 0x06: name = "trapdis";       break;
				case 0x07: name = "trapenb";       break;
				case 0x0B: name = "tret"; isRet = true; break;
				case 0x0C: name = "ldshadow";      break;
				case 0x0D: name = "stshadow";      break;
				case 0x1F: name = "iret"; isRet = true; break;
				case 0x24: name = "devmove";       break;
				case 0x2E: name = "restart";       break;
				case 0x2F: name = "causeerror";    break;
				case 0x30: name = "nop";           break;
				case 0x4C: name = "stclock";       break;
				case 0x4D: name = "ldclock";       break;
				case 0x4E: name = "clockdis";      break;
				case 0x4F: name = "clockenb";      break;
				case 0x8C: name = "ldprodid";      break;
				case 0x8D: name = "reboot";        break;
				}
			}
			if (name) {
				result.rtl = instantiate(pc, name);
				if (isRet) result.rtl->appendStmt(new ReturnStatement);
			} else {
				result.valid = false;  // Invalid instruction
				result.numBytes = 0;  // FIXME:  Does this really need to be cleared?
			}


#line 392 "st20decoder.cpp"

            
          }
          
          break;
        default: assert(0);
      } /* (MATCH_w_8_0 >> 4 & 0xf) -- fc at 0 --*/ 
    
  }goto MATCH_finished_a; 
  
  MATCH_finished_a: (void)0; /*placeholder for label*/
  
}
#line 406 "st20decoder.cpp"

#line 287 "machine/st20/decoder.m"
		break;
	}

	if (result.valid && !result.rtl)
		result.rtl = new RTL(pc);  // FIXME:  Why return an empty RTL?
	return result;
}

#line 417 "st20decoder.cpp"

