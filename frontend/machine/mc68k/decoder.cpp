#define sign_extend(N,SIZE) (((int)((N) << (sizeof(unsigned)*8-(SIZE)))) >> (sizeof(unsigned)*8-(SIZE)))
#include <assert.h>

#line 1 "machine/mc68k/decoder.m"
/*
 * Copyright (C) 1996, Princeton University or Owen Braun ?????
 * Copyright (C) 2000, The University of Queensland
 * Copyright (C) 2000-2001, Sun Microsystems, Inc
 *
 * See the file "LICENSE.TERMS" for information on usage and
 * redistribution of this file, and for a DISCLAIMER OF ALL
 * WARRANTIES.
 *
 */

/*==============================================================================
 * FILE:       decoder.m
 * OVERVIEW:   Implementation of the higher level mc68000 specific parts of the
 *             NJMCDecoder class.
 *
 * (C) 2000 The University of Queensland, BT Group & Sun Microsystems, Inc.
 *============================================================================*/

#include "global.h"
#include "decoder.h"
#include "prog.h"
#include "ss.h"
#include "rtl.h"
#include "proc.h"
#include "csr.h"
#include "mc68k.pat.h"

/**********************************
 * NJMCDecoder methods.
 **********************************/   

/*==============================================================================
 * FUNCTION:       NJMCDecoder::decodeInstruction
 * OVERVIEW:       Decodes a machine instruction and returns an RTL instance. In
 *                 most cases a single instruction is decoded. However, if a
 *                 higher level construct that may consist of multiple
 *                 instructions is matched, then there may be a need to return
 *                 more than one RTL. The caller_prologue2 is an example of such
 *                 a construct which encloses an abritary instruction that must
 *                 be decoded into its own RTL (386 example)
 * PARAMETERS:     pc - the native address of the pc
 *                 delta - the difference between the above address and the
 *                   host address of the pc (i.e. the address that the pc is at
 *                   in the loaded object file)
 *                 RTLDict - the dictionary of RTL templates used to instantiate
 *                   the RTL for the instruction being decoded
 *                 proc - the enclosing procedure
 *                 pProc - the enclosing procedure
 * RETURNS:        a DecodeResult structure containing all the information
 *                   gathered during decoding
 *============================================================================*/
DecodeResult& NJMCDecoder::decodeInstruction (ADDRESS pc, int delta,
	UserProc* proc = NULL)
{
	static DecodeResult result;
	ADDRESS hostPC = pc + delta;

	// Clear the result structure;
	result.reset();

	// The actual list of instantiated RTs
	list<RT*>* RTs = NULL;

	// Try matching a logue first
	int addr, locals, stackSize, d16, d32, reg;
	ADDRESS saveHostPC = hostPC;
	Logue* logue;
	if ((logue = InstructionPatterns::std_call(csr, hostPC, addr)) != NULL) {
		/*
		 * Direct call
		 */
		HLCall* newCall = new HLCall(pc, 0, RTs);
		result.rtl = newCall;
		result.numBytes = hostPC - saveHostPC;

		// Set the destination expression
		newCall->setDest(addr - delta);
		newCall->setPrologue(logue);

		// Save RTL for the latest call
		//lastCall = newCall;
		SHOW_ASM("std_call "<<addr)
	}

	else if ((logue = InstructionPatterns::near_call(csr, hostPC, addr))
        != NULL) {
		/*
		 * Call with short displacement (16 bit instruction)
		 */
		HLCall* newCall = new HLCall(pc, 0, RTs);
		result.rtl = newCall;
		result.numBytes = hostPC - saveHostPC;

		// Set the destination expression
		newCall->setDest(addr - delta);
		newCall->setPrologue(logue);

		// Save RTL for the latest call
		//lastCall = newCall;
		SHOW_ASM("near_call " << addr)
	}

	else if ((logue = InstructionPatterns::pea_pea_add_rts(csr, hostPC, d32))
        != NULL) {
		/*
		 * pea E(pc) pea 4(pc) / addil #d32, (a7) / rts
         * Handle as a call
		 */
		HLCall* newCall = new HLCall(pc, 0, RTs);
		result.rtl = newCall;
		result.numBytes = hostPC - saveHostPC;

		// Set the destination expression. It's d32 past the address of the
        // d32 itself, which is pc+10
		newCall->setDest(pc + 10 + d32);
		newCall->setPrologue(logue);

		// Save RTL for the latest call
		//lastCall = newCall;
		SHOW_ASM("pea/pea/add/rts " << pc+10+d32)
	}

	else if ((logue = InstructionPatterns::pea_add_rts(csr, hostPC, d32))
        != NULL) {
		/*
		 * pea 4(pc) / addil #d32, (a7) / rts
         * Handle as a call followed by a return
		 */
		HLCall* newCall = new HLCall(pc, 0, RTs);
		result.rtl = newCall;
		result.numBytes = hostPC - saveHostPC;

		// Set the destination expression. It's d32 past the address of the
        // d32 itself, which is pc+6
		newCall->setDest(pc + 6 + d32);
		newCall->setPrologue(logue);

        // This call effectively is followed by a return
        newCall->setReturnAfterCall(true);

		// Save RTL for the latest call
		//lastCall = newCall;
		SHOW_ASM("pea/add/rts " << pc+6+d32)
	}

	else if ((logue = InstructionPatterns::trap_syscall(csr, hostPC, d16))
        != NULL) {
		/*
		 * trap / AXXX  (d16 set to the XXX)
         * Handle as a library call
		 */
		HLCall* newCall = new HLCall(pc, 0, RTs);
		result.rtl = newCall;
		result.numBytes = hostPC - saveHostPC;

		// Set the destination expression. For now, we put AAAAA000+d16 there
		newCall->setDest(0xAAAAA000 + d16);
		newCall->setPrologue(logue);

		SHOW_ASM("trap/syscall " << hex << 0xA000 + d16)
	}

/*
 * CALLEE PROLOGUES
 */
	else if ((logue = InstructionPatterns::link_save(csr, hostPC,
		locals, d16)) != NULL)
	{
		/*
		 * Standard link with save of registers using movem
		 */
		if (proc != NULL) {

			// Record the prologue of this callee
			assert(logue->getType() == Logue::CALLEE_PROLOGUE);
			proc->setPrologue((CalleePrologue*)logue);
		}
		result.rtl = new RTL(pc, RTs);
		result.numBytes = hostPC - saveHostPC;
		SHOW_ASM("link_save " << locals)
	}

	else if ((logue = InstructionPatterns::link_save1(csr, hostPC,
		locals, reg)) != NULL)
	{
		/*
		 * Standard link with save of 1 D register using move dn,-(a7)
		 */
		if (proc != NULL) {

			// Record the prologue of this callee
			assert(logue->getType() == Logue::CALLEE_PROLOGUE);
			proc->setPrologue((CalleePrologue*)logue);
		}
		result.rtl = new RTL(pc, RTs);
		result.numBytes = hostPC - saveHostPC;
		SHOW_ASM("link_save1 " << locals)
	}

	else if ((logue = InstructionPatterns::push_lea(csr, hostPC,
		locals, reg)) != NULL)
	{
		/*
		 * Just save of 1 D register using move dn,-(a7);
         * then an lea d16(a7), a7 to allocate the stack
		 */
        //locals = 0;             // No locals for this prologue
		if (proc != NULL) {

			// Record the prologue of this callee
			assert(logue->getType() == Logue::CALLEE_PROLOGUE);
			proc->setPrologue((CalleePrologue*)logue);
		}
		result.rtl = new RTL(pc, RTs);
		result.numBytes = hostPC - saveHostPC;
		SHOW_ASM("push_lea " << locals)
	}

	else if ((logue = InstructionPatterns::std_link(csr, hostPC,
		locals)) != NULL)
	{
		/*
		 * Standard link
		 */
		if (proc != NULL) {

			// Record the prologue of this callee
			assert(logue->getType() == Logue::CALLEE_PROLOGUE);
			proc->setPrologue((CalleePrologue*)logue);
		}
		result.rtl = new RTL(pc, RTs);
		result.numBytes = hostPC - saveHostPC;
		SHOW_ASM("std_link "<< locals)
	}

	else if ((logue = InstructionPatterns::bare_ret(csr, hostPC))
        != NULL)
	{
		/*
		 * Just a bare rts instruction
		 */
		if (proc != NULL) {

			// Record the prologue of this callee
			assert(logue->getType() == Logue::CALLEE_PROLOGUE);
			proc->setPrologue((CalleePrologue*)logue);
            proc->setEpilogue(new CalleeEpilogue("__dummy",list<string>()));
		}
		result.rtl = new HLReturn(pc, RTs);
		result.numBytes = hostPC - saveHostPC;
		SHOW_ASM("bare_ret")
	}

	else if ((logue = InstructionPatterns::std_ret(csr, hostPC)) != NULL) {
		/*
		 * An unlink and return
		 */
		if (proc!= NULL) {

			// Record the epilogue of this callee
			assert(logue->getType() == Logue::CALLEE_EPILOGUE);
			proc->setEpilogue((CalleeEpilogue*)logue);
		}

		result.rtl = new HLReturn(pc, RTs);
		result.numBytes = hostPC - saveHostPC;
		SHOW_ASM("std_ret")
	}

	else if ((logue = InstructionPatterns::rest_ret(csr, hostPC, d16)) != NULL)
    {
		/*
		 * A restore (movem stack to registers) then return
		 */
		if (proc!= NULL) {

			// Record the epilogue of this callee
			assert(logue->getType() == Logue::CALLEE_EPILOGUE);
			proc->setEpilogue((CalleeEpilogue*)logue);
		}

		result.rtl = new HLReturn(pc, RTs);
		result.numBytes = hostPC - saveHostPC;
		SHOW_ASM("rest_ret")
	}

	else if ((logue = InstructionPatterns::rest1_ret(csr, hostPC, reg)) != NULL)
    {
		/*
		 * A pop (move (a7)+ to one D register) then unlink and return
		 */
		if (proc!= NULL) {

			// Record the epilogue of this callee
			assert(logue->getType() == Logue::CALLEE_EPILOGUE);
			proc->setEpilogue((CalleeEpilogue*)logue);
		}

		result.rtl = new HLReturn(pc, RTs);
		result.numBytes = hostPC - saveHostPC;
		SHOW_ASM("rest1_ret")
	}

	else if ((logue = InstructionPatterns::pop_ret(csr, hostPC, reg)) != NULL)
    {
		/*
		 * A pop (move (a7)+ to one D register) then just return
		 */
		if (proc!= NULL) {

			// Record the epilogue of this callee
			assert(logue->getType() == Logue::CALLEE_EPILOGUE);
			proc->setEpilogue((CalleeEpilogue*)logue);
		}

		result.rtl = new HLReturn(pc, RTs);
		result.numBytes = hostPC - saveHostPC;
		SHOW_ASM("pop_ret")
	}

    else if ((logue = InstructionPatterns::clear_stack(csr, hostPC, stackSize))
        != NULL)
    {
        /*
         * Remove parameters from the stack
         */
        RTs = instantiate(pc, "clear_stack", dis_Num(stackSize));

        result.rtl = new RTL(pc, RTs);
        result.numBytes = hostPC - saveHostPC;
    }

	else {

		ADDRESS nextPC;
        int bump = 0, bumpr;
        SemStr* ss;



#line 339 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 339 "machine/mc68k/decoder.m"
    hostPC
    ;
  char *MATCH_name;
  static char *MATCH_name_cond_12[] = {
    "bra", (char *)0, "bhi", "bls", "bcc", "bcs", "bne", "beq", "bvc", "bvs", 
    "bpl", "bmi", "bge", "blt", "bgt", "ble", 
  };
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    
      switch((MATCH_w_16_0 >> 12 & 0xf) /* op at 0 */) {
        case 0: case 1: case 2: case 3: case 7: case 8: case 9: case 10: 
        case 11: case 12: case 13: case 14: case 15: 
          goto MATCH_label_de0; break;
        case 4: 
          if ((MATCH_w_16_0 >> 3 & 0x7) /* adrm at 0 */ == 2) 
            if ((MATCH_w_16_0 >> 9 & 0x7) /* reg1 at 0 */ == 7) 
              if ((MATCH_w_16_0 >> 8 & 0x1) /* sb at 0 */ == 1) 
                goto MATCH_label_de0;  /*opt-block+*/
              else 
                
                  switch((MATCH_w_16_0 >> 6 & 0x3) /* sz at 0 */) {
                    case 0: case 1: 
                      goto MATCH_label_de0; break;
                    case 2: 
                      { 
                        unsigned ea = addressToPC(MATCH_p);
                        nextPC = 2 + MATCH_p; 
                        
#line 342 "machine/mc68k/decoder.m"
                        

                        			/*

                        			 * Register call

                        			 */

                        			// Mike: there should probably be a HLNwayCall class for this!

                        			HLCall* newCall = new HLCall(pc, 0, RTs);

                        			// Record the fact that this is a computed call

                        			newCall->setIsComputed();

                        			// Set the destination expression

                        			newCall->setDest(cEA(ea, pc, 32));

                        			result.rtl = newCall;

                        			// Only one instruction, so size of result is size of this decode

                        			result.numBytes = nextPC - hostPC;

                        	

                        
                        
                        
                      }
                      
                      break;
                    case 3: 
                      { 
                        unsigned ea = addressToPC(MATCH_p);
                        nextPC = 2 + MATCH_p; 
                        
#line 356 "machine/mc68k/decoder.m"
                        

                        			/*

                        			 * Register jump

                        			 */

                        			HLNwayJump* newJump = new HLNwayJump(pc, RTs);

                        			// Record the fact that this is a computed call

                        			newJump->setIsComputed();

                        			// Set the destination expression

                        			newJump->setDest(cEA(ea, pc, 32));

                        			result.rtl = newJump;

                        			// Only one instruction, so size of result is size of this decode

                        			result.numBytes = nextPC - hostPC;

                        		

                        		/*

                        		 * Unconditional branches

                        		 */

                        
                        
                        
                      }
                      
                      break;
                    default: assert(0);
                  } /* (MATCH_w_16_0 >> 6 & 0x3) -- sz at 0 --*/   
            else 
              goto MATCH_label_de0;  /*opt-block+*/ 
          else 
            goto MATCH_label_de0;  /*opt-block+*/
          break;
        case 5: 
          
            switch((MATCH_w_16_0 >> 8 & 0xf) /* cond at 0 */) {
              case 0: case 1: case 8: case 9: 
                goto MATCH_label_de0; break;
              case 2: 
                if (((MATCH_w_16_0 >> 3 & 0x7) /* adrm at 0 */ == 0 || 
                  2 <= (MATCH_w_16_0 >> 3 & 0x7) /* adrm at 0 */ && 
                  (MATCH_w_16_0 >> 3 & 0x7) /* adrm at 0 */ < 5) && 
                  (MATCH_w_16_0 >> 6 & 0x3) /* sz at 0 */ == 3) { 
                  MATCH_name = "shi"; 
                  { 
                    char *name = MATCH_name;
                    unsigned ea = addressToPC(MATCH_p);
                    nextPC = 2 + MATCH_p; 
                    
#line 427 "machine/mc68k/decoder.m"
                    

                                ss = daEA(ea, pc, bump, bumpr, 1);

                    			RTs = instantiate(pc, name, ss);

                    			SETS(name, ss, HLJCOND_JSG)

                    
                    
                    
                  }
                  
                } /*opt-block*/
                else 
                  goto MATCH_label_de0;  /*opt-block+*/
                
                break;
              case 3: 
                if (((MATCH_w_16_0 >> 3 & 0x7) /* adrm at 0 */ == 0 || 
                  2 <= (MATCH_w_16_0 >> 3 & 0x7) /* adrm at 0 */ && 
                  (MATCH_w_16_0 >> 3 & 0x7) /* adrm at 0 */ < 5) && 
                  (MATCH_w_16_0 >> 6 & 0x3) /* sz at 0 */ == 3) { 
                  MATCH_name = "sls"; 
                  { 
                    char *name = MATCH_name;
                    unsigned ea = addressToPC(MATCH_p);
                    nextPC = 2 + MATCH_p; 
                    
#line 431 "machine/mc68k/decoder.m"
                    

                                ss = daEA(ea, pc, bump, bumpr, 1);

                    			RTs = instantiate(pc, name, ss);

                    			SETS(name, ss, HLJCOND_JULE)

                    
                    
                    
                  }
                  
                } /*opt-block*/
                else 
                  goto MATCH_label_de0;  /*opt-block+*/
                
                break;
              case 4: 
                if (((MATCH_w_16_0 >> 3 & 0x7) /* adrm at 0 */ == 0 || 
                  2 <= (MATCH_w_16_0 >> 3 & 0x7) /* adrm at 0 */ && 
                  (MATCH_w_16_0 >> 3 & 0x7) /* adrm at 0 */ < 5) && 
                  (MATCH_w_16_0 >> 6 & 0x3) /* sz at 0 */ == 3) { 
                  MATCH_name = "scc"; 
                  { 
                    char *name = MATCH_name;
                    unsigned ea = addressToPC(MATCH_p);
                    nextPC = 2 + MATCH_p; 
                    
#line 435 "machine/mc68k/decoder.m"
                    

                                ss = daEA(ea, pc, bump, bumpr, 1);

                    			RTs = instantiate(pc, name, ss);

                    			SETS(name, ss, HLJCOND_JUGE)

                    
                    
                    
                  }
                  
                } /*opt-block*/
                else 
                  goto MATCH_label_de0;  /*opt-block+*/
                
                break;
              case 5: 
                if (((MATCH_w_16_0 >> 3 & 0x7) /* adrm at 0 */ == 0 || 
                  2 <= (MATCH_w_16_0 >> 3 & 0x7) /* adrm at 0 */ && 
                  (MATCH_w_16_0 >> 3 & 0x7) /* adrm at 0 */ < 5) && 
                  (MATCH_w_16_0 >> 6 & 0x3) /* sz at 0 */ == 3) { 
                  MATCH_name = "scs"; 
                  { 
                    char *name = MATCH_name;
                    unsigned ea = addressToPC(MATCH_p);
                    nextPC = 2 + MATCH_p; 
                    
#line 439 "machine/mc68k/decoder.m"
                    

                                ss = daEA(ea, pc, bump, bumpr, 1);

                    			RTs = instantiate(pc, name, ss);

                    			SETS(name, ss, HLJCOND_JUL)

                    
                    
                    
                  }
                  
                } /*opt-block*/
                else 
                  goto MATCH_label_de0;  /*opt-block+*/
                
                break;
              case 6: 
                if (((MATCH_w_16_0 >> 3 & 0x7) /* adrm at 0 */ == 0 || 
                  2 <= (MATCH_w_16_0 >> 3 & 0x7) /* adrm at 0 */ && 
                  (MATCH_w_16_0 >> 3 & 0x7) /* adrm at 0 */ < 5) && 
                  (MATCH_w_16_0 >> 6 & 0x3) /* sz at 0 */ == 3) { 
                  MATCH_name = "sne"; 
                  { 
                    char *name = MATCH_name;
                    unsigned ea = addressToPC(MATCH_p);
                    nextPC = 2 + MATCH_p; 
                    
#line 443 "machine/mc68k/decoder.m"
                    

                                ss = daEA(ea, pc, bump, bumpr, 1);

                    			RTs = instantiate(pc, name, ss);

                    			SETS(name, ss, HLJCOND_JNE)

                    
                    
                    
                  }
                  
                } /*opt-block*/
                else 
                  goto MATCH_label_de0;  /*opt-block+*/
                
                break;
              case 7: 
                if (((MATCH_w_16_0 >> 3 & 0x7) /* adrm at 0 */ == 0 || 
                  2 <= (MATCH_w_16_0 >> 3 & 0x7) /* adrm at 0 */ && 
                  (MATCH_w_16_0 >> 3 & 0x7) /* adrm at 0 */ < 5) && 
                  (MATCH_w_16_0 >> 6 & 0x3) /* sz at 0 */ == 3) { 
                  MATCH_name = "seq"; 
                  { 
                    char *name = MATCH_name;
                    unsigned ea = addressToPC(MATCH_p);
                    nextPC = 2 + MATCH_p; 
                    
#line 447 "machine/mc68k/decoder.m"
                    

                                ss = daEA(ea, pc, bump, bumpr, 1);

                    			RTs = instantiate(pc, name, ss);

                    			SETS(name, ss, HLJCOND_JE)

                    		//| svc(ea) [name] =>

                            //  ss = daEA(ea, pc, bump, bumpr, 1);

                    		//	RTs = instantiate(pc, name, ss);

                    		//	SETS(name, ss, HLJCOND_)

                    		//| svs(ea) [name] =>

                            //  ss = daEA(ea, pc, bump, bumpr, 1);

                    		//	RTs = instantiate(pc, name, ss);

                    		//	SETS(name, ss, HLJCOND_)

                    
                    
                    
                  }
                  
                } /*opt-block*/
                else 
                  goto MATCH_label_de0;  /*opt-block+*/
                
                break;
              case 10: 
                if (((MATCH_w_16_0 >> 3 & 0x7) /* adrm at 0 */ == 0 || 
                  2 <= (MATCH_w_16_0 >> 3 & 0x7) /* adrm at 0 */ && 
                  (MATCH_w_16_0 >> 3 & 0x7) /* adrm at 0 */ < 5) && 
                  (MATCH_w_16_0 >> 6 & 0x3) /* sz at 0 */ == 3) { 
                  MATCH_name = "spl"; 
                  { 
                    char *name = MATCH_name;
                    unsigned ea = addressToPC(MATCH_p);
                    nextPC = 2 + MATCH_p; 
                    
#line 459 "machine/mc68k/decoder.m"
                    

                                ss = daEA(ea, pc, bump, bumpr, 1);

                    			RTs = instantiate(pc, name, ss);

                    			SETS(name, ss, HLJCOND_JPOS)

                    
                    
                    
                  }
                  
                } /*opt-block*/
                else 
                  goto MATCH_label_de0;  /*opt-block+*/
                
                break;
              case 11: 
                if (((MATCH_w_16_0 >> 3 & 0x7) /* adrm at 0 */ == 0 || 
                  2 <= (MATCH_w_16_0 >> 3 & 0x7) /* adrm at 0 */ && 
                  (MATCH_w_16_0 >> 3 & 0x7) /* adrm at 0 */ < 5) && 
                  (MATCH_w_16_0 >> 6 & 0x3) /* sz at 0 */ == 3) { 
                  MATCH_name = "smi"; 
                  { 
                    char *name = MATCH_name;
                    unsigned ea = addressToPC(MATCH_p);
                    nextPC = 2 + MATCH_p; 
                    
#line 463 "machine/mc68k/decoder.m"
                    

                                ss = daEA(ea, pc, bump, bumpr, 1);

                    			RTs = instantiate(pc, name, ss);

                    			SETS(name, ss, HLJCOND_JMI)

                    
                    
                    
                  }
                  
                } /*opt-block*/
                else 
                  goto MATCH_label_de0;  /*opt-block+*/
                
                break;
              case 12: 
                if (((MATCH_w_16_0 >> 3 & 0x7) /* adrm at 0 */ == 0 || 
                  2 <= (MATCH_w_16_0 >> 3 & 0x7) /* adrm at 0 */ && 
                  (MATCH_w_16_0 >> 3 & 0x7) /* adrm at 0 */ < 5) && 
                  (MATCH_w_16_0 >> 6 & 0x3) /* sz at 0 */ == 3) { 
                  MATCH_name = "sge"; 
                  { 
                    char *name = MATCH_name;
                    unsigned ea = addressToPC(MATCH_p);
                    nextPC = 2 + MATCH_p; 
                    
#line 467 "machine/mc68k/decoder.m"
                    

                                ss = daEA(ea, pc, bump, bumpr, 1);

                    			RTs = instantiate(pc, name, ss);

                    			SETS(name, ss, HLJCOND_JSGE)

                    
                    
                    
                  }
                  
                } /*opt-block*/
                else 
                  goto MATCH_label_de0;  /*opt-block+*/
                
                break;
              case 13: 
                if (((MATCH_w_16_0 >> 3 & 0x7) /* adrm at 0 */ == 0 || 
                  2 <= (MATCH_w_16_0 >> 3 & 0x7) /* adrm at 0 */ && 
                  (MATCH_w_16_0 >> 3 & 0x7) /* adrm at 0 */ < 5) && 
                  (MATCH_w_16_0 >> 6 & 0x3) /* sz at 0 */ == 3) { 
                  MATCH_name = "slt"; 
                  { 
                    char *name = MATCH_name;
                    unsigned ea = addressToPC(MATCH_p);
                    nextPC = 2 + MATCH_p; 
                    
#line 471 "machine/mc68k/decoder.m"
                    

                                ss = daEA(ea, pc, bump, bumpr, 1);

                    			RTs = instantiate(pc, name, ss);

                    			SETS(name, ss, HLJCOND_JSL)

                    
                    
                    
                  }
                  
                } /*opt-block*/
                else 
                  goto MATCH_label_de0;  /*opt-block+*/
                
                break;
              case 14: 
                if (((MATCH_w_16_0 >> 3 & 0x7) /* adrm at 0 */ == 0 || 
                  2 <= (MATCH_w_16_0 >> 3 & 0x7) /* adrm at 0 */ && 
                  (MATCH_w_16_0 >> 3 & 0x7) /* adrm at 0 */ < 5) && 
                  (MATCH_w_16_0 >> 6 & 0x3) /* sz at 0 */ == 3) { 
                  MATCH_name = "sgt"; 
                  { 
                    char *name = MATCH_name;
                    unsigned ea = addressToPC(MATCH_p);
                    nextPC = 2 + MATCH_p; 
                    
#line 475 "machine/mc68k/decoder.m"
                    

                                ss = daEA(ea, pc, bump, bumpr, 1);

                    			RTs = instantiate(pc, name, ss);

                    			SETS(name, ss, HLJCOND_JSG)

                    
                    
                    
                  }
                  
                } /*opt-block*/
                else 
                  goto MATCH_label_de0;  /*opt-block+*/
                
                break;
              case 15: 
                if (((MATCH_w_16_0 >> 3 & 0x7) /* adrm at 0 */ == 0 || 
                  2 <= (MATCH_w_16_0 >> 3 & 0x7) /* adrm at 0 */ && 
                  (MATCH_w_16_0 >> 3 & 0x7) /* adrm at 0 */ < 5) && 
                  (MATCH_w_16_0 >> 6 & 0x3) /* sz at 0 */ == 3) { 
                  MATCH_name = "sle"; 
                  { 
                    char *name = MATCH_name;
                    unsigned ea = addressToPC(MATCH_p);
                    nextPC = 2 + MATCH_p; 
                    
#line 479 "machine/mc68k/decoder.m"
                    

                                ss = daEA(ea, pc, bump, bumpr, 1);

                    			RTs = instantiate(pc, name, ss);

                    			SETS(name, ss, HLJCOND_JSLE)

                    // HACK: Still need to do .ex versions of set, jsr, jmp

                    	

                    
                    
                    
                  }
                  
                } /*opt-block*/
                else 
                  goto MATCH_label_de0;  /*opt-block+*/
                
                break;
              default: assert(0);
            } /* (MATCH_w_16_0 >> 8 & 0xf) -- cond at 0 --*/ 
          break;
        case 6: 
          
            switch((MATCH_w_16_0 >> 8 & 0xf) /* cond at 0 */) {
              case 0: 
                MATCH_name = MATCH_name_cond_12[(MATCH_w_16_0 >> 8 & 0xf) 
                      /* cond at 0 */]; 
                { 
                  char *name = MATCH_name;
                  unsigned d = addressToPC(MATCH_p);
                  nextPC = 2 + MATCH_p; 
                  
#line 372 "machine/mc68k/decoder.m"
                  

                  			ss = BTA(d, result, pc);

                              UNCOND_JUMP(name, nextPC - hostPC, ss);

                  	

                  		/*

                  		 * Conditional branches

                  		 */

                  
                  
                  
                }
                
                break;
              case 1: 
                goto MATCH_label_de0; break;
              case 2: 
                MATCH_name = MATCH_name_cond_12[(MATCH_w_16_0 >> 8 & 0xf) 
                      /* cond at 0 */]; 
                { 
                  char *name = MATCH_name;
                  unsigned d = addressToPC(MATCH_p);
                  nextPC = 2 + MATCH_p; 
                  
#line 397 "machine/mc68k/decoder.m"
                  

                              ss = BTA(d, result, pc);

                  			COND_JUMP(name, nextPC - hostPC, ss, HLJCOND_JUG)

                  
                  
                  
                }
                
                break;
              case 3: 
                MATCH_name = MATCH_name_cond_12[(MATCH_w_16_0 >> 8 & 0xf) 
                      /* cond at 0 */]; 
                { 
                  char *name = MATCH_name;
                  unsigned d = addressToPC(MATCH_p);
                  nextPC = 2 + MATCH_p; 
                  
#line 400 "machine/mc68k/decoder.m"
                  

                              ss = BTA(d, result, pc);

                  			COND_JUMP(name, nextPC - hostPC, ss, HLJCOND_JULE)

                  
                  
                  
                }
                
                break;
              case 4: 
                MATCH_name = MATCH_name_cond_12[(MATCH_w_16_0 >> 8 & 0xf) 
                      /* cond at 0 */]; 
                { 
                  char *name = MATCH_name;
                  unsigned d = addressToPC(MATCH_p);
                  nextPC = 2 + MATCH_p; 
                  
#line 409 "machine/mc68k/decoder.m"
                  

                              ss = BTA(d, result, pc);

                  			COND_JUMP(name, nextPC - hostPC, ss, HLJCOND_JUGE)

                  
                  
                  
                }
                
                break;
              case 5: 
                MATCH_name = MATCH_name_cond_12[(MATCH_w_16_0 >> 8 & 0xf) 
                      /* cond at 0 */]; 
                { 
                  char *name = MATCH_name;
                  unsigned d = addressToPC(MATCH_p);
                  nextPC = 2 + MATCH_p; 
                  
#line 412 "machine/mc68k/decoder.m"
                  

                              ss = BTA(d, result, pc);

                  			COND_JUMP(name, nextPC - hostPC, ss, HLJCOND_JUL)

                  
                  
                  
                }
                
                break;
              case 6: 
                MATCH_name = MATCH_name_cond_12[(MATCH_w_16_0 >> 8 & 0xf) 
                      /* cond at 0 */]; 
                { 
                  char *name = MATCH_name;
                  unsigned d = addressToPC(MATCH_p);
                  nextPC = 2 + MATCH_p; 
                  
#line 403 "machine/mc68k/decoder.m"
                  

                              ss = BTA(d, result, pc);

                  			COND_JUMP(name, nextPC - hostPC, ss, HLJCOND_JNE)

                  
                  
                  
                }
                
                break;
              case 7: 
                MATCH_name = MATCH_name_cond_12[(MATCH_w_16_0 >> 8 & 0xf) 
                      /* cond at 0 */]; 
                { 
                  char *name = MATCH_name;
                  unsigned d = addressToPC(MATCH_p);
                  nextPC = 2 + MATCH_p; 
                  
#line 406 "machine/mc68k/decoder.m"
                  

                              ss = BTA(d, result, pc);

                  			COND_JUMP(name, nextPC - hostPC, ss, HLJCOND_JE)

                  
                  
                  
                }
                
                break;
              case 8: 
                MATCH_name = MATCH_name_cond_12[(MATCH_w_16_0 >> 8 & 0xf) 
                      /* cond at 0 */]; 
                { 
                  char *name = MATCH_name;
                  unsigned d = addressToPC(MATCH_p);
                  nextPC = 2 + MATCH_p; 
                  
#line 415 "machine/mc68k/decoder.m"
                  

                              ss = BTA(d, result, pc);

                  			COND_JUMP(name, nextPC - hostPC, ss, (JCOND_TYPE)0)

                  
                  
                  
                }
                
                break;
              case 9: 
                MATCH_name = MATCH_name_cond_12[(MATCH_w_16_0 >> 8 & 0xf) 
                      /* cond at 0 */]; 
                { 
                  char *name = MATCH_name;
                  unsigned d = addressToPC(MATCH_p);
                  nextPC = 2 + MATCH_p; 
                  
#line 418 "machine/mc68k/decoder.m"
                  

                              ss = BTA(d, result, pc);

                  			COND_JUMP(name, nextPC - hostPC, ss, (JCOND_TYPE)0)

                  	

                  	

                          // MVE: I'm assuming that we won't ever see shi(-(a7)) or the like.

                          // This would unbalance the stack, although it would be legal for

                          // address registers other than a7. For now, we ignore the possibility

                          // of having to bump a register

                  
                  
                  
                }
                
                break;
              case 10: 
                MATCH_name = MATCH_name_cond_12[(MATCH_w_16_0 >> 8 & 0xf) 
                      /* cond at 0 */]; 
                { 
                  char *name = MATCH_name;
                  unsigned d = addressToPC(MATCH_p);
                  nextPC = 2 + MATCH_p; 
                  
#line 391 "machine/mc68k/decoder.m"
                  

                              ss = BTA(d, result, pc);

                  			COND_JUMP(name, nextPC - hostPC, ss, HLJCOND_JPOS)

                  
                  
                  
                }
                
                break;
              case 11: 
                MATCH_name = MATCH_name_cond_12[(MATCH_w_16_0 >> 8 & 0xf) 
                      /* cond at 0 */]; 
                { 
                  char *name = MATCH_name;
                  unsigned d = addressToPC(MATCH_p);
                  nextPC = 2 + MATCH_p; 
                  
#line 394 "machine/mc68k/decoder.m"
                  

                              ss = BTA(d, result, pc);

                  			COND_JUMP(name, nextPC - hostPC, ss, HLJCOND_JMI)

                  
                  
                  
                }
                
                break;
              case 12: 
                MATCH_name = MATCH_name_cond_12[(MATCH_w_16_0 >> 8 & 0xf) 
                      /* cond at 0 */]; 
                { 
                  char *name = MATCH_name;
                  unsigned d = addressToPC(MATCH_p);
                  nextPC = 2 + MATCH_p; 
                  
#line 385 "machine/mc68k/decoder.m"
                  

                              ss = BTA(d, result, pc);

                  			COND_JUMP(name, nextPC - hostPC, ss, HLJCOND_JSGE)

                  
                  
                  
                }
                
                break;
              case 13: 
                MATCH_name = MATCH_name_cond_12[(MATCH_w_16_0 >> 8 & 0xf) 
                      /* cond at 0 */]; 
                { 
                  char *name = MATCH_name;
                  unsigned d = addressToPC(MATCH_p);
                  nextPC = 2 + MATCH_p; 
                  
#line 388 "machine/mc68k/decoder.m"
                  

                              ss = BTA(d, result, pc);

                  			COND_JUMP(name, nextPC - hostPC, ss, HLJCOND_JSL)

                  
                  
                  
                }
                
                break;
              case 14: 
                MATCH_name = MATCH_name_cond_12[(MATCH_w_16_0 >> 8 & 0xf) 
                      /* cond at 0 */]; 
                { 
                  char *name = MATCH_name;
                  unsigned d = addressToPC(MATCH_p);
                  nextPC = 2 + MATCH_p; 
                  
#line 379 "machine/mc68k/decoder.m"
                  

                              ss = BTA(d, result, pc);

                  			COND_JUMP(name, nextPC - hostPC, ss, HLJCOND_JSG)

                  
                  
                  
                }
                
                break;
              case 15: 
                MATCH_name = MATCH_name_cond_12[(MATCH_w_16_0 >> 8 & 0xf) 
                      /* cond at 0 */]; 
                { 
                  char *name = MATCH_name;
                  unsigned d = addressToPC(MATCH_p);
                  nextPC = 2 + MATCH_p; 
                  
#line 382 "machine/mc68k/decoder.m"
                  

                              ss = BTA(d, result, pc);

                  			COND_JUMP(name, nextPC - hostPC, ss, HLJCOND_JSLE)

                  
                  
                  
                }
                
                break;
              default: assert(0);
            } /* (MATCH_w_16_0 >> 8 & 0xf) -- cond at 0 --*/ 
          break;
        default: assert(0);
      } /* (MATCH_w_16_0 >> 12 & 0xf) -- op at 0 --*/ 
    
  }goto MATCH_finished_de; 
  
  MATCH_label_de0: (void)0; /*placeholder for label*/ 
    { 
      nextPC = MATCH_p; 
      
#line 485 "machine/mc68k/decoder.m"
      
      			result.rtl = new RTL(pc,

      				decodeLowLevelInstruction(hostPC, pc, result));

      
      
      
    } 
    goto MATCH_finished_de; 
    
  MATCH_finished_de: (void)0; /*placeholder for label*/
  
}

#line 489 "machine/mc68k/decoder.m"
	}
	return result;
}

/*==============================================================================
 * These are machine specific functions used to decode instruction
 * operands into SemStrs.
 *============================================================================*/

// Branch target
SemStr* NJMCDecoder::BTA(ADDRESS d, DecodeResult& result, ADDRESS pc)
{

  SemStr* ret = new SemStr(32);
  ret->push(idIntConst);



#line 504 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 504 "machine/mc68k/decoder.m"
    d
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  unsigned /* [0..65535] */ MATCH_w_16_16;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    if ((MATCH_w_16_0 & 0xff) /* data8 at 0 */ == 0) { 
      MATCH_w_16_16 = getWord(2 + MATCH_p); 
      { 
        int /* [~32768..32767] */ dsp16 = 
          sign_extend((MATCH_w_16_16 & 0xffff) /* d16 at 16 */, 16);
        
#line 507 "machine/mc68k/decoder.m"
         {

                        ret->push(pc+2 + dsp16);

                        result.numBytes += 2;

                    }

        

        
        
        
      }
      
    } /*opt-block*/
    else { 
      unsigned dsp8 = (MATCH_w_16_0 & 0xff) /* data8 at 0 */;
      
#line 512 "machine/mc68k/decoder.m"
        {

                      // Casts needed to work around MLTK bug

                      ret->push(pc+2 + (int)(char)dsp8);

                  }

      
      
      
    } /*opt-block*//*opt-block+*/
    
  }goto MATCH_finished_ce; 
  
  MATCH_finished_ce: (void)0; /*placeholder for label*/
  
}

#line 517 "machine/mc68k/decoder.m"
    
  return ret;
}


void NJMCDecoder::pIllegalMode(ADDRESS pc)
{
    ostrstream ost;
    ost << "Illegal addressing mode at " << hex << pc;
    error(str(ost));
}

SemStr* NJMCDecoder::pDDirect(int r2, int size)
{
    SemStr* ret = new SemStr(size);
    ret->push(idRegOf);
    ret->push(idIntConst);
    ret->push(r2);
    return ret;
}

SemStr* NJMCDecoder::pADirect(int r2, int size)
{
    SemStr* ret = new SemStr(size);
    ret->push(idRegOf);
    ret->push(idIntConst);
    ret->push(r2 + 8);          // First A register is r[8]
    return ret;
}

SemStr* NJMCDecoder::pIndirect(int r2, int size)
{
    SemStr* ret = new SemStr(size);
    ret->push(idMemOf);
    ret->push(idRegOf);
    ret->push(idIntConst);
    ret->push(r2 + 8);          // First A register is r[8]
    return ret;
}

SemStr* NJMCDecoder::pPostInc(int r2, int& bump, int& bumpr, int size)
{
    // Treat this as (an), set bump to size, and bumpr to r2+8
    // Note special semantics when r2 == 7 (stack pointer): if size == 1, then
    // the system will change it to 2 to keep the stack word aligned
    if ((r2 == 7) && (size == 8)) size = 16;
    SemStr* ret = new SemStr(size/8);
    ret->push(idMemOf);
    ret->push(idRegOf);
    ret->push(idIntConst);
    ret->push(r2 + 8);          // First A register is r[8]
    bump = size/8;              // Amount to bump register by
    bumpr = r2 + 8;             // Register to bump
    return ret;
}

SemStr* NJMCDecoder::pPreDec(int r2, int& bump, int& bumpr, int size)
{
    // We treat this as -size(an), set bump to -size, and bumpr to r2+8
    // Note special semantics when r2 == 7 (stack pointer): if size == 1, then
    // the system will change it to 2 to keep the stack word aligned
    if ((r2 == 7) && (size == 8)) size = 16;
    SemStr* ret = new SemStr(size);
    ret->push(idMemOf); ret->push(idPlus);
    ret->push(idRegOf);
    ret->push(idIntConst);
    ret->push(r2 + 8);          // First A register is r[8]
    ret->push(idIntConst); ret->push(-size/8);
    bump = -size/8;             // Amount to bump register by
    bumpr = r2 + 8;             // Register to bump
    return ret;
}

SemStr* NJMCDecoder::alEA(ADDRESS ea, ADDRESS pc, int& bump, int& bumpr,
    int size)
{
  SemStr* ret = new SemStr(size);


#line 593 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 593 "machine/mc68k/decoder.m"
    ea
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    
      switch((MATCH_w_16_0 >> 3 & 0x7) /* adrm at 0 */) {
        case 0: 
          { 
            unsigned reg2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 594 "machine/mc68k/decoder.m"
             ret = pDDirect(reg2, size);

            
            
            
          }
          
          break;
        case 1: 
          { 
            unsigned reg2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 595 "machine/mc68k/decoder.m"
             ret = pADirect(reg2, size);

            
            
            
          }
          
          break;
        case 2: 
          { 
            unsigned reg2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 596 "machine/mc68k/decoder.m"
             ret = pIndirect(reg2, size);

            
            
            
          }
          
          break;
        case 3: 
          { 
            unsigned reg2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 597 "machine/mc68k/decoder.m"
             ret = pPostInc(reg2, bump, bumpr, size);

            
            
            
          }
          
          break;
        case 4: 
          { 
            unsigned reg2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 598 "machine/mc68k/decoder.m"
             ret = pPreDec(reg2, bump, bumpr, size);

            
            
            
          }
          
          break;
        case 5: case 6: case 7: 
          
#line 599 "machine/mc68k/decoder.m"
          pIllegalMode(pc);

          
          
          
          break;
        default: assert(0);
      } /* (MATCH_w_16_0 >> 3 & 0x7) -- adrm at 0 --*/ 
    
  }goto MATCH_finished_be; 
  
  MATCH_finished_be: (void)0; /*placeholder for label*/
  
}

#line 602 "machine/mc68k/decoder.m"
  return ret;
}

SemStr* NJMCDecoder::amEA(ADDRESS ea, ADDRESS pc, int& bump, int& bumpr,
    int size)
{
  SemStr* ret = new SemStr(size);


#line 608 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 608 "machine/mc68k/decoder.m"
    ea
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    
      switch((MATCH_w_16_0 >> 3 & 0x7) /* adrm at 0 */) {
        case 0: 
          { 
            unsigned reg2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 609 "machine/mc68k/decoder.m"
             ret = pDDirect(reg2, size);

            
            
            
          }
          
          break;
        case 1: 
          { 
            unsigned reg2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 610 "machine/mc68k/decoder.m"
             ret = pADirect(reg2, size);

            
            
            
          }
          
          break;
        case 2: 
          { 
            unsigned reg2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 611 "machine/mc68k/decoder.m"
             ret = pIndirect(reg2, size);

            
            
            
          }
          
          break;
        case 3: 
          { 
            unsigned reg2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 612 "machine/mc68k/decoder.m"
             ret = pPostInc(reg2, bump, bumpr, size);

            
            
            
          }
          
          break;
        case 4: 
          { 
            unsigned reg2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 613 "machine/mc68k/decoder.m"
             ret = pPreDec(reg2, bump, bumpr, size);

            
            
            
          }
          
          break;
        case 5: case 6: case 7: 
          
#line 614 "machine/mc68k/decoder.m"
          pIllegalMode(pc);

          
          
          
          break;
        default: assert(0);
      } /* (MATCH_w_16_0 >> 3 & 0x7) -- adrm at 0 --*/ 
    
  }goto MATCH_finished_ae; 
  
  MATCH_finished_ae: (void)0; /*placeholder for label*/
  
}

#line 617 "machine/mc68k/decoder.m"
  return ret;
}

SemStr* NJMCDecoder::awlEA(ADDRESS ea, ADDRESS pc, int& bump, int& bumpr,
    int size)
{
  SemStr* ret = new SemStr(size);


#line 623 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 623 "machine/mc68k/decoder.m"
    ea
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    
      switch((MATCH_w_16_0 >> 3 & 0x7) /* adrm at 0 */) {
        case 0: 
          { 
            unsigned reg2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 624 "machine/mc68k/decoder.m"
             ret = pDDirect(reg2, size);

            
            
            
          }
          
          break;
        case 1: 
          { 
            unsigned reg2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 625 "machine/mc68k/decoder.m"
             ret = pADirect(reg2, size);

            
            
            
          }
          
          break;
        case 2: 
          { 
            unsigned reg2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 626 "machine/mc68k/decoder.m"
             ret = pIndirect(reg2, size);

            
            
            
          }
          
          break;
        case 3: 
          { 
            unsigned reg2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 627 "machine/mc68k/decoder.m"
             ret = pPostInc(reg2, bump, bumpr, size);

            
            
            
          }
          
          break;
        case 4: 
          { 
            unsigned reg2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 628 "machine/mc68k/decoder.m"
             ret = pPreDec(reg2, bump, bumpr, size);

            
            
            
          }
          
          break;
        case 5: case 6: case 7: 
          
#line 629 "machine/mc68k/decoder.m"
          pIllegalMode(pc);

          
          
          
          break;
        default: assert(0);
      } /* (MATCH_w_16_0 >> 3 & 0x7) -- adrm at 0 --*/ 
    
  }goto MATCH_finished_zd; 
  
  MATCH_finished_zd: (void)0; /*placeholder for label*/
  
}

#line 632 "machine/mc68k/decoder.m"
  return ret;
}

SemStr* NJMCDecoder::cEA(ADDRESS ea, ADDRESS pc, int size)
{
  SemStr* ret = new SemStr(size);


#line 637 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 637 "machine/mc68k/decoder.m"
    ea
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    if ((MATCH_w_16_0 >> 3 & 0x7) /* adrm at 0 */ == 2) { 
      unsigned reg2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
      
#line 638 "machine/mc68k/decoder.m"
       ret = pIndirect(reg2, size);

      
      
      
    } /*opt-block*//*opt-block+*/
    else 
      
#line 639 "machine/mc68k/decoder.m"
      pIllegalMode(pc);

      
       /*opt-block+*/
    
  }goto MATCH_finished_yd; 
  
  MATCH_finished_yd: (void)0; /*placeholder for label*/
  
}

#line 642 "machine/mc68k/decoder.m"
  return ret;
}

SemStr* NJMCDecoder::dEA(ADDRESS ea, ADDRESS pc, int& bump, int& bumpr,
    int size)
{
  SemStr* ret = new SemStr(size);


#line 648 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 648 "machine/mc68k/decoder.m"
    ea
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    
      switch((MATCH_w_16_0 >> 3 & 0x7) /* adrm at 0 */) {
        case 0: 
          { 
            unsigned reg2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 649 "machine/mc68k/decoder.m"
             ret = pDDirect(reg2, size);

            
            
            
          }
          
          break;
        case 1: case 5: case 6: case 7: 
          
#line 653 "machine/mc68k/decoder.m"
          pIllegalMode(pc);

          
          
          
          break;
        case 2: 
          { 
            unsigned reg2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 650 "machine/mc68k/decoder.m"
             ret = pIndirect(reg2, size);

            
            
            
          }
          
          break;
        case 3: 
          { 
            unsigned reg2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 651 "machine/mc68k/decoder.m"
             ret = pPostInc(reg2, bump, bumpr, size);

            
            
            
          }
          
          break;
        case 4: 
          { 
            unsigned reg2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 652 "machine/mc68k/decoder.m"
             ret = pPreDec(reg2, bump, bumpr, size);

            
            
            
          }
          
          break;
        default: assert(0);
      } /* (MATCH_w_16_0 >> 3 & 0x7) -- adrm at 0 --*/ 
    
  }goto MATCH_finished_xd; 
  
  MATCH_finished_xd: (void)0; /*placeholder for label*/
  
}

#line 656 "machine/mc68k/decoder.m"
  return ret;
}

SemStr* NJMCDecoder::daEA(ADDRESS ea, ADDRESS pc, int& bump, int& bumpr,
    int size)
{
  SemStr* ret = new SemStr(size);


#line 662 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 662 "machine/mc68k/decoder.m"
    ea
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    
      switch((MATCH_w_16_0 >> 3 & 0x7) /* adrm at 0 */) {
        case 0: 
          { 
            unsigned reg2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 663 "machine/mc68k/decoder.m"
             ret = pDDirect(reg2, size);

            
            
            
          }
          
          break;
        case 1: case 5: case 6: case 7: 
          
#line 667 "machine/mc68k/decoder.m"
          pIllegalMode(pc);

          
          
          
          break;
        case 2: 
          { 
            unsigned reg2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 664 "machine/mc68k/decoder.m"
             ret = pIndirect(reg2, size);

            
            
            
          }
          
          break;
        case 3: 
          { 
            unsigned reg2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 665 "machine/mc68k/decoder.m"
             ret = pPostInc(reg2, bump, bumpr, size);

            
            
            
          }
          
          break;
        case 4: 
          { 
            unsigned reg2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 666 "machine/mc68k/decoder.m"
             ret = pPreDec(reg2, bump, bumpr, size);

            
            
            
          }
          
          break;
        default: assert(0);
      } /* (MATCH_w_16_0 >> 3 & 0x7) -- adrm at 0 --*/ 
    
  }goto MATCH_finished_wd; 
  
  MATCH_finished_wd: (void)0; /*placeholder for label*/
  
}

#line 670 "machine/mc68k/decoder.m"
  return ret;
}

SemStr* NJMCDecoder::dBEA(ADDRESS ea, ADDRESS pc, int& bump, int& bumpr,
    int size)
{
  SemStr* ret = new SemStr(size);


#line 676 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 676 "machine/mc68k/decoder.m"
    ea
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    
      switch((MATCH_w_16_0 >> 3 & 0x7) /* adrm at 0 */) {
        case 0: 
          { 
            unsigned reg2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 677 "machine/mc68k/decoder.m"
             ret = pDDirect(reg2, size);

            
            
            
          }
          
          break;
        case 1: case 5: case 6: case 7: 
          
#line 681 "machine/mc68k/decoder.m"
          pIllegalMode(pc);

          
          
          
          break;
        case 2: 
          { 
            unsigned reg2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 678 "machine/mc68k/decoder.m"
             ret = pIndirect(reg2, size);

            
            
            
          }
          
          break;
        case 3: 
          { 
            unsigned reg2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 679 "machine/mc68k/decoder.m"
             ret = pPostInc(reg2, bump, bumpr, size);

            
            
            
          }
          
          break;
        case 4: 
          { 
            unsigned reg2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 680 "machine/mc68k/decoder.m"
             ret = pPreDec(reg2, bump, bumpr, size);

            
            
            
          }
          
          break;
        default: assert(0);
      } /* (MATCH_w_16_0 >> 3 & 0x7) -- adrm at 0 --*/ 
    
  }goto MATCH_finished_vd; 
  
  MATCH_finished_vd: (void)0; /*placeholder for label*/
  
}

#line 684 "machine/mc68k/decoder.m"
  return ret;
}

SemStr* NJMCDecoder::dWEA(ADDRESS ea, ADDRESS pc, int& bump, int& bumpr,
    int size)
{
  SemStr* ret = new SemStr(size);


#line 690 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 690 "machine/mc68k/decoder.m"
    ea
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    
      switch((MATCH_w_16_0 >> 3 & 0x7) /* adrm at 0 */) {
        case 0: 
          { 
            unsigned reg2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 691 "machine/mc68k/decoder.m"
             ret = pDDirect(reg2, size);

            
            
            
          }
          
          break;
        case 1: case 5: case 6: case 7: 
          
#line 695 "machine/mc68k/decoder.m"
          pIllegalMode(pc);

          
          
          
          break;
        case 2: 
          { 
            unsigned reg2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 692 "machine/mc68k/decoder.m"
             ret = pIndirect(reg2, size);

            
            
            
          }
          
          break;
        case 3: 
          { 
            unsigned reg2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 693 "machine/mc68k/decoder.m"
             ret = pPostInc(reg2, bump, bumpr, size);

            
            
            
          }
          
          break;
        case 4: 
          { 
            unsigned reg2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 694 "machine/mc68k/decoder.m"
             ret = pPreDec(reg2, bump, bumpr, size);

            
            
            
          }
          
          break;
        default: assert(0);
      } /* (MATCH_w_16_0 >> 3 & 0x7) -- adrm at 0 --*/ 
    
  }goto MATCH_finished_ud; 
  
  MATCH_finished_ud: (void)0; /*placeholder for label*/
  
}

#line 698 "machine/mc68k/decoder.m"
  return ret;
}

SemStr* NJMCDecoder::maEA(ADDRESS ea, ADDRESS pc, int& bump, int& bumpr,
    int size)
{
  SemStr* ret = new SemStr(size);


#line 704 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 704 "machine/mc68k/decoder.m"
    ea
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    
      switch((MATCH_w_16_0 >> 3 & 0x7) /* adrm at 0 */) {
        case 0: case 1: case 5: case 6: case 7: 
          
#line 708 "machine/mc68k/decoder.m"
          pIllegalMode(pc);

          
          
          
          break;
        case 2: 
          { 
            unsigned reg2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 705 "machine/mc68k/decoder.m"
             ret = pIndirect(reg2, size);

            
            
            
          }
          
          break;
        case 3: 
          { 
            unsigned reg2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 706 "machine/mc68k/decoder.m"
             ret = pPostInc(reg2, bump, bumpr, size);

            
            
            
          }
          
          break;
        case 4: 
          { 
            unsigned reg2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 707 "machine/mc68k/decoder.m"
             ret = pPreDec(reg2, bump, bumpr, size);

            
            
            
          }
          
          break;
        default: assert(0);
      } /* (MATCH_w_16_0 >> 3 & 0x7) -- adrm at 0 --*/ 
    
  }goto MATCH_finished_td; 
  
  MATCH_finished_td: (void)0; /*placeholder for label*/
  
}

#line 711 "machine/mc68k/decoder.m"
  return ret;
}

SemStr* NJMCDecoder::msEA(ADDRESS ea, ADDRESS pc, int& bump, int& bumpr,
    int size)
{
  SemStr* ret = new SemStr(size);


#line 717 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 717 "machine/mc68k/decoder.m"
    ea
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    
      switch((MATCH_w_16_0 >> 3 & 0x7) /* adrm at 0 */) {
        case 0: 
          { 
            unsigned reg2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 718 "machine/mc68k/decoder.m"
             ret = pDDirect(reg2, size);

            
            
            
          }
          
          break;
        case 1: 
          { 
            unsigned reg2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 719 "machine/mc68k/decoder.m"
             ret = pADirect(reg2, size);

            
            
            
          }
          
          break;
        case 2: 
          { 
            unsigned reg2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 720 "machine/mc68k/decoder.m"
             ret = pIndirect(reg2, size);

            
            
            
          }
          
          break;
        case 3: 
          { 
            unsigned reg2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 721 "machine/mc68k/decoder.m"
             ret = pPostInc(reg2, bump, bumpr, size);

            
            
            
          }
          
          break;
        case 4: 
          { 
            unsigned reg2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 722 "machine/mc68k/decoder.m"
             ret = pPreDec(reg2, bump, bumpr, size);

            
            
            
          }
          
          break;
        case 5: case 6: case 7: 
          
#line 723 "machine/mc68k/decoder.m"
          pIllegalMode(pc);

          
          
          
          break;
        default: assert(0);
      } /* (MATCH_w_16_0 >> 3 & 0x7) -- adrm at 0 --*/ 
    
  }goto MATCH_finished_sd; 
  
  MATCH_finished_sd: (void)0; /*placeholder for label*/
  
}

#line 726 "machine/mc68k/decoder.m"
  return ret;
}

SemStr* NJMCDecoder::mdEA(ADDRESS ea, ADDRESS pc, int& bump, int& bumpr,
    int size)
{
  SemStr* ret = new SemStr(size);


#line 732 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 732 "machine/mc68k/decoder.m"
    ea
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    
      switch((MATCH_w_16_0 >> 6 & 0x7) /* MDadrm at 0 */) {
        case 0: 
          { 
            unsigned reg1 = (MATCH_w_16_0 >> 9 & 0x7) /* reg1 at 0 */;
            
#line 733 "machine/mc68k/decoder.m"
             ret = pDDirect(reg1, size);

            
            
            
          }
          
          break;
        case 1: 
          { 
            unsigned reg1 = (MATCH_w_16_0 >> 9 & 0x7) /* reg1 at 0 */;
            
#line 734 "machine/mc68k/decoder.m"
             ret = pADirect(reg1, size);

            
            
            
          }
          
          break;
        case 2: 
          { 
            unsigned reg1 = (MATCH_w_16_0 >> 9 & 0x7) /* reg1 at 0 */;
            
#line 735 "machine/mc68k/decoder.m"
             ret = pIndirect(reg1, size);

            
            
            
          }
          
          break;
        case 3: 
          { 
            unsigned reg1 = (MATCH_w_16_0 >> 9 & 0x7) /* reg1 at 0 */;
            
#line 736 "machine/mc68k/decoder.m"
             ret = pPostInc(reg1, bump, bumpr, size);

            
            
            
          }
          
          break;
        case 4: 
          { 
            unsigned reg1 = (MATCH_w_16_0 >> 9 & 0x7) /* reg1 at 0 */;
            
#line 737 "machine/mc68k/decoder.m"
             ret = pPreDec(reg1, bump, bumpr, size);

            
            
            
          }
          
          break;
        case 5: case 6: case 7: 
          
#line 738 "machine/mc68k/decoder.m"
          pIllegalMode(pc);

          
          
          
          break;
        default: assert(0);
      } /* (MATCH_w_16_0 >> 6 & 0x7) -- MDadrm at 0 --*/ 
    
  }goto MATCH_finished_rd; 
  
  MATCH_finished_rd: (void)0; /*placeholder for label*/
  
}

#line 741 "machine/mc68k/decoder.m"
  return ret;
}

SemStr* NJMCDecoder::mrEA(ADDRESS ea, ADDRESS pc, int& bump, int& bumpr,
    int size)
{
  SemStr* ret = new SemStr(size);


#line 747 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 747 "machine/mc68k/decoder.m"
    ea
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    
      switch((MATCH_w_16_0 >> 3 & 0x7) /* adrm at 0 */) {
        case 0: case 1: case 4: case 5: case 6: case 7: 
          
#line 750 "machine/mc68k/decoder.m"
          pIllegalMode(pc);

          
          
          
          break;
        case 2: 
          { 
            unsigned reg2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 748 "machine/mc68k/decoder.m"
             ret = pIndirect(reg2, size);

            
            
            
          }
          
          break;
        case 3: 
          { 
            unsigned reg2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 749 "machine/mc68k/decoder.m"
             ret = pPostInc(reg2, bump, bumpr, size);

            
            
            
          }
          
          break;
        default: assert(0);
      } /* (MATCH_w_16_0 >> 3 & 0x7) -- adrm at 0 --*/ 
    
  }goto MATCH_finished_qd; 
  
  MATCH_finished_qd: (void)0; /*placeholder for label*/
  
}

#line 753 "machine/mc68k/decoder.m"
  return ret;
}

SemStr* NJMCDecoder::rmEA(ADDRESS ea, ADDRESS pc, int& bump, int& bumpr,
    int size)
{
  SemStr* ret = new SemStr(size);


#line 759 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 759 "machine/mc68k/decoder.m"
    ea
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    
      switch((MATCH_w_16_0 >> 3 & 0x7) /* adrm at 0 */) {
        case 0: case 1: case 3: case 5: case 6: case 7: 
          
#line 762 "machine/mc68k/decoder.m"
          pIllegalMode(pc);

          
          
          
          break;
        case 2: 
          { 
            unsigned reg2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 760 "machine/mc68k/decoder.m"
             ret = pIndirect(reg2, size);

            
            
            
          }
          
          break;
        case 4: 
          { 
            unsigned reg2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 761 "machine/mc68k/decoder.m"
             ret = pPreDec(reg2, bump, bumpr, size);

            
            
            
          }
          
          break;
        default: assert(0);
      } /* (MATCH_w_16_0 >> 3 & 0x7) -- adrm at 0 --*/ 
    
  }goto MATCH_finished_pd; 
  
  MATCH_finished_pd: (void)0; /*placeholder for label*/
  
}

#line 765 "machine/mc68k/decoder.m"
  return ret;
}


SemStr* NJMCDecoder::pADisp(int d16, int r, int size)
{
  SemStr* ret = new SemStr(size);
  // d16(Ar) -> m[ + r[ int r+8 ] int d16]
  ret->push(idMemOf); ret->push(idPlus); ret->push(idRegOf);
  ret->push(idIntConst); ret->push(r+8);
  ret->push(idIntConst); ret->push(d16);
  return ret;
}

SemStr* NJMCDecoder::pAIndex(int d8, int r, int iT, int iR, int iS, int size)
{
    SemStr* ret = new SemStr(size);
    // d8(Ar, A/Di.[wl] ->
    //   m[ + + r[ int r+8 ] ! size[ 16/32 r[ int iT<<3+iR ]] int i8 ]
    ret->push(idMemOf); ret->push(idPlus); ret->push(idPlus);
    ret->push(idRegOf); ret->push(idIntConst); ret->push(r+8);
    ret->push(idSignExt);
    ret->push(idSize);  ret->push(iS == 0 ? 16 : 32);
    ret->push(idRegOf); ret->push(idIntConst); ret->push((iT<<3) + iR);
    ret->push(idIntConst); ret->push(d8);
    return ret;
}

SemStr* NJMCDecoder::pPcDisp(ADDRESS label, int delta, int size)
{
    // Note: label is in the host address space, so need to subtract delta
    SemStr* ret = new SemStr(size);
    // d16(pc) -> m[ code pc+d16 ]
    // Note: we use "code" instead of "int" to flag the fact that this address
    // is relative to the pc, and hence needs translation before use in the
    // target machine
    ret->push(idMemOf); ret->push(idCodeAddr);
    ret->push(label - delta);
    return ret;
}

SemStr* NJMCDecoder::pPcIndex(int d8, int iT, int iR, int iS, ADDRESS nextPc, int size)
{
    // Note: nextPc is expected to have +2 or +4 etc already added to it!
    SemStr* ret = new SemStr(size);
    // d8(pc, A/Di.[wl] ->
    //   m[ + pc+i8 ! size[ 16/32 r[ int iT<<3+iR ]]]
    ret->push(idMemOf); ret->push(idPlus);
    ret->push(idIntConst); ret->push(nextPc+d8);
    ret->push(idSignExt);
    ret->push(idSize);  ret->push(iS == 0 ? 16 : 32);
    ret->push(idRegOf); ret->push(idIntConst); ret->push((iT<<3) + iR);
    return ret;
}

SemStr* NJMCDecoder::pAbsW(int d16, int size)
{
  // (d16).w  ->  size[ ss m[ int d16 ]]
  // Note: d16 should already have been sign extended to 32 bits
  SemStr* ret = new SemStr(size);
  ret->push(idSize); ret->push(size);
  ret->push(idMemOf); ret->push(idIntConst); ret->push(d16);
  return ret;
}

SemStr* NJMCDecoder::pAbsL(int d32, int size)
{
  // (d32).w  ->  size[ ss m[ int d32 ]]
  SemStr* ret = new SemStr(size);
  ret->push(idSize); ret->push(size);
  ret->push(idMemOf); ret->push(idIntConst); ret->push(d32);
  return ret;
}

SemStr* NJMCDecoder::pImmB(int d8)
{
  // #d8 -> int d8
  // Should already be sign extended to 32 bits
  SemStr* ret = new SemStr(8);
  ret->push(idIntConst); ret->push(d8);
  return ret;
}

SemStr* NJMCDecoder::pImmW(int d16)
{
  // #d16 -> int d16
  // Should already be sign extended to 32 bits
  SemStr* ret = new SemStr(16);
  ret->push(idIntConst); ret->push(d16);
  return ret;
}

SemStr* NJMCDecoder::pImmL(int d32)
{
  SemStr* ret = new SemStr(32);
  ret->push(idIntConst); ret->push(d32);
  return ret;
}

void NJMCDecoder::pNonzeroByte(ADDRESS pc)
{
    ostrstream ost;
    ost << "Non zero upper byte at " << hex << pc;
    error(str(ost));
}


SemStr* NJMCDecoder::alEAX(ADDRESS eax, ADDRESS x, DecodeResult& result,
    ADDRESS pc, int size)
{
  SemStr* ret;
  int reg2, mode;



#line 877 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 877 "machine/mc68k/decoder.m"
    eax
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    
      switch((MATCH_w_16_0 >> 3 & 0x7) /* adrm at 0 */) {
        case 0: case 1: case 2: case 3: case 4: 
          goto MATCH_label_od0; break;
        case 5: 
          { 
            unsigned r2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 878 "machine/mc68k/decoder.m"
             { mode = 0; reg2 = r2; result.numBytes += 2;}

            
            
            
          }
          
          break;
        case 6: 
          { 
            unsigned r2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 879 "machine/mc68k/decoder.m"
             { mode = 1; reg2 = r2; result.numBytes += 2;}

            
            
            
          }
          
          break;
        case 7: 
          
            switch((MATCH_w_16_0 & 0x7) /* reg2 at 0 */) {
              case 0: 
                
#line 880 "machine/mc68k/decoder.m"
                 { mode = 4; result.numBytes += 2;}

                
                
                
                break;
              case 1: 
                
#line 881 "machine/mc68k/decoder.m"
                 { mode = 5; result.numBytes += 4;}

                
                
                
                break;
              case 2: case 3: case 4: case 5: case 6: case 7: 
                goto MATCH_label_od0; break;
              default: assert(0);
            } /* (MATCH_w_16_0 & 0x7) -- reg2 at 0 --*/ 
          break;
        default: assert(0);
      } /* (MATCH_w_16_0 >> 3 & 0x7) -- adrm at 0 --*/ 
    
  }goto MATCH_finished_od; 
  
  MATCH_label_od0: (void)0; /*placeholder for label*/ 
    
#line 882 "machine/mc68k/decoder.m"
    pIllegalMode(pc);

    
     
    goto MATCH_finished_od; 
    
  MATCH_finished_od: (void)0; /*placeholder for label*/
  
}

#line 885 "machine/mc68k/decoder.m"

  switch (mode) {
    case 0 : {


#line 887 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 887 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    { 
      int /* [~32768..32767] */ d16 = 
        sign_extend((MATCH_w_16_0 & 0xffff) /* d16 at 0 */, 16);
      
#line 888 "machine/mc68k/decoder.m"
       ret = pADisp(d16, reg2, size);

      
      
      
    }
    
  }goto MATCH_finished_nd; 
  
  MATCH_finished_nd: (void)0; /*placeholder for label*/
  
}

#line 891 "machine/mc68k/decoder.m"
      break;
    }
    case 1 : {


#line 893 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 893 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    { 
      int /* [~128..127] */ d8 = 
        sign_extend((MATCH_w_16_0 & 0xff) /* disp8 at 0 */, 8);
      unsigned iR = (MATCH_w_16_0 >> 12 & 0x7) /* iReg at 0 */;
      unsigned iS = (MATCH_w_16_0 >> 11 & 0x1) /* iSize at 0 */;
      unsigned iT = (MATCH_w_16_0 >> 15 & 0x1) /* iType at 0 */;
      
#line 894 "machine/mc68k/decoder.m"
       ret = pAIndex(d8, reg2, iT, iR, iS, size);

      
      
      
    }
    
  }goto MATCH_finished_md; 
  
  MATCH_finished_md: (void)0; /*placeholder for label*/
  
}

#line 897 "machine/mc68k/decoder.m"
      break;
    }
    case 4 : {


#line 899 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 899 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    { 
      int /* [~32768..32767] */ d16 = 
        sign_extend((MATCH_w_16_0 & 0xffff) /* d16 at 0 */, 16);
      
#line 900 "machine/mc68k/decoder.m"
       ret = pAbsW(d16, size);

      
      
      
    }
    
  }goto MATCH_finished_ld; 
  
  MATCH_finished_ld: (void)0; /*placeholder for label*/
  
}

#line 903 "machine/mc68k/decoder.m"
      break;
    }
    case 5 : {


#line 905 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 905 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  unsigned /* [0..65535] */ MATCH_w_16_16;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    MATCH_w_16_16 = getWord(2 + MATCH_p); 
    { 
      unsigned d32 = 
        ((MATCH_w_16_0 & 0xffff) /* d16 at 0 */ << 16) + 
        (MATCH_w_16_16 & 0xffff) /* d16 at 16 */;
      
#line 906 "machine/mc68k/decoder.m"
       ret = pAbsL(d32, size);

      
      
      
    }
    
  }goto MATCH_finished_kd; 
  
  MATCH_finished_kd: (void)0; /*placeholder for label*/
  
}

#line 909 "machine/mc68k/decoder.m"
      break;
    }
    default : pIllegalMode(pc); break;
  } 
  return ret;
}


SemStr* NJMCDecoder::amEAX(ADDRESS eax, ADDRESS x, DecodeResult& result,
    ADDRESS pc, int delta, int size)
{
  SemStr* ret;
  int reg2, mode;



#line 922 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 922 "machine/mc68k/decoder.m"
    eax
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    
      switch((MATCH_w_16_0 >> 3 & 0x7) /* adrm at 0 */) {
        case 0: case 1: case 2: case 3: case 4: 
          goto MATCH_label_jd0; break;
        case 5: 
          { 
            unsigned r2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 923 "machine/mc68k/decoder.m"
             { mode = 0; reg2 = r2; result.numBytes += 2;}

            
            
            
          }
          
          break;
        case 6: 
          { 
            unsigned r2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 924 "machine/mc68k/decoder.m"
             { mode = 1; reg2 = r2; result.numBytes += 2;}

            
            
            
          }
          
          break;
        case 7: 
          
            switch((MATCH_w_16_0 & 0x7) /* reg2 at 0 */) {
              case 0: 
                
#line 927 "machine/mc68k/decoder.m"
                 { mode = 4; result.numBytes += 2;}

                
                
                
                break;
              case 1: 
                
#line 928 "machine/mc68k/decoder.m"
                 { mode = 5; result.numBytes += 4;}

                
                
                
                break;
              case 2: 
                
#line 925 "machine/mc68k/decoder.m"
                 { mode = 2; result.numBytes += 2;}

                
                
                
                break;
              case 3: 
                
#line 926 "machine/mc68k/decoder.m"
                 { mode = 3; result.numBytes += 2;}

                
                
                
                break;
              case 4: 
                
                  switch((MATCH_w_16_0 >> 6 & 0x3) /* sz at 0 */) {
                    case 0: 
                      
#line 929 "machine/mc68k/decoder.m"
                       { mode = 6; result.numBytes += 2;}

                      
                      
                      
                      break;
                    case 1: 
                      
#line 930 "machine/mc68k/decoder.m"
                       { mode = 7; result.numBytes += 2;}

                      
                      
                      
                      break;
                    case 2: 
                      
#line 931 "machine/mc68k/decoder.m"
                       { mode = 8; result.numBytes += 4;}

                      
                      
                      
                      break;
                    case 3: 
                      goto MATCH_label_jd0; break;
                    default: assert(0);
                  } /* (MATCH_w_16_0 >> 6 & 0x3) -- sz at 0 --*/ 
                break;
              case 5: case 6: case 7: 
                goto MATCH_label_jd0; break;
              default: assert(0);
            } /* (MATCH_w_16_0 & 0x7) -- reg2 at 0 --*/ 
          break;
        default: assert(0);
      } /* (MATCH_w_16_0 >> 3 & 0x7) -- adrm at 0 --*/ 
    
  }goto MATCH_finished_jd; 
  
  MATCH_label_jd0: (void)0; /*placeholder for label*/ 
    
#line 932 "machine/mc68k/decoder.m"
    pIllegalMode(pc);

    
     
    goto MATCH_finished_jd; 
    
  MATCH_finished_jd: (void)0; /*placeholder for label*/
  
}

#line 935 "machine/mc68k/decoder.m"

  switch (mode) {
    case 0 : {


#line 937 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 937 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    { 
      int /* [~32768..32767] */ d16 = 
        sign_extend((MATCH_w_16_0 & 0xffff) /* d16 at 0 */, 16);
      
#line 938 "machine/mc68k/decoder.m"
       ret = pADisp(d16, reg2, size);

      
      
      
    }
    
  }goto MATCH_finished_id; 
  
  MATCH_finished_id: (void)0; /*placeholder for label*/
  
}

#line 941 "machine/mc68k/decoder.m"
      break;
    }
    case 1 : {


#line 943 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 943 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    { 
      int /* [~128..127] */ d8 = 
        sign_extend((MATCH_w_16_0 & 0xff) /* disp8 at 0 */, 8);
      unsigned iR = (MATCH_w_16_0 >> 12 & 0x7) /* iReg at 0 */;
      unsigned iS = (MATCH_w_16_0 >> 11 & 0x1) /* iSize at 0 */;
      unsigned iT = (MATCH_w_16_0 >> 15 & 0x1) /* iType at 0 */;
      
#line 944 "machine/mc68k/decoder.m"
       ret = pAIndex(d8, reg2, iT, iR, iS, size);

      
      
      
    }
    
  }goto MATCH_finished_hd; 
  
  MATCH_finished_hd: (void)0; /*placeholder for label*/
  
}

#line 947 "machine/mc68k/decoder.m"
      break;
    }
    case 2 : {


#line 949 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 949 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    { 
      unsigned d16 = 
        sign_extend((MATCH_w_16_0 & 0xffff) /* d16 at 0 */, 16) + 
        addressToPC(MATCH_p);
      
#line 950 "machine/mc68k/decoder.m"
       ret = pPcDisp(d16, delta, size);

      
      
      
    }
    
  }goto MATCH_finished_gd; 
  
  MATCH_finished_gd: (void)0; /*placeholder for label*/
  
}

#line 953 "machine/mc68k/decoder.m"
      break;
    }
    case 3 : {


#line 955 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 955 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    { 
      int /* [~128..127] */ d8 = 
        sign_extend((MATCH_w_16_0 & 0xff) /* disp8 at 0 */, 8);
      unsigned iR = (MATCH_w_16_0 >> 12 & 0x7) /* iReg at 0 */;
      unsigned iS = (MATCH_w_16_0 >> 11 & 0x1) /* iSize at 0 */;
      unsigned iT = (MATCH_w_16_0 >> 15 & 0x1) /* iType at 0 */;
      
#line 956 "machine/mc68k/decoder.m"
      

              ret = pPcIndex(d8, iT, iR, iS, pc+2, size);

      
      
      
    }
    
  }goto MATCH_finished_fd; 
  
  MATCH_finished_fd: (void)0; /*placeholder for label*/
  
}

#line 960 "machine/mc68k/decoder.m"
      break;
    }
    case 4 : {


#line 962 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 962 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    { 
      int /* [~32768..32767] */ d16 = 
        sign_extend((MATCH_w_16_0 & 0xffff) /* d16 at 0 */, 16);
      
#line 963 "machine/mc68k/decoder.m"
       ret = pAbsW(d16, size);

      
      
      
    }
    
  }goto MATCH_finished_ed; 
  
  MATCH_finished_ed: (void)0; /*placeholder for label*/
  
}

#line 966 "machine/mc68k/decoder.m"
      break;
    }
    case 5 : {


#line 968 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 968 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  unsigned /* [0..65535] */ MATCH_w_16_16;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    MATCH_w_16_16 = getWord(2 + MATCH_p); 
    { 
      unsigned d32 = 
        ((MATCH_w_16_0 & 0xffff) /* d16 at 0 */ << 16) + 
        (MATCH_w_16_16 & 0xffff) /* d16 at 16 */;
      
#line 969 "machine/mc68k/decoder.m"
       ret = pAbsL(d32, size);

      
      
      
    }
    
  }goto MATCH_finished_dd; 
  
  MATCH_finished_dd: (void)0; /*placeholder for label*/
  
}

#line 972 "machine/mc68k/decoder.m"
      break;
    }
    case 6 : {


#line 974 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 974 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    if ((MATCH_w_16_0 >> 12 & 0x7) /* iReg at 0 */ == 0 && 
      (MATCH_w_16_0 >> 11 & 0x1) /* iSize at 0 */ == 0 && 
      (MATCH_w_16_0 >> 15 & 0x1) /* iType at 0 */ == 0 && 
      (1 <= (MATCH_w_16_0 >> 8 & 0x7) /* null at 0 */ && 
      (MATCH_w_16_0 >> 8 & 0x7) /* null at 0 */ < 8) || 
      (MATCH_w_16_0 >> 12 & 0x7) /* iReg at 0 */ == 0 && 
      (MATCH_w_16_0 >> 11 & 0x1) /* iSize at 0 */ == 0 && 
      (MATCH_w_16_0 >> 15 & 0x1) /* iType at 0 */ == 1 || 
      (MATCH_w_16_0 >> 12 & 0x7) /* iReg at 0 */ == 0 && 
      (MATCH_w_16_0 >> 11 & 0x1) /* iSize at 0 */ == 1 || 
      1 <= (MATCH_w_16_0 >> 12 & 0x7) /* iReg at 0 */ && 
      (MATCH_w_16_0 >> 12 & 0x7) /* iReg at 0 */ < 8) 
      goto MATCH_label_cd0;  /*opt-block+*/
    else { 
      unsigned d8 = (MATCH_w_16_0 & 0xff) /* disp8 at 0 */;
      
#line 975 "machine/mc68k/decoder.m"
       ret = pImmB(d8);

      
      
      
    } /*opt-block*//*opt-block+*/
    
  }goto MATCH_finished_cd; 
  
  MATCH_label_cd0: (void)0; /*placeholder for label*/ 
    
#line 976 "machine/mc68k/decoder.m"
    pNonzeroByte(pc);

    
     
    goto MATCH_finished_cd; 
    
  MATCH_finished_cd: (void)0; /*placeholder for label*/
  
}

#line 979 "machine/mc68k/decoder.m"
      break;
    }
    case 7 : {


#line 981 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 981 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    { 
      unsigned d16 = (MATCH_w_16_0 & 0xffff) /* d16 at 0 */;
      
#line 982 "machine/mc68k/decoder.m"
       ret = pImmW(d16);

      
      
      
    }
    
  }goto MATCH_finished_bd; 
  
  MATCH_finished_bd: (void)0; /*placeholder for label*/
  
}

#line 985 "machine/mc68k/decoder.m"
      break;
    }
    case 8 : {


#line 987 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 987 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  unsigned /* [0..65535] */ MATCH_w_16_16;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    MATCH_w_16_16 = getWord(2 + MATCH_p); 
    { 
      unsigned d32 = 
        ((MATCH_w_16_0 & 0xffff) /* d16 at 0 */ << 16) + 
        (MATCH_w_16_16 & 0xffff) /* d16 at 16 */;
      
#line 988 "machine/mc68k/decoder.m"
       ret = pImmL(d32);

      
      
      
    }
    
  }goto MATCH_finished_ad; 
  
  MATCH_finished_ad: (void)0; /*placeholder for label*/
  
}

#line 991 "machine/mc68k/decoder.m"
      break;
    }
    default : pIllegalMode(pc); break;
  } 
  return ret;
}


SemStr* NJMCDecoder::awlEAX(ADDRESS eax, ADDRESS x, DecodeResult& result,
    ADDRESS pc, int delta, int size)
{
  SemStr* ret;
  int reg2, mode;



#line 1004 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1004 "machine/mc68k/decoder.m"
    eax
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    
      switch((MATCH_w_16_0 >> 3 & 0x7) /* adrm at 0 */) {
        case 0: case 1: case 2: case 3: case 4: 
          goto MATCH_label_zc0; break;
        case 5: 
          { 
            unsigned r2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 1005 "machine/mc68k/decoder.m"
             { mode = 0; reg2 = r2; result.numBytes += 2;}

            
            
            
          }
          
          break;
        case 6: 
          { 
            unsigned r2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 1006 "machine/mc68k/decoder.m"
             { mode = 1; reg2 = r2; result.numBytes += 2;}

            
            
            
          }
          
          break;
        case 7: 
          
            switch((MATCH_w_16_0 & 0x7) /* reg2 at 0 */) {
              case 0: 
                
#line 1009 "machine/mc68k/decoder.m"
                 { mode = 4; result.numBytes += 2;}

                
                
                
                break;
              case 1: 
                
#line 1010 "machine/mc68k/decoder.m"
                 { mode = 5; result.numBytes += 4;}

                
                
                
                break;
              case 2: 
                
#line 1007 "machine/mc68k/decoder.m"
                 { mode = 2; result.numBytes += 2;}

                
                
                
                break;
              case 3: 
                
#line 1008 "machine/mc68k/decoder.m"
                 { mode = 3; result.numBytes += 2;}

                
                
                
                break;
              case 4: 
                if ((MATCH_w_16_0 >> 8 & 0x1) /* sb at 0 */ == 1) 
                  
#line 1012 "machine/mc68k/decoder.m"
                   { mode = 8; result.numBytes += 4;}

                  
                   /*opt-block+*/
                else 
                  
#line 1011 "machine/mc68k/decoder.m"
                   { mode = 7; result.numBytes += 2;}

                  
                   /*opt-block+*/
                
                break;
              case 5: case 6: case 7: 
                goto MATCH_label_zc0; break;
              default: assert(0);
            } /* (MATCH_w_16_0 & 0x7) -- reg2 at 0 --*/ 
          break;
        default: assert(0);
      } /* (MATCH_w_16_0 >> 3 & 0x7) -- adrm at 0 --*/ 
    
  }goto MATCH_finished_zc; 
  
  MATCH_label_zc0: (void)0; /*placeholder for label*/ 
    
#line 1013 "machine/mc68k/decoder.m"
    pIllegalMode(pc);

    
     
    goto MATCH_finished_zc; 
    
  MATCH_finished_zc: (void)0; /*placeholder for label*/
  
}

#line 1016 "machine/mc68k/decoder.m"

  switch (mode) {
    case 0 : {


#line 1018 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1018 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    { 
      int /* [~32768..32767] */ d16 = 
        sign_extend((MATCH_w_16_0 & 0xffff) /* d16 at 0 */, 16);
      
#line 1019 "machine/mc68k/decoder.m"
       ret = pADisp(d16, reg2, size);

      
      
      
    }
    
  }goto MATCH_finished_yc; 
  
  MATCH_finished_yc: (void)0; /*placeholder for label*/
  
}

#line 1022 "machine/mc68k/decoder.m"
      break;
    }
    case 1 : {


#line 1024 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1024 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    { 
      int /* [~128..127] */ d8 = 
        sign_extend((MATCH_w_16_0 & 0xff) /* disp8 at 0 */, 8);
      unsigned iR = (MATCH_w_16_0 >> 12 & 0x7) /* iReg at 0 */;
      unsigned iS = (MATCH_w_16_0 >> 11 & 0x1) /* iSize at 0 */;
      unsigned iT = (MATCH_w_16_0 >> 15 & 0x1) /* iType at 0 */;
      
#line 1025 "machine/mc68k/decoder.m"
       ret = pAIndex(d8, reg2, iT, iR, iS, size);

      
      
      
    }
    
  }goto MATCH_finished_xc; 
  
  MATCH_finished_xc: (void)0; /*placeholder for label*/
  
}

#line 1028 "machine/mc68k/decoder.m"
      break;
    }
    case 2 : {


#line 1030 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1030 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    { 
      unsigned d16 = 
        sign_extend((MATCH_w_16_0 & 0xffff) /* d16 at 0 */, 16) + 
        addressToPC(MATCH_p);
      
#line 1031 "machine/mc68k/decoder.m"
       ret = pPcDisp(d16, delta, size);

      
      
      
    }
    
  }goto MATCH_finished_wc; 
  
  MATCH_finished_wc: (void)0; /*placeholder for label*/
  
}

#line 1034 "machine/mc68k/decoder.m"
      break;
    }
    case 3 : {


#line 1036 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1036 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    { 
      int /* [~128..127] */ d8 = 
        sign_extend((MATCH_w_16_0 & 0xff) /* disp8 at 0 */, 8);
      unsigned iR = (MATCH_w_16_0 >> 12 & 0x7) /* iReg at 0 */;
      unsigned iS = (MATCH_w_16_0 >> 11 & 0x1) /* iSize at 0 */;
      unsigned iT = (MATCH_w_16_0 >> 15 & 0x1) /* iType at 0 */;
      
#line 1037 "machine/mc68k/decoder.m"
      

              ret = pPcIndex(d8, iT, iR, iS, pc+2, size);

      
      
      
    }
    
  }goto MATCH_finished_vc; 
  
  MATCH_finished_vc: (void)0; /*placeholder for label*/
  
}

#line 1041 "machine/mc68k/decoder.m"
      break;
    }
    case 4 : {


#line 1043 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1043 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    { 
      int /* [~32768..32767] */ d16 = 
        sign_extend((MATCH_w_16_0 & 0xffff) /* d16 at 0 */, 16);
      
#line 1044 "machine/mc68k/decoder.m"
       ret = pAbsW(d16, size);

      
      
      
    }
    
  }goto MATCH_finished_uc; 
  
  MATCH_finished_uc: (void)0; /*placeholder for label*/
  
}

#line 1047 "machine/mc68k/decoder.m"
      break;
    }
    case 5 : {


#line 1049 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1049 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  unsigned /* [0..65535] */ MATCH_w_16_16;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    MATCH_w_16_16 = getWord(2 + MATCH_p); 
    { 
      unsigned d32 = 
        ((MATCH_w_16_0 & 0xffff) /* d16 at 0 */ << 16) + 
        (MATCH_w_16_16 & 0xffff) /* d16 at 16 */;
      
#line 1050 "machine/mc68k/decoder.m"
       ret = pAbsL(d32, size);

      
      
      
    }
    
  }goto MATCH_finished_tc; 
  
  MATCH_finished_tc: (void)0; /*placeholder for label*/
  
}

#line 1053 "machine/mc68k/decoder.m"
      break;
    }
    case 7 : {


#line 1055 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1055 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    { 
      unsigned d16 = (MATCH_w_16_0 & 0xffff) /* d16 at 0 */;
      
#line 1056 "machine/mc68k/decoder.m"
       ret = pImmW(d16);

      
      
      
    }
    
  }goto MATCH_finished_sc; 
  
  MATCH_finished_sc: (void)0; /*placeholder for label*/
  
}

#line 1059 "machine/mc68k/decoder.m"
      break;
    }
    case 8 : {


#line 1061 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1061 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  unsigned /* [0..65535] */ MATCH_w_16_16;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    MATCH_w_16_16 = getWord(2 + MATCH_p); 
    { 
      unsigned d32 = 
        ((MATCH_w_16_0 & 0xffff) /* d16 at 0 */ << 16) + 
        (MATCH_w_16_16 & 0xffff) /* d16 at 16 */;
      
#line 1062 "machine/mc68k/decoder.m"
       ret = pImmL(d32);

      
      
      
    }
    
  }goto MATCH_finished_rc; 
  
  MATCH_finished_rc: (void)0; /*placeholder for label*/
  
}

#line 1065 "machine/mc68k/decoder.m"
      break;
    }
    default : pIllegalMode(pc); break;
  } 
  return ret;
}


SemStr* NJMCDecoder::cEAX(ADDRESS eax, ADDRESS x, DecodeResult& result,
    ADDRESS pc, int delta, int size)
{
  SemStr* ret;
  int reg2, mode;



#line 1078 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1078 "machine/mc68k/decoder.m"
    eax
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    
      switch((MATCH_w_16_0 >> 3 & 0x7) /* adrm at 0 */) {
        case 0: case 1: case 2: case 3: case 4: 
          goto MATCH_label_qc0; break;
        case 5: 
          { 
            unsigned r2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 1079 "machine/mc68k/decoder.m"
             { mode = 0; reg2 = r2; result.numBytes += 2;}

            
            
            
          }
          
          break;
        case 6: 
          { 
            unsigned r2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 1080 "machine/mc68k/decoder.m"
             { mode = 1; reg2 = r2; result.numBytes += 2;}

            
            
            
          }
          
          break;
        case 7: 
          
            switch((MATCH_w_16_0 & 0x7) /* reg2 at 0 */) {
              case 0: 
                
#line 1083 "machine/mc68k/decoder.m"
                 { mode = 4; result.numBytes += 2;}

                
                
                
                break;
              case 1: 
                
#line 1084 "machine/mc68k/decoder.m"
                 { mode = 5; result.numBytes += 4;}

                
                
                
                break;
              case 2: 
                
#line 1081 "machine/mc68k/decoder.m"
                 { mode = 2; result.numBytes += 2;}

                
                
                
                break;
              case 3: 
                
#line 1082 "machine/mc68k/decoder.m"
                 { mode = 3; result.numBytes += 2;}

                
                
                
                break;
              case 4: case 5: case 6: case 7: 
                goto MATCH_label_qc0; break;
              default: assert(0);
            } /* (MATCH_w_16_0 & 0x7) -- reg2 at 0 --*/ 
          break;
        default: assert(0);
      } /* (MATCH_w_16_0 >> 3 & 0x7) -- adrm at 0 --*/ 
    
  }goto MATCH_finished_qc; 
  
  MATCH_label_qc0: (void)0; /*placeholder for label*/ 
    
#line 1085 "machine/mc68k/decoder.m"
    pIllegalMode(pc);

    
     
    goto MATCH_finished_qc; 
    
  MATCH_finished_qc: (void)0; /*placeholder for label*/
  
}

#line 1088 "machine/mc68k/decoder.m"

  switch (mode) {
    case 0 : {


#line 1090 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1090 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    { 
      int /* [~32768..32767] */ d16 = 
        sign_extend((MATCH_w_16_0 & 0xffff) /* d16 at 0 */, 16);
      
#line 1091 "machine/mc68k/decoder.m"
       ret = pADisp(d16, reg2, size);

      
      
      
    }
    
  }goto MATCH_finished_pc; 
  
  MATCH_finished_pc: (void)0; /*placeholder for label*/
  
}

#line 1094 "machine/mc68k/decoder.m"
      break;
    }
    case 1 : {


#line 1096 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1096 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    { 
      int /* [~128..127] */ d8 = 
        sign_extend((MATCH_w_16_0 & 0xff) /* disp8 at 0 */, 8);
      unsigned iR = (MATCH_w_16_0 >> 12 & 0x7) /* iReg at 0 */;
      unsigned iS = (MATCH_w_16_0 >> 11 & 0x1) /* iSize at 0 */;
      unsigned iT = (MATCH_w_16_0 >> 15 & 0x1) /* iType at 0 */;
      
#line 1097 "machine/mc68k/decoder.m"
       ret = pAIndex(d8, reg2, iT, iR, iS, size);

      
      
      
    }
    
  }goto MATCH_finished_oc; 
  
  MATCH_finished_oc: (void)0; /*placeholder for label*/
  
}

#line 1100 "machine/mc68k/decoder.m"
      break;
    }
    case 2 : {


#line 1102 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1102 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    { 
      unsigned label = 
        sign_extend((MATCH_w_16_0 & 0xffff) /* d16 at 0 */, 16) + 
        addressToPC(MATCH_p);
      
#line 1103 "machine/mc68k/decoder.m"
       ret = pPcDisp(label, delta, size);

      
      
      
    }
    
  }goto MATCH_finished_nc; 
  
  MATCH_finished_nc: (void)0; /*placeholder for label*/
  
}

#line 1106 "machine/mc68k/decoder.m"
      break;
    }
    case 3 : {


#line 1108 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1108 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    { 
      int /* [~128..127] */ d8 = 
        sign_extend((MATCH_w_16_0 & 0xff) /* disp8 at 0 */, 8);
      unsigned iR = (MATCH_w_16_0 >> 12 & 0x7) /* iReg at 0 */;
      unsigned iS = (MATCH_w_16_0 >> 11 & 0x1) /* iSize at 0 */;
      unsigned iT = (MATCH_w_16_0 >> 15 & 0x1) /* iType at 0 */;
      
#line 1109 "machine/mc68k/decoder.m"
       ret = pPcIndex(d8, iT, iR, iS, pc+2, size);

      
      
      
    }
    
  }goto MATCH_finished_mc; 
  
  MATCH_finished_mc: (void)0; /*placeholder for label*/
  
}

#line 1112 "machine/mc68k/decoder.m"
      break;
    }
    case 4 : {


#line 1114 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1114 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    { 
      int /* [~32768..32767] */ d16 = 
        sign_extend((MATCH_w_16_0 & 0xffff) /* d16 at 0 */, 16);
      
#line 1115 "machine/mc68k/decoder.m"
       ret = pAbsW(d16, size);

      
      
      
    }
    
  }goto MATCH_finished_lc; 
  
  MATCH_finished_lc: (void)0; /*placeholder for label*/
  
}

#line 1118 "machine/mc68k/decoder.m"
      break;
    }
    case 5 : {


#line 1120 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1120 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  unsigned /* [0..65535] */ MATCH_w_16_16;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    MATCH_w_16_16 = getWord(2 + MATCH_p); 
    { 
      unsigned d32 = 
        ((MATCH_w_16_0 & 0xffff) /* d16 at 0 */ << 16) + 
        (MATCH_w_16_16 & 0xffff) /* d16 at 16 */;
      
#line 1121 "machine/mc68k/decoder.m"
       ret = pAbsL(d32, size);

      
      
      
    }
    
  }goto MATCH_finished_kc; 
  
  MATCH_finished_kc: (void)0; /*placeholder for label*/
  
}

#line 1124 "machine/mc68k/decoder.m"
      break;
    }
    default : pIllegalMode(pc); break;
  }  
  return ret;
}


SemStr* NJMCDecoder::dEAX(ADDRESS eax, ADDRESS x, DecodeResult& result,
    ADDRESS pc, int delta, int size)
{
  SemStr* ret;
  int reg2, mode;



#line 1137 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1137 "machine/mc68k/decoder.m"
    eax
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    
      switch((MATCH_w_16_0 >> 3 & 0x7) /* adrm at 0 */) {
        case 0: case 1: case 2: case 3: case 4: 
          goto MATCH_label_jc0; break;
        case 5: 
          { 
            unsigned r2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 1138 "machine/mc68k/decoder.m"
             { mode = 0; reg2 = r2; result.numBytes += 2;}

            
            
            
          }
          
          break;
        case 6: 
          { 
            unsigned r2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 1139 "machine/mc68k/decoder.m"
             { mode = 1; reg2 = r2; result.numBytes += 2;}

            
            
            
          }
          
          break;
        case 7: 
          
            switch((MATCH_w_16_0 & 0x7) /* reg2 at 0 */) {
              case 0: 
                
#line 1142 "machine/mc68k/decoder.m"
                 { mode = 4; result.numBytes += 2;}

                
                
                
                break;
              case 1: 
                
#line 1143 "machine/mc68k/decoder.m"
                 { mode = 5; result.numBytes += 4;}

                
                
                
                break;
              case 2: 
                
#line 1140 "machine/mc68k/decoder.m"
                 { mode = 2; result.numBytes += 2;}

                
                
                
                break;
              case 3: 
                
#line 1141 "machine/mc68k/decoder.m"
                 { mode = 3; result.numBytes += 2;}

                
                
                
                break;
              case 4: 
                
                  switch((MATCH_w_16_0 >> 6 & 0x3) /* sz at 0 */) {
                    case 0: 
                      
#line 1144 "machine/mc68k/decoder.m"
                       { mode = 6; result.numBytes += 2;}

                      
                      
                      
                      break;
                    case 1: 
                      
#line 1145 "machine/mc68k/decoder.m"
                       { mode = 7; result.numBytes += 2;}

                      
                      
                      
                      break;
                    case 2: 
                      
#line 1146 "machine/mc68k/decoder.m"
                       { mode = 8; result.numBytes += 4;}

                      
                      
                      
                      break;
                    case 3: 
                      goto MATCH_label_jc0; break;
                    default: assert(0);
                  } /* (MATCH_w_16_0 >> 6 & 0x3) -- sz at 0 --*/ 
                break;
              case 5: case 6: case 7: 
                goto MATCH_label_jc0; break;
              default: assert(0);
            } /* (MATCH_w_16_0 & 0x7) -- reg2 at 0 --*/ 
          break;
        default: assert(0);
      } /* (MATCH_w_16_0 >> 3 & 0x7) -- adrm at 0 --*/ 
    
  }goto MATCH_finished_jc; 
  
  MATCH_label_jc0: (void)0; /*placeholder for label*/ 
    
#line 1147 "machine/mc68k/decoder.m"
    pIllegalMode(pc);

    
     
    goto MATCH_finished_jc; 
    
  MATCH_finished_jc: (void)0; /*placeholder for label*/
  
}

#line 1150 "machine/mc68k/decoder.m"

  switch (mode) {
    case 0 : {


#line 1152 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1152 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    { 
      int /* [~32768..32767] */ d16 = 
        sign_extend((MATCH_w_16_0 & 0xffff) /* d16 at 0 */, 16);
      
#line 1153 "machine/mc68k/decoder.m"
       ret = pADisp(d16, reg2, size);

      
      
      
    }
    
  }goto MATCH_finished_ic; 
  
  MATCH_finished_ic: (void)0; /*placeholder for label*/
  
}

#line 1156 "machine/mc68k/decoder.m"
      break;
    }
    case 1 : {


#line 1158 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1158 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    { 
      int /* [~128..127] */ d8 = 
        sign_extend((MATCH_w_16_0 & 0xff) /* disp8 at 0 */, 8);
      unsigned iR = (MATCH_w_16_0 >> 12 & 0x7) /* iReg at 0 */;
      unsigned iS = (MATCH_w_16_0 >> 11 & 0x1) /* iSize at 0 */;
      unsigned iT = (MATCH_w_16_0 >> 15 & 0x1) /* iType at 0 */;
      
#line 1159 "machine/mc68k/decoder.m"
       ret = pAIndex(d8, reg2, iT, iR, iS, size);

      
      
      
    }
    
  }goto MATCH_finished_hc; 
  
  MATCH_finished_hc: (void)0; /*placeholder for label*/
  
}

#line 1162 "machine/mc68k/decoder.m"
      break;
    }
    case 2 : {


#line 1164 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1164 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    { 
      unsigned label = 
        sign_extend((MATCH_w_16_0 & 0xffff) /* d16 at 0 */, 16) + 
        addressToPC(MATCH_p);
      
#line 1165 "machine/mc68k/decoder.m"
       ret = pPcDisp(label, delta, size);

      
      
      
    }
    
  }goto MATCH_finished_gc; 
  
  MATCH_finished_gc: (void)0; /*placeholder for label*/
  
}

#line 1168 "machine/mc68k/decoder.m"
      break;
    }
    case 3 : {


#line 1170 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1170 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    { 
      int /* [~128..127] */ d8 = 
        sign_extend((MATCH_w_16_0 & 0xff) /* disp8 at 0 */, 8);
      unsigned iR = (MATCH_w_16_0 >> 12 & 0x7) /* iReg at 0 */;
      unsigned iS = (MATCH_w_16_0 >> 11 & 0x1) /* iSize at 0 */;
      unsigned iT = (MATCH_w_16_0 >> 15 & 0x1) /* iType at 0 */;
      
#line 1171 "machine/mc68k/decoder.m"
       ret = pPcIndex(d8, iT, iR, iS, pc+2, size);

      
      
      
    }
    
  }goto MATCH_finished_fc; 
  
  MATCH_finished_fc: (void)0; /*placeholder for label*/
  
}

#line 1174 "machine/mc68k/decoder.m"
      break;
    }
    case 4 : {


#line 1176 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1176 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    { 
      int /* [~32768..32767] */ d16 = 
        sign_extend((MATCH_w_16_0 & 0xffff) /* d16 at 0 */, 16);
      
#line 1177 "machine/mc68k/decoder.m"
       ret = pAbsW(d16, size);

      
      
      
    }
    
  }goto MATCH_finished_ec; 
  
  MATCH_finished_ec: (void)0; /*placeholder for label*/
  
}

#line 1180 "machine/mc68k/decoder.m"
      break;
    }
    case 5 : {


#line 1182 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1182 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  unsigned /* [0..65535] */ MATCH_w_16_16;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    MATCH_w_16_16 = getWord(2 + MATCH_p); 
    { 
      unsigned d32 = 
        ((MATCH_w_16_0 & 0xffff) /* d16 at 0 */ << 16) + 
        (MATCH_w_16_16 & 0xffff) /* d16 at 16 */;
      
#line 1183 "machine/mc68k/decoder.m"
       ret = pAbsL(d32, size);

      
      
      
    }
    
  }goto MATCH_finished_dc; 
  
  MATCH_finished_dc: (void)0; /*placeholder for label*/
  
}

#line 1186 "machine/mc68k/decoder.m"
      break;
    }
    case 6 : {


#line 1188 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1188 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    if ((MATCH_w_16_0 >> 12 & 0x7) /* iReg at 0 */ == 0 && 
      (MATCH_w_16_0 >> 11 & 0x1) /* iSize at 0 */ == 0 && 
      (MATCH_w_16_0 >> 15 & 0x1) /* iType at 0 */ == 0 && 
      (1 <= (MATCH_w_16_0 >> 8 & 0x7) /* null at 0 */ && 
      (MATCH_w_16_0 >> 8 & 0x7) /* null at 0 */ < 8) || 
      (MATCH_w_16_0 >> 12 & 0x7) /* iReg at 0 */ == 0 && 
      (MATCH_w_16_0 >> 11 & 0x1) /* iSize at 0 */ == 0 && 
      (MATCH_w_16_0 >> 15 & 0x1) /* iType at 0 */ == 1 || 
      (MATCH_w_16_0 >> 12 & 0x7) /* iReg at 0 */ == 0 && 
      (MATCH_w_16_0 >> 11 & 0x1) /* iSize at 0 */ == 1 || 
      1 <= (MATCH_w_16_0 >> 12 & 0x7) /* iReg at 0 */ && 
      (MATCH_w_16_0 >> 12 & 0x7) /* iReg at 0 */ < 8) 
      goto MATCH_label_cc0;  /*opt-block+*/
    else { 
      unsigned d8 = (MATCH_w_16_0 & 0xff) /* disp8 at 0 */;
      
#line 1189 "machine/mc68k/decoder.m"
       ret = pImmB(d8);

      
      
      
    } /*opt-block*//*opt-block+*/
    
  }goto MATCH_finished_cc; 
  
  MATCH_label_cc0: (void)0; /*placeholder for label*/ 
    
#line 1190 "machine/mc68k/decoder.m"
    pNonzeroByte(pc);

    
     
    goto MATCH_finished_cc; 
    
  MATCH_finished_cc: (void)0; /*placeholder for label*/
  
}

#line 1193 "machine/mc68k/decoder.m"
      break;
    }
    case 7 : {


#line 1195 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1195 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    { 
      unsigned d16 = (MATCH_w_16_0 & 0xffff) /* d16 at 0 */;
      
#line 1196 "machine/mc68k/decoder.m"
       ret = pImmW(d16);

      
      
      
    }
    
  }goto MATCH_finished_bc; 
  
  MATCH_finished_bc: (void)0; /*placeholder for label*/
  
}

#line 1199 "machine/mc68k/decoder.m"
      break;
    }
    case 8 : {


#line 1201 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1201 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  unsigned /* [0..65535] */ MATCH_w_16_16;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    MATCH_w_16_16 = getWord(2 + MATCH_p); 
    { 
      unsigned d32 = 
        ((MATCH_w_16_0 & 0xffff) /* d16 at 0 */ << 16) + 
        (MATCH_w_16_16 & 0xffff) /* d16 at 16 */;
      
#line 1202 "machine/mc68k/decoder.m"
       ret = pImmL(d32);

      
      
      
    }
    
  }goto MATCH_finished_ac; 
  
  MATCH_finished_ac: (void)0; /*placeholder for label*/
  
}

#line 1205 "machine/mc68k/decoder.m"
      break;
    }
    default : pIllegalMode(pc); break;
  }  
  return ret;
}


SemStr* NJMCDecoder::daEAX(ADDRESS eax, ADDRESS x, DecodeResult& result,
    ADDRESS pc, int size)
{
  SemStr* ret;
  int reg2, mode;



#line 1218 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1218 "machine/mc68k/decoder.m"
    eax
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    
      switch((MATCH_w_16_0 >> 3 & 0x7) /* adrm at 0 */) {
        case 0: case 1: case 2: case 3: case 4: 
          goto MATCH_label_zb0; break;
        case 5: 
          { 
            unsigned r2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 1219 "machine/mc68k/decoder.m"
             { mode = 0; reg2 = r2; result.numBytes += 2;}

            
            
            
          }
          
          break;
        case 6: 
          { 
            unsigned r2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 1220 "machine/mc68k/decoder.m"
             { mode = 1; reg2 = r2; result.numBytes += 2;}

            
            
            
          }
          
          break;
        case 7: 
          
            switch((MATCH_w_16_0 & 0x7) /* reg2 at 0 */) {
              case 0: 
                
#line 1221 "machine/mc68k/decoder.m"
                 { mode = 4; result.numBytes += 2;}

                
                
                
                break;
              case 1: 
                
#line 1222 "machine/mc68k/decoder.m"
                 { mode = 5; result.numBytes += 4;}

                
                
                
                break;
              case 2: case 3: case 4: case 5: case 6: case 7: 
                goto MATCH_label_zb0; break;
              default: assert(0);
            } /* (MATCH_w_16_0 & 0x7) -- reg2 at 0 --*/ 
          break;
        default: assert(0);
      } /* (MATCH_w_16_0 >> 3 & 0x7) -- adrm at 0 --*/ 
    
  }goto MATCH_finished_zb; 
  
  MATCH_label_zb0: (void)0; /*placeholder for label*/ 
    
#line 1223 "machine/mc68k/decoder.m"
    pIllegalMode(pc);

    
     
    goto MATCH_finished_zb; 
    
  MATCH_finished_zb: (void)0; /*placeholder for label*/
  
}

#line 1226 "machine/mc68k/decoder.m"

  switch (mode) {
    case 0 : {


#line 1228 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1228 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    { 
      int /* [~32768..32767] */ d16 = 
        sign_extend((MATCH_w_16_0 & 0xffff) /* d16 at 0 */, 16);
      
#line 1229 "machine/mc68k/decoder.m"
       ret = pADisp(d16, reg2, size);

      
      
      
    }
    
  }goto MATCH_finished_yb; 
  
  MATCH_finished_yb: (void)0; /*placeholder for label*/
  
}

#line 1232 "machine/mc68k/decoder.m"
      break;
    }
    case 1 : {


#line 1234 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1234 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    { 
      int /* [~128..127] */ d8 = 
        sign_extend((MATCH_w_16_0 & 0xff) /* disp8 at 0 */, 8);
      unsigned iR = (MATCH_w_16_0 >> 12 & 0x7) /* iReg at 0 */;
      unsigned iS = (MATCH_w_16_0 >> 11 & 0x1) /* iSize at 0 */;
      unsigned iT = (MATCH_w_16_0 >> 15 & 0x1) /* iType at 0 */;
      
#line 1235 "machine/mc68k/decoder.m"
       ret = pAIndex(d8, reg2, iT, iR, iS, size);

      
      
      
    }
    
  }goto MATCH_finished_xb; 
  
  MATCH_finished_xb: (void)0; /*placeholder for label*/
  
}

#line 1238 "machine/mc68k/decoder.m"
      break;
    }
    case 4 : {


#line 1240 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1240 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    { 
      int /* [~32768..32767] */ d16 = 
        sign_extend((MATCH_w_16_0 & 0xffff) /* d16 at 0 */, 16);
      
#line 1241 "machine/mc68k/decoder.m"
       ret = pAbsW(d16, size);

      
      
      
    }
    
  }goto MATCH_finished_wb; 
  
  MATCH_finished_wb: (void)0; /*placeholder for label*/
  
}

#line 1244 "machine/mc68k/decoder.m"
      break;
    }
    case 5 : {


#line 1246 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1246 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  unsigned /* [0..65535] */ MATCH_w_16_16;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    MATCH_w_16_16 = getWord(2 + MATCH_p); 
    { 
      unsigned d32 = 
        ((MATCH_w_16_0 & 0xffff) /* d16 at 0 */ << 16) + 
        (MATCH_w_16_16 & 0xffff) /* d16 at 16 */;
      
#line 1247 "machine/mc68k/decoder.m"
       ret = pAbsL(d32, size);

      
      
      
    }
    
  }goto MATCH_finished_vb; 
  
  MATCH_finished_vb: (void)0; /*placeholder for label*/
  
}

#line 1250 "machine/mc68k/decoder.m"
      break;
    }
    default : pIllegalMode(pc); break;
  } 
  return ret;
}


SemStr* NJMCDecoder::dBEAX(ADDRESS eax, ADDRESS x, DecodeResult& result,
    ADDRESS pc, int delta, int size)
{
  SemStr* ret;
  int reg2, mode;



#line 1263 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1263 "machine/mc68k/decoder.m"
    eax
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    
      switch((MATCH_w_16_0 >> 3 & 0x7) /* adrm at 0 */) {
        case 0: case 1: case 2: case 3: case 4: 
          goto MATCH_label_ub0; break;
        case 5: 
          { 
            unsigned r2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 1264 "machine/mc68k/decoder.m"
             { mode = 0; reg2 = r2; result.numBytes += 2;}

            
            
            
          }
          
          break;
        case 6: 
          { 
            unsigned r2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 1265 "machine/mc68k/decoder.m"
             { mode = 1; reg2 = r2; result.numBytes += 2;}

            
            
            
          }
          
          break;
        case 7: 
          
            switch((MATCH_w_16_0 & 0x7) /* reg2 at 0 */) {
              case 0: 
                
#line 1268 "machine/mc68k/decoder.m"
                 { mode = 4; result.numBytes += 2;}

                
                
                
                break;
              case 1: 
                
#line 1269 "machine/mc68k/decoder.m"
                 { mode = 5; result.numBytes += 4;}

                
                
                
                break;
              case 2: 
                
#line 1266 "machine/mc68k/decoder.m"
                 { mode = 2; result.numBytes += 2;}

                
                
                
                break;
              case 3: 
                
#line 1267 "machine/mc68k/decoder.m"
                 { mode = 3; result.numBytes += 2;}

                
                
                
                break;
              case 4: 
                
#line 1270 "machine/mc68k/decoder.m"
                 { mode = 6; result.numBytes += 2;}

                
                
                
                break;
              case 5: case 6: case 7: 
                goto MATCH_label_ub0; break;
              default: assert(0);
            } /* (MATCH_w_16_0 & 0x7) -- reg2 at 0 --*/ 
          break;
        default: assert(0);
      } /* (MATCH_w_16_0 >> 3 & 0x7) -- adrm at 0 --*/ 
    
  }goto MATCH_finished_ub; 
  
  MATCH_label_ub0: (void)0; /*placeholder for label*/ 
    
#line 1271 "machine/mc68k/decoder.m"
    pIllegalMode(pc);

    
     
    goto MATCH_finished_ub; 
    
  MATCH_finished_ub: (void)0; /*placeholder for label*/
  
}

#line 1274 "machine/mc68k/decoder.m"

  switch (mode) {
    case 0 : {


#line 1276 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1276 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    { 
      int /* [~32768..32767] */ d16 = 
        sign_extend((MATCH_w_16_0 & 0xffff) /* d16 at 0 */, 16);
      
#line 1277 "machine/mc68k/decoder.m"
       ret = pADisp(d16, reg2, size);

      
      
      
    }
    
  }goto MATCH_finished_tb; 
  
  MATCH_finished_tb: (void)0; /*placeholder for label*/
  
}

#line 1280 "machine/mc68k/decoder.m"
      break;
    }
    case 1 : {


#line 1282 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1282 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    { 
      int /* [~128..127] */ d8 = 
        sign_extend((MATCH_w_16_0 & 0xff) /* disp8 at 0 */, 8);
      unsigned iR = (MATCH_w_16_0 >> 12 & 0x7) /* iReg at 0 */;
      unsigned iS = (MATCH_w_16_0 >> 11 & 0x1) /* iSize at 0 */;
      unsigned iT = (MATCH_w_16_0 >> 15 & 0x1) /* iType at 0 */;
      
#line 1283 "machine/mc68k/decoder.m"
       ret = pAIndex(d8, reg2, iT, iR, iS, size);

      
      
      
    }
    
  }goto MATCH_finished_sb; 
  
  MATCH_finished_sb: (void)0; /*placeholder for label*/
  
}

#line 1286 "machine/mc68k/decoder.m"
      break;
    }
    case 2 : {


#line 1288 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1288 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    { 
      unsigned label = 
        sign_extend((MATCH_w_16_0 & 0xffff) /* d16 at 0 */, 16) + 
        addressToPC(MATCH_p);
      
#line 1289 "machine/mc68k/decoder.m"
       ret = pPcDisp(label, delta, size);

      
      
      
    }
    
  }goto MATCH_finished_rb; 
  
  MATCH_finished_rb: (void)0; /*placeholder for label*/
  
}

#line 1292 "machine/mc68k/decoder.m"
      break;
    }
    case 3 : {


#line 1294 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1294 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    { 
      int /* [~128..127] */ d8 = 
        sign_extend((MATCH_w_16_0 & 0xff) /* disp8 at 0 */, 8);
      unsigned iR = (MATCH_w_16_0 >> 12 & 0x7) /* iReg at 0 */;
      unsigned iS = (MATCH_w_16_0 >> 11 & 0x1) /* iSize at 0 */;
      unsigned iT = (MATCH_w_16_0 >> 15 & 0x1) /* iType at 0 */;
      
#line 1295 "machine/mc68k/decoder.m"
      

              ret = pPcIndex(d8, iT, iR, iS, pc+2, size);

      
      
      
    }
    
  }goto MATCH_finished_qb; 
  
  MATCH_finished_qb: (void)0; /*placeholder for label*/
  
}

#line 1299 "machine/mc68k/decoder.m"
      break;
    }
    case 4 : {


#line 1301 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1301 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    { 
      int /* [~32768..32767] */ d16 = 
        sign_extend((MATCH_w_16_0 & 0xffff) /* d16 at 0 */, 16);
      
#line 1302 "machine/mc68k/decoder.m"
       ret = pAbsW(d16, size);

      
      
      
    }
    
  }goto MATCH_finished_pb; 
  
  MATCH_finished_pb: (void)0; /*placeholder for label*/
  
}

#line 1305 "machine/mc68k/decoder.m"
      break;
    }
    case 5 : {


#line 1307 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1307 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  unsigned /* [0..65535] */ MATCH_w_16_16;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    MATCH_w_16_16 = getWord(2 + MATCH_p); 
    { 
      unsigned d32 = 
        ((MATCH_w_16_0 & 0xffff) /* d16 at 0 */ << 16) + 
        (MATCH_w_16_16 & 0xffff) /* d16 at 16 */;
      
#line 1308 "machine/mc68k/decoder.m"
       ret = pAbsL(d32, size);

      
      
      
    }
    
  }goto MATCH_finished_ob; 
  
  MATCH_finished_ob: (void)0; /*placeholder for label*/
  
}

#line 1311 "machine/mc68k/decoder.m"
      break;
    }
    case 6 : {


#line 1313 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1313 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    if ((MATCH_w_16_0 >> 12 & 0x7) /* iReg at 0 */ == 0 && 
      (MATCH_w_16_0 >> 11 & 0x1) /* iSize at 0 */ == 0 && 
      (MATCH_w_16_0 >> 15 & 0x1) /* iType at 0 */ == 0 && 
      (1 <= (MATCH_w_16_0 >> 8 & 0x7) /* null at 0 */ && 
      (MATCH_w_16_0 >> 8 & 0x7) /* null at 0 */ < 8) || 
      (MATCH_w_16_0 >> 12 & 0x7) /* iReg at 0 */ == 0 && 
      (MATCH_w_16_0 >> 11 & 0x1) /* iSize at 0 */ == 0 && 
      (MATCH_w_16_0 >> 15 & 0x1) /* iType at 0 */ == 1 || 
      (MATCH_w_16_0 >> 12 & 0x7) /* iReg at 0 */ == 0 && 
      (MATCH_w_16_0 >> 11 & 0x1) /* iSize at 0 */ == 1 || 
      1 <= (MATCH_w_16_0 >> 12 & 0x7) /* iReg at 0 */ && 
      (MATCH_w_16_0 >> 12 & 0x7) /* iReg at 0 */ < 8) 
      goto MATCH_label_nb0;  /*opt-block+*/
    else { 
      unsigned d8 = (MATCH_w_16_0 & 0xff) /* disp8 at 0 */;
      
#line 1314 "machine/mc68k/decoder.m"
       ret = pImmB(d8);

      
      
      
    } /*opt-block*//*opt-block+*/
    
  }goto MATCH_finished_nb; 
  
  MATCH_label_nb0: (void)0; /*placeholder for label*/ 
    
#line 1315 "machine/mc68k/decoder.m"
    pNonzeroByte(pc);

    
     
    goto MATCH_finished_nb; 
    
  MATCH_finished_nb: (void)0; /*placeholder for label*/
  
}

#line 1318 "machine/mc68k/decoder.m"
      break;
    }
    default : pIllegalMode(pc); break;
  } 
  return ret;
}


SemStr* NJMCDecoder::dWEAX(ADDRESS eax, ADDRESS x, DecodeResult& result,
    ADDRESS pc, int delta, int size)
{
  SemStr* ret;
  int reg2, mode;



#line 1331 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1331 "machine/mc68k/decoder.m"
    eax
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    
      switch((MATCH_w_16_0 >> 3 & 0x7) /* adrm at 0 */) {
        case 0: case 1: case 2: case 3: case 4: 
          goto MATCH_label_mb0; break;
        case 5: 
          { 
            unsigned r2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 1332 "machine/mc68k/decoder.m"
             { mode = 0; reg2 = r2; result.numBytes += 2;}

            
            
            
          }
          
          break;
        case 6: 
          { 
            unsigned r2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 1333 "machine/mc68k/decoder.m"
             { mode = 1; reg2 = r2; result.numBytes += 2;}

            
            
            
          }
          
          break;
        case 7: 
          
            switch((MATCH_w_16_0 & 0x7) /* reg2 at 0 */) {
              case 0: 
                
#line 1336 "machine/mc68k/decoder.m"
                 { mode = 4; result.numBytes += 2;}

                
                
                
                break;
              case 1: 
                
#line 1337 "machine/mc68k/decoder.m"
                 { mode = 5; result.numBytes += 4;}

                
                
                
                break;
              case 2: 
                
#line 1334 "machine/mc68k/decoder.m"
                 { mode = 2; result.numBytes += 2;}

                
                
                
                break;
              case 3: 
                
#line 1335 "machine/mc68k/decoder.m"
                 { mode = 3; result.numBytes += 2;}

                
                
                
                break;
              case 4: 
                
#line 1338 "machine/mc68k/decoder.m"
                 { mode = 7; result.numBytes += 2;}

                
                
                
                break;
              case 5: case 6: case 7: 
                goto MATCH_label_mb0; break;
              default: assert(0);
            } /* (MATCH_w_16_0 & 0x7) -- reg2 at 0 --*/ 
          break;
        default: assert(0);
      } /* (MATCH_w_16_0 >> 3 & 0x7) -- adrm at 0 --*/ 
    
  }goto MATCH_finished_mb; 
  
  MATCH_label_mb0: (void)0; /*placeholder for label*/ 
    
#line 1339 "machine/mc68k/decoder.m"
    pIllegalMode(pc);

    
     
    goto MATCH_finished_mb; 
    
  MATCH_finished_mb: (void)0; /*placeholder for label*/
  
}

#line 1342 "machine/mc68k/decoder.m"

  switch (mode) {
    case 0 : {


#line 1344 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1344 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    { 
      int /* [~32768..32767] */ d16 = 
        sign_extend((MATCH_w_16_0 & 0xffff) /* d16 at 0 */, 16);
      
#line 1345 "machine/mc68k/decoder.m"
       ret = pADisp(d16, reg2, size);

      
      
      
    }
    
  }goto MATCH_finished_lb; 
  
  MATCH_finished_lb: (void)0; /*placeholder for label*/
  
}

#line 1348 "machine/mc68k/decoder.m"
      break;
    }
    case 1 : {


#line 1350 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1350 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    { 
      int /* [~128..127] */ d8 = 
        sign_extend((MATCH_w_16_0 & 0xff) /* disp8 at 0 */, 8);
      unsigned iR = (MATCH_w_16_0 >> 12 & 0x7) /* iReg at 0 */;
      unsigned iS = (MATCH_w_16_0 >> 11 & 0x1) /* iSize at 0 */;
      unsigned iT = (MATCH_w_16_0 >> 15 & 0x1) /* iType at 0 */;
      
#line 1351 "machine/mc68k/decoder.m"
       ret = pAIndex(d8, reg2, iT, iR, iS, size);

      
      
      
    }
    
  }goto MATCH_finished_kb; 
  
  MATCH_finished_kb: (void)0; /*placeholder for label*/
  
}

#line 1354 "machine/mc68k/decoder.m"
      break;
    }
    case 2 : {


#line 1356 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1356 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    { 
      unsigned label = 
        sign_extend((MATCH_w_16_0 & 0xffff) /* d16 at 0 */, 16) + 
        addressToPC(MATCH_p);
      
#line 1357 "machine/mc68k/decoder.m"
       ret = pPcDisp(label, delta, size);

      
      
      
    }
    
  }goto MATCH_finished_jb; 
  
  MATCH_finished_jb: (void)0; /*placeholder for label*/
  
}

#line 1360 "machine/mc68k/decoder.m"
      break;
    }
    case 3 : {


#line 1362 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1362 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    { 
      int /* [~128..127] */ d8 = 
        sign_extend((MATCH_w_16_0 & 0xff) /* disp8 at 0 */, 8);
      unsigned iR = (MATCH_w_16_0 >> 12 & 0x7) /* iReg at 0 */;
      unsigned iS = (MATCH_w_16_0 >> 11 & 0x1) /* iSize at 0 */;
      unsigned iT = (MATCH_w_16_0 >> 15 & 0x1) /* iType at 0 */;
      
#line 1363 "machine/mc68k/decoder.m"
      

              ret = pPcIndex(d8, iT, iR, iS, pc+2, size);

      
      
      
    }
    
  }goto MATCH_finished_ib; 
  
  MATCH_finished_ib: (void)0; /*placeholder for label*/
  
}

#line 1367 "machine/mc68k/decoder.m"
      break;
    }
    case 4 : {


#line 1369 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1369 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    { 
      int /* [~32768..32767] */ d16 = 
        sign_extend((MATCH_w_16_0 & 0xffff) /* d16 at 0 */, 16);
      
#line 1370 "machine/mc68k/decoder.m"
       ret = pAbsW(d16, size);

      
      
      
    }
    
  }goto MATCH_finished_hb; 
  
  MATCH_finished_hb: (void)0; /*placeholder for label*/
  
}

#line 1373 "machine/mc68k/decoder.m"
      break;
    }
    case 5 : {


#line 1375 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1375 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  unsigned /* [0..65535] */ MATCH_w_16_16;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    MATCH_w_16_16 = getWord(2 + MATCH_p); 
    { 
      unsigned d32 = 
        ((MATCH_w_16_0 & 0xffff) /* d16 at 0 */ << 16) + 
        (MATCH_w_16_16 & 0xffff) /* d16 at 16 */;
      
#line 1376 "machine/mc68k/decoder.m"
       ret = pAbsL(d32, size);

      
      
      
    }
    
  }goto MATCH_finished_gb; 
  
  MATCH_finished_gb: (void)0; /*placeholder for label*/
  
}

#line 1379 "machine/mc68k/decoder.m"
      break;
    }
    case 7 : {


#line 1381 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1381 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    { 
      unsigned d16 = (MATCH_w_16_0 & 0xffff) /* d16 at 0 */;
      
#line 1382 "machine/mc68k/decoder.m"
       ret = pImmW(d16);

      
      
      
    }
    
  }goto MATCH_finished_fb; 
  
  MATCH_finished_fb: (void)0; /*placeholder for label*/
  
}

#line 1385 "machine/mc68k/decoder.m"
      break;
    }
    default : pIllegalMode(pc); break;
  } 
  return ret;
}


SemStr* NJMCDecoder::maEAX(ADDRESS eax, ADDRESS x, DecodeResult& result,
    ADDRESS pc, int size)
{
  SemStr* ret;
  int reg2, mode;



#line 1398 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1398 "machine/mc68k/decoder.m"
    eax
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    
      switch((MATCH_w_16_0 >> 3 & 0x7) /* adrm at 0 */) {
        case 0: case 1: case 2: case 3: case 4: 
          goto MATCH_label_eb0; break;
        case 5: 
          { 
            unsigned r2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 1399 "machine/mc68k/decoder.m"
             { mode = 0; reg2 = r2; result.numBytes += 2;}

            
            
            
          }
          
          break;
        case 6: 
          { 
            unsigned r2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 1400 "machine/mc68k/decoder.m"
             { mode = 1; reg2 = r2; result.numBytes += 2;}

            
            
            
          }
          
          break;
        case 7: 
          
            switch((MATCH_w_16_0 & 0x7) /* reg2 at 0 */) {
              case 0: 
                
#line 1401 "machine/mc68k/decoder.m"
                 { mode = 4; result.numBytes += 2;}

                
                
                
                break;
              case 1: 
                
#line 1402 "machine/mc68k/decoder.m"
                 { mode = 5; result.numBytes += 4;}

                
                
                
                break;
              case 2: case 3: case 4: case 5: case 6: case 7: 
                goto MATCH_label_eb0; break;
              default: assert(0);
            } /* (MATCH_w_16_0 & 0x7) -- reg2 at 0 --*/ 
          break;
        default: assert(0);
      } /* (MATCH_w_16_0 >> 3 & 0x7) -- adrm at 0 --*/ 
    
  }goto MATCH_finished_eb; 
  
  MATCH_label_eb0: (void)0; /*placeholder for label*/ 
    
#line 1403 "machine/mc68k/decoder.m"
    pIllegalMode(pc);

    
     
    goto MATCH_finished_eb; 
    
  MATCH_finished_eb: (void)0; /*placeholder for label*/
  
}

#line 1406 "machine/mc68k/decoder.m"

  switch (mode) {
    case 0 : {


#line 1408 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1408 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    { 
      int /* [~32768..32767] */ d16 = 
        sign_extend((MATCH_w_16_0 & 0xffff) /* d16 at 0 */, 16);
      
#line 1409 "machine/mc68k/decoder.m"
       ret = pADisp(d16, reg2, size);

      
      
      
    }
    
  }goto MATCH_finished_db; 
  
  MATCH_finished_db: (void)0; /*placeholder for label*/
  
}

#line 1412 "machine/mc68k/decoder.m"
      break;
    }
    case 1 : {


#line 1414 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1414 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    { 
      int /* [~128..127] */ d8 = 
        sign_extend((MATCH_w_16_0 & 0xff) /* disp8 at 0 */, 8);
      unsigned iR = (MATCH_w_16_0 >> 12 & 0x7) /* iReg at 0 */;
      unsigned iS = (MATCH_w_16_0 >> 11 & 0x1) /* iSize at 0 */;
      unsigned iT = (MATCH_w_16_0 >> 15 & 0x1) /* iType at 0 */;
      
#line 1415 "machine/mc68k/decoder.m"
       ret = pAIndex(d8, reg2, iT, iR, iS, size);

      
      
      
    }
    
  }goto MATCH_finished_cb; 
  
  MATCH_finished_cb: (void)0; /*placeholder for label*/
  
}

#line 1418 "machine/mc68k/decoder.m"
      break;
    }
    case 4 : {


#line 1420 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1420 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    { 
      int /* [~32768..32767] */ d16 = 
        sign_extend((MATCH_w_16_0 & 0xffff) /* d16 at 0 */, 16);
      
#line 1421 "machine/mc68k/decoder.m"
       ret = pAbsW(d16, size);

      
      
      
    }
    
  }goto MATCH_finished_bb; 
  
  MATCH_finished_bb: (void)0; /*placeholder for label*/
  
}

#line 1424 "machine/mc68k/decoder.m"
      break;
    }
    case 5 : {


#line 1426 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1426 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  unsigned /* [0..65535] */ MATCH_w_16_16;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    MATCH_w_16_16 = getWord(2 + MATCH_p); 
    { 
      unsigned d32 = 
        ((MATCH_w_16_0 & 0xffff) /* d16 at 0 */ << 16) + 
        (MATCH_w_16_16 & 0xffff) /* d16 at 16 */;
      
#line 1427 "machine/mc68k/decoder.m"
       ret = pAbsL(d32, size);

      
      
      
    }
    
  }goto MATCH_finished_ab; 
  
  MATCH_finished_ab: (void)0; /*placeholder for label*/
  
}

#line 1430 "machine/mc68k/decoder.m"
      break;
    }
    default : pIllegalMode(pc); break;
  } 
  return ret;
}


SemStr* NJMCDecoder::msEAX(ADDRESS eax, ADDRESS x, DecodeResult& result,
    ADDRESS pc, int delta, int size)
{
  SemStr* ret;
  int reg2, mode;



#line 1443 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1443 "machine/mc68k/decoder.m"
    eax
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    
      switch((MATCH_w_16_0 >> 3 & 0x7) /* adrm at 0 */) {
        case 0: case 1: case 2: case 3: case 4: 
          goto MATCH_label_z0; break;
        case 5: 
          { 
            unsigned r2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 1444 "machine/mc68k/decoder.m"
             { mode = 0; reg2 = r2; result.numBytes += 2;}

            
            
            
          }
          
          break;
        case 6: 
          { 
            unsigned r2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 1445 "machine/mc68k/decoder.m"
             { mode = 1; reg2 = r2; result.numBytes += 2;}

            
            
            
          }
          
          break;
        case 7: 
          
            switch((MATCH_w_16_0 & 0x7) /* reg2 at 0 */) {
              case 0: 
                
#line 1448 "machine/mc68k/decoder.m"
                 { mode = 4; result.numBytes += 2;}

                
                
                
                break;
              case 1: case 5: case 6: case 7: 
                goto MATCH_label_z0; break;
              case 2: 
                
#line 1446 "machine/mc68k/decoder.m"
                 { mode = 2; result.numBytes += 2;}

                
                
                
                break;
              case 3: 
                
#line 1447 "machine/mc68k/decoder.m"
                 { mode = 3; result.numBytes += 2;}

                
                
                
                break;
              case 4: 
                
                  switch((MATCH_w_16_0 >> 12 & 0xf) /* op at 0 */) {
                    case 0: case 2: case 4: case 5: case 6: case 7: case 8: 
                    case 9: case 10: case 11: case 12: case 13: case 14: 
                    case 15: 
                      goto MATCH_label_z0; break;
                    case 1: 
                      
#line 1449 "machine/mc68k/decoder.m"
                       { mode = 6; result.numBytes += 2;}

                      
                      
                      
                      break;
                    case 3: 
                      
#line 1450 "machine/mc68k/decoder.m"
                       { mode = 7; result.numBytes += 2;}

                      
                      
                      
                      break;
                    default: assert(0);
                  } /* (MATCH_w_16_0 >> 12 & 0xf) -- op at 0 --*/ 
                break;
              default: assert(0);
            } /* (MATCH_w_16_0 & 0x7) -- reg2 at 0 --*/ 
          break;
        default: assert(0);
      } /* (MATCH_w_16_0 >> 3 & 0x7) -- adrm at 0 --*/ 
    
  }goto MATCH_finished_z; 
  
  MATCH_label_z0: (void)0; /*placeholder for label*/ 
    
#line 1451 "machine/mc68k/decoder.m"
    pIllegalMode(pc);

    
     
    goto MATCH_finished_z; 
    
  MATCH_finished_z: (void)0; /*placeholder for label*/
  
}

#line 1454 "machine/mc68k/decoder.m"

  switch (mode) {
    case 0 : {


#line 1456 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1456 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    { 
      int /* [~32768..32767] */ d16 = 
        sign_extend((MATCH_w_16_0 & 0xffff) /* d16 at 0 */, 16);
      
#line 1457 "machine/mc68k/decoder.m"
       ret = pADisp(d16, reg2, size);

      
      
      
    }
    
  }goto MATCH_finished_y; 
  
  MATCH_finished_y: (void)0; /*placeholder for label*/
  
}

#line 1460 "machine/mc68k/decoder.m"
      break;
    }
    case 1 : {


#line 1462 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1462 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    { 
      int /* [~128..127] */ d8 = 
        sign_extend((MATCH_w_16_0 & 0xff) /* disp8 at 0 */, 8);
      unsigned iR = (MATCH_w_16_0 >> 12 & 0x7) /* iReg at 0 */;
      unsigned iS = (MATCH_w_16_0 >> 11 & 0x1) /* iSize at 0 */;
      unsigned iT = (MATCH_w_16_0 >> 15 & 0x1) /* iType at 0 */;
      
#line 1463 "machine/mc68k/decoder.m"
       ret = pAIndex(d8, reg2, iT, iR, iS, size);

      
      
      
    }
    
  }goto MATCH_finished_x; 
  
  MATCH_finished_x: (void)0; /*placeholder for label*/
  
}

#line 1466 "machine/mc68k/decoder.m"
      break;
    }
    case 2 : {


#line 1468 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1468 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    { 
      unsigned label = 
        sign_extend((MATCH_w_16_0 & 0xffff) /* d16 at 0 */, 16) + 
        addressToPC(MATCH_p);
      
#line 1469 "machine/mc68k/decoder.m"
       ret = pPcDisp(label, delta, size);

      
      
      
    }
    
  }goto MATCH_finished_w; 
  
  MATCH_finished_w: (void)0; /*placeholder for label*/
  
}

#line 1472 "machine/mc68k/decoder.m"
      break;
    }
    case 3 : {


#line 1474 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1474 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    { 
      int /* [~128..127] */ d8 = 
        sign_extend((MATCH_w_16_0 & 0xff) /* disp8 at 0 */, 8);
      unsigned iR = (MATCH_w_16_0 >> 12 & 0x7) /* iReg at 0 */;
      unsigned iS = (MATCH_w_16_0 >> 11 & 0x1) /* iSize at 0 */;
      unsigned iT = (MATCH_w_16_0 >> 15 & 0x1) /* iType at 0 */;
      
#line 1475 "machine/mc68k/decoder.m"
      

              ret = pPcIndex(d8, iT, iR, iS, pc+2, size);

      
      
      
    }
    
  }goto MATCH_finished_v; 
  
  MATCH_finished_v: (void)0; /*placeholder for label*/
  
}

#line 1479 "machine/mc68k/decoder.m"
      break;
    }
    case 4 : {


#line 1481 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1481 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    { 
      int /* [~32768..32767] */ d16 = 
        sign_extend((MATCH_w_16_0 & 0xffff) /* d16 at 0 */, 16);
      
#line 1482 "machine/mc68k/decoder.m"
       ret = pAbsW(d16, size);

      
      
      
    }
    
  }goto MATCH_finished_u; 
  
  MATCH_finished_u: (void)0; /*placeholder for label*/
  
}

#line 1485 "machine/mc68k/decoder.m"
      break;
    }
    case 6 : {


#line 1487 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1487 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    if ((MATCH_w_16_0 >> 12 & 0x7) /* iReg at 0 */ == 0 && 
      (MATCH_w_16_0 >> 11 & 0x1) /* iSize at 0 */ == 0 && 
      (MATCH_w_16_0 >> 15 & 0x1) /* iType at 0 */ == 0 && 
      (1 <= (MATCH_w_16_0 >> 8 & 0x7) /* null at 0 */ && 
      (MATCH_w_16_0 >> 8 & 0x7) /* null at 0 */ < 8) || 
      (MATCH_w_16_0 >> 12 & 0x7) /* iReg at 0 */ == 0 && 
      (MATCH_w_16_0 >> 11 & 0x1) /* iSize at 0 */ == 0 && 
      (MATCH_w_16_0 >> 15 & 0x1) /* iType at 0 */ == 1 || 
      (MATCH_w_16_0 >> 12 & 0x7) /* iReg at 0 */ == 0 && 
      (MATCH_w_16_0 >> 11 & 0x1) /* iSize at 0 */ == 1 || 
      1 <= (MATCH_w_16_0 >> 12 & 0x7) /* iReg at 0 */ && 
      (MATCH_w_16_0 >> 12 & 0x7) /* iReg at 0 */ < 8) 
      goto MATCH_label_t0;  /*opt-block+*/
    else { 
      unsigned d8 = (MATCH_w_16_0 & 0xff) /* disp8 at 0 */;
      
#line 1488 "machine/mc68k/decoder.m"
       ret = pImmB(d8);

      
      
      
    } /*opt-block*//*opt-block+*/
    
  }goto MATCH_finished_t; 
  
  MATCH_label_t0: (void)0; /*placeholder for label*/ 
    
#line 1489 "machine/mc68k/decoder.m"
    pNonzeroByte(pc);

    
     
    goto MATCH_finished_t; 
    
  MATCH_finished_t: (void)0; /*placeholder for label*/
  
}

#line 1492 "machine/mc68k/decoder.m"
      break;
    }
    case 7 : {


#line 1494 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1494 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    { 
      unsigned d16 = (MATCH_w_16_0 & 0xffff) /* d16 at 0 */;
      
#line 1495 "machine/mc68k/decoder.m"
       ret = pImmW(d16);

      
      
      
    }
    
  }goto MATCH_finished_s; 
  
  MATCH_finished_s: (void)0; /*placeholder for label*/
  
}

#line 1498 "machine/mc68k/decoder.m"
      break;
    }
    default : pIllegalMode(pc); break;
  } 
  return ret;
}


SemStr* NJMCDecoder::msEAXL(ADDRESS eaxl, int d32, DecodeResult& result,
    ADDRESS pc, int size)
{
  SemStr* ret;
  int reg2, mode;



#line 1511 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1511 "machine/mc68k/decoder.m"
    eaxl
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    
      switch((MATCH_w_16_0 & 0x7) /* reg2 at 0 */) {
        case 0: case 2: case 3: case 5: case 6: case 7: 
          goto MATCH_label_r0; break;
        case 1: 
          if ((MATCH_w_16_0 >> 3 & 0x7) /* adrm at 0 */ == 7) 
            
#line 1512 "machine/mc68k/decoder.m"
             { mode = 5; result.numBytes += 4;}

            
             /*opt-block+*/
          else 
            goto MATCH_label_r0;  /*opt-block+*/
          
          break;
        case 4: 
          if ((MATCH_w_16_0 >> 12 & 0xf) /* op at 0 */ == 2 && 
            (MATCH_w_16_0 >> 3 & 0x7) /* adrm at 0 */ == 7) 
            
#line 1513 "machine/mc68k/decoder.m"
             { mode = 8; result.numBytes += 4;}

            
             /*opt-block+*/
          else 
            goto MATCH_label_r0;  /*opt-block+*/
          
          break;
        default: assert(0);
      } /* (MATCH_w_16_0 & 0x7) -- reg2 at 0 --*/ 
    
  }goto MATCH_finished_r; 
  
  MATCH_label_r0: (void)0; /*placeholder for label*/ 
    
#line 1514 "machine/mc68k/decoder.m"
    pIllegalMode(pc);

    
     
    goto MATCH_finished_r; 
    
  MATCH_finished_r: (void)0; /*placeholder for label*/
  
}

#line 1517 "machine/mc68k/decoder.m"

  switch (mode) {
    case 5 : {
      ret = pAbsL(d32, size);
      break;
    }
    case 8 : {
      ret = pImmL(d32);
      break;
    }
    default : pIllegalMode(pc); break;
  } 
  return ret;
}


SemStr* NJMCDecoder::mdEAX(ADDRESS eax, ADDRESS x, DecodeResult& result,
    ADDRESS pc, int size)
{
  SemStr* ret;
  int reg1, mode;



#line 1538 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1538 "machine/mc68k/decoder.m"
    eax
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    
      switch((MATCH_w_16_0 >> 6 & 0x7) /* MDadrm at 0 */) {
        case 0: case 1: case 2: case 3: case 4: case 7: 
          if ((MATCH_w_16_0 >> 8 & 0x1) /* sb at 0 */ == 1) 
            if ((MATCH_w_16_0 >> 6 & 0x3) /* sz at 0 */ == 3) 
              
                switch((MATCH_w_16_0 >> 9 & 0x7) /* reg1 at 0 */) {
                  case 0: 
                    
#line 1541 "machine/mc68k/decoder.m"
                     { mode = 4; result.numBytes += 2;}

                    
                    
                    
                    break;
                  case 1: 
                    
#line 1542 "machine/mc68k/decoder.m"
                     { mode = 5; result.numBytes += 4;}

                    
                    
                    
                    break;
                  case 2: case 3: case 4: case 5: case 6: case 7: 
                    goto MATCH_label_q0; break;
                  default: assert(0);
                } /* (MATCH_w_16_0 >> 9 & 0x7) -- reg1 at 0 --*/  
            else 
              goto MATCH_label_q0;  /*opt-block+*/ 
          else 
            goto MATCH_label_q0;  /*opt-block+*/
          break;
        case 5: 
          { 
            unsigned r1 = (MATCH_w_16_0 >> 9 & 0x7) /* reg1 at 0 */;
            
#line 1539 "machine/mc68k/decoder.m"
             { mode = 0; reg1 = r1; result.numBytes += 2;}

            
            
            
          }
          
          break;
        case 6: 
          { 
            unsigned r1 = (MATCH_w_16_0 >> 9 & 0x7) /* reg1 at 0 */;
            
#line 1540 "machine/mc68k/decoder.m"
             { mode = 1; reg1 = r1; result.numBytes += 2;}

            
            
            
          }
          
          break;
        default: assert(0);
      } /* (MATCH_w_16_0 >> 6 & 0x7) -- MDadrm at 0 --*/ 
    
  }goto MATCH_finished_q; 
  
  MATCH_label_q0: (void)0; /*placeholder for label*/ 
    
#line 1543 "machine/mc68k/decoder.m"
    pIllegalMode(pc);

    
     
    goto MATCH_finished_q; 
    
  MATCH_finished_q: (void)0; /*placeholder for label*/
  
}

#line 1546 "machine/mc68k/decoder.m"

  switch (mode) {
    case 0 : {


#line 1548 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1548 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    { 
      int /* [~32768..32767] */ d16 = 
        sign_extend((MATCH_w_16_0 & 0xffff) /* d16 at 0 */, 16);
      
#line 1549 "machine/mc68k/decoder.m"
       ret = pADisp(d16, reg1, size);

      
      
      
    }
    
  }goto MATCH_finished_p; 
  
  MATCH_finished_p: (void)0; /*placeholder for label*/
  
}

#line 1552 "machine/mc68k/decoder.m"
      break;
    }
    case 1 : {


#line 1554 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1554 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    { 
      int /* [~128..127] */ d8 = 
        sign_extend((MATCH_w_16_0 & 0xff) /* disp8 at 0 */, 8);
      unsigned iR = (MATCH_w_16_0 >> 12 & 0x7) /* iReg at 0 */;
      unsigned iS = (MATCH_w_16_0 >> 11 & 0x1) /* iSize at 0 */;
      unsigned iT = (MATCH_w_16_0 >> 15 & 0x1) /* iType at 0 */;
      
#line 1555 "machine/mc68k/decoder.m"
       ret = pAIndex(d8, reg1, iT, iR, iS, size);

      
      
      
    }
    
  }goto MATCH_finished_o; 
  
  MATCH_finished_o: (void)0; /*placeholder for label*/
  
}

#line 1558 "machine/mc68k/decoder.m"
      break;
    }
    case 4 : {


#line 1560 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1560 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    { 
      unsigned d16 = (MATCH_w_16_0 & 0xffff) /* d16 at 0 */;
      
#line 1561 "machine/mc68k/decoder.m"
       ret = pAbsW(d16, size);

      
      
      
    }
    
  }goto MATCH_finished_n; 
  
  MATCH_finished_n: (void)0; /*placeholder for label*/
  
}

#line 1564 "machine/mc68k/decoder.m"
      break;
    }
    case 5 : {


#line 1566 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1566 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  unsigned /* [0..65535] */ MATCH_w_16_16;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    MATCH_w_16_16 = getWord(2 + MATCH_p); 
    { 
      unsigned d32 = 
        ((MATCH_w_16_0 & 0xffff) /* d16 at 0 */ << 16) + 
        (MATCH_w_16_16 & 0xffff) /* d16 at 16 */;
      
#line 1567 "machine/mc68k/decoder.m"
       ret = pAbsL(d32, size);

      
      
      
    }
    
  }goto MATCH_finished_m; 
  
  MATCH_finished_m: (void)0; /*placeholder for label*/
  
}

#line 1570 "machine/mc68k/decoder.m"
      break;
    }
    default : pIllegalMode(pc); break;
  } 
  return ret;
}


SemStr* NJMCDecoder::mrEAX(ADDRESS eax, ADDRESS x, DecodeResult& result,
    ADDRESS pc, int delta, int size)
{
  SemStr* ret;
  int reg2, mode;



#line 1583 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1583 "machine/mc68k/decoder.m"
    eax
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    
      switch((MATCH_w_16_0 >> 3 & 0x7) /* adrm at 0 */) {
        case 0: case 1: case 2: case 3: case 4: 
          goto MATCH_label_l0; break;
        case 5: 
          { 
            unsigned r2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 1584 "machine/mc68k/decoder.m"
             { mode = 0; reg2 = r2; result.numBytes += 2;}

            
            
            
          }
          
          break;
        case 6: 
          { 
            unsigned r2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 1585 "machine/mc68k/decoder.m"
             { mode = 1; reg2 = r2; result.numBytes += 2;}

            
            
            
          }
          
          break;
        case 7: 
          
            switch((MATCH_w_16_0 & 0x7) /* reg2 at 0 */) {
              case 0: 
                
#line 1588 "machine/mc68k/decoder.m"
                 { mode = 4; result.numBytes += 2;}

                
                
                
                break;
              case 1: 
                
#line 1589 "machine/mc68k/decoder.m"
                 { mode = 5; result.numBytes += 4;}

                
                
                
                break;
              case 2: 
                
#line 1586 "machine/mc68k/decoder.m"
                 { mode = 2; result.numBytes += 2;}

                
                
                
                break;
              case 3: 
                
#line 1587 "machine/mc68k/decoder.m"
                 { mode = 3; result.numBytes += 2;}

                
                
                
                break;
              case 4: case 5: case 6: case 7: 
                goto MATCH_label_l0; break;
              default: assert(0);
            } /* (MATCH_w_16_0 & 0x7) -- reg2 at 0 --*/ 
          break;
        default: assert(0);
      } /* (MATCH_w_16_0 >> 3 & 0x7) -- adrm at 0 --*/ 
    
  }goto MATCH_finished_l; 
  
  MATCH_label_l0: (void)0; /*placeholder for label*/ 
    
#line 1590 "machine/mc68k/decoder.m"
    pIllegalMode(pc);

    
     
    goto MATCH_finished_l; 
    
  MATCH_finished_l: (void)0; /*placeholder for label*/
  
}

#line 1593 "machine/mc68k/decoder.m"

  switch (mode) {
    case 0 : {


#line 1595 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1595 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    { 
      int /* [~32768..32767] */ d16 = 
        sign_extend((MATCH_w_16_0 & 0xffff) /* d16 at 0 */, 16);
      
#line 1596 "machine/mc68k/decoder.m"
       ret = pADisp(d16, reg2, size);

      
      
      
    }
    
  }goto MATCH_finished_k; 
  
  MATCH_finished_k: (void)0; /*placeholder for label*/
  
}

#line 1599 "machine/mc68k/decoder.m"
      break;
    }
    case 1 : {


#line 1601 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1601 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    { 
      int /* [~128..127] */ d8 = 
        sign_extend((MATCH_w_16_0 & 0xff) /* disp8 at 0 */, 8);
      unsigned iR = (MATCH_w_16_0 >> 12 & 0x7) /* iReg at 0 */;
      unsigned iS = (MATCH_w_16_0 >> 11 & 0x1) /* iSize at 0 */;
      unsigned iT = (MATCH_w_16_0 >> 15 & 0x1) /* iType at 0 */;
      
#line 1602 "machine/mc68k/decoder.m"
       ret = pAIndex(d8, reg2, iT, iR, iS, size);

      
      
      
    }
    
  }goto MATCH_finished_j; 
  
  MATCH_finished_j: (void)0; /*placeholder for label*/
  
}

#line 1605 "machine/mc68k/decoder.m"
      break;
    }
    case 2 : {


#line 1607 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1607 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    { 
      unsigned label = 
        sign_extend((MATCH_w_16_0 & 0xffff) /* d16 at 0 */, 16) + 
        addressToPC(MATCH_p);
      
#line 1608 "machine/mc68k/decoder.m"
       ret = pPcDisp(label, delta, size);

      
      
      
    }
    
  }goto MATCH_finished_i; 
  
  MATCH_finished_i: (void)0; /*placeholder for label*/
  
}

#line 1611 "machine/mc68k/decoder.m"
      break;
    }
    case 3 : {


#line 1613 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1613 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    { 
      int /* [~128..127] */ d8 = 
        sign_extend((MATCH_w_16_0 & 0xff) /* disp8 at 0 */, 8);
      unsigned iR = (MATCH_w_16_0 >> 12 & 0x7) /* iReg at 0 */;
      unsigned iS = (MATCH_w_16_0 >> 11 & 0x1) /* iSize at 0 */;
      unsigned iT = (MATCH_w_16_0 >> 15 & 0x1) /* iType at 0 */;
      
#line 1614 "machine/mc68k/decoder.m"
      

              ret = pPcIndex(d8, iT, iR, iS, pc+2, size);

      
      
      
    }
    
  }goto MATCH_finished_h; 
  
  MATCH_finished_h: (void)0; /*placeholder for label*/
  
}

#line 1618 "machine/mc68k/decoder.m"
      break;
    }
    case 4 : {


#line 1620 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1620 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    { 
      int /* [~32768..32767] */ d16 = 
        sign_extend((MATCH_w_16_0 & 0xffff) /* d16 at 0 */, 16);
      
#line 1621 "machine/mc68k/decoder.m"
       ret = pAbsW(d16, size);

      
      
      
    }
    
  }goto MATCH_finished_g; 
  
  MATCH_finished_g: (void)0; /*placeholder for label*/
  
}

#line 1624 "machine/mc68k/decoder.m"
      break;
    }
    case 5 : {


#line 1626 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1626 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  unsigned /* [0..65535] */ MATCH_w_16_16;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    MATCH_w_16_16 = getWord(2 + MATCH_p); 
    { 
      unsigned d32 = 
        ((MATCH_w_16_0 & 0xffff) /* d16 at 0 */ << 16) + 
        (MATCH_w_16_16 & 0xffff) /* d16 at 16 */;
      
#line 1627 "machine/mc68k/decoder.m"
       ret = pAbsL(d32, size);

      
      
      
    }
    
  }goto MATCH_finished_f; 
  
  MATCH_finished_f: (void)0; /*placeholder for label*/
  
}

#line 1630 "machine/mc68k/decoder.m"
      break;
    }
    default : pIllegalMode(pc); break;
  } 
  return ret;
}


SemStr* NJMCDecoder::rmEAX(ADDRESS eax, ADDRESS x, DecodeResult& result,
    ADDRESS pc, int size)
{
  SemStr* ret;
  int reg2, mode;



#line 1643 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1643 "machine/mc68k/decoder.m"
    eax
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    
      switch((MATCH_w_16_0 >> 3 & 0x7) /* adrm at 0 */) {
        case 0: case 1: case 2: case 3: case 4: 
          goto MATCH_label_e0; break;
        case 5: 
          { 
            unsigned r2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 1644 "machine/mc68k/decoder.m"
             { mode = 0; reg2 = r2; result.numBytes += 2;}

            
            
            
          }
          
          break;
        case 6: 
          { 
            unsigned r2 = (MATCH_w_16_0 & 0x7) /* reg2 at 0 */;
            
#line 1645 "machine/mc68k/decoder.m"
             { mode = 1; reg2 = r2; result.numBytes += 2;}

            
            
            
          }
          
          break;
        case 7: 
          
            switch((MATCH_w_16_0 & 0x7) /* reg2 at 0 */) {
              case 0: 
                
#line 1646 "machine/mc68k/decoder.m"
                 { mode = 4; result.numBytes += 2;}

                
                
                
                break;
              case 1: 
                
#line 1647 "machine/mc68k/decoder.m"
                 { mode = 5; result.numBytes += 4;}

                
                
                
                break;
              case 2: case 3: case 4: case 5: case 6: case 7: 
                goto MATCH_label_e0; break;
              default: assert(0);
            } /* (MATCH_w_16_0 & 0x7) -- reg2 at 0 --*/ 
          break;
        default: assert(0);
      } /* (MATCH_w_16_0 >> 3 & 0x7) -- adrm at 0 --*/ 
    
  }goto MATCH_finished_e; 
  
  MATCH_label_e0: (void)0; /*placeholder for label*/ 
    
#line 1648 "machine/mc68k/decoder.m"
    pIllegalMode(pc);

    
     
    goto MATCH_finished_e; 
    
  MATCH_finished_e: (void)0; /*placeholder for label*/
  
}

#line 1651 "machine/mc68k/decoder.m"

  switch (mode) {
    case 0 : {


#line 1653 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1653 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    { 
      int /* [~32768..32767] */ d16 = 
        sign_extend((MATCH_w_16_0 & 0xffff) /* d16 at 0 */, 16);
      
#line 1654 "machine/mc68k/decoder.m"
       ret = pADisp(d16, reg2, size);

      
      
      
    }
    
  }goto MATCH_finished_d; 
  
  MATCH_finished_d: (void)0; /*placeholder for label*/
  
}

#line 1657 "machine/mc68k/decoder.m"
      break;
    }
    case 1 : {


#line 1659 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1659 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    { 
      int /* [~128..127] */ d8 = 
        sign_extend((MATCH_w_16_0 & 0xff) /* disp8 at 0 */, 8);
      unsigned iR = (MATCH_w_16_0 >> 12 & 0x7) /* iReg at 0 */;
      unsigned iS = (MATCH_w_16_0 >> 11 & 0x1) /* iSize at 0 */;
      unsigned iT = (MATCH_w_16_0 >> 15 & 0x1) /* iType at 0 */;
      
#line 1660 "machine/mc68k/decoder.m"
       ret = pAIndex(d8, reg2, iT, iR, iS, size);

      
      
      
    }
    
  }goto MATCH_finished_c; 
  
  MATCH_finished_c: (void)0; /*placeholder for label*/
  
}

#line 1663 "machine/mc68k/decoder.m"
      break;
    }
    case 4 : {


#line 1665 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1665 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    { 
      int /* [~32768..32767] */ d16 = 
        sign_extend((MATCH_w_16_0 & 0xffff) /* d16 at 0 */, 16);
      
#line 1666 "machine/mc68k/decoder.m"
       ret = pAbsW(d16, size);

      
      
      
    }
    
  }goto MATCH_finished_b; 
  
  MATCH_finished_b: (void)0; /*placeholder for label*/
  
}

#line 1669 "machine/mc68k/decoder.m"
      break;
    }
    case 5 : {


#line 1671 "machine/mc68k/decoder.m"
{ 
  dword MATCH_p = 
    
#line 1671 "machine/mc68k/decoder.m"
    x
    ;
  unsigned /* [0..65535] */ MATCH_w_16_0;
  unsigned /* [0..65535] */ MATCH_w_16_16;
  { 
    MATCH_w_16_0 = getWord(MATCH_p); 
    MATCH_w_16_16 = getWord(2 + MATCH_p); 
    { 
      unsigned d32 = 
        ((MATCH_w_16_0 & 0xffff) /* d16 at 0 */ << 16) + 
        (MATCH_w_16_16 & 0xffff) /* d16 at 16 */;
      
#line 1672 "machine/mc68k/decoder.m"
       ret = pAbsL(d32, size);

      
      
      
    }
    
  }goto MATCH_finished_a; 
  
  MATCH_finished_a: (void)0; /*placeholder for label*/
  
}

#line 1675 "machine/mc68k/decoder.m"
      break;
    }
    default : pIllegalMode(pc); break;
  } 
  return ret;
}

/*==============================================================================
 * FUNCTION:      isFuncPrologue()
 * OVERVIEW:      Check to see if the instructions at the given offset match
 *                  any callee prologue, i.e. does it look like this offset
 *                  is a pointer to a function?
 * PARAMETERS:    hostPC - pointer to the code in question (native address)
 * RETURNS:       True if a match found
 *============================================================================*/
bool isFuncPrologue(ADDRESS hostPC)
{
    int locals, reg, d16;

    if ((InstructionPatterns::link_save(prog.csrSrc, hostPC, locals, d16))
        != NULL)
            return true;
    if ((InstructionPatterns::link_save1(prog.csrSrc, hostPC, locals, reg))
        != NULL)
            return true;
    if ((InstructionPatterns::push_lea(prog.csrSrc, hostPC, locals, reg))
        != NULL)
            return true;
    if ((InstructionPatterns::std_link(prog.csrSrc, hostPC, locals)) != NULL)
        return true;

    return false;
}



