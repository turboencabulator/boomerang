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

#define DIS_ROI     (dis_RegImm(roi))
#define DIS_ADDR    (dis_Eaddr(addr))
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

SparcDecoder::SparcDecoder(Prog *prog) :
	NJMCDecoder(prog)
{
	std::string file = Boomerang::get()->getProgPath() + "frontend/machine/sparc/sparc.ssl";
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
 * Create an RTL for a Bx instruction.
 *
 * \param pc     The location counter.
 * \param stmts  Ptr to list of Statement pointers.
 * \param name   Instruction name (e.g. "BNE,a", or "BPNE").
 *
 * \returns  Pointer to newly created RTL, or nullptr if invalid.
 */
RTL *
SparcDecoder::createBranchRtl(ADDRESS pc, std::list<Statement *> *stmts, const char *name)
{
	auto res = new RTL(pc, stmts);
	auto br = new BranchStatement();
	res->appendStmt(br);
	if (name[0] == 'F') {
		// fbranch is any of [ FBN FBNE FBLG FBUL FBL   FBUG FBG   FBU
		//                     FBA FBE  FBUE FBGE FBUGE FBLE FBULE FBO ],
		// fbranches are not the same as ibranches, so need a whole different set of tests
		if (name[2] == 'U')
			name++;  // Just ignore unordered (for now)
		switch (name[2]) {
		case 'E':
			br->setCondType(BRANCH_JE, true);  // FBE
			break;
		case 'L':
			if (name[3] == 'G')
				br->setCondType(BRANCH_JNE, true);   // FBLG
			else if (name[3] == 'E')
				br->setCondType(BRANCH_JSLE, true);  // FBLE
			else
				br->setCondType(BRANCH_JSL, true);   // FBL
			break;
		case 'G':
			if (name[3] == 'E')
				br->setCondType(BRANCH_JSGE, true);  // FBGE
			else
				br->setCondType(BRANCH_JSG, true);   // FBG
			break;
		case 'N':
			if (name[3] == 'E')
				br->setCondType(BRANCH_JNE, true);   // FBNE
			// Else it's FBN!
			break;
		default:
			std::cerr << "unknown float branch " << name << std::endl;
			delete res;
			res = nullptr;
		}
		return res;
	}

	// ibranch is any of [ BN BE  BLE BL  BLEU BCS BNEG BVS
	//                     BA BNE BG  BGE BGU  BCC BPOS BVC ],
	// Note: BPN, BPE, etc handled below
	switch (name[1]) {
	case 'E':
		br->setCondType(BRANCH_JE);  // BE
		break;
	case 'L':
		if (name[2] == 'E') {
			if (name[3] == 'U')
				br->setCondType(BRANCH_JULE);  // BLEU
			else
				br->setCondType(BRANCH_JSLE);  // BLE
		} else {
			br->setCondType(BRANCH_JSL);  // BL
		}
		break;
	case 'N':
		// BNE, BNEG (won't see BN)
		if (name[3] == 'G')
			br->setCondType(BRANCH_JMI);  // BNEG
		else
			br->setCondType(BRANCH_JNE);  // BNE
		break;
	case 'C':
		// BCC, BCS
		if (name[2] == 'C')
			br->setCondType(BRANCH_JUGE);  // BCC
		else
			br->setCondType(BRANCH_JUL);   // BCS
		break;
	case 'V':
		// BVC, BVS; should never see these now
		if (name[2] == 'C')
			std::cerr << "Decoded BVC instruction\n";  // BVC
		else
			std::cerr << "Decoded BVS instruction\n";  // BVS
		break;
	case 'G':
		// BGE, BG, BGU
		if (name[2] == 'E')
			br->setCondType(BRANCH_JSGE);  // BGE
		else if (name[2] == 'U')
			br->setCondType(BRANCH_JUG);   // BGU
		else
			br->setCondType(BRANCH_JSG);   // BG
		break;
	case 'P':
		if (name[2] == 'O') {
			br->setCondType(BRANCH_JPOS);  // BPOS
			break;
		}
		// Else, it's a BPXX; remove the P (for predicted) and try again
		// (recurse)
		// B P P O S ...
		// 0 1 2 3 4 ...
		char temp[8];
		temp[0] = 'B';
		strcpy(temp + 1, name + 2);
		delete res;
		return createBranchRtl(pc, stmts, temp);
	default:
		std::cerr << "unknown non-float branch " << name << std::endl;
	}
	return res;
}


/**
 * Attempt to decode the high level instruction at a given address and return
 * the corresponding HL type (e.g. CallStatement, GotoStatement etc).  If no
 * high level instruction exists at the given address, then simply return the
 * RTL for the low level instruction at this address.  There is an option to
 * also include the low level statements for a HL instruction.
 *
 * \param pc     The native address of the pc.
 * \param delta  The difference between the above address and the host address
 *               of the pc (i.e. the address that the pc is at in the loaded
 *               object file).
 * \param proc   The enclosing procedure.  This can be nullptr for those of us
 *               who are using this method in an interpreter.
 *
 * \returns  A DecodeResult structure containing all the information gathered
 *           during decoding.
 */
DecodeResult &
SparcDecoder::decodeInstruction(ADDRESS pc, ptrdiff_t delta)
{
	static DecodeResult result;
	ADDRESS hostPC = pc + delta;

	// Clear the result structure;
	result.reset();

	// The actual list of instantiated statements
	std::list<Statement *> *stmts = nullptr;

	ADDRESS nextPC = NO_ADDRESS;


#line 220 "sparcdecoder.cpp"

#line 214 "machine/sparc/decoder.m"
{ 
  ADDRESS MATCH_p = 
    
#line 214 "machine/sparc/decoder.m"
hostPC
#line 228 "sparcdecoder.cpp"
;
  const char *MATCH_name;
  static const char *MATCH_name_cond_0[] = {
    "BPN", "BPE", "BPLE", "BPL", "BPLEU", "BPCS", "BPNEG", "BPVS", "BPA,a", 
    "BPNE", "BPG", "BPGE", "BPGU", "BPCC", "BPPOS", "BPVC", 
  };
  static const char *MATCH_name_cond_1[] = {
    "BPN,a", "BPE,a", "BPLE,a", "BPL,a", "BPLEU,a", "BPCS,a", "BPNEG,a", 
    "BPVS,a", "BA", "BPNE,a", "BPG,a", "BPGE,a", "BPGU,a", "BPCC,a", 
    "BPPOS,a", "BPVC,a", 
  };
  static const char *MATCH_name_cond_2[] = {
    "BN", "BE", "BLE", "BL", "BLEU", "BCS", "BNEG", "BVS", "BA,a", "BNE", 
    "BG", "BGE", "BGU", "BCC", "BPOS", "BVC", 
  };
  static const char *MATCH_name_cond_3[] = {
    "BN,a", "BE,a", "BLE,a", "BL,a", "BLEU,a", "BCS,a", "BNEG,a", "BVS,a", 
    "FBA", "BNE,a", "BG,a", "BGE,a", "BGU,a", "BCC,a", "BPOS,a", "BVC,a", 
  };
  static const char *MATCH_name_cond_5[] = {
    "FBN", "FBNE", "FBLG", "FBUL", "FBL", "FBUG", "FBG", "FBU", "FBA,a", 
    "FBE", "FBUE", "FBGE", "FBUGE", "FBLE", "FBULE", "FBO", 
  };
  static const char *MATCH_name_cond_6[] = {
    "FBN,a", "FBNE,a", "FBLG,a", "FBUL,a", "FBL,a", "FBUG,a", "FBG,a", 
    "FBU,a", "CBA", "FBE,a", "FBUE,a", "FBGE,a", "FBUGE,a", "FBLE,a", 
    "FBULE,a", "FBO,a", 
  };
  static const char *MATCH_name_cond_7[] = {
    "CBN", "CB123", "CB12", "CB13", "CB1", "CB23", "CB2", "CB3", "CBA,a", 
    "CB0", "CB03", "CB02", "CB023", "CB01", "CB013", "CB012", 
  };
  static const char *MATCH_name_cond_8[] = {
    "CBN,a", "CB123,a", "CB12,a", "CB13,a", "CB1,a", "CB23,a", "CB2,a", 
    "CB3,a", "TA", "CB0,a", "CB03,a", "CB02,a", "CB023,a", "CB01,a", 
    "CB013,a", "CB012,a", 
  };
  static const char *MATCH_name_op3_46[] = {
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 
    NULL, NULL, NULL, NULL, NULL, "RDPSR", "RDWIM", "RDTBR", 
  };
  static const char *MATCH_name_opf_51[] = {
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
  static const char *MATCH_name_cond_53[] = {
    "TN", "TE", "TLE", "TL", "TLEU", "TCS", "TNEG", "TVS", NULL, "TNE", "TG", 
    "TGE", "TGU", "TCC", "TPOS", "TVC", 
  };
  static const char *MATCH_name_i_66[] = {"LDA", "LDF", };
  static const char *MATCH_name_i_67[] = {"LDUBA", "LDFSR", };
  static const char *MATCH_name_i_68[] = {"LDUHA", "LDDF", };
  static const char *MATCH_name_i_69[] = {"LDDA", "STF", };
  static const char *MATCH_name_i_70[] = {"STA", "STFSR", };
  static const char *MATCH_name_i_71[] = {"STBA", "STDFQ", };
  static const char *MATCH_name_i_72[] = {"STHA", "STDF", };
  static const char *MATCH_name_i_73[] = {"STDA", "LDCSR", };
  static const char *MATCH_name_i_74[] = {"LDSBA", "STCSR", };
  static const char *MATCH_name_i_75[] = {"LDSHA", "STDCQ", };
  unsigned MATCH_w_32_0;
  { 
    MATCH_w_32_0 = getDword(MATCH_p); 
    
      switch((MATCH_w_32_0 >> 30 & 0x3) /* op at 0 */) {
        case 0: 
          
            switch((MATCH_w_32_0 >> 22 & 0x7) /* op2 at 0 */) {
              case 0: 
                nextPC = 4 + MATCH_p; 
                
#line 626 "machine/sparc/decoder.m"

	//| UNIMP(n) =>
		stmts = nullptr;
		result.valid = false;


#line 326 "sparcdecoder.cpp"

                
                
                break;
              case 1: 
                if ((MATCH_w_32_0 >> 29 & 0x1) /* a at 0 */ == 1) 
                  if ((MATCH_w_32_0 >> 25 & 0xf) /* cond at 0 */ == 8) { 
                    MATCH_name = MATCH_name_cond_0[(MATCH_w_32_0 >> 25 & 0xf) 
                          /* cond at 0 */]; 
                    goto MATCH_label_d0; 
                    
                  } /*opt-block*/
                  else { 
                    MATCH_name = MATCH_name_cond_1[(MATCH_w_32_0 >> 25 & 0xf) 
                          /* cond at 0 */]; 
                    goto MATCH_label_d0; 
                    
                  } /*opt-block*/ /*opt-block+*/
                else 
                  if ((MATCH_w_32_0 >> 25 & 0xf) /* cond at 0 */ == 8) { 
                    unsigned tgt = 
                      4 * sign_extend(
                                  (MATCH_w_32_0 & 0x7ffff) /* disp19 at 0 */, 
                                  19) + addressToPC(MATCH_p);
                    nextPC = 4 + MATCH_p; 
                    
#line 397 "machine/sparc/decoder.m"
  /* Can see bpa xcc,tgt in 32 bit code */
	//| BPA(cc01, tgt) => // cc01 does not matter because is unconditional
		auto jump = new GotoStatement;

		result.type = SD;
		result.rtl = new RTL(pc, stmts);
		result.rtl->appendStmt(jump);
		jump->setDest(tgt - delta);
		SHOW_ASM("BPA " << std::hex << tgt - delta)
		DEBUG_STMTS


#line 366 "sparcdecoder.cpp"

                    
                  } /*opt-block*//*opt-block+*/
                  else { 
                    MATCH_name = MATCH_name_cond_0[(MATCH_w_32_0 >> 25 & 0xf) 
                          /* cond at 0 */]; 
                    { 
                      const char *name = MATCH_name;
                      unsigned cc01 = 
                        (MATCH_w_32_0 >> 20 & 0x3) /* cc01 at 0 */;
                      unsigned tgt = 
                        4 * sign_extend(
                                    (MATCH_w_32_0 & 0x7ffff) 
                                          /* disp19 at 0 */, 19) + 
                        addressToPC(MATCH_p);
                      nextPC = 4 + MATCH_p; 
                      
#line 408 "machine/sparc/decoder.m"

		if (cc01 != 0) {  /* If 64 bit cc used, can't handle */
			result.valid = false;
			result.rtl = new RTL;
			result.numBytes = nextPC - hostPC;
			return result;
		}
		GotoStatement *jump = nullptr;
		RTL *rtl = nullptr;
		if (strcmp(name, "BPN") == 0) {
			jump = new GotoStatement;
			rtl = new RTL(pc, stmts);
			rtl->appendStmt(jump);
		} else if (strcmp(name, "BPVS") == 0 || strcmp(name, "BPVC") == 0) {
			jump = new GotoStatement;
			rtl = new RTL(pc, stmts);
			rtl->appendStmt(jump);
		} else {
			rtl = createBranchRtl(pc, stmts, name);
			// The BranchStatement will be the last Stmt of the rtl
			jump = (GotoStatement *)rtl->getList().back();
		}

		// The class of this instruction depends on whether or not
		// it is one of the 'unconditional' conditional branches
		// "BPN" (or the pseudo unconditionals BPVx)
		result.type = SCD;
		if (strcmp(name, "BPVC") == 0)
			result.type = SD;
		if ((strcmp(name, "BPN") == 0) || (strcmp(name, "BPVS") == 0))
			result.type = NCT;

		result.rtl = rtl;
		jump->setDest(tgt - delta);
		SHOW_ASM(name << " " << std::hex << tgt - delta)
		DEBUG_STMTS


#line 423 "sparcdecoder.cpp"

                      
                    }
                    
                  } /*opt-block*/ /*opt-block+*/
                break;
              case 2: 
                if ((MATCH_w_32_0 >> 29 & 0x1) /* a at 0 */ == 1) 
                  if ((MATCH_w_32_0 >> 25 & 0xf) /* cond at 0 */ == 8) { 
                    MATCH_name = MATCH_name_cond_2[(MATCH_w_32_0 >> 25 & 0xf) 
                          /* cond at 0 */]; 
                    goto MATCH_label_d2; 
                    
                  } /*opt-block*/
                  else { 
                    MATCH_name = MATCH_name_cond_3[(MATCH_w_32_0 >> 25 & 0xf) 
                          /* cond at 0 */]; 
                    goto MATCH_label_d2; 
                    
                  } /*opt-block*/ /*opt-block+*/
                else 
                  if ((MATCH_w_32_0 >> 25 & 0xf) /* cond at 0 */ == 8) { 
                    MATCH_name = MATCH_name_cond_1[(MATCH_w_32_0 >> 25 & 0xf) 
                          /* cond at 0 */]; 
                    goto MATCH_label_d1; 
                    
                  } /*opt-block*/
                  else { 
                    MATCH_name = MATCH_name_cond_2[(MATCH_w_32_0 >> 25 & 0xf) 
                          /* cond at 0 */]; 
                    goto MATCH_label_d1; 
                    
                  } /*opt-block*/ /*opt-block+*/
                break;
              case 3: case 5: 
                goto MATCH_label_d3; break;
              case 4: 
                if ((MATCH_w_32_0 & 0x3fffff) /* imm22 at 0 */ == 0 && 
                  (MATCH_w_32_0 >> 25 & 0x1f) /* rd at 0 */ == 0) { 
                  MATCH_name = "NOP"; 
                  { 
                    const char *name = MATCH_name;
                    nextPC = 4 + MATCH_p; 
                    
#line 478 "machine/sparc/decoder.m"

		result.type = NOP;
		stmts = instantiate(pc, name);


#line 474 "sparcdecoder.cpp"

                    
                  }
                  
                } /*opt-block*/
                else 
                  goto MATCH_label_d4;  /*opt-block+*/
                
                break;
              case 6: 
                if ((MATCH_w_32_0 >> 29 & 0x1) /* a at 0 */ == 1) 
                  if ((MATCH_w_32_0 >> 25 & 0xf) /* cond at 0 */ == 8) { 
                    MATCH_name = MATCH_name_cond_5[(MATCH_w_32_0 >> 25 & 0xf) 
                          /* cond at 0 */]; 
                    goto MATCH_label_d2; 
                    
                  } /*opt-block*/
                  else { 
                    MATCH_name = MATCH_name_cond_6[(MATCH_w_32_0 >> 25 & 0xf) 
                          /* cond at 0 */]; 
                    goto MATCH_label_d2; 
                    
                  } /*opt-block*/ /*opt-block+*/
                else 
                  if ((MATCH_w_32_0 >> 25 & 0xf) /* cond at 0 */ == 8) { 
                    MATCH_name = MATCH_name_cond_3[(MATCH_w_32_0 >> 25 & 0xf) 
                          /* cond at 0 */]; 
                    goto MATCH_label_d1; 
                    
                  } /*opt-block*/
                  else { 
                    MATCH_name = MATCH_name_cond_5[(MATCH_w_32_0 >> 25 & 0xf) 
                          /* cond at 0 */]; 
                    goto MATCH_label_d1; 
                    
                  } /*opt-block*/ /*opt-block+*/
                break;
              case 7: 
                if ((MATCH_w_32_0 >> 29 & 0x1) /* a at 0 */ == 1) 
                  if ((MATCH_w_32_0 >> 25 & 0xf) /* cond at 0 */ == 8) { 
                    MATCH_name = MATCH_name_cond_7[(MATCH_w_32_0 >> 25 & 0xf) 
                          /* cond at 0 */]; 
                    goto MATCH_label_d2; 
                    
                  } /*opt-block*/
                  else { 
                    MATCH_name = MATCH_name_cond_8[(MATCH_w_32_0 >> 25 & 0xf) 
                          /* cond at 0 */]; 
                    goto MATCH_label_d2; 
                    
                  } /*opt-block*/ /*opt-block+*/
                else 
                  if ((MATCH_w_32_0 >> 25 & 0xf) /* cond at 0 */ == 8) { 
                    MATCH_name = MATCH_name_cond_6[(MATCH_w_32_0 >> 25 & 0xf) 
                          /* cond at 0 */]; 
                    goto MATCH_label_d1; 
                    
                  } /*opt-block*/
                  else { 
                    MATCH_name = MATCH_name_cond_7[(MATCH_w_32_0 >> 25 & 0xf) 
                          /* cond at 0 */]; 
                    goto MATCH_label_d1; 
                    
                  } /*opt-block*/ /*opt-block+*/
                break;
              default: assert(0);
            } /* (MATCH_w_32_0 >> 22 & 0x7) -- op2 at 0 --*/ 
          break;
        case 1: 
          { 
            unsigned addr = 
              4 * sign_extend((MATCH_w_32_0 & 0x3fffffff) /* disp30 at 0 */, 
                          30) + addressToPC(MATCH_p);
            nextPC = 4 + MATCH_p; 
            
#line 216 "machine/sparc/decoder.m"

		/*
		 * A standard call
		 */
		auto newCall = new CallStatement;

		// Set the destination
		ADDRESS nativeDest = addr - delta;
		newCall->setDest(nativeDest);
		Proc *destProc = prog->setNewProc(nativeDest);
		if (destProc == (Proc *)-1) destProc = nullptr;
		newCall->setDestProc(destProc);
		result.rtl = new RTL(pc, stmts);
		result.rtl->appendStmt(newCall);
		result.type = SD;
		SHOW_ASM("call__ " << std::hex << (nativeDest))
		DEBUG_STMTS


#line 570 "sparcdecoder.cpp"

            
          }
          
          break;
        case 2: 
          
            switch((MATCH_w_32_0 >> 19 & 0x3f) /* op3 at 0 */) {
              case 0: 
                MATCH_name = "ADD"; goto MATCH_label_d5; break;
              case 1: 
                MATCH_name = "AND"; goto MATCH_label_d5; break;
              case 2: 
                MATCH_name = "OR"; goto MATCH_label_d5; break;
              case 3: 
                MATCH_name = "XOR"; goto MATCH_label_d5; break;
              case 4: 
                MATCH_name = "SUB"; goto MATCH_label_d5; break;
              case 5: 
                MATCH_name = "ANDN"; goto MATCH_label_d5; break;
              case 6: 
                MATCH_name = "ORN"; goto MATCH_label_d5; break;
              case 7: 
                MATCH_name = "XNOR"; goto MATCH_label_d5; break;
              case 8: 
                MATCH_name = "ADDX"; goto MATCH_label_d5; break;
              case 9: case 13: case 25: case 29: case 44: case 45: case 46: 
              case 47: case 54: case 55: case 59: case 62: case 63: 
                goto MATCH_label_d3; break;
              case 10: 
                MATCH_name = "UMUL"; goto MATCH_label_d5; break;
              case 11: 
                MATCH_name = "SMUL"; goto MATCH_label_d5; break;
              case 12: 
                MATCH_name = "SUBX"; goto MATCH_label_d5; break;
              case 14: 
                MATCH_name = "UDIV"; goto MATCH_label_d5; break;
              case 15: 
                MATCH_name = "SDIV"; goto MATCH_label_d5; break;
              case 16: 
                MATCH_name = "ADDcc"; goto MATCH_label_d5; break;
              case 17: 
                MATCH_name = "ANDcc"; goto MATCH_label_d5; break;
              case 18: 
                MATCH_name = "ORcc"; goto MATCH_label_d5; break;
              case 19: 
                MATCH_name = "XORcc"; goto MATCH_label_d5; break;
              case 20: 
                MATCH_name = "SUBcc"; goto MATCH_label_d5; break;
              case 21: 
                MATCH_name = "ANDNcc"; goto MATCH_label_d5; break;
              case 22: 
                MATCH_name = "ORNcc"; goto MATCH_label_d5; break;
              case 23: 
                MATCH_name = "XNORcc"; goto MATCH_label_d5; break;
              case 24: 
                MATCH_name = "ADDXcc"; goto MATCH_label_d5; break;
              case 26: 
                MATCH_name = "UMULcc"; goto MATCH_label_d5; break;
              case 27: 
                MATCH_name = "SMULcc"; goto MATCH_label_d5; break;
              case 28: 
                MATCH_name = "SUBXcc"; goto MATCH_label_d5; break;
              case 30: 
                MATCH_name = "UDIVcc"; goto MATCH_label_d5; break;
              case 31: 
                MATCH_name = "SDIVcc"; goto MATCH_label_d5; break;
              case 32: 
                MATCH_name = "TADDcc"; goto MATCH_label_d5; break;
              case 33: 
                MATCH_name = "TSUBcc"; goto MATCH_label_d5; break;
              case 34: 
                MATCH_name = "TADDccTV"; goto MATCH_label_d5; break;
              case 35: 
                MATCH_name = "TSUBccTV"; goto MATCH_label_d5; break;
              case 36: 
                MATCH_name = "MULScc"; goto MATCH_label_d5; break;
              case 37: 
                MATCH_name = "SLL"; goto MATCH_label_d5; break;
              case 38: 
                MATCH_name = "SRL"; goto MATCH_label_d5; break;
              case 39: 
                MATCH_name = "SRA"; goto MATCH_label_d5; break;
              case 40: 
                if ((MATCH_w_32_0 >> 14 & 0x1f) /* rs1 at 0 */ == 0) { 
                  MATCH_name = "RDY"; 
                  { 
                    const char *name = MATCH_name;
                    unsigned rd = (MATCH_w_32_0 >> 25 & 0x1f) /* rd at 0 */;
                    nextPC = 4 + MATCH_p; 
                    
#line 530 "machine/sparc/decoder.m"

		stmts = instantiate(pc, name, DIS_RD);


#line 667 "sparcdecoder.cpp"

                    
                  }
                  
                } /*opt-block*/
                else 
                  goto MATCH_label_d3;  /*opt-block+*/
                
                break;
              case 41: 
                MATCH_name = MATCH_name_op3_46[(MATCH_w_32_0 >> 19 & 0x3f) 
                      /* op3 at 0 */]; 
                { 
                  const char *name = MATCH_name;
                  unsigned rd = (MATCH_w_32_0 >> 25 & 0x1f) /* rd at 0 */;
                  nextPC = 4 + MATCH_p; 
                  
#line 533 "machine/sparc/decoder.m"

		stmts = instantiate(pc, name, DIS_RD);


#line 690 "sparcdecoder.cpp"

                  
                }
                
                break;
              case 42: 
                MATCH_name = MATCH_name_op3_46[(MATCH_w_32_0 >> 19 & 0x3f) 
                      /* op3 at 0 */]; 
                { 
                  const char *name = MATCH_name;
                  unsigned rd = (MATCH_w_32_0 >> 25 & 0x1f) /* rd at 0 */;
                  nextPC = 4 + MATCH_p; 
                  
#line 536 "machine/sparc/decoder.m"

		stmts = instantiate(pc, name, DIS_RD);


#line 709 "sparcdecoder.cpp"

                  
                }
                
                break;
              case 43: 
                MATCH_name = MATCH_name_op3_46[(MATCH_w_32_0 >> 19 & 0x3f) 
                      /* op3 at 0 */]; 
                { 
                  const char *name = MATCH_name;
                  unsigned rd = (MATCH_w_32_0 >> 25 & 0x1f) /* rd at 0 */;
                  nextPC = 4 + MATCH_p; 
                  
#line 539 "machine/sparc/decoder.m"

		stmts = instantiate(pc, name, DIS_RD);


#line 728 "sparcdecoder.cpp"

                  
                }
                
                break;
              case 48: 
                if (1 <= (MATCH_w_32_0 >> 25 & 0x1f) /* rd at 0 */ && 
                  (MATCH_w_32_0 >> 25 & 0x1f) /* rd at 0 */ < 32) 
                  goto MATCH_label_d3;  /*opt-block+*/
                else { 
                  MATCH_name = "WRY"; 
                  { 
                    const char *name = MATCH_name;
                    unsigned roi = addressToPC(MATCH_p);
                    unsigned rs1 = (MATCH_w_32_0 >> 14 & 0x1f) /* rs1 at 0 */;
                    nextPC = 4 + MATCH_p; 
                    
#line 542 "machine/sparc/decoder.m"

		stmts = instantiate(pc, name, DIS_RS1, DIS_ROI);


#line 751 "sparcdecoder.cpp"

                    
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
                  
#line 545 "machine/sparc/decoder.m"

		stmts = instantiate(pc, name, DIS_RS1, DIS_ROI);


#line 772 "sparcdecoder.cpp"

                  
                }
                
                break;
              case 50: 
                MATCH_name = "WRWIM"; 
                { 
                  const char *name = MATCH_name;
                  unsigned roi = addressToPC(MATCH_p);
                  unsigned rs1 = (MATCH_w_32_0 >> 14 & 0x1f) /* rs1 at 0 */;
                  nextPC = 4 + MATCH_p; 
                  
#line 548 "machine/sparc/decoder.m"

		stmts = instantiate(pc, name, DIS_RS1, DIS_ROI);


#line 791 "sparcdecoder.cpp"

                  
                }
                
                break;
              case 51: 
                MATCH_name = "WRTBR"; 
                { 
                  const char *name = MATCH_name;
                  unsigned roi = addressToPC(MATCH_p);
                  unsigned rs1 = (MATCH_w_32_0 >> 14 & 0x1f) /* rs1 at 0 */;
                  nextPC = 4 + MATCH_p; 
                  
#line 551 "machine/sparc/decoder.m"

		stmts = instantiate(pc, name, DIS_RS1, DIS_ROI);


#line 810 "sparcdecoder.cpp"

                  
                }
                
                break;
              case 52: 
                if (80 <= (MATCH_w_32_0 >> 5 & 0x1ff) /* opf at 0 */ && 
                  (MATCH_w_32_0 >> 5 & 0x1ff) /* opf at 0 */ < 196 || 
                  212 <= (MATCH_w_32_0 >> 5 & 0x1ff) /* opf at 0 */ && 
                  (MATCH_w_32_0 >> 5 & 0x1ff) /* opf at 0 */ < 512) 
                  goto MATCH_label_d3;  /*opt-block+*/
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
                      goto MATCH_label_d3; break;
                    case 1: case 5: case 9: case 41: 
                      MATCH_name = 
                        MATCH_name_opf_51[(MATCH_w_32_0 >> 5 & 0x1ff) 
                            /* opf at 0 */]; 
                      { 
                        const char *name = MATCH_name;
                        unsigned fds = 
                          (MATCH_w_32_0 >> 25 & 0x1f) /* fds at 0 */;
                        unsigned fs2s = (MATCH_w_32_0 & 0x1f) /* fs2s at 0 */;
                        nextPC = 4 + MATCH_p; 
                        
#line 557 "machine/sparc/decoder.m"

		stmts = instantiate(pc, name, DIS_FS2S, DIS_FDS);


#line 852 "sparcdecoder.cpp"

                        
                      }
                      
                      break;
                    case 42: 
                      MATCH_name = 
                        MATCH_name_opf_51[(MATCH_w_32_0 >> 5 & 0x1ff) 
                            /* opf at 0 */]; 
                      { 
                        const char *name = MATCH_name;
                        unsigned fdd = 
                          (MATCH_w_32_0 >> 25 & 0x1f) /* fdd at 0 */;
                        unsigned fs2d = (MATCH_w_32_0 & 0x1f) /* fs2d at 0 */;
                        nextPC = 4 + MATCH_p; 
                        
#line 608 "machine/sparc/decoder.m"

		stmts = instantiate(pc, name, DIS_FS2D, DIS_FDD);


#line 874 "sparcdecoder.cpp"

                        
                      }
                      
                      break;
                    case 43: 
                      MATCH_name = 
                        MATCH_name_opf_51[(MATCH_w_32_0 >> 5 & 0x1ff) 
                            /* opf at 0 */]; 
                      { 
                        const char *name = MATCH_name;
                        unsigned fdq = 
                          (MATCH_w_32_0 >> 25 & 0x1f) /* fdq at 0 */;
                        unsigned fs2q = (MATCH_w_32_0 & 0x1f) /* fs2q at 0 */;
                        nextPC = 4 + MATCH_p; 
                        
#line 611 "machine/sparc/decoder.m"

		stmts = instantiate(pc, name, DIS_FS2Q, DIS_FDQ);


	// In V9, the privileged RETT becomes user-mode RETURN
	// It has the semantics of "ret restore" without the add part of the restore

#line 899 "sparcdecoder.cpp"

                        
                      }
                      
                      break;
                    case 65: case 69: case 73: case 77: 
                      MATCH_name = 
                        MATCH_name_opf_51[(MATCH_w_32_0 >> 5 & 0x1ff) 
                            /* opf at 0 */]; 
                      { 
                        const char *name = MATCH_name;
                        unsigned fds = 
                          (MATCH_w_32_0 >> 25 & 0x1f) /* fds at 0 */;
                        unsigned fs1s = 
                          (MATCH_w_32_0 >> 14 & 0x1f) /* fs1s at 0 */;
                        unsigned fs2s = (MATCH_w_32_0 & 0x1f) /* fs2s at 0 */;
                        nextPC = 4 + MATCH_p; 
                        
#line 560 "machine/sparc/decoder.m"

		stmts = instantiate(pc, name, DIS_FS1S, DIS_FS2S, DIS_FDS);


#line 923 "sparcdecoder.cpp"

                        
                      }
                      
                      break;
                    case 66: case 70: case 74: case 78: 
                      MATCH_name = 
                        MATCH_name_opf_51[(MATCH_w_32_0 >> 5 & 0x1ff) 
                            /* opf at 0 */]; 
                      { 
                        const char *name = MATCH_name;
                        unsigned fdd = 
                          (MATCH_w_32_0 >> 25 & 0x1f) /* fdd at 0 */;
                        unsigned fs1d = 
                          (MATCH_w_32_0 >> 14 & 0x1f) /* fs1d at 0 */;
                        unsigned fs2d = (MATCH_w_32_0 & 0x1f) /* fs2d at 0 */;
                        nextPC = 4 + MATCH_p; 
                        
#line 563 "machine/sparc/decoder.m"

		stmts = instantiate(pc, name, DIS_FS1D, DIS_FS2D, DIS_FDD);


#line 947 "sparcdecoder.cpp"

                        
                      }
                      
                      break;
                    case 67: case 71: case 75: case 79: 
                      MATCH_name = 
                        MATCH_name_opf_51[(MATCH_w_32_0 >> 5 & 0x1ff) 
                            /* opf at 0 */]; 
                      { 
                        const char *name = MATCH_name;
                        unsigned fdq = 
                          (MATCH_w_32_0 >> 25 & 0x1f) /* fdq at 0 */;
                        unsigned fs1q = 
                          (MATCH_w_32_0 >> 14 & 0x1f) /* fs1q at 0 */;
                        unsigned fs2q = (MATCH_w_32_0 & 0x1f) /* fs2q at 0 */;
                        nextPC = 4 + MATCH_p; 
                        
#line 566 "machine/sparc/decoder.m"

		stmts = instantiate(pc, name, DIS_FS1Q, DIS_FS2Q, DIS_FDQ);


#line 971 "sparcdecoder.cpp"

                        
                      }
                      
                      break;
                    case 196: case 209: 
                      MATCH_name = 
                        MATCH_name_opf_51[(MATCH_w_32_0 >> 5 & 0x1ff) 
                            /* opf at 0 */]; 
                      { 
                        const char *name = MATCH_name;
                        unsigned fds = 
                          (MATCH_w_32_0 >> 25 & 0x1f) /* fds at 0 */;
                        unsigned fs2s = (MATCH_w_32_0 & 0x1f) /* fs2s at 0 */;
                        nextPC = 4 + MATCH_p; 
                        
#line 578 "machine/sparc/decoder.m"

		stmts = instantiate(pc, name, DIS_FS2S, DIS_FDS);

	// Note: itod and dtoi have different sized registers

#line 994 "sparcdecoder.cpp"

                        
                      }
                      
                      break;
                    case 198: 
                      MATCH_name = 
                        MATCH_name_opf_51[(MATCH_w_32_0 >> 5 & 0x1ff) 
                            /* opf at 0 */]; 
                      { 
                        const char *name = MATCH_name;
                        unsigned fds = 
                          (MATCH_w_32_0 >> 25 & 0x1f) /* fds at 0 */;
                        unsigned fs2d = (MATCH_w_32_0 & 0x1f) /* fs2d at 0 */;
                        nextPC = 4 + MATCH_p; 
                        
#line 594 "machine/sparc/decoder.m"

		stmts = instantiate(pc, name, DIS_FS2D, DIS_FDS);


#line 1016 "sparcdecoder.cpp"

                        
                      }
                      
                      break;
                    case 199: 
                      MATCH_name = 
                        MATCH_name_opf_51[(MATCH_w_32_0 >> 5 & 0x1ff) 
                            /* opf at 0 */]; 
                      { 
                        const char *name = MATCH_name;
                        unsigned fds = 
                          (MATCH_w_32_0 >> 25 & 0x1f) /* fds at 0 */;
                        unsigned fs2q = (MATCH_w_32_0 & 0x1f) /* fs2q at 0 */;
                        nextPC = 4 + MATCH_p; 
                        
#line 599 "machine/sparc/decoder.m"

		stmts = instantiate(pc, name, DIS_FS2Q, DIS_FDS);


#line 1038 "sparcdecoder.cpp"

                        
                      }
                      
                      break;
                    case 200: 
                      MATCH_name = 
                        MATCH_name_opf_51[(MATCH_w_32_0 >> 5 & 0x1ff) 
                            /* opf at 0 */]; 
                      { 
                        const char *name = MATCH_name;
                        unsigned fdd = 
                          (MATCH_w_32_0 >> 25 & 0x1f) /* fdd at 0 */;
                        unsigned fs2s = (MATCH_w_32_0 & 0x1f) /* fs2s at 0 */;
                        nextPC = 4 + MATCH_p; 
                        
#line 582 "machine/sparc/decoder.m"

		stmts = instantiate(pc, name, DIS_FS2S, DIS_FDD);

#line 1059 "sparcdecoder.cpp"

                        
                      }
                      
                      break;
                    case 201: 
                      MATCH_name = 
                        MATCH_name_opf_51[(MATCH_w_32_0 >> 5 & 0x1ff) 
                            /* opf at 0 */]; 
                      { 
                        const char *name = MATCH_name;
                        unsigned fdd = 
                          (MATCH_w_32_0 >> 25 & 0x1f) /* fdd at 0 */;
                        unsigned fs2s = (MATCH_w_32_0 & 0x1f) /* fs2s at 0 */;
                        nextPC = 4 + MATCH_p; 
                        
#line 592 "machine/sparc/decoder.m"

		stmts = instantiate(pc, name, DIS_FS2S, DIS_FDD);

#line 1080 "sparcdecoder.cpp"

                        
                      }
                      
                      break;
                    case 203: 
                      MATCH_name = 
                        MATCH_name_opf_51[(MATCH_w_32_0 >> 5 & 0x1ff) 
                            /* opf at 0 */]; 
                      { 
                        const char *name = MATCH_name;
                        unsigned fdd = 
                          (MATCH_w_32_0 >> 25 & 0x1f) /* fdd at 0 */;
                        unsigned fs2q = (MATCH_w_32_0 & 0x1f) /* fs2q at 0 */;
                        nextPC = 4 + MATCH_p; 
                        
#line 604 "machine/sparc/decoder.m"

		stmts = instantiate(pc, name, DIS_FS2Q, DIS_FDD);



#line 1103 "sparcdecoder.cpp"

                        
                      }
                      
                      break;
                    case 204: 
                      MATCH_name = 
                        MATCH_name_opf_51[(MATCH_w_32_0 >> 5 & 0x1ff) 
                            /* opf at 0 */]; 
                      { 
                        const char *name = MATCH_name;
                        unsigned fdq = 
                          (MATCH_w_32_0 >> 25 & 0x1f) /* fdq at 0 */;
                        unsigned fs2s = (MATCH_w_32_0 & 0x1f) /* fs2s at 0 */;
                        nextPC = 4 + MATCH_p; 
                        
#line 587 "machine/sparc/decoder.m"

		stmts = instantiate(pc, name, DIS_FS2S, DIS_FDQ);

#line 1124 "sparcdecoder.cpp"

                        
                      }
                      
                      break;
                    case 205: 
                      MATCH_name = 
                        MATCH_name_opf_51[(MATCH_w_32_0 >> 5 & 0x1ff) 
                            /* opf at 0 */]; 
                      { 
                        const char *name = MATCH_name;
                        unsigned fdq = 
                          (MATCH_w_32_0 >> 25 & 0x1f) /* fdq at 0 */;
                        unsigned fs2s = (MATCH_w_32_0 & 0x1f) /* fs2s at 0 */;
                        nextPC = 4 + MATCH_p; 
                        
#line 597 "machine/sparc/decoder.m"

		stmts = instantiate(pc, name, DIS_FS2S, DIS_FDQ);

#line 1145 "sparcdecoder.cpp"

                        
                      }
                      
                      break;
                    case 206: 
                      MATCH_name = 
                        MATCH_name_opf_51[(MATCH_w_32_0 >> 5 & 0x1ff) 
                            /* opf at 0 */]; 
                      { 
                        const char *name = MATCH_name;
                        unsigned fdq = 
                          (MATCH_w_32_0 >> 25 & 0x1f) /* fdq at 0 */;
                        unsigned fs2d = (MATCH_w_32_0 & 0x1f) /* fs2d at 0 */;
                        nextPC = 4 + MATCH_p; 
                        
#line 602 "machine/sparc/decoder.m"

		stmts = instantiate(pc, name, DIS_FS2D, DIS_FDQ);

#line 1166 "sparcdecoder.cpp"

                        
                      }
                      
                      break;
                    case 210: 
                      MATCH_name = 
                        MATCH_name_opf_51[(MATCH_w_32_0 >> 5 & 0x1ff) 
                            /* opf at 0 */]; 
                      { 
                        const char *name = MATCH_name;
                        unsigned fds = 
                          (MATCH_w_32_0 >> 25 & 0x1f) /* fds at 0 */;
                        unsigned fs2d = (MATCH_w_32_0 & 0x1f) /* fs2d at 0 */;
                        nextPC = 4 + MATCH_p; 
                        
#line 584 "machine/sparc/decoder.m"

		stmts = instantiate(pc, name, DIS_FS2D, DIS_FDS);


#line 1188 "sparcdecoder.cpp"

                        
                      }
                      
                      break;
                    case 211: 
                      MATCH_name = 
                        MATCH_name_opf_51[(MATCH_w_32_0 >> 5 & 0x1ff) 
                            /* opf at 0 */]; 
                      { 
                        const char *name = MATCH_name;
                        unsigned fds = 
                          (MATCH_w_32_0 >> 25 & 0x1f) /* fds at 0 */;
                        unsigned fs2q = (MATCH_w_32_0 & 0x1f) /* fs2q at 0 */;
                        nextPC = 4 + MATCH_p; 
                        
#line 589 "machine/sparc/decoder.m"

		stmts = instantiate(pc, name, DIS_FS2Q, DIS_FDS);


#line 1210 "sparcdecoder.cpp"

                        
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
                  goto MATCH_label_d3;  /*opt-block+*/
                else 
                  switch((MATCH_w_32_0 >> 5 & 0x1ff) /* opf at 0 */) {
                    case 84: 
                      goto MATCH_label_d3; break;
                    case 81: case 85: 
                      MATCH_name = 
                        MATCH_name_opf_51[(MATCH_w_32_0 >> 5 & 0x1ff) 
                            /* opf at 0 */]; 
                      { 
                        const char *name = MATCH_name;
                        unsigned fs1s = 
                          (MATCH_w_32_0 >> 14 & 0x1f) /* fs1s at 0 */;
                        unsigned fs2s = (MATCH_w_32_0 & 0x1f) /* fs2s at 0 */;
                        nextPC = 4 + MATCH_p; 
                        
#line 569 "machine/sparc/decoder.m"

		stmts = instantiate(pc, name, DIS_FS1S, DIS_FS2S);


#line 1245 "sparcdecoder.cpp"

                        
                      }
                      
                      break;
                    case 82: case 86: 
                      MATCH_name = 
                        MATCH_name_opf_51[(MATCH_w_32_0 >> 5 & 0x1ff) 
                            /* opf at 0 */]; 
                      { 
                        const char *name = MATCH_name;
                        unsigned fs1d = 
                          (MATCH_w_32_0 >> 14 & 0x1f) /* fs1d at 0 */;
                        unsigned fs2d = (MATCH_w_32_0 & 0x1f) /* fs2d at 0 */;
                        nextPC = 4 + MATCH_p; 
                        
#line 572 "machine/sparc/decoder.m"

		stmts = instantiate(pc, name, DIS_FS1D, DIS_FS2D);


#line 1267 "sparcdecoder.cpp"

                        
                      }
                      
                      break;
                    case 83: case 87: 
                      MATCH_name = 
                        MATCH_name_opf_51[(MATCH_w_32_0 >> 5 & 0x1ff) 
                            /* opf at 0 */]; 
                      { 
                        const char *name = MATCH_name;
                        unsigned fs1q = 
                          (MATCH_w_32_0 >> 14 & 0x1f) /* fs1q at 0 */;
                        unsigned fs2q = (MATCH_w_32_0 & 0x1f) /* fs2q at 0 */;
                        nextPC = 4 + MATCH_p; 
                        
#line 575 "machine/sparc/decoder.m"

		stmts = instantiate(pc, name, DIS_FS1Q, DIS_FS2Q);


#line 1289 "sparcdecoder.cpp"

                        
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
                            case 0: case 1: case 2: case 3: case 4: case 5: 
                            case 6: case 7: case 8: case 9: case 10: case 11: 
                            case 12: case 13: case 14: case 16: case 17: 
                            case 18: case 19: case 20: case 21: case 22: 
                            case 23: case 24: case 25: case 26: case 27: 
                            case 28: case 29: case 30: 
                              goto MATCH_label_d6; break;
                            case 15: 
                              if ((MATCH_w_32_0 & 0x1fff) 
                                      /* simm13 at 0 */ == 8) { 
                                nextPC = 4 + MATCH_p; 
                                
#line 262 "machine/sparc/decoder.m"

		/*
		 * Just a ret (leaf; uses %o7 instead of %i7)
		 */
		result.rtl = new RTL(pc, stmts);
		result.rtl->appendStmt(new ReturnStatement);
		result.type = DD;
		SHOW_ASM("retl_")
		DEBUG_STMTS


#line 1329 "sparcdecoder.cpp"

                                
                              } /*opt-block*//*opt-block+*/
                              else 
                                goto MATCH_label_d6;  /*opt-block+*/
                              
                              break;
                            case 31: 
                              if ((MATCH_w_32_0 & 0x1fff) 
                                      /* simm13 at 0 */ == 8) { 
                                nextPC = 4 + MATCH_p; 
                                
#line 252 "machine/sparc/decoder.m"

		/*
		 * Just a ret (non leaf)
		 */
		result.rtl = new RTL(pc, stmts);
		result.rtl->appendStmt(new ReturnStatement);
		result.type = DD;
		SHOW_ASM("ret_")
		DEBUG_STMTS


#line 1354 "sparcdecoder.cpp"

                                
                              } /*opt-block*//*opt-block+*/
                              else 
                                goto MATCH_label_d6;  /*opt-block+*/
                              
                              break;
                            default: assert(0);
                          } /* (MATCH_w_32_0 >> 14 & 0x1f) -- rs1 at 0 --*/ 
                        break;
                      case 1: case 2: case 3: case 4: case 5: case 6: case 7: 
                      case 8: case 9: case 10: case 11: case 12: case 13: 
                      case 14: case 16: case 17: case 18: case 19: case 20: 
                      case 21: case 22: case 23: case 24: case 25: case 26: 
                      case 27: case 28: case 29: case 30: case 31: 
                        goto MATCH_label_d6; break;
                      case 15: 
                        goto MATCH_label_d7; break;
                      default: assert(0);
                    } /* (MATCH_w_32_0 >> 25 & 0x1f) -- rd at 0 --*/  
                else 
                  if ((MATCH_w_32_0 >> 25 & 0x1f) /* rd at 0 */ == 15) 
                    goto MATCH_label_d7;  /*opt-block+*/
                  else 
                    goto MATCH_label_d6;  /*opt-block+*/ /*opt-block+*/
                break;
              case 57: 
                MATCH_name = "RETURN"; 
                { 
                  const char *name = MATCH_name;
                  unsigned addr = addressToPC(MATCH_p);
                  nextPC = 4 + MATCH_p; 
                  
#line 617 "machine/sparc/decoder.m"

		stmts = instantiate(pc, name, DIS_ADDR);
		result.rtl = new RTL(pc, stmts);
		result.rtl->appendStmt(new ReturnStatement);
		result.type = DD;


#line 1396 "sparcdecoder.cpp"

                  
                }
                
                break;
              case 58: 
                if (0 <= (MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */ && 
                  (MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */ < 2 && 
                  (MATCH_w_32_0 >> 25 & 0xf) /* cond at 0 */ == 8) { 
                  MATCH_name = MATCH_name_cond_8[(MATCH_w_32_0 >> 25 & 0xf) 
                        /* cond at 0 */]; 
                  goto MATCH_label_d8; 
                  
                } /*opt-block*/
                else { 
                  MATCH_name = MATCH_name_cond_53[(MATCH_w_32_0 >> 25 & 0xf) 
                        /* cond at 0 */]; 
                  goto MATCH_label_d8; 
                  
                } /*opt-block*/
                
                break;
              case 60: 
                { 
                  unsigned rd = (MATCH_w_32_0 >> 25 & 0x1f) /* rd at 0 */;
                  unsigned roi = addressToPC(MATCH_p);
                  unsigned rs1 = (MATCH_w_32_0 >> 14 & 0x1f) /* rs1 at 0 */;
                  nextPC = 4 + MATCH_p; 
                  
#line 468 "machine/sparc/decoder.m"

		// Decided to treat SAVE as an ordinary instruction
		// That is, use the large list of effects from the SSL file, and
		// hope that optimisation will vastly help the common cases
		stmts = instantiate(pc, "SAVE", DIS_RS1, DIS_ROI, DIS_RD);


#line 1434 "sparcdecoder.cpp"

                  
                }
                
                break;
              case 61: 
                { 
                  unsigned rd = (MATCH_w_32_0 >> 25 & 0x1f) /* rd at 0 */;
                  unsigned roi = addressToPC(MATCH_p);
                  unsigned rs1 = (MATCH_w_32_0 >> 14 & 0x1f) /* rs1 at 0 */;
                  nextPC = 4 + MATCH_p; 
                  
#line 474 "machine/sparc/decoder.m"

		// Decided to treat RESTORE as an ordinary instruction
		stmts = instantiate(pc, "RESTORE", DIS_RS1, DIS_ROI, DIS_RD);


#line 1453 "sparcdecoder.cpp"

                  
                }
                
                break;
              default: assert(0);
            } /* (MATCH_w_32_0 >> 19 & 0x3f) -- op3 at 0 --*/ 
          break;
        case 3: 
          
            switch((MATCH_w_32_0 >> 19 & 0x3f) /* op3 at 0 */) {
              case 0: 
                MATCH_name = "LD"; goto MATCH_label_d9; break;
              case 1: 
                MATCH_name = "LDUB"; goto MATCH_label_d9; break;
              case 2: 
                MATCH_name = "LDUH"; goto MATCH_label_d9; break;
              case 3: 
                MATCH_name = "LDD"; goto MATCH_label_d9; break;
              case 4: 
                MATCH_name = "ST"; goto MATCH_label_d10; break;
              case 5: 
                MATCH_name = "STB"; goto MATCH_label_d10; break;
              case 6: 
                MATCH_name = "STH"; goto MATCH_label_d10; break;
              case 7: 
                MATCH_name = "STD"; goto MATCH_label_d10; break;
              case 8: case 11: case 12: case 14: case 24: case 27: case 28: 
              case 30: case 34: case 40: case 41: case 42: case 43: case 44: 
              case 45: case 46: case 47: case 48: case 50: case 51: case 52: 
              case 55: case 56: case 57: case 58: case 59: case 60: case 61: 
              case 62: case 63: 
                goto MATCH_label_d3; break;
              case 9: 
                MATCH_name = "LDSB"; goto MATCH_label_d9; break;
              case 10: 
                MATCH_name = "LDSH"; goto MATCH_label_d9; break;
              case 13: 
                MATCH_name = "LDSTUB"; goto MATCH_label_d9; break;
              case 15: 
                MATCH_name = "SWAP."; goto MATCH_label_d9; break;
              case 16: 
                if ((MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */ == 0) { 
                  MATCH_name = 
                    MATCH_name_i_66[(MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */]; 
                  goto MATCH_label_d11; 
                  
                } /*opt-block*/
                else 
                  goto MATCH_label_d3;  /*opt-block+*/
                
                break;
              case 17: 
                if ((MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */ == 0) { 
                  MATCH_name = 
                    MATCH_name_i_67[(MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */]; 
                  goto MATCH_label_d11; 
                  
                } /*opt-block*/
                else 
                  goto MATCH_label_d3;  /*opt-block+*/
                
                break;
              case 18: 
                if ((MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */ == 0) { 
                  MATCH_name = 
                    MATCH_name_i_68[(MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */]; 
                  goto MATCH_label_d11; 
                  
                } /*opt-block*/
                else 
                  goto MATCH_label_d3;  /*opt-block+*/
                
                break;
              case 19: 
                if ((MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */ == 0) { 
                  MATCH_name = 
                    MATCH_name_i_69[(MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */]; 
                  goto MATCH_label_d11; 
                  
                } /*opt-block*/
                else 
                  goto MATCH_label_d3;  /*opt-block+*/
                
                break;
              case 20: 
                if ((MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */ == 0) { 
                  MATCH_name = 
                    MATCH_name_i_70[(MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */]; 
                  goto MATCH_label_d12; 
                  
                } /*opt-block*/
                else 
                  goto MATCH_label_d3;  /*opt-block+*/
                
                break;
              case 21: 
                if ((MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */ == 0) { 
                  MATCH_name = 
                    MATCH_name_i_71[(MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */]; 
                  goto MATCH_label_d12; 
                  
                } /*opt-block*/
                else 
                  goto MATCH_label_d3;  /*opt-block+*/
                
                break;
              case 22: 
                if ((MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */ == 0) { 
                  MATCH_name = 
                    MATCH_name_i_72[(MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */]; 
                  goto MATCH_label_d12; 
                  
                } /*opt-block*/
                else 
                  goto MATCH_label_d3;  /*opt-block+*/
                
                break;
              case 23: 
                if ((MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */ == 0) { 
                  MATCH_name = 
                    MATCH_name_i_73[(MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */]; 
                  goto MATCH_label_d12; 
                  
                } /*opt-block*/
                else 
                  goto MATCH_label_d3;  /*opt-block+*/
                
                break;
              case 25: 
                if ((MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */ == 0) { 
                  MATCH_name = 
                    MATCH_name_i_74[(MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */]; 
                  goto MATCH_label_d11; 
                  
                } /*opt-block*/
                else 
                  goto MATCH_label_d3;  /*opt-block+*/
                
                break;
              case 26: 
                if ((MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */ == 0) { 
                  MATCH_name = 
                    MATCH_name_i_75[(MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */]; 
                  goto MATCH_label_d11; 
                  
                } /*opt-block*/
                else 
                  goto MATCH_label_d3;  /*opt-block+*/
                
                break;
              case 29: 
                if ((MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */ == 0) { 
                  MATCH_name = "LDSTUBA"; 
                  goto MATCH_label_d11; 
                  
                } /*opt-block*/
                else 
                  goto MATCH_label_d3;  /*opt-block+*/
                
                break;
              case 31: 
                if ((MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */ == 0) { 
                  MATCH_name = "SWAPA"; 
                  goto MATCH_label_d11; 
                  
                } /*opt-block*/
                else 
                  goto MATCH_label_d3;  /*opt-block+*/
                
                break;
              case 32: 
                if ((MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */ == 1) { 
                  MATCH_name = 
                    MATCH_name_i_66[(MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */]; 
                  goto MATCH_label_d13; 
                  
                } /*opt-block*/
                else { 
                  MATCH_name = "LDF"; 
                  goto MATCH_label_d13; 
                  
                } /*opt-block*/
                
                break;
              case 33: 
                if ((MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */ == 1) { 
                  MATCH_name = 
                    MATCH_name_i_67[(MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */]; 
                  goto MATCH_label_d14; 
                  
                } /*opt-block*/
                else { 
                  MATCH_name = "LDFSR"; 
                  goto MATCH_label_d14; 
                  
                } /*opt-block*/
                
                break;
              case 35: 
                if ((MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */ == 1) { 
                  MATCH_name = 
                    MATCH_name_i_68[(MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */]; 
                  goto MATCH_label_d15; 
                  
                } /*opt-block*/
                else { 
                  MATCH_name = "LDDF"; 
                  goto MATCH_label_d15; 
                  
                } /*opt-block*/
                
                break;
              case 36: 
                if ((MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */ == 1) { 
                  MATCH_name = 
                    MATCH_name_i_69[(MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */]; 
                  goto MATCH_label_d16; 
                  
                } /*opt-block*/
                else { 
                  MATCH_name = "STF"; 
                  goto MATCH_label_d16; 
                  
                } /*opt-block*/
                
                break;
              case 37: 
                if ((MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */ == 1) { 
                  MATCH_name = 
                    MATCH_name_i_70[(MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */]; 
                  goto MATCH_label_d17; 
                  
                } /*opt-block*/
                else { 
                  MATCH_name = "STFSR"; 
                  goto MATCH_label_d17; 
                  
                } /*opt-block*/
                
                break;
              case 38: 
                if ((MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */ == 1) { 
                  MATCH_name = 
                    MATCH_name_i_71[(MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */]; 
                  goto MATCH_label_d18; 
                  
                } /*opt-block*/
                else { 
                  MATCH_name = "STDFQ"; 
                  goto MATCH_label_d18; 
                  
                } /*opt-block*/
                
                break;
              case 39: 
                if ((MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */ == 1) { 
                  MATCH_name = 
                    MATCH_name_i_72[(MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */]; 
                  goto MATCH_label_d19; 
                  
                } /*opt-block*/
                else { 
                  MATCH_name = "STDF"; 
                  goto MATCH_label_d19; 
                  
                } /*opt-block*/
                
                break;
              case 49: 
                if ((MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */ == 1) { 
                  MATCH_name = 
                    MATCH_name_i_73[(MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */]; 
                  goto MATCH_label_d20; 
                  
                } /*opt-block*/
                else { 
                  MATCH_name = "LDCSR"; 
                  goto MATCH_label_d20; 
                  
                } /*opt-block*/
                
                break;
              case 53: 
                if ((MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */ == 1) { 
                  MATCH_name = 
                    MATCH_name_i_74[(MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */]; 
                  goto MATCH_label_d21; 
                  
                } /*opt-block*/
                else { 
                  MATCH_name = "STCSR"; 
                  goto MATCH_label_d21; 
                  
                } /*opt-block*/
                
                break;
              case 54: 
                if ((MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */ == 1) { 
                  MATCH_name = 
                    MATCH_name_i_75[(MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */]; 
                  goto MATCH_label_d22; 
                  
                } /*opt-block*/
                else { 
                  MATCH_name = "STDCQ"; 
                  goto MATCH_label_d22; 
                  
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
      unsigned cc01 = (MATCH_w_32_0 >> 20 & 0x3) /* cc01 at 0 */;
      unsigned tgt = 
        4 * sign_extend((MATCH_w_32_0 & 0x7ffff) /* disp19 at 0 */, 19) + 
        addressToPC(MATCH_p);
      nextPC = 4 + MATCH_p; 
      
#line 315 "machine/sparc/decoder.m"

		/*
		 * Anulled, predicted branch (treat as for non predicted)
		 */

		// Instantiate a GotoStatement for the unconditional branches, HLJconds for the rest.
		if (cc01 != 0) {  /* If 64 bit cc used, can't handle */
			result.valid = false;
			result.rtl = new RTL;
			result.numBytes = nextPC - hostPC;
			return result;
		}
		GotoStatement *jump = nullptr;
		RTL *rtl = nullptr;  // Init to nullptr to suppress a warning
		if (strcmp(name, "BPA,a") == 0 || strcmp(name, "BPN,a") == 0) {
			jump = new GotoStatement;
			rtl = new RTL(pc, stmts);
			rtl->appendStmt(jump);
		} else if (strcmp(name, "BPVS,a") == 0 || strcmp(name, "BPVC,a") == 0) {
			jump = new GotoStatement;
			rtl = new RTL(pc, stmts);
			rtl->appendStmt(jump);
		} else {
			rtl = createBranchRtl(pc, stmts, name);
			jump = (GotoStatement *)rtl->getList().back();
		}

		// The class of this instruction depends on whether or not it is one of the 'unconditional' conditional branches
		// "BPA,A" or "BPN,A"
		result.type = SCDAN;
		if ((strcmp(name, "BPA,a") == 0) || (strcmp(name, "BPVC,a") == 0)) {
			result.type = SU;
		} else {
			result.type = SKIP;
		}

		result.rtl = rtl;
		jump->setDest(tgt - delta);
		SHOW_ASM(name << " " << std::hex << tgt - delta)
		DEBUG_STMTS


#line 1825 "sparcdecoder.cpp"

      
    } 
    goto MATCH_finished_d; 
    
  MATCH_label_d1: (void)0; /*placeholder for label*/ 
    { 
      const char *name = MATCH_name;
      unsigned tgt = 
        4 * sign_extend((MATCH_w_32_0 & 0x3fffff) /* disp22 at 0 */, 22) + 
        addressToPC(MATCH_p);
      nextPC = 4 + MATCH_p; 
      
#line 356 "machine/sparc/decoder.m"

		/*
		 * Non anulled branch
		 */
		// First, check for CBxxx branches (branches that depend on co-processor instructions). These are invalid,
		// as far as we are concerned
		if (name[0] == 'C') {
			result.valid = false;
			result.rtl = new RTL;
			result.numBytes = nextPC - hostPC;
			return result;
		}
		// Instantiate a GotoStatement for the unconditional branches, BranchStatement for the rest
		GotoStatement *jump = nullptr;
		RTL *rtl = nullptr;
		if (strcmp(name, "BA") == 0 || strcmp(name, "BN") == 0) {
			jump = new GotoStatement;
			rtl = new RTL(pc, stmts);
			rtl->appendStmt(jump);
		} else if (strcmp(name, "BVS") == 0 || strcmp(name, "BVC") == 0) {
			jump = new GotoStatement;
			rtl = new RTL(pc, stmts);
			rtl->appendStmt(jump);
		} else {
			rtl = createBranchRtl(pc, stmts, name);
			jump = (BranchStatement *)rtl->getList().back();
		}

		// The class of this instruction depends on whether or not it is one of the 'unconditional' conditional branches
		// "BA" or "BN" (or the pseudo unconditionals BVx)
		result.type = SCD;
		if ((strcmp(name, "BA") == 0) || (strcmp(name, "BVC") == 0))
			result.type = SD;
		if ((strcmp(name, "BN") == 0) || (strcmp(name, "BVS") == 0))
			result.type = NCT;

		result.rtl = rtl;
		jump->setDest(tgt - delta);
		SHOW_ASM(name << " " << std::hex << tgt - delta)
		DEBUG_STMTS


#line 1882 "sparcdecoder.cpp"

      
    } 
    goto MATCH_finished_d; 
    
  MATCH_label_d2: (void)0; /*placeholder for label*/ 
    { 
      const char *name = MATCH_name;
      unsigned tgt = 
        4 * sign_extend((MATCH_w_32_0 & 0x3fffff) /* disp22 at 0 */, 22) + 
        addressToPC(MATCH_p);
      nextPC = 4 + MATCH_p; 
      
#line 272 "machine/sparc/decoder.m"

		/*
		 * Anulled branch
		 */

		// First, check for CBxxx branches (branches that depend on co-processor instructions). These are invalid,
		// as far as we are concerned
		if (name[0] == 'C') {
			result.valid = false;
			result.rtl = new RTL;
			result.numBytes = nextPC - hostPC;
			return result;
		}
		// Instantiate a GotoStatement for the unconditional branches, HLJconds for the rest.
		GotoStatement *jump = nullptr;
		RTL *rtl = nullptr;  // Init to nullptr to suppress a warning
		if (strcmp(name, "BA,a") == 0 || strcmp(name, "BN,a") == 0) {
			jump = new GotoStatement;
			rtl = new RTL(pc, stmts);
			rtl->appendStmt(jump);
		} else if (strcmp(name, "BVS,a") == 0 || strcmp(name, "BVC,a") == 0) {
			jump = new GotoStatement;
			rtl = new RTL(pc, stmts);
			rtl->appendStmt(jump);
		} else {
			rtl = createBranchRtl(pc, stmts, name);
			jump = (GotoStatement *)rtl->getList().back();
		}

		// The class of this instruction depends on whether or not it is one of the 'unconditional' conditional branches
		// "BA,A" or "BN,A"
		result.type = SCDAN;
		if ((strcmp(name, "BA,a") == 0) || (strcmp(name, "BVC,a") == 0)) {
			result.type = SU;
		} else {
			result.type = SKIP;
		}

		result.rtl = rtl;
		jump->setDest(tgt - delta);
		SHOW_ASM(name << " " << std::hex << tgt - delta)
		DEBUG_STMTS


#line 1941 "sparcdecoder.cpp"

      
    } 
    goto MATCH_finished_d; 
    
  MATCH_label_d3: (void)0; /*placeholder for label*/ 
    { 
      nextPC = 4 + MATCH_p; 
      
#line 631 "machine/sparc/decoder.m"

	//| inst = n =>
		// What does this mean?
		result.valid = false;
		stmts = nullptr;


#line 1959 "sparcdecoder.cpp"

      
    } 
    goto MATCH_finished_d; 
    
  MATCH_label_d4: (void)0; /*placeholder for label*/ 
    { 
      unsigned imm22 = (MATCH_w_32_0 & 0x3fffff) /* imm22 at 0 */ << 10;
      unsigned rd = (MATCH_w_32_0 >> 25 & 0x1f) /* rd at 0 */;
      nextPC = 4 + MATCH_p; 
      
#line 482 "machine/sparc/decoder.m"

		stmts = instantiate(pc, "sethi", dis_Num(imm22), DIS_RD);


#line 1976 "sparcdecoder.cpp"

      
    } 
    goto MATCH_finished_d; 
    
  MATCH_label_d5: (void)0; /*placeholder for label*/ 
    { 
      const char *name = MATCH_name;
      unsigned rd = (MATCH_w_32_0 >> 25 & 0x1f) /* rd at 0 */;
      unsigned roi = addressToPC(MATCH_p);
      unsigned rs1 = (MATCH_w_32_0 >> 14 & 0x1f) /* rs1 at 0 */;
      nextPC = 4 + MATCH_p; 
      
#line 554 "machine/sparc/decoder.m"

		stmts = instantiate(pc, name, DIS_RS1, DIS_ROI, DIS_RD);


#line 1995 "sparcdecoder.cpp"

      
    } 
    goto MATCH_finished_d; 
    
  MATCH_label_d6: (void)0; /*placeholder for label*/ 
    { 
      unsigned addr = addressToPC(MATCH_p);
      nextPC = 4 + MATCH_p; 
      
#line 445 "machine/sparc/decoder.m"

	//| JMPL(addr, rd) =>
		/*
		 * JMPL, with rd != %o7, i.e. register jump
		 * Note: if rd==%o7, then would be handled with the call_ arm
		 */
		auto jump = new CaseStatement;
		// Record the fact that it is a computed jump
		jump->setIsComputed();
		result.rtl = new RTL(pc, stmts);
		result.rtl->appendStmt(jump);
		result.type = DD;
		jump->setDest(DIS_ADDR);
		SHOW_ASM("JMPL ")
		DEBUG_STMTS


	//  //  //  //  //  //  //  //
	//                          //
	//   Ordinary instructions  //
	//                          //
	//  //  //  //  //  //  //  //


#line 2031 "sparcdecoder.cpp"

      
    } 
    goto MATCH_finished_d; 
    
  MATCH_label_d7: (void)0; /*placeholder for label*/ 
    { 
      unsigned addr = addressToPC(MATCH_p);
      nextPC = 4 + MATCH_p; 
      
#line 234 "machine/sparc/decoder.m"

		/*
		 * A JMPL with rd == %o7, i.e. a register call
		 */
		auto newCall = new CallStatement;

		// Record the fact that this is a computed call
		newCall->setIsComputed();

		// Set the destination expression
		newCall->setDest(DIS_ADDR);
		result.rtl = new RTL(pc, stmts);
		result.rtl->appendStmt(newCall);
		result.type = DD;

		SHOW_ASM("call_ " << *DIS_ADDR)
		DEBUG_STMTS


#line 2062 "sparcdecoder.cpp"

      
    } 
    goto MATCH_finished_d; 
    
  MATCH_label_d8: (void)0; /*placeholder for label*/ 
    { 
      const char *name = MATCH_name;
      unsigned addr = addressToPC(MATCH_p);
      nextPC = 4 + MATCH_p; 
      
#line 623 "machine/sparc/decoder.m"

		stmts = instantiate(pc, name, DIS_ADDR);


#line 2079 "sparcdecoder.cpp"

      
    } 
    goto MATCH_finished_d; 
    
  MATCH_label_d9: (void)0; /*placeholder for label*/ 
    { 
      const char *name = MATCH_name;
      unsigned addr = addressToPC(MATCH_p);
      unsigned rd = (MATCH_w_32_0 >> 25 & 0x1f) /* rd at 0 */;
      nextPC = 4 + MATCH_p; 
      
#line 485 "machine/sparc/decoder.m"

		stmts = instantiate(pc, name, DIS_ADDR, DIS_RD);


#line 2097 "sparcdecoder.cpp"

      
    } 
    goto MATCH_finished_d; 
    
  MATCH_label_d10: (void)0; /*placeholder for label*/ 
    { 
      const char *name = MATCH_name;
      unsigned addr = addressToPC(MATCH_p);
      unsigned rd = (MATCH_w_32_0 >> 25 & 0x1f) /* rd at 0 */;
      nextPC = 4 + MATCH_p; 
      
#line 498 "machine/sparc/decoder.m"

		// Note: RD is on the "right hand side" only for stores
		stmts = instantiate(pc, name, DIS_RDR, DIS_ADDR);


#line 2116 "sparcdecoder.cpp"

      
    } 
    goto MATCH_finished_d; 
    
  MATCH_label_d11: (void)0; /*placeholder for label*/ 
    { 
      const char *name = MATCH_name;
      unsigned addr = addressToPC(MATCH_p);
      unsigned rd = (MATCH_w_32_0 >> 25 & 0x1f) /* rd at 0 */;
      nextPC = 4 + MATCH_p; 
      
#line 494 "machine/sparc/decoder.m"

	//| load_asi(addr, asi, rd) [name] => // Note: this could be serious!
		stmts = instantiate(pc, name, DIS_RD, DIS_ADDR);


#line 2135 "sparcdecoder.cpp"

      
    } 
    goto MATCH_finished_d; 
    
  MATCH_label_d12: (void)0; /*placeholder for label*/ 
    { 
      const char *name = MATCH_name;
      unsigned addr = addressToPC(MATCH_p);
      unsigned rd = (MATCH_w_32_0 >> 25 & 0x1f) /* rd at 0 */;
      nextPC = 4 + MATCH_p; 
      
#line 508 "machine/sparc/decoder.m"

	//| sto_asi(rd, addr, asi) [name] => // Note: this could be serious!
		stmts = instantiate(pc, name, DIS_RDR, DIS_ADDR);


#line 2154 "sparcdecoder.cpp"

      
    } 
    goto MATCH_finished_d; 
    
  MATCH_label_d13: (void)0; /*placeholder for label*/ 
    { 
      const char *name = MATCH_name;
      unsigned addr = addressToPC(MATCH_p);
      unsigned fds = (MATCH_w_32_0 >> 25 & 0x1f) /* fds at 0 */;
      nextPC = 4 + MATCH_p; 
      
#line 488 "machine/sparc/decoder.m"

		stmts = instantiate(pc, name, DIS_ADDR, DIS_FDS);


#line 2172 "sparcdecoder.cpp"

      
    } 
    goto MATCH_finished_d; 
    
  MATCH_label_d14: (void)0; /*placeholder for label*/ 
    { 
      const char *name = MATCH_name;
      unsigned addr = addressToPC(MATCH_p);
      nextPC = 4 + MATCH_p; 
      
#line 512 "machine/sparc/decoder.m"

		stmts = instantiate(pc, name, DIS_ADDR);


#line 2189 "sparcdecoder.cpp"

      
    } 
    goto MATCH_finished_d; 
    
  MATCH_label_d15: (void)0; /*placeholder for label*/ 
    { 
      const char *name = MATCH_name;
      unsigned addr = addressToPC(MATCH_p);
      unsigned fdd = (MATCH_w_32_0 >> 25 & 0x1f) /* fdd at 0 */;
      nextPC = 4 + MATCH_p; 
      
#line 491 "machine/sparc/decoder.m"

		stmts = instantiate(pc, name, DIS_ADDR, DIS_FDD);


#line 2207 "sparcdecoder.cpp"

      
    } 
    goto MATCH_finished_d; 
    
  MATCH_label_d16: (void)0; /*placeholder for label*/ 
    { 
      const char *name = MATCH_name;
      unsigned addr = addressToPC(MATCH_p);
      unsigned fds = (MATCH_w_32_0 >> 25 & 0x1f) /* fds at 0 */;
      nextPC = 4 + MATCH_p; 
      
#line 502 "machine/sparc/decoder.m"

		stmts = instantiate(pc, name, DIS_FDS, DIS_ADDR);


#line 2225 "sparcdecoder.cpp"

      
    } 
    goto MATCH_finished_d; 
    
  MATCH_label_d17: (void)0; /*placeholder for label*/ 
    { 
      const char *name = MATCH_name;
      unsigned addr = addressToPC(MATCH_p);
      nextPC = 4 + MATCH_p; 
      
#line 518 "machine/sparc/decoder.m"

		stmts = instantiate(pc, name, DIS_ADDR);


#line 2242 "sparcdecoder.cpp"

      
    } 
    goto MATCH_finished_d; 
    
  MATCH_label_d18: (void)0; /*placeholder for label*/ 
    { 
      const char *name = MATCH_name;
      unsigned addr = addressToPC(MATCH_p);
      nextPC = 4 + MATCH_p; 
      
#line 524 "machine/sparc/decoder.m"

		stmts = instantiate(pc, name, DIS_ADDR);


#line 2259 "sparcdecoder.cpp"

      
    } 
    goto MATCH_finished_d; 
    
  MATCH_label_d19: (void)0; /*placeholder for label*/ 
    { 
      const char *name = MATCH_name;
      unsigned addr = addressToPC(MATCH_p);
      unsigned fdd = (MATCH_w_32_0 >> 25 & 0x1f) /* fdd at 0 */;
      nextPC = 4 + MATCH_p; 
      
#line 505 "machine/sparc/decoder.m"

		stmts = instantiate(pc, name, DIS_FDD, DIS_ADDR);


#line 2277 "sparcdecoder.cpp"

      
    } 
    goto MATCH_finished_d; 
    
  MATCH_label_d20: (void)0; /*placeholder for label*/ 
    { 
      const char *name = MATCH_name;
      unsigned addr = addressToPC(MATCH_p);
      nextPC = 4 + MATCH_p; 
      
#line 515 "machine/sparc/decoder.m"

		stmts = instantiate(pc, name, DIS_ADDR);


#line 2294 "sparcdecoder.cpp"

      
    } 
    goto MATCH_finished_d; 
    
  MATCH_label_d21: (void)0; /*placeholder for label*/ 
    { 
      const char *name = MATCH_name;
      unsigned addr = addressToPC(MATCH_p);
      nextPC = 4 + MATCH_p; 
      
#line 521 "machine/sparc/decoder.m"

		stmts = instantiate(pc, name, DIS_ADDR);


#line 2311 "sparcdecoder.cpp"

      
    } 
    goto MATCH_finished_d; 
    
  MATCH_label_d22: (void)0; /*placeholder for label*/ 
    { 
      const char *name = MATCH_name;
      unsigned addr = addressToPC(MATCH_p);
      nextPC = 4 + MATCH_p; 
      
#line 527 "machine/sparc/decoder.m"

		stmts = instantiate(pc, name, DIS_ADDR);


#line 2328 "sparcdecoder.cpp"

      
    } 
    goto MATCH_finished_d; 
    
  MATCH_finished_d: (void)0; /*placeholder for label*/
  
}
#line 2337 "sparcdecoder.cpp"

#line 641 "machine/sparc/decoder.m"

	result.numBytes = nextPC - hostPC;
	if (result.valid && !result.rtl)  // Don't override higher level res
		result.rtl = new RTL(pc, stmts);

	return result;
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
SparcDecoder::dis_RegImm(unsigned pc)
{

#line 2387 "sparcdecoder.cpp"

#line 687 "machine/sparc/decoder.m"
{ 
  ADDRESS MATCH_p = 
    
#line 687 "machine/sparc/decoder.m"
pc
#line 2395 "sparcdecoder.cpp"
;
  unsigned MATCH_w_32_0;
  { 
    MATCH_w_32_0 = getDword(MATCH_p); 
    if ((MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */ == 1) { 
      int /* [~4096..4095] */ i = 
        sign_extend((MATCH_w_32_0 & 0x1fff) /* simm13 at 0 */, 13);
      
#line 688 "machine/sparc/decoder.m"

		Exp *expr = new Const(i);
		return expr;

#line 2409 "sparcdecoder.cpp"

      
    } /*opt-block*//*opt-block+*/
    else { 
      unsigned rs2 = (MATCH_w_32_0 & 0x1f) /* rs2 at 0 */;
      
#line 691 "machine/sparc/decoder.m"

		return dis_RegRhs(rs2);

#line 2420 "sparcdecoder.cpp"

      
    } /*opt-block*//*opt-block+*/
    
  }goto MATCH_finished_c; 
  
  MATCH_finished_c: (void)0; /*placeholder for label*/
  
}
#line 2430 "sparcdecoder.cpp"

#line 694 "machine/sparc/decoder.m"
}

/**
 * Converts a dynamic address to a Exp* expression.
 * E.g. %o7 --> r[ 15 ]
 *
 * \param pc      The instruction stream address of the dynamic address.
 * \param ignore  Redundant parameter on SPARC.
 *
 * \returns  The Exp* representation of the given address.
 */
Exp *
SparcDecoder::dis_Eaddr(ADDRESS pc, int ignore /* = 0 */)
{
	Exp *expr;


#line 2450 "sparcdecoder.cpp"

#line 710 "machine/sparc/decoder.m"
{ 
  ADDRESS MATCH_p = 
    
#line 710 "machine/sparc/decoder.m"
pc
#line 2458 "sparcdecoder.cpp"
;
  unsigned MATCH_w_32_0;
  { 
    MATCH_w_32_0 = getDword(MATCH_p); 
    if ((MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */ == 1) 
      if ((MATCH_w_32_0 >> 14 & 0x1f) /* rs1 at 0 */ == 0) { 
        int /* [~4096..4095] */ i = 
          sign_extend((MATCH_w_32_0 & 0x1fff) /* simm13 at 0 */, 13);
        
#line 717 "machine/sparc/decoder.m"

		expr = new Const((int)i);

#line 2472 "sparcdecoder.cpp"

        
      } /*opt-block*//*opt-block+*/
      else { 
        int /* [~4096..4095] */ i = 
          sign_extend((MATCH_w_32_0 & 0x1fff) /* simm13 at 0 */, 13);
        unsigned rs1 = (MATCH_w_32_0 >> 14 & 0x1f) /* rs1 at 0 */;
        
#line 719 "machine/sparc/decoder.m"

		expr = new Binary(opPlus,
		                  Location::regOf(rs1),
		                  new Const((int)i));

#line 2487 "sparcdecoder.cpp"

        
      } /*opt-block*//*opt-block+*/ /*opt-block+*/
    else 
      if ((MATCH_w_32_0 & 0x1f) /* rs2 at 0 */ == 0) { 
        unsigned rs1 = (MATCH_w_32_0 >> 14 & 0x1f) /* rs1 at 0 */;
        
#line 711 "machine/sparc/decoder.m"

		expr = Location::regOf(rs1);

#line 2499 "sparcdecoder.cpp"

        
      } /*opt-block*//*opt-block+*/
      else { 
        unsigned rs1 = (MATCH_w_32_0 >> 14 & 0x1f) /* rs1 at 0 */;
        unsigned rs2 = (MATCH_w_32_0 & 0x1f) /* rs2 at 0 */;
        
#line 713 "machine/sparc/decoder.m"

		expr = new Binary(opPlus,
		                  Location::regOf(rs1),
		                  Location::regOf(rs2));

#line 2513 "sparcdecoder.cpp"

        
      } /*opt-block*//*opt-block+*/ /*opt-block+*/
    
  }goto MATCH_finished_b; 
  
  MATCH_finished_b: (void)0; /*placeholder for label*/
  
}
#line 2523 "sparcdecoder.cpp"

#line 724 "machine/sparc/decoder.m"

	return expr;
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
 * \param hostPC  Pointer to the code in question (host address).
 * \returns       True if a match found.
 */
bool
SparcDecoder::isRestore(ADDRESS hostPC)
{

#line 2567 "sparcdecoder.cpp"

#line 764 "machine/sparc/decoder.m"
{ 
  ADDRESS MATCH_p = 
    
#line 764 "machine/sparc/decoder.m"
hostPC
#line 2575 "sparcdecoder.cpp"
;
  unsigned MATCH_w_32_0;
  { 
    MATCH_w_32_0 = getDword(MATCH_p); 
    if ((MATCH_w_32_0 >> 30 & 0x3) /* op at 0 */ == 2 && 
      (MATCH_w_32_0 >> 19 & 0x3f) /* op3 at 0 */ == 61 && 
      (0 <= (MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */ && 
      (MATCH_w_32_0 >> 13 & 0x1) /* i at 0 */ < 2)) 
      
#line 765 "machine/sparc/decoder.m"

	//| RESTORE(a, b, c) =>
		return true;

#line 2590 "sparcdecoder.cpp"
 /*opt-block+*/
    else 
      goto MATCH_label_a0;  /*opt-block+*/
    
  }goto MATCH_finished_a; 
  
  MATCH_label_a0: (void)0; /*placeholder for label*/ 
    
#line 768 "machine/sparc/decoder.m"

		return false;

#line 2603 "sparcdecoder.cpp"
 
    goto MATCH_finished_a; 
    
  MATCH_finished_a: (void)0; /*placeholder for label*/
  
}
#line 2610 "sparcdecoder.cpp"

#line 771 "machine/sparc/decoder.m"
}

/*
 * These are the fetch routines.
 */

/**
 * \returns The next 4-byte word from image pointed to by lc.
 */
uint32_t
SparcDecoder::getDword(ADDRESS lc)
{
	uint8_t *p = (uint8_t *)lc;
	return (p[0] << 24)
	     + (p[1] << 16)
	     + (p[2] <<  8)
	     +  p[3];
}

#line 2632 "sparcdecoder.cpp"

