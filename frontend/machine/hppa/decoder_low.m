/*==============================================================================
 * FILE:        decoder_low.m
 * OVERVIEW:    Low level New Jersey Machine Code Toolkit match file for the
 *              HP Pa/risc architecture (basically PA/RISC version 1.1)
 *
 * (C) 2000-01 The University of Queensland, BT group
 *============================================================================*/

#include "global.h"
#include "decoder.h"
#include "hppa-names.h"
#include "rtl.h"

void c_null(ADDRESS hostpc, char **garble);
unsigned long c_wcr(ADDRESS hostpc, char **garble);
bool c_c_n(ADDRESS hostpc);
void addr(ADDRESS hostpc);

/* get4Bytes - returns next 4-Byte from image pointed to by lc.
   Fetch in a big-endian manner  */
uint32_t
getDword(unsigned lc)
{
	return (uint32_t)((((((*(uint8_t *) lc       << 8)
	                     + *(uint8_t *)(lc + 1)) << 8)
	                     + *(uint8_t *)(lc + 2)) << 8)
	                     + *(uint8_t *)(lc + 3));
}

bool
c_c_n(ADDRESS hostpc)
{
	bool result = true;
	match hostpc to
	| c_c_nonneg() => result = true;
	| c_c_neg()    => result = false;
	endmatch
	return result;
}

SemStr *
NJMCDecoder::c_c(ADDRESS hostpc, int &cond)
{
	static const char *c_c_names[] = {
		"c_c_no", "c_c_eq", "c_c_l", "c_c_le", "c_c_ul", "c_c_ule", "c_c_sv",
		"c_c_od", "c_c_yes", "c_c_ne", "c_c_ge", "c_c_g", "c_c_uge", "c_c_ug",
		"c_c_nsv", "c_c_ev"
	};
	static const int cmpib_codes[] = { 4, 1, 2, 3, 12, 9, 10, 11 };
	static const int sep_codes[] = { 0, 1, 2, 7, 8, 9, 10, 15 };
	match hostpc to
	| c_arith_w(c3, cmplt) =>
		cond = c3 + (c_c_n(cmplt) ? 0 : 8);
	| c_arith_dw(c3, cmplt) =>
		cond = c3 + (c_c_n(cmplt) ? 0 : 8);
	| c_arith_none() =>
		cond = 0;
	| c_sep(c3_16) =>
		cond = sep_codes[c3_16];
	| c_cmpb_w(c3, cmplt) =>
		cond = c3 + (c_c_n(cmplt) ? 0 : 8);
	| c_cmpb_dw(c3, cmplt) =>
		cond = c3 + (c_c_n(cmplt) ? 0 : 8);
	| c_cmpib_dw(c3) =>
		cond = cmpib_codes[c3];
	| c_bbs_w(c) =>
		cond = 1 + (c ? 0 : 8);
	| c_bbs_dw(c) =>
		cond = 1 + (c ? 0 : 8);
	else
		cond = 0;
	endmatch
	return instantiateNamedParam(c_c_names[cond]);
}

unsigned long
c_wcr(ADDRESS hostpc, char **garble)
{
#if 0
	unsigned long regl;
	match hostpc to
	| c_mfctl(r_06) =>
		regl = r_06;
	| c_mfctl_w() =>
		*garble += sprintf(*garble, ".w");
		regl = 11;
	else
		regl = 0;
		//sprintf("#c_WCR%08X#", getDword(hostpc));
	endmatch
	return regl;
#else
	return 0;
#endif
}

void
c_null(ADDRESS hostpc, char **garble)
{
#if 0
	match hostpc to
	| c_br_nnull() =>
	| c_br_null() =>
		*garble += sprintf(*garble, ".n");
	else
		//printf("#c_NULL%08X#", getDword(hostpc));
	endmatch
#endif
}

/*==============================================================================
 * FUNCTION:       NJMCDecoder::decodeLowLevelInstruction
 * OVERVIEW:       Decodes a machine instruction and returns an instantiated
 *                  list of RTs.
 * NOTE:           A side effect of decoding the completers is that there may
 *                  be some semantics added to members preInstSem or postInstSem
 *                  This is made part of the final RTL in decoder.m
 * PARAMETERS:     hostPC - the address of the pc in the loaded Elf object
 *                 pc - the virtual address of the pc
 *                 result - a reference parameter that has a fields for the
 *                  number of bytes decoded, their validity, etc
 * RETURNS:        the instantiated list of RTs
 *============================================================================*/
list<RT *> *
NJMCDecoder::decodeLowLevelInstruction(ADDRESS hostPC, ADDRESS pc, DecodeResult &result)
{
	ADDRESS nextPC;

	list<RT *> *RTs = NULL;
	int condvalue;
	match [nextPC] hostPC to
	| arith(cmplt, r_11, r_06, t_27) [name] =>
		/*  Arith,cc_16   r_11, r_06, t_27 */
		RTs = instantiate(pc, name, dis_Reg(r_11), dis_Reg(r_06), dis_Reg(t_27), c_c(cmplt, condvalue));
	| arith_imm(cmplt, imm11, r_06, t_11) [name] =>
		/* arith_imm,cc_16 imm11!,r_06,t_11 */
		RTs = instantiate(pc, name, dis_Num(imm11), dis_Reg(r_06), dis_Reg(t_11), c_c(cmplt, condvalue));
	| ADDIL(imm21, r_06) [name] =>
		RTs = instantiate(pc, name, dis_Num(imm21), dis_Reg(r_06));
	| LDIL(imm21, t_06) [name] =>
		RTs = instantiate(pc, name, dis_Num(imm21), dis_Reg(t_06));
	| iloads(c_addr, xd, s, b, t_27) [name] =>
		RTs = instantiate(pc, name, dis_c_addr(c_addr), dis_xd(xd), dis_Sreg(s), dis_Reg(b), dis_Reg(t_27));
	| istores(c_addr, r_11, xd, s, b) [name] =>
		RTs = instantiate(pc, name, dis_c_addr(c_addr), dis_Reg(r_11), dis_xd(xd), dis_Sreg(s), dis_Reg(b));
	| fwloads(c_addr, xd, s, b, t_27) [name] =>
		RTs = instantiate(pc, name, dis_c_addr(c_addr), dis_xd(xd), dis_Sreg(s), dis_Reg(b), dis_Freg(t_27, 0));
	| fwstores(c_addr, r_27, xd, s, b) [name] =>
		RTs = instantiate(pc, name, dis_c_addr(c_addr), dis_Freg(r_27, 0), dis_xd(xd), dis_Sreg(s), dis_Reg(b));
	| fdloads(c_addr, xd, s, b, t_27) [name] =>
		RTs = instantiate(pc, name, dis_c_addr(c_addr), dis_xd(xd), dis_Sreg(s), dis_Reg(b), dis_Freg(t_27, 1));
	| fdstores(c_addr, r_27, xd, s, b) [name] =>
		RTs = instantiate(pc, name, dis_c_addr(c_addr), dis_Freg(r_27, 1), dis_xd(xd), dis_Sreg(s), dis_Reg(b));
	| iloads_ldisp(c_addr, xd, s, b, r_11) [name] =>
		RTs = instantiate(pc, name, dis_c_addr(c_addr), dis_xd(xd), dis_Sreg(s), dis_Reg(b), dis_Reg(r_11));
	| istores_ldisp(c_addr, r_11, xd, s, b) [name] =>
		RTs = instantiate(pc, name, dis_c_addr(c_addr), dis_Reg(r_11), dis_xd(xd), dis_Sreg(s), dis_Reg(b));
	| LDO(ldisp, b, t) [name] =>
		RTs = instantiate(pc, name, dis_Num(ldisp), dis_Reg(b), dis_Reg(t));
	| VSHD(r1, r2, t, c) [name] =>
		RTs = instantiate(pc, name, dis_Reg(r1), dis_Reg(r2), dis_Reg(t), c_c(c, condvalue));
	| SHD(r1, r2, p, t, c) [name] =>
		RTs = instantiate(pc, name, dis_Reg(r1), dis_Reg(r2), dis_Num(p), dis_Reg(t), c_c(c, condvalue));
	| ext_var(r, len, t, c) [name] =>
		RTs = instantiate(pc, name, dis_Reg(r), dis_Num(len), dis_Reg(t), c_c(c, condvalue));
	| ext_fix(r, p, len, t, c) [name] =>
		RTs = instantiate(pc, name, dis_Reg(r), dis_Num(p), dis_Num(len), dis_Reg(t), c_c(c, condvalue));
	| dep_var(r, len, t, c) [name] =>
		RTs = instantiate(pc, name, dis_Reg(r), dis_Num(len), dis_Reg(t), c_c(c, condvalue));
	| dep_fix(r, p, len, t, c) [name] =>
		RTs = instantiate(pc, name, dis_Reg(r), dis_Num(p), dis_Num(len), dis_Reg(t), c_c(c, condvalue));
	| dep_ivar(i, len, t, c) [name] =>
		RTs = instantiate(pc, name, dis_Num(i), dis_Num(len), dis_Reg(t), c_c(c, condvalue));
	| dep_ifix(i, p, len, t, c) [name] =>
		RTs = instantiate(pc, name, dis_Num(i), dis_Num(p), dis_Num(len), dis_Reg(t), c_c(c, condvalue));
	| ubranch(_, ubr_target, t_06) [name] =>
	//| ubranch(nulli, ubr_target, t_06) [name] =>
		/* ubranch,cmplt,n  target,t_06) */
		RTs = instantiate(pc, name, dis_Num(ubr_target), dis_Reg(t_06));
	| BL.LONG(_, ubr_target) [name] =>
	//| BL.LONG(nulli, ubr_target) [name] =>
		/* BL.LONG cmplt,n  target,2) */
		RTs = instantiate(pc, name, dis_Num(ubr_target));
	| BLR(_, x_11, t_06) [name] =>
	//| BLR(nulli, x_11, t_06) [name] =>
		/* BLR,n x,t */
		RTs = instantiate(pc, name, dis_Reg(x_11), dis_Reg(t_06));
	| BV(_, x_11, b_06) [name] =>
	//| BV(nulli, x_11, b_06) [name] =>
		/* BV,n x_11(b_06) */
		RTs = instantiate(pc, name, dis_Reg(x_11), dis_Reg(b_06));
	| bve(p_31, _, b_06) [name] =>
	//| bve(p_31, nulli, b_06) [name] =>
		/* BVE.l BVE.lp BVE.p BVE  */
		RTs = instantiate(pc, name, p_31, dis_Reg(b_06));
	| BREAK(im5_27, im13_06) [name] =>
		RTs = instantiate(pc, name, dis_Num(im5_27), dis_Num(im13_06));
	| sysop_i_t(im10_06, t_27) [name] =>
		RTs = instantiate(pc, name, dis_Num(im10_06), dis_Reg(t_27));
	| sysop_simple [name] =>
		RTs = instantiate(pc, name);
	| sysop_r(r_11) [name] =>
		RTs = instantiate(pc, name, dis_Reg(r_11));
	| sysop_cr_t(cmplt, t_27) [name] =>
		RTs = instantiate(pc, name, dis_c_wcr(cmplt), dis_Reg(t_27));
	| MTCTL(r_11, ct_06) [name] =>
		RTs = instantiate(pc, name, dis_Reg(r_11), dis_ct(ct_06));
	| MFIA(t_27) [name] =>
		RTs = instantiate(pc, name, dis_Reg(t_27));
	// Floating point instructions. Note that the floating point format is being
	// passed as an ss inf the form of an integer constant (using dis_Num())
	| flt_c0_all(fmt, rf, tf) [name] =>
		RTs = instantiate(pc, name, dis_Num(fmt), dis_Freg(rf, fmt), dis_Freg(tf, fmt));
	| flt_c1_all(sf, df, rf, tf) [name] =>
		RTs = instantiate(pc, name, dis_Num(sf), dis_Num(df), dis_Freg(rf, sf), dis_Freg(tf, df));
	| flt_c2_all(sf, df, rf, tf) [name] =>
		RTs = instantiate(pc, name, dis_Num(sf), dis_Num(df), dis_Freg(rf, sf), dis_Freg(tf, df));
	| flt_c3_all(fmt, fr1, fr2, frt) [name] =>
		RTs = instantiate(pc, name, dis_Num(fmt), dis_Freg(fr1, fmt), dis_Freg(fr2, fmt), dis_Freg(frt, fmt));
	| XMPYU(fr1, fr2, frt) =>
		// This instruction has fixed register sizes
		RTs = instantiate(pc, "XMPYU", dis_Freg(fr1, 0), dis_Freg(fr2, 0), dis_Freg(frt, 1));
//	| LDSID(s2_16, b_06, t_27) [name] =>
//	| MTSP(r_11, sr) [name] =>
//	| MFSP(sr, t_27) [name] =>
	else
		//RTs = NULL;
		result.valid = false;
		cout << "Undecoded instruction " << hex << *(int *)hostPC << " at " << pc << " (opcode " << ((*(unsigned *)hostPC) >> 26) << ")\n";
	endmatch

	result.numBytes = (nextPC - hostPC);
	return RTs;
}

/*
	| LDWl(cmplt, ldisp, s2_16, b_06, t_11) [name] =>
		*garble += sprintf(*garble, "%s", name);
		c_disps(cmplt);
		*garble += sprintf(*garble, "  %d(%s,%s),%s", ldisp, s2_16_names[s2_16], b_06_names[b_06], t_11_names[t_11]);
*/
