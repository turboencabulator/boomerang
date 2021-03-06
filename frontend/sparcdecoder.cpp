#define sign_extend(N,SIZE) (((int)((N) << (sizeof(unsigned)*8-(SIZE)))) >> (sizeof(unsigned)*8-(SIZE)))
#include <assert.h>

#line 1 "machine/sparc/decoder.m"
/**
 * \file
 * \brief Implementation of the SPARC specific parts of the SparcDecoder
 *        class.
 *
 * \authors
 * Copyright (C) 1996-2001, The University of Queensland
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "sparcdecoder.h"

#include "boomerang.h"
#include "exp.h"
#include "prog.h"
#include "rtl.h"
#include "statement.h"

#include <cstring>

class Proc;

#define DIS_ROI     (dis_RegImm(roi, bf))
#define DIS_ADDR    (dis_Eaddr(addr, bf))
#define DIS_RD      (dis_RegLhs(rd))
#define DIS_RDR     (dis_RegRhs(rd))
#define DIS_RS1     (dis_RegRhs(rs1))
#define DIS_FS1S    (dis_RegRhs(fs1s + 32))
#define DIS_FS2S    (dis_RegRhs(fs2s + 32))
// Note: Sparc V9 has a second set of double precision registers that have an
// odd index. So far we only support V8
#define DIS_FDS     (dis_RegLhs(fds + 32))
#define DIS_FS1D    (dis_RegRhs((fs1d >> 1) + 64))
#define DIS_FS2D    (dis_RegRhs((fs2d >> 1) + 64))
#define DIS_FDD     (dis_RegLhs((fdd  >> 1) + 64))
#define DIS_FDQ     (dis_RegLhs((fdq  >> 2) + 80))
#define DIS_FS1Q    (dis_RegRhs((fs1q >> 2) + 80))
#define DIS_FS2Q    (dis_RegRhs((fs2q >> 2) + 80))

#define addressToPC(pc) (pc)
#define fetch32(pc) bf->readNative4(pc)

SparcDecoder::SparcDecoder(Prog *prog) :
	NJMCDecoder(prog)
{
	std::string file = Boomerang::get().getProgPath() + "frontend/machine/sparc/sparc.ssl";
	RTLDict.readSSLFile(file);
}

#if 0 // Cruft?
// For now...
int
SparcDecoder::decodeAssemblyInstruction(ADDRESS, ptrdiff_t)
{
	return 0;
}
#endif

/**
 * Create a HL Statement for a Bx instruction.  Instantiates a GotoStatement
 * for the unconditional branches, or a BranchStatement for the rest.
 *
 * \param pc    The location counter.
 * \param dest  The branch destination address.
 *
 * \returns  Pointer to newly created Statement, or null if invalid.
 */
Statement *
SparcDecoder::createBranch(ADDRESS pc, ADDRESS dest, const BinaryFile *bf)
{

#line 84 "sparcdecoder.cpp"

#line 78 "machine/sparc/decoder.m"
{ 
  ADDRESS MATCH_p = 
    
#line 78 "machine/sparc/decoder.m"
pc
#line 92 "sparcdecoder.cpp"
;
  unsigned MATCH_w_32_0;
  { 
    MATCH_w_32_0 = fetch32(MATCH_p); 
    
      switch((MATCH_w_32_0 >> 22 & 0x7) /* op2 at 0 */) {
        case 0: case 3: case 4: case 5: case 7: 
          goto MATCH_label_e0; break;
        case 1: 
          if ((MATCH_w_32_0 >> 30 & 0x3) /* op at 0 */ == 0) 
            
              switch((MATCH_w_32_0 >> 25 & 0xf) /* cond at 0 */) {
                case 0: 
                  
#line 149 "machine/sparc/decoder.m"

		return new GotoStatement(dest);

#line 111 "sparcdecoder.cpp"

                  
                  break;
                case 1: 
                  
#line 153 "machine/sparc/decoder.m"
 {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JE);
		return br;
	}

#line 124 "sparcdecoder.cpp"

                  
                  break;
                case 2: 
                  
#line 163 "machine/sparc/decoder.m"
 {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JSLE);
		return br;
	}

#line 137 "sparcdecoder.cpp"

                  
                  break;
                case 3: 
                  
#line 173 "machine/sparc/decoder.m"
 {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JSL);
		return br;
	}

#line 150 "sparcdecoder.cpp"

                  
                  break;
                case 4: 
                  
#line 183 "machine/sparc/decoder.m"
 {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JULE);
		return br;
	}

#line 163 "sparcdecoder.cpp"

                  
                  break;
                case 5: 
                  
#line 193 "machine/sparc/decoder.m"
 {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JUL);
		return br;
	}

#line 176 "sparcdecoder.cpp"

                  
                  break;
                case 6: 
                  
#line 203 "machine/sparc/decoder.m"
 {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JMI);
		return br;
	}

#line 189 "sparcdecoder.cpp"

                  
                  break;
                case 7: 
                  
#line 213 "machine/sparc/decoder.m"

		return new GotoStatement(dest);

#line 199 "sparcdecoder.cpp"

                  
                  break;
                case 8: 
                  
#line 151 "machine/sparc/decoder.m"

		return new GotoStatement(dest);

#line 209 "sparcdecoder.cpp"

                  
                  break;
                case 9: 
                  
#line 158 "machine/sparc/decoder.m"
 {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JNE);
		return br;
	}

#line 222 "sparcdecoder.cpp"

                  
                  break;
                case 10: 
                  
#line 168 "machine/sparc/decoder.m"
 {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JSG);
		return br;
	}

#line 235 "sparcdecoder.cpp"

                  
                  break;
                case 11: 
                  
#line 178 "machine/sparc/decoder.m"
 {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JSGE);
		return br;
	}

#line 248 "sparcdecoder.cpp"

                  
                  break;
                case 12: 
                  
#line 188 "machine/sparc/decoder.m"
 {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JUG);
		return br;
	}

#line 261 "sparcdecoder.cpp"

                  
                  break;
                case 13: 
                  
#line 198 "machine/sparc/decoder.m"
 {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JUGE);
		return br;
	}

#line 274 "sparcdecoder.cpp"

                  
                  break;
                case 14: 
                  
#line 208 "machine/sparc/decoder.m"
 {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JPOS);
		return br;
	}

#line 287 "sparcdecoder.cpp"

                  
                  break;
                case 15: 
                  
#line 215 "machine/sparc/decoder.m"

		return new GotoStatement(dest);


#line 298 "sparcdecoder.cpp"

                  
                  break;
                default: assert(0);
              } /* (MATCH_w_32_0 >> 25 & 0xf) -- cond at 0 --*/  
          else 
            goto MATCH_label_e0;  /*opt-block+*/
          break;
        case 2: 
          if ((MATCH_w_32_0 >> 30 & 0x3) /* op at 0 */ == 0) 
            
              switch((MATCH_w_32_0 >> 25 & 0xf) /* cond at 0 */) {
                case 0: 
                  
#line 79 "machine/sparc/decoder.m"

		return new GotoStatement(dest);

#line 317 "sparcdecoder.cpp"

                  
                  break;
                case 1: 
                  
#line 83 "machine/sparc/decoder.m"
 {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JE);
		return br;
	}

#line 330 "sparcdecoder.cpp"

                  
                  break;
                case 2: 
                  
#line 93 "machine/sparc/decoder.m"
 {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JSLE);
		return br;
	}

#line 343 "sparcdecoder.cpp"

                  
                  break;
                case 3: 
                  
#line 103 "machine/sparc/decoder.m"
 {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JSL);
		return br;
	}

#line 356 "sparcdecoder.cpp"

                  
                  break;
                case 4: 
                  
#line 113 "machine/sparc/decoder.m"
 {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JULE);
		return br;
	}

#line 369 "sparcdecoder.cpp"

                  
                  break;
                case 5: 
                  
#line 123 "machine/sparc/decoder.m"
 {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JUL);
		return br;
	}

#line 382 "sparcdecoder.cpp"

                  
                  break;
                case 6: 
                  
#line 133 "machine/sparc/decoder.m"
 {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JMI);
		return br;
	}

#line 395 "sparcdecoder.cpp"

                  
                  break;
                case 7: 
                  
#line 143 "machine/sparc/decoder.m"

		return new GotoStatement(dest);

#line 405 "sparcdecoder.cpp"

                  
                  break;
                case 8: 
                  
#line 81 "machine/sparc/decoder.m"

		return new GotoStatement(dest);

#line 415 "sparcdecoder.cpp"

                  
                  break;
                case 9: 
                  
#line 88 "machine/sparc/decoder.m"
 {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JNE);
		return br;
	}

#line 428 "sparcdecoder.cpp"

                  
                  break;
                case 10: 
                  
#line 98 "machine/sparc/decoder.m"
 {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JSG);
		return br;
	}

#line 441 "sparcdecoder.cpp"

                  
                  break;
                case 11: 
                  
#line 108 "machine/sparc/decoder.m"
 {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JSGE);
		return br;
	}

#line 454 "sparcdecoder.cpp"

                  
                  break;
                case 12: 
                  
#line 118 "machine/sparc/decoder.m"
 {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JUG);
		return br;
	}

#line 467 "sparcdecoder.cpp"

                  
                  break;
                case 13: 
                  
#line 128 "machine/sparc/decoder.m"
 {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JUGE);
		return br;
	}

#line 480 "sparcdecoder.cpp"

                  
                  break;
                case 14: 
                  
#line 138 "machine/sparc/decoder.m"
 {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JPOS);
		return br;
	}

#line 493 "sparcdecoder.cpp"

                  
                  break;
                case 15: 
                  
#line 145 "machine/sparc/decoder.m"

		return new GotoStatement(dest);

	// Predicted branches are handled identically.

#line 505 "sparcdecoder.cpp"

                  
                  break;
                default: assert(0);
              } /* (MATCH_w_32_0 >> 25 & 0xf) -- cond at 0 --*/  
          else 
            goto MATCH_label_e0;  /*opt-block+*/
          break;
        case 6: 
          
            switch((MATCH_w_32_0 >> 25 & 0xf) /* cond at 0 */) {
              case 0: case 7: case 8: case 15: 
                goto MATCH_label_e0; break;
              case 1: 
                if ((MATCH_w_32_0 >> 30 & 0x3) /* op at 0 */ == 0) 
                  
#line 250 "machine/sparc/decoder.m"
 {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JNE, true);
		return br;
	}

#line 529 "sparcdecoder.cpp"
 /*opt-block+*/
                else 
                  goto MATCH_label_e0;  /*opt-block+*/
                
                break;
              case 2: 
                if ((MATCH_w_32_0 >> 30 & 0x3) /* op at 0 */ == 0) 
                  
#line 218 "machine/sparc/decoder.m"
 {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JNE, true);
		return br;
	}

#line 545 "sparcdecoder.cpp"
 /*opt-block+*/
                else 
                  goto MATCH_label_e0;  /*opt-block+*/
                
                break;
              case 3: 
                if ((MATCH_w_32_0 >> 30 & 0x3) /* op at 0 */ == 0) 
                  
#line 260 "machine/sparc/decoder.m"
 {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JSL, true);
		return br;
	}

#line 561 "sparcdecoder.cpp"
 /*opt-block+*/
                else 
                  goto MATCH_label_e0;  /*opt-block+*/
                
                break;
              case 4: 
                if ((MATCH_w_32_0 >> 30 & 0x3) /* op at 0 */ == 0) 
                  
#line 228 "machine/sparc/decoder.m"
 {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JSL, true);
		return br;
	}

#line 577 "sparcdecoder.cpp"
 /*opt-block+*/
                else 
                  goto MATCH_label_e0;  /*opt-block+*/
                
                break;
              case 5: 
                if ((MATCH_w_32_0 >> 30 & 0x3) /* op at 0 */ == 0) 
                  
#line 275 "machine/sparc/decoder.m"
 {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JSG, true);
		return br;
	}


#line 594 "sparcdecoder.cpp"
 /*opt-block+*/
                else 
                  goto MATCH_label_e0;  /*opt-block+*/
                
                break;
              case 6: 
                if ((MATCH_w_32_0 >> 30 & 0x3) /* op at 0 */ == 0) 
                  
#line 243 "machine/sparc/decoder.m"
 {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JSG, true);
		return br;
	}

	// Just ignore unordered (for now)

#line 612 "sparcdecoder.cpp"
 /*opt-block+*/
                else 
                  goto MATCH_label_e0;  /*opt-block+*/
                
                break;
              case 9: 
                if ((MATCH_w_32_0 >> 30 & 0x3) /* op at 0 */ == 0) 
                  
#line 223 "machine/sparc/decoder.m"
 {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JE, true);
		return br;
	}

#line 628 "sparcdecoder.cpp"
 /*opt-block+*/
                else 
                  goto MATCH_label_e0;  /*opt-block+*/
                
                break;
              case 10: 
                if ((MATCH_w_32_0 >> 30 & 0x3) /* op at 0 */ == 0) 
                  
#line 255 "machine/sparc/decoder.m"
 {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JE, true);
		return br;
	}

#line 644 "sparcdecoder.cpp"
 /*opt-block+*/
                else 
                  goto MATCH_label_e0;  /*opt-block+*/
                
                break;
              case 11: 
                if ((MATCH_w_32_0 >> 30 & 0x3) /* op at 0 */ == 0) 
                  
#line 233 "machine/sparc/decoder.m"
 {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JSGE, true);
		return br;
	}

#line 660 "sparcdecoder.cpp"
 /*opt-block+*/
                else 
                  goto MATCH_label_e0;  /*opt-block+*/
                
                break;
              case 12: 
                if ((MATCH_w_32_0 >> 30 & 0x3) /* op at 0 */ == 0) 
                  
#line 265 "machine/sparc/decoder.m"
 {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JSGE, true);
		return br;
	}

#line 676 "sparcdecoder.cpp"
 /*opt-block+*/
                else 
                  goto MATCH_label_e0;  /*opt-block+*/
                
                break;
              case 13: 
                if ((MATCH_w_32_0 >> 30 & 0x3) /* op at 0 */ == 0) 
                  
#line 238 "machine/sparc/decoder.m"
 {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JSLE, true);
		return br;
	}

#line 692 "sparcdecoder.cpp"
 /*opt-block+*/
                else 
                  goto MATCH_label_e0;  /*opt-block+*/
                
                break;
              case 14: 
                if ((MATCH_w_32_0 >> 30 & 0x3) /* op at 0 */ == 0) 
                  
#line 270 "machine/sparc/decoder.m"
 {
		auto br = new BranchStatement(dest);
		br->setCondType(BRANCH_JSLE, true);
		return br;
	}

#line 708 "sparcdecoder.cpp"
 /*opt-block+*/
                else 
                  goto MATCH_label_e0;  /*opt-block+*/
                
                break;
              default: assert(0);
            } /* (MATCH_w_32_0 >> 25 & 0xf) -- cond at 0 --*/ 
          break;
        default: assert(0);
      } /* (MATCH_w_32_0 >> 22 & 0x7) -- op2 at 0 --*/ 
    
  }goto MATCH_finished_e; 
  
  MATCH_label_e0: (void)0; /*placeholder for label*/ 
    
#line 281 "machine/sparc/decoder.m"
{
		// FBA, FBN, FBU, FBO are unhandled.
		// CBxxx branches (branches that depend on co-processor
		// instructions) are invalid, as far as we are concerned.
	}

#line 731 "sparcdecoder.cpp"
 
    goto MATCH_finished_e; 
    
  MATCH_finished_e: (void)0; /*placeholder for label*/
  
}
#line 738 "sparcdecoder.cpp"

#line 287 "machine/sparc/decoder.m"
	return nullptr;
}

void
SparcDecoder::decodeInstruction(DecodeResult &result, ADDRESS pc, const BinaryFile *bf)
{
	// Clear the result structure;
	result.reset();

	ADDRESS nextPC = NO_ADDRESS;

#line 752 "sparcdecoder.cpp"

#line 297 "machine/sparc/decoder.m"
{ 
  ADDRESS MATCH_p = 
    
#line 297 "machine/sparc/decoder.m"
pc
#line 760 "sparcdecoder.cpp"
;
  const char *MATCH_name;
  static const char *MATCH_name_cond_0[] = {
    "BPN", "BPE", "BPLE", "BPL", "BPLEU", "BPCS", "BPNEG", "BPVS", "BPA", 
    "BPNE", "BPG", "BPGE", "BPGU", "BPCC", "BPPOS", "BPVC", 
  };
  static const char *MATCH_name_cond_1[] = {
    "BPN,a", "BPE,a", "BPLE,a", "BPL,a", "BPLEU,a", "BPCS,a", "BPNEG,a", 
    "BPVS,a", "BPA,a", "BPNE,a", "BPG,a", "BPGE,a", "BPGU,a", "BPCC,a", 
    "BPPOS,a", "BPVC,a", 
  };
  static const char *MATCH_name_cond_2[] = {
    "BN", "BE", "BLE", "BL", "BLEU", "BCS", "BNEG", "BVS", "BA", "BNE", "BG", 
    "BGE", "BGU", "BCC", "BPOS", "BVC", 
  };
  static const char *MATCH_name_cond_3[] = {
    "BN,a", "BE,a", "BLE,a", "BL,a", "BLEU,a", "BCS,a", "BNEG,a", "BVS,a", 
    "BA,a", "BNE,a", "BG,a", "BGE,a", "BGU,a", "BCC,a", "BPOS,a", "BVC,a", 
  };
  static const char *MATCH_name_rd_4[] = {
    "NOP", "sethi", "sethi", "sethi", "sethi", "sethi", "sethi", "sethi", 
    "sethi", "sethi", "sethi", "sethi", "sethi", "sethi", "sethi", "sethi", 
    "sethi", "sethi", "sethi", "sethi", "sethi", "sethi", "sethi", "sethi", 
    "sethi", "sethi", "sethi", "sethi", "sethi", "sethi", "sethi", "sethi", 
  };
  static const char *MATCH_name_cond_5[] = {
    "FBN", "FBNE", "FBLG", "FBUL", "FBL", "FBUG", "FBG", "FBU", "FBA", "FBE", 
    "FBUE", "FBGE", "FBUGE", "FBLE", "FBULE", "FBO", 
  };
  static const char *MATCH_name_cond_6[] = {
    "FBN,a", "FBNE,a", "FBLG,a", "FBUL,a", "FBL,a", "FBUG,a", "FBG,a", 
    "FBU,a", "FBA,a", "FBE,a", "FBUE,a", "FBGE,a", "FBUGE,a", "FBLE,a", 
    "FBULE,a", "FBO,a", 
  };
  static const char *MATCH_name_cond_7[] = {
    "CBN", "CB123", "CB12", "CB13", "CB1", "CB23", "CB2", "CB3", "CBA", 
    "CB0", "CB03", "CB02", "CB023", "CB01", "CB013", "CB012", 
  };
  static const char *MATCH_name_cond_8[] = {
    "CBN,a", "CB123,a", "CB12,a", "CB13,a", "CB1,a", "CB23,a", "CB2,a", 
    "CB3,a", "CBA,a", "CB0,a", "CB03,a", "CB02,a", "CB023,a", "CB01,a", 
    "CB013,a", "CB012,a", 
  };
  static const char *MATCH_name_rs1_46[] = {
    "RDY", "JMPL", "JMPL", "JMPL", "JMPL", "JMPL", "JMPL", "JMPL", "JMPL", 
    "JMPL", "JMPL", "JMPL", "JMPL", "JMPL", "JMPL", NULL, "JMPL", "JMPL", 
    "JMPL", "JMPL", "JMPL", "JMPL", "JMPL", "JMPL", "JMPL", "JMPL", "JMPL", 
    "JMPL", "JMPL", "JMPL", "JMPL", 
  };
  static const char *MATCH_name_op3_47[] = {
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 
    NULL, NULL, NULL, NULL, NULL, "RDPSR", "RDWIM", "RDTBR", 
  };
  static const char *MATCH_name_opf_52[] = {
    NULL, "FMOVs", NULL, NULL, NULL, "FNEGs", NULL, NULL, NULL, "FABSs", 
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, "FSQRTs", "FSQRTd", "FSQRTq", 
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, "FADDs", "FADDd", 
    "FADDq", NULL, "FSUBs", "FSUBd", "FSUBq", NULL, "FMULs", "FMULd", 
    "FMULq", NULL, "FDIVs", "FDIVd", "FDIVq", NULL, "FCMPs", "FCMPd", 
    "FCMPq", NULL, "FCMPEs", "FCMPEd", "FCMPEq", NULL, NULL, NULL, NULL, 
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, "FiTOs", NULL, "FdTOs", 
    "FqTOs", "FiTOd", "FsTOd", NULL, "FqTOd", "FiTOq", "FsTOq", "FdTOq", 
    NULL, NULL, "FsTOi", "FdTOi", "FqTOi", 
  };
  static const char *MATCH_name_cond_57[] = {
    "TN", "TE", "TLE", "TL", "TLEU", "TCS", "TNEG", "TVS", "TA", "TNE", "TG", 
    "TGE", "TGU", "TCC", "TPOS", "TVC", 
  };
  static const char *MATCH_name_i_72[] = {"LDA", "LDF", };
  static const char *MATCH_name_i_73[] = {"LDUBA", "LDFSR", };
  static const char *MATCH_name_i_74[] = {"LDUHA", "LDDF", };
  static const char *MATCH_name_i_75[] = {"LDDA", "STF", };
  static const char *MATCH_name_i_76[] = {"STA", "STFSR", };
  static const char *MATCH_name_i_77[] = {"STBA", "STDFQ", };
  static const char *MATCH_name_i_78[] = {"STHA", "STDF", };
  static const char *MATCH_name_i_79[] = {"STDA", "LDCSR", };
  static const char *MATCH_name_i_80[] = {"LDSBA", "STCSR", };
  static const char *MATCH_name_i_81[] = {"LDSHA", "STDCQ", };
  unsigned MATCH_w_32_0;
  { 
    MATCH_w_32_0 = fetch32(MATCH_p); 
    
      switch((MATCH_w_32_0 >> 30 & 0x3) /* op at 0 */) {
        case 0: 
          
            switch((MATCH_w_32_0 >> 22 & 0x7) /* op2 at 0 */) {
              case 0: 
                nextPC = 4 + MATCH_p; 
                
#line 627 "machine/sparc/decoder.m"

	//| UNIMP(n) =>
		result.valid = false;


#line 869 "sparcdecoder.cpp"

                
                
                break;
              case 1: 
                if ((MATCH_w_32_0 >> 29 & 0x1) /* a at 0 */ == 1 && 
                  (0 <= (MATCH_w_32_0 >> 25 & 0xf) /* cond at 0 */ && 
                  (MATCH_w_32_0 >> 25 & 0xf) /* cond at 0 */ < 16)) { 
                  MATCH_name = MATCH_name_cond_1[(MATCH_w_32_0 >> 25 & 0xf) 
                        /* cond at 0 */]; 
                  { 
                    const char *name = MATCH_name;
                    unsigned cc01 = 
                      (MATCH_w_32_0 >> 20 & 0x3) /* cc01 at 0 */;
                    unsigned tgt = 
                      4 * sign_extend(
                                  (MATCH_w_32_0 & 0x7ffff) /* disp19 at 0 */, 
                                  19) + addressToPC(MATCH_p);
                    nextPC = 4 + MATCH_p; 
                    
#line 374 "machine/sparc/decoder.m"

		/*
		 * Anulled, predicted branch (treat as for non predicted)
		 */
		/* If 64 bit cc used, can't handle */
		if (strcmp(name, "BPA,a") != 0 && strcmp(name, "BPN,a") != 0 && cc01 != 0) {
			result.valid = false;
			result.rtl = new RTL(pc);  // FIXME:  Is this needed when invalid?
			result.numBytes = nextPC - pc;
			return;
		}
		auto br = createBranch(pc, tgt, bf);
		result.rtl = new RTL(pc, br);

		// The class of this instruction depends on whether or not
		// it is one of the 'unconditional' conditional branches
		// "BPA,A" or "BPN,A"
		result.type = SCDAN;
		if (strcmp(name, "BPA,a") == 0 || strcmp(name, "BPVC,a") == 0) {
			result.type = SU;
		} else {
			result.type = SKIP;
		}

		SHOW_ASM(name << " " << std::hex << tgt << std::dec);
		DEBUG_STMTS


#line 919 "sparcdecoder.cpp"

                    
                  }
                  
                } /*opt-block*/
                else { 
                  MATCH_name = MATCH_name_cond_0[(MATCH_w_32_0 >> 25 & 0xf) 
                        /* cond at 0 */]; 
                  { 
                    const char *name = MATCH_name;
                    unsigned cc01 = 
                      (MATCH_w_32_0 >> 20 & 0x3) /* cc01 at 0 */;
                    unsigned tgt = 
                      4 * sign_extend(
                                  (MATCH_w_32_0 & 0x7ffff) /* disp19 at 0 */, 
                                  19) + addressToPC(MATCH_p);
                    nextPC = 4 + MATCH_p; 
                    
#line 426 "machine/sparc/decoder.m"

		/* If 64 bit cc used, can't handle */
		if (strcmp(name, "BPA") != 0 && strcmp(name, "BPN") != 0 && cc01 != 0) {
			result.valid = false;
			result.rtl = new RTL(pc);  // FIXME:  Is this needed when invalid?
			result.numBytes = nextPC - pc;
			return;
		}
		auto br = createBranch(pc, tgt, bf);
		result.rtl = new RTL(pc, br);

		// The class of this instruction depends on whether or not
		// it is one of the 'unconditional' conditional branches
		// "BPA" or "BPN" (or the pseudo unconditionals BPVx)
		result.type = SCD;
		if (strcmp(name, "BPA") == 0 || strcmp(name, "BPVC") == 0)
			result.type = SD;
		if (strcmp(name, "BPN") == 0 || strcmp(name, "BPVS") == 0)
			result.type = NCT;

		SHOW_ASM(name << " " << std::hex << tgt << std::dec);
		DEBUG_STMTS


#line 963 "sparcdecoder.cpp"

                    
                  }
                  
                } /*opt-block*/
                
                break;
              case 2: 
                if ((MATCH_w_32_0 >> 29 & 0x1) /* a at 0 */ == 1 && 
                  (0 <= (MATCH_w_32_0 >> 25 & 0xf) /* cond at 0 */ && 
                  (MATCH_w_32_0 >> 25 & 0xf) /* cond at 0 */ < 16)) { 
                  MATCH_name = MATCH_name_cond_3[(MATCH_w_32_0 >> 25 & 0xf) 
                        /* cond at 0 */]; 
                  goto MATCH_label_d1; 
                  
                } /*opt-block*/
                else { 
                  MATCH_name = MATCH_name_cond_2[(MATCH_w_32_0 >> 25 & 0xf) 
                        /* cond at 0 */]; 
                  goto MATCH_label_d0; 
                  
                } /*opt-block*/
                
                break;
              case 3: case 5: 
                goto MATCH_label_d2; break;
              case 4: 
                if ((MATCH_w_32_0 & 0x3fffff) /* imm22 at 0 */ == 0) 
                  if ((MATCH_w_32_0 >> 25 & 0x1f) /* rd at 0 */ == 0) { 
                    MATCH_name = MATCH_name_rd_4[(MATCH_w_32_0 >> 25 & 0x1f) 
                          /* rd at 0 */]; 
                    { 
                      const char *name = MATCH_name;
                      nextPC = 4 + MATCH_p; 
                      
#line 480 "machine/sparc/decoder.m"

		result.type = NOP;
		result.rtl = instantiate(pc, name);


#line 1005 "sparcdecoder.cpp"

                      
                    }
                    
                  } /*opt-block*/
                  else { 
                    MATCH_name = MATCH_name_rd_4[(MATCH_w_32_0 >> 25 & 0x1f) 
                          /* rd at 0 */]; 
                    goto MATCH_label_d3; 
                    
                  } /*opt-block*/ /*opt-block+*/
                else { 
                  MATCH_name = "sethi"; 
                  goto MATCH_label_d3; 
                  
                } /*opt-block*/
                break;
              case 6: 
                if ((MATCH_w_32_0 >> 29 & 0x1) /* a at 0 */ == 1 && 
                  (0 <= (MATCH_w_32_0 >> 25 & 0xf) /* cond at 0 */ && 
                  (MATCH_w_32_0 >> 25 & 0xf) /* cond at 0 */ < 16)) { 
                  MATCH_name = MATCH_name_cond_6[(MATCH_w_32_0 >> 25 & 0xf) 
                        /* cond at 0 */]; 
                  goto MATCH_label_d1; 
                  
                } /*opt-block*/
                else { 
                  MATCH_name = MATCH_name_cond_5[(MATCH_w_32_0 >> 25 & 0xf) 
                        /* cond at 0 */]; 
                  goto MATCH_label_d0; 
                  
                } /*opt-block*/
                
                break;
              case 7: 
                if ((MATCH_w_32_0 >> 29 & 0x1) /* a at 0 */ == 1 && 
                  (0 <= (MATCH_w_32_0 >> 25 & 0xf) /* cond at 0 */ && 
                  (MATCH_w_32_0 >> 25 & 0xf) /* cond at 0 */ < 16)) { 
                  MATCH_name = MATCH_name_cond_8[(MATCH_w_32_0 >> 25 & 0xf) 
                        /* cond at 0 */]; 
                  goto MATCH_label_d1; 
                  
                } /*opt-block*/
                else { 
                  MATCH_name = MATCH_name_cond_7[(MATCH_w_32_0 >> 25 & 0xf) 
                        /* cond at 0 */]; 
                  goto MATCH_label_d0; 
                  
                } /*opt-block*/
                
                break;
              default: assert(0);
            } /* (MATCH_w_32_0 >> 22 & 0x7) -- op2 at 0 --*/ 
          break;
        case 1: 
          MATCH_name = "call__"; 
          { 
            const char *name = MATCH_name;
            unsigned addr = 
              4 * sign_extend((MATCH_w_32_0 & 0x3fffffff) /* disp30 at 0 */, 
                          30) + addressToPC(MATCH_p);
            nextPC = 4 + MATCH_p; 
            
#line 299 "machine/sparc/decoder.m"

		/*
		 * A standard call
		 */
		auto newCall = new CallStatement(addr);

		Proc *destProc = prog->setNewProc(addr);
		if (destProc == (Proc *)-1) destProc = nullptr;
		newCall->setDestProc(destProc);
		result.rtl = new RTL(pc, newCall);
		result.type = SD;
		SHOW_ASM(name << " " << std::hex << addr << std::dec);
		DEBUG_STMTS


#line 1085 "sparcdecoder.cpp"

            
          }
          
          break;
        case 2: 
          
            switch((MATCH_w_32_0 >> 19 & 0x3f) /* op3 at 0 */) {
              case 0: 
                MATCH_name = "ADD"; goto MATCH_label_d4; break;
              case 1: 
                MATCH_name = "AND"; goto MATCH_label_d4; break;
              case 2: 
                MATCH_name = "OR"; goto MATCH_label_d4; break;
              case 3: 
                MATCH_name = "XOR"; goto MATCH_label_d4; break;
              case 4: 
                MATCH_name = "SUB"; goto MATCH_label_d4; break;
              case 5: 
                MATCH_name = "ANDN"; goto MATCH_label_d4; break;
              case 6: 
                MATCH_name = "ORN"; goto MATCH_label_d4; break;
              case 7: 
                MATCH_name = "XNOR"; goto MATCH_label_d4; break;
              case 8: 
                MATCH_name = "ADDX"; goto MATCH_label_d4; break;
              case 9: case 13: case 25: case 29: case 44: case 45: case 46: 
              case 47: case 54: case 55: case 59: case 62: case 63: 
                goto MATCH_label_d2; break;
              case 10: 
                MATCH_name = "UMUL"; goto MATCH_label_d4; break;
              case 11: 
                MATCH_name = "SMUL"; goto MATCH_label_d4; break;
              case 12: 
                MATCH_name = "SUBX"; goto MATCH_label_d4; break;
              case 14: 
                MATCH_name = "UDIV"; goto MATCH_label_d4; break;
              case 15: 
                MATCH_name = "SDIV"; goto MATCH_label_d4; break;
              case 16: 
                MATCH_name = "ADDcc"; goto MATCH_label_d4; break;
              case 17: 
                MATCH_name = "ANDcc"; goto MATCH_label_d4; break;
              case 18: 
                MATCH_name = "ORcc"; goto MATCH_label_d4; break;
              case 19: 
                MATCH_name = "XORcc"; goto MATCH_label_d4; break;
              case 20: 
                MATCH_name = "SUBcc"; goto MATCH_label_d4; break;
              case 21: 
                MATCH_name = "ANDNcc"; goto MATCH_label_d4; break;
              case 22: 
                MATCH_name = "ORNcc"; goto MATCH_label_d4; break;
              case 23: 
                MATCH_name = "XNORcc"; goto MATCH_label_d4; break;
              case 24: 
                MATCH_name = "ADDXcc"; goto MATCH_label_d4; break;
              case 26: 
                MATCH_name = "UMULcc"; goto MATCH_label_d4; break;
              case 27: 
                MATCH_name = "SMULcc"; goto MATCH_label_d4; break;
              case 28: 
                MATCH_name = "SUBXcc"; goto MATCH_label_d4; break;
              case 30: 
                MATCH_name = "UDIVcc"; goto MATCH_label_d4; break;
              case 31: 
                MATCH_name = "SDIVcc"; goto MATCH_label_d4; break;
              case 32: 
                MATCH_name = "TADDcc"; goto MATCH_label_d4; break;
              case 33: 
                MATCH_name = "TSUBcc"; goto MATCH_label_d4; break;
              case 34: 
                MATCH_name = "TADDccTV"; goto MATCH_label_d4; break;
              case 35: 
                MATCH_name = "TSUBccTV"; goto MATCH_label_d4; break;
              case 36: 
                MATCH_name = "MULScc"; goto MATCH_label_d4; break;
              case 37: 
                MATCH_name = "SLL"; goto MATCH_label_d4; break;
              case 38: 
                MATCH_name = "SRL"; goto MATCH_label_d4; break;
              case 39: 
                MATCH_name = "SRA"; goto MATCH_label_d4; break;
              case 40: 
                if ((MATCH_w_32_0 >> 14 & 0x1f) /* rs1 at 0 */ == 0) { 
                  MATCH_name = MATCH_name_rs1_46[(MATCH_w_32_0 >> 14 & 0x1f) 
                        /* rs1 at 0 */]; 
                  { 
                    const char *name = MATCH_name;
                    unsigned rd = (MATCH_w_32_0 >> 25 & 0x1f) /* rd at 0 */;
                    nextPC = 4 + MATCH_p; 
                    
#line 532 "machine/sparc/decoder.m"

		result.rtl = instantiate(pc, name, DIS_RD);


#line 1183 "sparcdecoder.cpp"

                    
                  }
                  
                } /*opt-block*/
                else 
                  goto MATCH_label_d2;  /*opt-block+*/
                
                break;
              case 41: 
                MATCH_name = MATCH_name_op3_47[(MATCH_w_32_0 >> 19 & 0x3f) 
                      /* op3 at 0 */]; 
                { 
                  const char *name = MATCH_name;
                  unsigned rd = (MATCH_w_32_0 >> 25 & 0x1f) /* rd at 0 */;
                  nextPC = 4 + MATCH_p; 
                  
#line 535 "machine/sparc/decoder.m"

		result.rtl = instantiate(pc, name, DIS_RD);


#line 1206 "sparcdecoder.cpp"

                  
                }
                
                break;
              case 42: 
                MATCH_name = MATCH_name_op3_47[(MATCH_w_32_0 >> 19 & 0x3f) 
                      /* op3 at 0 */]; 
                { 
                  const char *name = MATCH_name;
                  unsigned rd = (MATCH_w_32_0 >> 25 & 0x1f) /* rd at 0 */;
                  nextPC = 4 + MATCH_p; 
                  
#line 538 "machine/sparc/decoder.m"

		result.rtl = instantiate(pc, name, DIS_RD);


#line 1225 "sparcdecoder.cpp"

                  
                }
                
                break;
              case 43: 
                MATCH_name = MATCH_name_op3_47[(MATCH_w_32_0 >> 19 & 0x3f) 
                      /* op3 at 0 */]; 
                { 
                  const char *name = MATCH_name;
                  unsigned rd = (MATCH_w_32_0 >> 25 & 0x1f) /* rd at 0 */;
                  nextPC = 4 + MATCH_p; 
                  
#line 541 "machine/sparc/decoder.m"

		result.rtl = instantiate(pc, name, DIS_RD);


#line 1244 "sparcdecoder.cpp"

                  
                }
                
                break;
              case 48: 
                if (1 <= (MATCH_w_32_0 >> 25 & 0x1f) /* rd at 0 */ && 
                  (MATCH_w_32_0 >> 25 & 0x1f) /* rd at 0 */ < 32) 
                  goto MATCH_label_d2;  /*opt-block+*/
                else { 
                  MATCH_name = "WRY"; 
                  { 
                    const char *name = MATCH_name;
                    unsigned roi = addressToPC(MATCH_p);
                    unsigned rs1 = (MATCH_w_32_0 >> 14 & 0x1f) /* rs1 at 0 */;
                    nextPC = 4 + MATCH_p; 
                    
#line 544 "machine/sparc/decoder.m"

		result.rtl = instantiate(pc, name, DIS_RS1, DIS_ROI);


#line 1267 "sparcdecoder.cpp"

                    
                  }
                  
                } /*opt-block*/
                
                break;
              case 49: 
                MATCH_name = "WRPSR"; 
                { 
                  const char *name = MATCH_name;
                  unsigned roi = addressToPC(MATCH_p);
                  unsigned rs1 = (MATCH_w_32_0 >> 14 & 0x1f) /* rs1 at 0 */;
                  nextPC = 4 + MATCH_p; 
                  
#line 547 "machine/sparc/decoder.m"

		result.rtl = instantiate(pc, name, DIS_RS1, DIS_ROI);


#line 1288 "sparcdecoder.cpp"

                  
                }
                
                break;
              case 50: 
                MATCH_name = "WRWIM"; 
                { 
                  const char *name = MATCH_name;
                  unsigned roi = addressToPC(MATCH_p);
                  unsigned rs1 = (MATCH_w_32_0 >> 14 & 0x1f) /* rs1 at 0 */;
                  nextPC = 4 + MATCH_p; 
                  
#line 550 "machine/sparc/decoder.m"

		result.rtl = instantiate(pc, name, DIS_RS1, DIS_ROI);


#line 1307 "sparcdecoder.cpp"

                  
                }
                
                break;
              case 51: 
                MATCH_name = "WRTBR"; 
                { 
                  const char *name = MATCH_name;
                  unsigned roi = addressToPC(MATCH_p);
                  unsigned rs1 = (MATCH_w_32_0 >> 14 & 0x1f) /* rs1 at 0 */;
                  nextPC = 4 + MATCH_p; 
                  
#line 553 "machine/sparc/decoder.m"

		result.rtl = instantiate(pc, name, DIS_RS1, DIS_ROI);


#line 1326 "sparcdecoder.cpp"

                  
                }
                
                break;
              case 52: 
                if (80 <= (MATCH_w_32_0 >> 5 & 0x1ff) /* opf at 0 */ && 
                  (MATCH_w_32_0 >> 5 & 0x1ff) /* opf at 0 */ < 196 || 
                  212 <= (MATCH_w_32_0 >> 5 & 0x1ff) /* opf at 0 */ && 
                  (MATCH_w_32_0 >> 5 & 0x1ff) /* opf at 0 */ < 512) 
                  goto MATCH_label_d2;  /*opt-block+*/
                else 
                  switch((MATCH_w_32_0 >> 5 & 0x1ff) /* opf at 0 */) {
                    case 0: case 2: case 3: case 4: case 6: case 7: case 8: 
                    case 10: case 11: case 12: case 13: case 14: case 15: 
                    case 16: case 17: case 18: case 19: case 20: case 21: 
                    case 22: case 23: case 24: case 25: case 26: case 27: 
                    case 28: case 29: case 30: case 31: case 32: case 33: 
                    case 34: case 35: case 36: case 37: case 38: case 39: 
                    case 40: case 44: case 45: case 46: case 47: case 48: 
                    case 49: case 50: case 51: case 52: case 53: case 54: 
                    case 55: case 56: case 57: case 58: case 59: case 60: 
                    case 61: case 62: case 63: case 64: case 68: case 72: 
                    case 76: case 197: case 202: case 207: case 208: 
                      goto MATCH_label_d2; break;
                    case 1: case 5: case 9: case 41: 
                      MATCH_name = 
                        MATCH_name_opf_52[(MATCH_w_32_0 >> 5 & 0x1ff) 
                            /* opf at 0 */]; 
                      { 
                        const char *name = MATCH_name;
                        unsigned fds = 
                          (MATCH_w_32_0 >> 25 & 0x1f) /* fds at 0 */;
                        unsigned fs2s = (MATCH_w_32_0 & 0x1f) /* fs2s at 0 */;
                        nextPC = 4 + MATCH_p; 
                        
#line 559 "machine/sparc/decoder.m"

		result.rtl = instantiate(pc, name, DIS_FS2S, DIS_FDS);


#line 1368 "sparcdecoder.cpp"

                        
                      }
                      
                      break;
                    case 42: 
                      MATCH_name = 
                        MATCH_name_opf_52[(MATCH_w_32_0 >> 5 & 0x1ff) 
                            /* opf at 0 */]; 
                      { 
                        const char *name = MATCH_name;
                        unsigned fdd = 
                          (MATCH_w_32_0 >> 25 & 0x1f) /* fdd at 0 */;
                        unsigned fs2d = (MATCH_w_32_0 & 0x1f) /* fs2d at 0 */;
                        nextPC = 4 + MATCH_p; 
                        
#line 610 "machine/sparc/decoder.m"

		result.rtl = instantiate(pc, name, DIS_FS2D, DIS_FDD);


#line 1390 "sparcdecoder.cpp"

                        
                      }
                      
                      break;
                    case 43: 
                      MATCH_name = 
                        MATCH_name_opf_52[(MATCH_w_32_0 >> 5 & 0x1ff) 
                            /* opf at 0 */]; 
                      { 
                        const char *name = MATCH_name;
                        unsigned fdq = 
                          (MATCH_w_32_0 >> 25 & 0x1f) /* fdq at 0 */;
                        unsigned fs2q = (MATCH_w_32_0 & 0x1f) /* fs2q at 0 */;
                        nextPC = 4 + MATCH_p; 
                        
#line 613 "machine/sparc/decoder.m"

		result.rtl = instantiate(pc, name, DIS_FS2Q, DIS_FDQ);


	// In V9, the privileged RETT becomes user-mode RETURN
	// It has the semantics of "ret restore" without the add part of the restore

#line 1415 "sparcdecoder.cpp"

                        
                      }
                      
                      break;
                    case 65: case 69: case 73: case 77: 
                      MATCH_name = 
                        MATCH_name_opf_52[(MATCH_w_32_0 >> 5 & 0x1ff) 
                            /* opf at 0 */]; 
                      { 
                        const char *name = MATCH_name;
                        unsigned fds = 
                          (MATCH_w_32_0 >> 25 & 0x1f) /* fds at 0 */;
                        unsigned fs1s = 
                          (MATCH_w_32_0 >> 14 & 0x1f) /* fs1s at 0 */;
                        unsigned fs2s = (MATCH_w_32_0 & 0x1f) /* fs2s at 0 */;
                        nextPC = 4 + MATCH_p; 
                        
#line 562 "machine/sparc/decoder.m"

		result.rtl = instantiate(pc, name, DIS_FS1S, DIS_FS2S, DIS_FDS);


#line 1439 "sparcdecoder.cpp"

                        
                      }
                      
                      break;
                    case 66: case 70: case 74: case 78: 
                      MATCH_name = 
                        MATCH_name_opf_52[(MATCH_w_32_0 >> 5 & 0x1ff) 
                            /* opf at 0 */]; 
                      { 
                        const char *name = MATCH_name;
                        unsigned fdd = 
                          (MATCH_w_32_0 >> 25 & 0x1f) /* fdd at 0 */;
                        unsigned fs1d = 
                          (MATCH_w_32_0 >> 14 & 0x1f) /* fs1d at 0 */;
                        unsigned fs2d = (MATCH_w_32_0 & 0x1f) /* fs2d at 0 */;
                        nextPC = 4 + MATCH_p; 
                        
#line 565 "machine/sparc/decoder.m"

		result.rtl = instantiate(pc, name, DIS_FS1D, DIS_FS2D, DIS_FDD);


#line 1463 "sparcdecoder.cpp"

                        
                      }
                      
                      break;
                    case 67: case 71: case 75: case 79: 
                      MATCH_name = 
                        MATCH_name_opf_52[(MATCH_w_32_0 >> 5 & 0x1ff) 
                            /* opf at 0 */]; 
                      { 
                        const char *name = MATCH_name;
                        unsigned fdq = 
                          (MATCH_w_32_0 >> 25 & 0x1f) /* fdq at 0 */;
                        unsigned fs1q = 
                          (MATCH_w_32_0 >> 14 & 0x1f) /* fs1q at 0 */;
                        unsigned fs2q = (MATCH_w_32_0 & 0x1f) /* fs2q at 0 */;
                        nextPC = 4 + MATCH_p; 
                        
#line 568 "machine/sparc/decoder.m"

		result.rtl = instantiate(pc, name, DIS_FS1Q, DIS_FS2Q, DIS_FDQ);


#line 1487 "sparcdecoder.cpp"

                        
                      }
                      
                      break;
                    case 196: case 209: 
                      MATCH_name = 
                        MATCH_name_opf_52[(MATCH_w_32_0 >> 5 & 0x1ff) 
                            /* opf at 0 */]; 
                      { 
                        const char *name = MATCH_name;
                        unsigned fds = 
                          (MATCH_w_32_0 >> 25 & 0x1f) /* fds at 0 */;
                        unsigned fs2s = (MATCH_w_32_0 & 0x1f) /* fs2s at 0 */;
                        nextPC = 4 + MATCH_p; 
                        
#line 580 "machine/sparc/decoder.m"

		result.rtl = instantiate(pc, name, DIS_FS2S, DIS_FDS);

	// Note: itod and dtoi have different sized registers

#line 1510 "sparcdecoder.cpp"

                        
                      }
                      
                      break;
                    case 198: 
                      MATCH_name = 
                        MATCH_name_opf_52[(MATCH_w_32_0 >> 5 & 0x1ff) 
                            /* opf at 0 */]; 
                      { 
                        const char *name = MATCH_name;
                        unsigned fds = 
                          (MATCH_w_32_0 >> 25 & 0x1f) /* fds at 0 */;
                        unsigned fs2d = (MATCH_w_32_0 & 0x1f) /* fs2d at 0 */;
                        nextPC = 4 + MATCH_p; 
                        
#line 596 "machine/sparc/decoder.m"

		result.rtl = instantiate(pc, name, DIS_FS2D, DIS_FDS);


#line 1532 "sparcdecoder.cpp"

                        
                      }
                      
                      break;
                    case 199: 
                      MATCH_name = 
                        MATCH_name_opf_52[(MATCH_w_32_0 >> 5 & 0x1ff) 
                            /* opf at 0 */]; 
                      { 
                        const char *name = MATCH_name;
                        unsigned fds = 
                          (MATCH_w_32_0 >> 25 & 0x1f) /* fds at 0 */;
                        unsigned fs2q = (MATCH_w_32_0 & 0x1f) /* fs2q at 0 */;
                        nextPC = 4 + MATCH_p; 
                        
#line 601 "machine/sparc/decoder.m"

		result.rtl = instantiate(pc, name, DIS_FS2Q, DIS_FDS);


#line 1554 "sparcdecoder.cpp"

                        
                      }
                      
                      break;
                    case 200: 
                      MATCH_name = 
                        MATCH_name_opf_52[(MATCH_w_32_0 >> 5 & 0x1ff) 
                            /* opf at 0 */]; 
                      { 
                        const char *name = MATCH_name;
                        unsigned fdd = 
                          (MATCH_w_32_0 >> 25 & 0x1f) /* fdd at 0 */;
                        unsigned fs2s = (MATCH_w_32_0 & 0x1f) /* fs2s at 0 */;
                        nextPC = 4 + MATCH_p; 
                        
#line 584 "machine/sparc/decoder.m"

		result.rtl = instantiate(pc, name, DIS_FS2S, DIS_FDD);

#line 1575 "sparcdecoder.cpp"

                        
                      }
                      
                      break;
                    case 201: 
                      MATCH_name = 
                        MATCH_name_opf_52[(MATCH_w_32_0 >> 5 & 0x1ff) 
                            /* opf at 0 */]; 
                      { 
                        const char *name = MATCH_name;
                        unsigned fdd = 
                          (MATCH_w_32_0 >> 25 & 0x1f) /* fdd at 0 */;
                        unsigned fs2s = (MATCH_w_32_0 & 0x1f) /* fs2s at 0 */;
                        nextPC = 4 + MATCH_p; 
                        
#line 594 "machine/sparc/decoder.m"

		result.rtl = instantiate(pc, name, DIS_FS2S, DIS_FDD);

#line 1596 "sparcdecoder.cpp"

                        
                      }
                      
                      break;
                    case 203: 
                      MATCH_name = 
                        MATCH_name_opf_52[(MATCH_w_32_0 >> 5 & 0x1ff) 
                            /* opf at 0 */]; 
                      { 
                        const char *name = MATCH_name;
                        unsigned fdd = 
                          (MATCH_w_32_0 >> 25 & 0x1f) /* fdd at 0 */;
                        unsigned fs2q = (MATCH_w_32_0 & 0x1f) /* fs2q at 0 */;
                        nextPC = 4 + MATCH_p; 
                        
#line 606 "machine/sparc/decoder.m"

		result.rtl = instantiate(pc, name, DIS_FS2Q, DIS_FDD);



#line 1619 "sparcdecoder.cpp"

                        
                      }
                      
                      break;
                    case 204: 
                      MATCH_name = 
                        MATCH_name_opf_52[(MATCH_w_32_0 >> 5 & 0x1ff) 
                            /* opf at 0 */]; 
                      { 
                        const char *name = MATCH_name;
                        unsigned fdq = 
                          (MATCH_w_32_0 >> 25 & 0x1f) /* fdq at 0 */;
                        unsigned fs2s = (MATCH_w_32_0 & 0x1f) /* fs2s at 0 */;
                        nextPC = 4 + MATCH_p; 
                        
#line 589 "machine/sparc/decoder.m"

		result.rtl = instantiate(pc, name, DIS_FS2S, DIS_FDQ);

#line 1640 "sparcdecoder.cpp"

                        
                      }
                      
                      break;
                    case 205: 
                      MATCH_name = 
                        MATCH_name_opf_52[(MATCH_w_32_0 >> 5 & 0x1ff) 
                            /* opf at 0 */]; 
                      { 
                        const char *name = MATCH_name;
                        unsigned fdq = 
                          (MATCH_w_32_0 >> 25 & 0x1f) /* fdq at 0 */;
                        unsigned fs2s = (MATCH_w_32_0 & 0x1f) /* fs2s at 0 */;
                        nextPC = 4 + MATCH_p; 
                        
#line 599 "machine/sparc/decoder.m"

		result.rtl = instantiate(pc, name, DIS_FS2S, DIS_FDQ);

#line 1661 "sparcdecoder.cpp"

                        
                      }
                      
                      break;
                    case 206: 
                      MATCH_name = 
                        MATCH_name_opf_52[(MATCH_w_32_0 >> 5 & 0x1ff) 
                            /* opf at 0 */]; 
                      { 
                        const char *name = MATCH_name;
                        unsigned fdq = 
                          (MATCH_w_32_0 >> 25 & 0x1f) /* fdq at 0 */;
                        unsigned fs2d = (MATCH_w_32_0 & 0x1f) /* fs2d at 0 */;
                        nextPC = 4 + MATCH_p; 
                        
#line 604 "machine/sparc/decoder.m"

		result.rtl = instantiate(pc, name, DIS_FS2D, DIS_FDQ);

#line 1682 "sparcdecoder.cpp"

                        
                      }
                      
                      break;
                    case 210: 
                      MATCH_name = 
                        MATCH_name_opf_52[(MATCH_w_32_0 >> 5 & 0x1ff) 
                            /* opf at 0 */]; 
                      { 
                        const char *name = MATCH_name;
                        unsigned fds = 
                          (MATCH_w_32_0 >> 25 & 0x1f) /* fds at 0 */;
                        unsigned fs2d = (MATCH_w_32_0 & 0x1f) /* fs2d at 0 */;
                        nextPC = 4 + MATCH_p; 
                        
#line 586 "machine/sparc/decoder.m"

		result.rtl = instantiate(pc, name, DIS_FS2D, DIS_FDS);


#line 1704 "sparcdecoder.cpp"

                        
                      }
                      
                      break;
                    case 211: 
                      MATCH_name = 
                        MATCH_name_opf_52[(MATCH_w_32_0 >> 5 & 0x1ff) 
                            /* opf at 0 */]; 
                      { 
                        const char *name = MATCH_name;
                        unsigned fds = 
                          (MATCH_w_32_0 >> 25 & 0x1f) /* fds at 0 */;
                        unsigned fs2q = (MATCH_w_32_0 & 0x1f) /* fs2q at 0 */;
                        nextPC = 4 + MATCH_p; 
                        
#line 591 "machine/sparc/decoder.m"

		result.rtl = instantiate(pc, name, DIS_FS2Q, DIS_FDS);


#line 1726 "sparcdecoder.cpp"

                        
                      }
                      
                      break;
                    default: assert(0);
                  } /* (MATCH_w_32_0 >> 5 & 0x1ff) -- opf at 0 --*/ 
                break;
              case 53: 
                if (0 <= (MATCH_w_32_0 >> 5 & 0x1ff) /* opf at 0 */ && 
                  (MATCH_w_32_0 >> 5 & 0x1ff) /* opf at 0 */ < 81 || 
                  88 <= (MATCH_w_32_0 >> 5 & 0x1ff) /* opf at 0 */ && 
                  (MATCH_w_32_0 >> 5 & 0x1ff) /* opf at 0 */ < 512) 
                  goto MATCH_label_d2;  /*opt-block+*/
                else 
                  switch((MATCH_w_32_0 >> 5 & 0x1ff) /* opf at 0 */) {
                    case 84: 
                      goto MATCH_label_d2; break;
                    case 81: case 85: 
                      MATCH_name = 
                        MATCH_name_opf_52[(MATCH_w_32_0 >> 5 & 0x1ff) 
                            /* opf at 0 */]; 
                      { 
                        const char *name = MATCH_name;
                        unsigned fs1s = 
                          (MATCH_w_32_0 >> 14 & 0x1f) /* fs1s at 0 */;
                        unsigned fs2s = (MATCH_w_32_0 & 0x1f) /* fs2s at 0 */;
                        nextPC = 4 + MATCH_p; 
                        
#line 571 "machine/sparc/decoder.m"

		result.rtl = instantiate(pc, name, DIS_FS1S, DIS_FS2S);


#line 1761 "sparcdecoder.cpp"

                        
                      }
                      
                      break;
                    case 82: case 86: 
                      MATCH_name = 
                        MATCH_name_opf_52[(MATCH_w_32_0 >> 5 & 0x1ff) 
                            /* opf at 0 */]; 
                      { 
                        const char *name = MATCH_name;
                        unsigned fs1d = 
                          (MATCH_w_32_0 >> 14 & 0x1f) /* fs1d at 0 */;
                        unsigned fs2d = (MATCH_w_32_0 & 0x1f) /* fs2d at 0 */;
                        nextPC = 4 + MATCH_p; 
                        
#line 574 "machine/sparc/decoder.m"

		result.rtl = instantiate(pc, name, DIS_FS1D, DIS_FS2D);


#line 1783 "sparcdecoder.cpp"

                        
                      }
                      
                      break;
                    case 83: case 87: 
                      MATCH_name = 
                        MATCH_name_opf_52[(MATCH_w_32_0 >> 5 & 0x1ff) 
                            /* opf at 0 */]; 
                      { 
                        const char *name = MATCH_name;
                        unsigned fs1q = 
                          (MATCH_w_32_0 >> 14 & 0x1f) /* fs1q at 0 */;
                        unsigned fs2q = (MATCH_w_32_0 & 0x1f) /* fs2q at 0 */;
                        nextPC = 4 + MATCH_p; 
                        
#line 577 "machine/sparc/decoder.m"

		result.rtl = instantiate(pc, name, DIS_FS1Q, DIS_FS2Q);


#line 1805 "sparcdecoder.cpp"

                        
                      }
                      
                      break;
                    default: assert(0);
                  } /* (MATCH_w_32_0 >> 5 & 0x1ff) -- opf at 0 --*/ 
                break;
              case 56: 
                if ((MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */ == 1) 
                  
                    switch((MATCH_w_32_0 >> 25 & 0x1f) /* rd at 0 */) {
                      case 0: 
                        
                          switch((MATCH_w_32_0 >> 14 & 0x1f) /* rs1 at 0 */) {
                            case 0: 
                              MATCH_name = "JMPL"; goto MATCH_label_d5; break;
                            case 1: case 2: case 3: case 4: case 5: case 6: 
                            case 7: case 8: case 9: case 10: case 11: 
                            case 12: case 13: case 14: case 16: case 17: 
                            case 18: case 19: case 20: case 21: case 22: 
                            case 23: case 24: case 25: case 26: case 27: 
                            case 28: case 29: case 30: 
                              MATCH_name = 
                                MATCH_name_rs1_46[(MATCH_w_32_0 >> 14 & 0x1f) 
                                    /* rs1 at 0 */]; 
                              goto MATCH_label_d5; 
                              
                              break;
                            case 15: 
                              if ((MATCH_w_32_0 & 0x1fff) 
                                      /* simm13 at 0 */ == 8) { 
                                nextPC = 4 + MATCH_p; 
                                
#line 339 "machine/sparc/decoder.m"

		/*
		 * Just a ret (leaf; uses %o7 instead of %i7)
		 */
		result.rtl = new RTL(pc, new ReturnStatement);
		result.type = DD;
		SHOW_ASM("retl");
		DEBUG_STMTS


#line 1851 "sparcdecoder.cpp"

                                
                              } /*opt-block*//*opt-block+*/
                              else { 
                                MATCH_name = "JMPL"; 
                                goto MATCH_label_d5; 
                                
                              } /*opt-block*/
                              
                              break;
                            case 31: 
                              if ((MATCH_w_32_0 & 0x1fff) 
                                      /* simm13 at 0 */ == 8) { 
                                nextPC = 4 + MATCH_p; 
                                
#line 330 "machine/sparc/decoder.m"

		/*
		 * Just a ret (non leaf)
		 */
		result.rtl = new RTL(pc, new ReturnStatement);
		result.type = DD;
		SHOW_ASM("ret");
		DEBUG_STMTS


#line 1878 "sparcdecoder.cpp"

                                
                              } /*opt-block*//*opt-block+*/
                              else { 
                                MATCH_name = "JMPL"; 
                                goto MATCH_label_d5; 
                                
                              } /*opt-block*/
                              
                              break;
                            default: assert(0);
                          } /* (MATCH_w_32_0 >> 14 & 0x1f) -- rs1 at 0 --*/ 
                        break;
                      case 1: case 2: case 3: case 4: case 5: case 6: case 7: 
                      case 8: case 9: case 10: case 11: case 12: case 13: 
                      case 14: case 16: case 17: case 18: case 19: case 20: 
                      case 21: case 22: case 23: case 24: case 25: case 26: 
                      case 27: case 28: case 29: case 30: case 31: 
                        MATCH_name = "JMPL"; goto MATCH_label_d5; break;
                      case 15: 
                        goto MATCH_label_d6; break;
                      default: assert(0);
                    } /* (MATCH_w_32_0 >> 25 & 0x1f) -- rd at 0 --*/  
                else 
                  if ((MATCH_w_32_0 >> 25 & 0x1f) /* rd at 0 */ == 15) 
                    goto MATCH_label_d6;  /*opt-block+*/
                  else { 
                    MATCH_name = "JMPL"; 
                    goto MATCH_label_d5; 
                    
                  } /*opt-block*/ /*opt-block+*/
                break;
              case 57: 
                MATCH_name = "RETURN"; 
                { 
                  const char *name = MATCH_name;
                  unsigned addr = addressToPC(MATCH_p);
                  nextPC = 4 + MATCH_p; 
                  
#line 619 "machine/sparc/decoder.m"

		result.rtl = instantiate(pc, name, DIS_ADDR);
		result.rtl->appendStmt(new ReturnStatement);
		result.type = DD;


#line 1925 "sparcdecoder.cpp"

                  
                }
                
                break;
              case 58: 
                MATCH_name = MATCH_name_cond_57[(MATCH_w_32_0 >> 25 & 0xf) 
                      /* cond at 0 */]; 
                { 
                  const char *name = MATCH_name;
                  unsigned addr = addressToPC(MATCH_p);
                  nextPC = 4 + MATCH_p; 
                  
#line 624 "machine/sparc/decoder.m"

		result.rtl = instantiate(pc, name, DIS_ADDR);


#line 1944 "sparcdecoder.cpp"

                  
                }
                
                break;
              case 60: 
                MATCH_name = "SAVE"; 
                { 
                  const char *name = MATCH_name;
                  unsigned rd = (MATCH_w_32_0 >> 25 & 0x1f) /* rd at 0 */;
                  unsigned roi = addressToPC(MATCH_p);
                  unsigned rs1 = (MATCH_w_32_0 >> 14 & 0x1f) /* rs1 at 0 */;
                  nextPC = 4 + MATCH_p; 
                  
#line 470 "machine/sparc/decoder.m"

		// Decided to treat SAVE as an ordinary instruction
		// That is, use the large list of effects from the SSL file, and
		// hope that optimisation will vastly help the common cases
		result.rtl = instantiate(pc, name, DIS_RS1, DIS_ROI, DIS_RD);


#line 1967 "sparcdecoder.cpp"

                  
                }
                
                break;
              case 61: 
                MATCH_name = "RESTORE"; 
                { 
                  const char *name = MATCH_name;
                  unsigned rd = (MATCH_w_32_0 >> 25 & 0x1f) /* rd at 0 */;
                  unsigned roi = addressToPC(MATCH_p);
                  unsigned rs1 = (MATCH_w_32_0 >> 14 & 0x1f) /* rs1 at 0 */;
                  nextPC = 4 + MATCH_p; 
                  
#line 476 "machine/sparc/decoder.m"

		// Decided to treat RESTORE as an ordinary instruction
		result.rtl = instantiate(pc, name, DIS_RS1, DIS_ROI, DIS_RD);


#line 1988 "sparcdecoder.cpp"

                  
                }
                
                break;
              default: assert(0);
            } /* (MATCH_w_32_0 >> 19 & 0x3f) -- op3 at 0 --*/ 
          break;
        case 3: 
          
            switch((MATCH_w_32_0 >> 19 & 0x3f) /* op3 at 0 */) {
              case 0: 
                MATCH_name = "LD"; goto MATCH_label_d7; break;
              case 1: 
                MATCH_name = "LDUB"; goto MATCH_label_d7; break;
              case 2: 
                MATCH_name = "LDUH"; goto MATCH_label_d7; break;
              case 3: 
                MATCH_name = "LDD"; goto MATCH_label_d7; break;
              case 4: 
                MATCH_name = "ST"; goto MATCH_label_d8; break;
              case 5: 
                MATCH_name = "STB"; goto MATCH_label_d8; break;
              case 6: 
                MATCH_name = "STH"; goto MATCH_label_d8; break;
              case 7: 
                MATCH_name = "STD"; goto MATCH_label_d8; break;
              case 8: case 11: case 12: case 14: case 24: case 27: case 28: 
              case 30: case 34: case 40: case 41: case 42: case 43: case 44: 
              case 45: case 46: case 47: case 48: case 50: case 51: case 52: 
              case 55: case 56: case 57: case 58: case 59: case 60: case 61: 
              case 62: case 63: 
                goto MATCH_label_d2; break;
              case 9: 
                MATCH_name = "LDSB"; goto MATCH_label_d7; break;
              case 10: 
                MATCH_name = "LDSH"; goto MATCH_label_d7; break;
              case 13: 
                MATCH_name = "LDSTUB"; goto MATCH_label_d7; break;
              case 15: 
                MATCH_name = "SWAP."; goto MATCH_label_d7; break;
              case 16: 
                if ((MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */ == 0) { 
                  MATCH_name = 
                    MATCH_name_i_72[(MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */]; 
                  goto MATCH_label_d9; 
                  
                } /*opt-block*/
                else 
                  goto MATCH_label_d2;  /*opt-block+*/
                
                break;
              case 17: 
                if ((MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */ == 0) { 
                  MATCH_name = 
                    MATCH_name_i_73[(MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */]; 
                  goto MATCH_label_d9; 
                  
                } /*opt-block*/
                else 
                  goto MATCH_label_d2;  /*opt-block+*/
                
                break;
              case 18: 
                if ((MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */ == 0) { 
                  MATCH_name = 
                    MATCH_name_i_74[(MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */]; 
                  goto MATCH_label_d9; 
                  
                } /*opt-block*/
                else 
                  goto MATCH_label_d2;  /*opt-block+*/
                
                break;
              case 19: 
                if ((MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */ == 0) { 
                  MATCH_name = 
                    MATCH_name_i_75[(MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */]; 
                  goto MATCH_label_d9; 
                  
                } /*opt-block*/
                else 
                  goto MATCH_label_d2;  /*opt-block+*/
                
                break;
              case 20: 
                if ((MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */ == 0) { 
                  MATCH_name = 
                    MATCH_name_i_76[(MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */]; 
                  goto MATCH_label_d10; 
                  
                } /*opt-block*/
                else 
                  goto MATCH_label_d2;  /*opt-block+*/
                
                break;
              case 21: 
                if ((MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */ == 0) { 
                  MATCH_name = 
                    MATCH_name_i_77[(MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */]; 
                  goto MATCH_label_d10; 
                  
                } /*opt-block*/
                else 
                  goto MATCH_label_d2;  /*opt-block+*/
                
                break;
              case 22: 
                if ((MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */ == 0) { 
                  MATCH_name = 
                    MATCH_name_i_78[(MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */]; 
                  goto MATCH_label_d10; 
                  
                } /*opt-block*/
                else 
                  goto MATCH_label_d2;  /*opt-block+*/
                
                break;
              case 23: 
                if ((MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */ == 0) { 
                  MATCH_name = 
                    MATCH_name_i_79[(MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */]; 
                  goto MATCH_label_d10; 
                  
                } /*opt-block*/
                else 
                  goto MATCH_label_d2;  /*opt-block+*/
                
                break;
              case 25: 
                if ((MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */ == 0) { 
                  MATCH_name = 
                    MATCH_name_i_80[(MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */]; 
                  goto MATCH_label_d9; 
                  
                } /*opt-block*/
                else 
                  goto MATCH_label_d2;  /*opt-block+*/
                
                break;
              case 26: 
                if ((MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */ == 0) { 
                  MATCH_name = 
                    MATCH_name_i_81[(MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */]; 
                  goto MATCH_label_d9; 
                  
                } /*opt-block*/
                else 
                  goto MATCH_label_d2;  /*opt-block+*/
                
                break;
              case 29: 
                if ((MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */ == 0) { 
                  MATCH_name = "LDSTUBA"; 
                  goto MATCH_label_d9; 
                  
                } /*opt-block*/
                else 
                  goto MATCH_label_d2;  /*opt-block+*/
                
                break;
              case 31: 
                if ((MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */ == 0) { 
                  MATCH_name = "SWAPA"; 
                  goto MATCH_label_d9; 
                  
                } /*opt-block*/
                else 
                  goto MATCH_label_d2;  /*opt-block+*/
                
                break;
              case 32: 
                if ((MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */ == 1) { 
                  MATCH_name = 
                    MATCH_name_i_72[(MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */]; 
                  goto MATCH_label_d11; 
                  
                } /*opt-block*/
                else { 
                  MATCH_name = "LDF"; 
                  goto MATCH_label_d11; 
                  
                } /*opt-block*/
                
                break;
              case 33: 
                if ((MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */ == 1) { 
                  MATCH_name = 
                    MATCH_name_i_73[(MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */]; 
                  goto MATCH_label_d12; 
                  
                } /*opt-block*/
                else { 
                  MATCH_name = "LDFSR"; 
                  goto MATCH_label_d12; 
                  
                } /*opt-block*/
                
                break;
              case 35: 
                if ((MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */ == 1) { 
                  MATCH_name = 
                    MATCH_name_i_74[(MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */]; 
                  goto MATCH_label_d13; 
                  
                } /*opt-block*/
                else { 
                  MATCH_name = "LDDF"; 
                  goto MATCH_label_d13; 
                  
                } /*opt-block*/
                
                break;
              case 36: 
                if ((MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */ == 1) { 
                  MATCH_name = 
                    MATCH_name_i_75[(MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */]; 
                  goto MATCH_label_d14; 
                  
                } /*opt-block*/
                else { 
                  MATCH_name = "STF"; 
                  goto MATCH_label_d14; 
                  
                } /*opt-block*/
                
                break;
              case 37: 
                if ((MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */ == 1) { 
                  MATCH_name = 
                    MATCH_name_i_76[(MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */]; 
                  goto MATCH_label_d15; 
                  
                } /*opt-block*/
                else { 
                  MATCH_name = "STFSR"; 
                  goto MATCH_label_d15; 
                  
                } /*opt-block*/
                
                break;
              case 38: 
                if ((MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */ == 1) { 
                  MATCH_name = 
                    MATCH_name_i_77[(MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */]; 
                  goto MATCH_label_d16; 
                  
                } /*opt-block*/
                else { 
                  MATCH_name = "STDFQ"; 
                  goto MATCH_label_d16; 
                  
                } /*opt-block*/
                
                break;
              case 39: 
                if ((MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */ == 1) { 
                  MATCH_name = 
                    MATCH_name_i_78[(MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */]; 
                  goto MATCH_label_d17; 
                  
                } /*opt-block*/
                else { 
                  MATCH_name = "STDF"; 
                  goto MATCH_label_d17; 
                  
                } /*opt-block*/
                
                break;
              case 49: 
                if ((MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */ == 1) { 
                  MATCH_name = 
                    MATCH_name_i_79[(MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */]; 
                  goto MATCH_label_d18; 
                  
                } /*opt-block*/
                else { 
                  MATCH_name = "LDCSR"; 
                  goto MATCH_label_d18; 
                  
                } /*opt-block*/
                
                break;
              case 53: 
                if ((MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */ == 1) { 
                  MATCH_name = 
                    MATCH_name_i_80[(MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */]; 
                  goto MATCH_label_d19; 
                  
                } /*opt-block*/
                else { 
                  MATCH_name = "STCSR"; 
                  goto MATCH_label_d19; 
                  
                } /*opt-block*/
                
                break;
              case 54: 
                if ((MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */ == 1) { 
                  MATCH_name = 
                    MATCH_name_i_81[(MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */]; 
                  goto MATCH_label_d20; 
                  
                } /*opt-block*/
                else { 
                  MATCH_name = "STDCQ"; 
                  goto MATCH_label_d20; 
                  
                } /*opt-block*/
                
                break;
              default: assert(0);
            } /* (MATCH_w_32_0 >> 19 & 0x3f) -- op3 at 0 --*/ 
          break;
        default: assert(0);
      } /* (MATCH_w_32_0 >> 30 & 0x3) -- op at 0 --*/ 
    
  }goto MATCH_finished_d; 
  
  MATCH_label_d0: (void)0; /*placeholder for label*/ 
    { 
      const char *name = MATCH_name;
      unsigned tgt = 
        4 * sign_extend((MATCH_w_32_0 & 0x3fffff) /* disp22 at 0 */, 22) + 
        addressToPC(MATCH_p);
      nextPC = 4 + MATCH_p; 
      
#line 401 "machine/sparc/decoder.m"

		/*
		 * Non anulled branch
		 */
		auto br = createBranch(pc, tgt, bf);
		if (!br) {
			result.valid = false;
			result.rtl = new RTL(pc);  // FIXME:  Is this needed when invalid?
			result.numBytes = nextPC - pc;
			return;
		}
		result.rtl = new RTL(pc, br);

		// The class of this instruction depends on whether or not
		// it is one of the 'unconditional' conditional branches
		// "BA" or "BN" (or the pseudo unconditionals BVx)
		result.type = SCD;
		if (strcmp(name, "BA") == 0 || strcmp(name, "BVC") == 0)
			result.type = SD;
		if (strcmp(name, "BN") == 0 || strcmp(name, "BVS") == 0)
			result.type = NCT;

		SHOW_ASM(name << " " << std::hex << tgt << std::dec);
		DEBUG_STMTS


#line 2343 "sparcdecoder.cpp"

      
    } 
    goto MATCH_finished_d; 
    
  MATCH_label_d1: (void)0; /*placeholder for label*/ 
    { 
      const char *name = MATCH_name;
      unsigned tgt = 
        4 * sign_extend((MATCH_w_32_0 & 0x3fffff) /* disp22 at 0 */, 22) + 
        addressToPC(MATCH_p);
      nextPC = 4 + MATCH_p; 
      
#line 348 "machine/sparc/decoder.m"

		/*
		 * Anulled branch
		 */
		auto br = createBranch(pc, tgt, bf);
		if (!br) {
			result.valid = false;
			result.rtl = new RTL(pc);  // FIXME:  Is this needed when invalid?
			result.numBytes = nextPC - pc;
			return;
		}
		result.rtl = new RTL(pc, br);

		// The class of this instruction depends on whether or not
		// it is one of the 'unconditional' conditional branches
		// "BA,A" or "BN,A"
		result.type = SCDAN;
		if (strcmp(name, "BA,a") == 0 || strcmp(name, "BVC,a") == 0) {
			result.type = SU;
		} else {
			result.type = SKIP;
		}

		SHOW_ASM(name << " " << std::hex << tgt << std::dec);
		DEBUG_STMTS


#line 2385 "sparcdecoder.cpp"

      
    } 
    goto MATCH_finished_d; 
    
  MATCH_label_d2: (void)0; /*placeholder for label*/ 
    { 
      nextPC = 4 + MATCH_p; 
      
#line 631 "machine/sparc/decoder.m"

	//| inst = n =>
		// What does this mean?
		result.valid = false;


#line 2402 "sparcdecoder.cpp"

      
    } 
    goto MATCH_finished_d; 
    
  MATCH_label_d3: (void)0; /*placeholder for label*/ 
    { 
      const char *name = MATCH_name;
      unsigned imm22 = (MATCH_w_32_0 & 0x3fffff) /* imm22 at 0 */ << 10;
      unsigned rd = (MATCH_w_32_0 >> 25 & 0x1f) /* rd at 0 */;
      nextPC = 4 + MATCH_p; 
      
#line 484 "machine/sparc/decoder.m"

		result.rtl = instantiate(pc, name, dis_Num(imm22), DIS_RD);


#line 2420 "sparcdecoder.cpp"

      
    } 
    goto MATCH_finished_d; 
    
  MATCH_label_d4: (void)0; /*placeholder for label*/ 
    { 
      const char *name = MATCH_name;
      unsigned rd = (MATCH_w_32_0 >> 25 & 0x1f) /* rd at 0 */;
      unsigned roi = addressToPC(MATCH_p);
      unsigned rs1 = (MATCH_w_32_0 >> 14 & 0x1f) /* rs1 at 0 */;
      nextPC = 4 + MATCH_p; 
      
#line 556 "machine/sparc/decoder.m"

		result.rtl = instantiate(pc, name, DIS_RS1, DIS_ROI, DIS_RD);


#line 2439 "sparcdecoder.cpp"

      
    } 
    goto MATCH_finished_d; 
    
  MATCH_label_d5: (void)0; /*placeholder for label*/ 
    { 
      const char *name = MATCH_name;
      unsigned addr = addressToPC(MATCH_p);
      nextPC = 4 + MATCH_p; 
      
#line 449 "machine/sparc/decoder.m"

	//| JMPL(addr, rd) =>
		/*
		 * JMPL, with rd != %o7, i.e. register jump
		 * Note: if rd==%o7, then would be handled with the call_ arm
		 */
		auto jump = new CaseStatement(DIS_ADDR);
		// Record the fact that it is a computed jump
		jump->setIsComputed();
		result.rtl = new RTL(pc, jump);
		result.type = DD;
		SHOW_ASM(name);
		DEBUG_STMTS


	//  //  //  //  //  //  //  //
	//                          //
	//   Ordinary instructions  //
	//                          //
	//  //  //  //  //  //  //  //


#line 2474 "sparcdecoder.cpp"

      
    } 
    goto MATCH_finished_d; 
    
  MATCH_label_d6: (void)0; /*placeholder for label*/ 
    { 
      unsigned addr = addressToPC(MATCH_p);
      nextPC = 4 + MATCH_p; 
      
#line 313 "machine/sparc/decoder.m"

		/*
		 * A JMPL with rd == %o7, i.e. a register call
		 */
		auto dest = DIS_ADDR;
		auto newCall = new CallStatement(dest);

		// Record the fact that this is a computed call
		newCall->setIsComputed();

		// Set the destination expression
		result.rtl = new RTL(pc, newCall);
		result.type = DD;

		SHOW_ASM("call_ " << *dest);
		DEBUG_STMTS


#line 2504 "sparcdecoder.cpp"

      
    } 
    goto MATCH_finished_d; 
    
  MATCH_label_d7: (void)0; /*placeholder for label*/ 
    { 
      const char *name = MATCH_name;
      unsigned addr = addressToPC(MATCH_p);
      unsigned rd = (MATCH_w_32_0 >> 25 & 0x1f) /* rd at 0 */;
      nextPC = 4 + MATCH_p; 
      
#line 487 "machine/sparc/decoder.m"

		result.rtl = instantiate(pc, name, DIS_ADDR, DIS_RD);


#line 2522 "sparcdecoder.cpp"

      
    } 
    goto MATCH_finished_d; 
    
  MATCH_label_d8: (void)0; /*placeholder for label*/ 
    { 
      const char *name = MATCH_name;
      unsigned addr = addressToPC(MATCH_p);
      unsigned rd = (MATCH_w_32_0 >> 25 & 0x1f) /* rd at 0 */;
      nextPC = 4 + MATCH_p; 
      
#line 500 "machine/sparc/decoder.m"

		// Note: RD is on the "right hand side" only for stores
		result.rtl = instantiate(pc, name, DIS_RDR, DIS_ADDR);


#line 2541 "sparcdecoder.cpp"

      
    } 
    goto MATCH_finished_d; 
    
  MATCH_label_d9: (void)0; /*placeholder for label*/ 
    { 
      const char *name = MATCH_name;
      unsigned addr = addressToPC(MATCH_p);
      unsigned rd = (MATCH_w_32_0 >> 25 & 0x1f) /* rd at 0 */;
      nextPC = 4 + MATCH_p; 
      
#line 496 "machine/sparc/decoder.m"

	//| load_asi(addr, asi, rd) [name] => // Note: this could be serious!
		result.rtl = instantiate(pc, name, DIS_RD, DIS_ADDR);


#line 2560 "sparcdecoder.cpp"

      
    } 
    goto MATCH_finished_d; 
    
  MATCH_label_d10: (void)0; /*placeholder for label*/ 
    { 
      const char *name = MATCH_name;
      unsigned addr = addressToPC(MATCH_p);
      unsigned rd = (MATCH_w_32_0 >> 25 & 0x1f) /* rd at 0 */;
      nextPC = 4 + MATCH_p; 
      
#line 510 "machine/sparc/decoder.m"

	//| sto_asi(rd, addr, asi) [name] => // Note: this could be serious!
		result.rtl = instantiate(pc, name, DIS_RDR, DIS_ADDR);


#line 2579 "sparcdecoder.cpp"

      
    } 
    goto MATCH_finished_d; 
    
  MATCH_label_d11: (void)0; /*placeholder for label*/ 
    { 
      const char *name = MATCH_name;
      unsigned addr = addressToPC(MATCH_p);
      unsigned fds = (MATCH_w_32_0 >> 25 & 0x1f) /* fds at 0 */;
      nextPC = 4 + MATCH_p; 
      
#line 490 "machine/sparc/decoder.m"

		result.rtl = instantiate(pc, name, DIS_ADDR, DIS_FDS);


#line 2597 "sparcdecoder.cpp"

      
    } 
    goto MATCH_finished_d; 
    
  MATCH_label_d12: (void)0; /*placeholder for label*/ 
    { 
      const char *name = MATCH_name;
      unsigned addr = addressToPC(MATCH_p);
      nextPC = 4 + MATCH_p; 
      
#line 514 "machine/sparc/decoder.m"

		result.rtl = instantiate(pc, name, DIS_ADDR);


#line 2614 "sparcdecoder.cpp"

      
    } 
    goto MATCH_finished_d; 
    
  MATCH_label_d13: (void)0; /*placeholder for label*/ 
    { 
      const char *name = MATCH_name;
      unsigned addr = addressToPC(MATCH_p);
      unsigned fdd = (MATCH_w_32_0 >> 25 & 0x1f) /* fdd at 0 */;
      nextPC = 4 + MATCH_p; 
      
#line 493 "machine/sparc/decoder.m"

		result.rtl = instantiate(pc, name, DIS_ADDR, DIS_FDD);


#line 2632 "sparcdecoder.cpp"

      
    } 
    goto MATCH_finished_d; 
    
  MATCH_label_d14: (void)0; /*placeholder for label*/ 
    { 
      const char *name = MATCH_name;
      unsigned addr = addressToPC(MATCH_p);
      unsigned fds = (MATCH_w_32_0 >> 25 & 0x1f) /* fds at 0 */;
      nextPC = 4 + MATCH_p; 
      
#line 504 "machine/sparc/decoder.m"

		result.rtl = instantiate(pc, name, DIS_FDS, DIS_ADDR);


#line 2650 "sparcdecoder.cpp"

      
    } 
    goto MATCH_finished_d; 
    
  MATCH_label_d15: (void)0; /*placeholder for label*/ 
    { 
      const char *name = MATCH_name;
      unsigned addr = addressToPC(MATCH_p);
      nextPC = 4 + MATCH_p; 
      
#line 520 "machine/sparc/decoder.m"

		result.rtl = instantiate(pc, name, DIS_ADDR);


#line 2667 "sparcdecoder.cpp"

      
    } 
    goto MATCH_finished_d; 
    
  MATCH_label_d16: (void)0; /*placeholder for label*/ 
    { 
      const char *name = MATCH_name;
      unsigned addr = addressToPC(MATCH_p);
      nextPC = 4 + MATCH_p; 
      
#line 526 "machine/sparc/decoder.m"

		result.rtl = instantiate(pc, name, DIS_ADDR);


#line 2684 "sparcdecoder.cpp"

      
    } 
    goto MATCH_finished_d; 
    
  MATCH_label_d17: (void)0; /*placeholder for label*/ 
    { 
      const char *name = MATCH_name;
      unsigned addr = addressToPC(MATCH_p);
      unsigned fdd = (MATCH_w_32_0 >> 25 & 0x1f) /* fdd at 0 */;
      nextPC = 4 + MATCH_p; 
      
#line 507 "machine/sparc/decoder.m"

		result.rtl = instantiate(pc, name, DIS_FDD, DIS_ADDR);


#line 2702 "sparcdecoder.cpp"

      
    } 
    goto MATCH_finished_d; 
    
  MATCH_label_d18: (void)0; /*placeholder for label*/ 
    { 
      const char *name = MATCH_name;
      unsigned addr = addressToPC(MATCH_p);
      nextPC = 4 + MATCH_p; 
      
#line 517 "machine/sparc/decoder.m"

		result.rtl = instantiate(pc, name, DIS_ADDR);


#line 2719 "sparcdecoder.cpp"

      
    } 
    goto MATCH_finished_d; 
    
  MATCH_label_d19: (void)0; /*placeholder for label*/ 
    { 
      const char *name = MATCH_name;
      unsigned addr = addressToPC(MATCH_p);
      nextPC = 4 + MATCH_p; 
      
#line 523 "machine/sparc/decoder.m"

		result.rtl = instantiate(pc, name, DIS_ADDR);


#line 2736 "sparcdecoder.cpp"

      
    } 
    goto MATCH_finished_d; 
    
  MATCH_label_d20: (void)0; /*placeholder for label*/ 
    { 
      const char *name = MATCH_name;
      unsigned addr = addressToPC(MATCH_p);
      nextPC = 4 + MATCH_p; 
      
#line 529 "machine/sparc/decoder.m"

		result.rtl = instantiate(pc, name, DIS_ADDR);


#line 2753 "sparcdecoder.cpp"

      
    } 
    goto MATCH_finished_d; 
    
  MATCH_finished_d: (void)0; /*placeholder for label*/
  
}
#line 2762 "sparcdecoder.cpp"

#line 639 "machine/sparc/decoder.m"

	result.numBytes = nextPC - pc;
}

/**
 * Decode the register on the LHS.
 *
 * \param r  Register (0-31).
 * \returns  The expression representing the register.
 */
Exp *
SparcDecoder::dis_RegLhs(unsigned r)
{
	return Location::regOf(r);
}

/**
 * Decode the register on the RHS.
 *
 * \note  Replaces r[0] with const 0.
 * \note  Not used by DIS_RD since don't want 0 on LHS.
 *
 * \param r  Register (0-31).
 * \returns  The expression representing the register.
 */
Exp *
SparcDecoder::dis_RegRhs(unsigned r)
{
	if (r == 0)
		return new Const(0);
	return Location::regOf(r);
}

/**
 * Decode the register or immediate at the given address.
 *
 * \param pc  An address in the instruction stream.
 * \returns   The register or immediate at the given address.
 */
Exp *
SparcDecoder::dis_RegImm(ADDRESS pc, const BinaryFile *bf)
{

#line 2808 "sparcdecoder.cpp"

#line 681 "machine/sparc/decoder.m"
{ 
  ADDRESS MATCH_p = 
    
#line 681 "machine/sparc/decoder.m"
pc
#line 2816 "sparcdecoder.cpp"
;
  unsigned MATCH_w_32_0;
  { 
    MATCH_w_32_0 = fetch32(MATCH_p); 
    if ((MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */ == 1) { 
      int /* [~4096..4095] */ simm13 = 
        sign_extend((MATCH_w_32_0 & 0x1fff) /* simm13 at 0 */, 13);
      
#line 682 "machine/sparc/decoder.m"

		return new Const(simm13);

#line 2829 "sparcdecoder.cpp"

      
    } /*opt-block*//*opt-block+*/
    else { 
      unsigned rs2 = (MATCH_w_32_0 & 0x1f) /* rs2 at 0 */;
      
#line 684 "machine/sparc/decoder.m"

		return dis_RegRhs(rs2);

#line 2840 "sparcdecoder.cpp"

      
    } /*opt-block*//*opt-block+*/
    
  }goto MATCH_finished_c; 
  
  MATCH_finished_c: (void)0; /*placeholder for label*/
  
}
#line 2850 "sparcdecoder.cpp"

#line 687 "machine/sparc/decoder.m"
}

/**
 * Converts a dynamic address to a Exp* expression.
 * E.g. %o7 --> r[ 15 ]
 *
 * \param pc  The instruction stream address of the dynamic address.
 *
 * \returns  The Exp* representation of the given address.
 */
Exp *
SparcDecoder::dis_Eaddr(ADDRESS pc, const BinaryFile *bf)
{

#line 2867 "sparcdecoder.cpp"

#line 700 "machine/sparc/decoder.m"
{ 
  ADDRESS MATCH_p = 
    
#line 700 "machine/sparc/decoder.m"
pc
#line 2875 "sparcdecoder.cpp"
;
  unsigned MATCH_w_32_0;
  { 
    MATCH_w_32_0 = fetch32(MATCH_p); 
    if ((MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */ == 1) 
      if ((MATCH_w_32_0 >> 14 & 0x1f) /* rs1 at 0 */ == 0) { 
        int /* [~4096..4095] */ simm13 = 
          sign_extend((MATCH_w_32_0 & 0x1fff) /* simm13 at 0 */, 13);
        
#line 707 "machine/sparc/decoder.m"

		return new Const((int)simm13);

#line 2889 "sparcdecoder.cpp"

        
      } /*opt-block*//*opt-block+*/
      else { 
        unsigned rs1 = (MATCH_w_32_0 >> 14 & 0x1f) /* rs1 at 0 */;
        int /* [~4096..4095] */ simm13 = 
          sign_extend((MATCH_w_32_0 & 0x1fff) /* simm13 at 0 */, 13);
        
#line 709 "machine/sparc/decoder.m"

		return new Binary(opPlus,
		                  Location::regOf(rs1),
		                  new Const((int)simm13));

#line 2904 "sparcdecoder.cpp"

        
      } /*opt-block*//*opt-block+*/ /*opt-block+*/
    else 
      if ((MATCH_w_32_0 & 0x1f) /* rs2 at 0 */ == 0) { 
        unsigned rs1 = (MATCH_w_32_0 >> 14 & 0x1f) /* rs1 at 0 */;
        
#line 701 "machine/sparc/decoder.m"

		return Location::regOf(rs1);

#line 2916 "sparcdecoder.cpp"

        
      } /*opt-block*//*opt-block+*/
      else { 
        unsigned rs1 = (MATCH_w_32_0 >> 14 & 0x1f) /* rs1 at 0 */;
        unsigned rs2 = (MATCH_w_32_0 & 0x1f) /* rs2 at 0 */;
        
#line 703 "machine/sparc/decoder.m"

		return new Binary(opPlus,
		                  Location::regOf(rs1),
		                  Location::regOf(rs2));

#line 2930 "sparcdecoder.cpp"

        
      } /*opt-block*//*opt-block+*/ /*opt-block+*/
    
  }goto MATCH_finished_b; 
  
  MATCH_finished_b: (void)0; /*placeholder for label*/
  
}
#line 2940 "sparcdecoder.cpp"

#line 714 "machine/sparc/decoder.m"
}

#if 0 // Cruft?
/**
 * Check to see if the instructions at the given offset match any callee
 * prologue, i.e. does it look like this offset is a pointer to a function?
 *
 * \param hostPC  Pointer to the code in question (host address).
 * \returns       True if a match found.
 */
bool
SparcDecoder::isFuncPrologue(ADDRESS hostPC)
{
#if 0  // Can't do this without patterns. It was a bit of a hack anyway
	int hiVal, loVal, reg, locals;
	if (InstructionPatterns::new_reg_win(prog.csrSrc, hostPC, locals))
		return true;
	if (InstructionPatterns::new_reg_win_large(prog.csrSrc, hostPC, hiVal, loVal, reg))
		return true;
	if (InstructionPatterns::same_reg_win(prog.csrSrc, hostPC, locals))
		return true;
	if (InstructionPatterns::same_reg_win_large(prog.csrSrc, hostPC, hiVal, loVal, reg))
		return true;
#endif
	return false;
}
#endif

/**
 * Check to see if the instruction at the given offset is a restore
 * instruction.
 *
 * \param pc      Pointer to the code in question.
 * \returns       True if a match found.
 */
bool
SparcDecoder::isRestore(ADDRESS pc, const BinaryFile *bf)
{

#line 2982 "sparcdecoder.cpp"

#line 752 "machine/sparc/decoder.m"
{ 
  ADDRESS MATCH_p = 
    
#line 752 "machine/sparc/decoder.m"
pc
#line 2990 "sparcdecoder.cpp"
;
  unsigned MATCH_w_32_0;
  { 
    MATCH_w_32_0 = fetch32(MATCH_p); 
    if ((MATCH_w_32_0 >> 30 & 0x3) /* op at 0 */ == 2 && 
      (MATCH_w_32_0 >> 19 & 0x3f) /* op3 at 0 */ == 61) 
      
#line 753 "machine/sparc/decoder.m"

		return true;

#line 3002 "sparcdecoder.cpp"
 /*opt-block+*/
    else 
      goto MATCH_label_a0;  /*opt-block+*/
    
  }goto MATCH_finished_a; 
  
  MATCH_label_a0: (void)0; /*placeholder for label*/ 
    
#line 755 "machine/sparc/decoder.m"

		return false;

#line 3015 "sparcdecoder.cpp"
 
    goto MATCH_finished_a; 
    
  MATCH_finished_a: (void)0; /*placeholder for label*/
  
}
#line 3022 "sparcdecoder.cpp"

#line 758 "machine/sparc/decoder.m"
}

#line 3027 "sparcdecoder.cpp"

