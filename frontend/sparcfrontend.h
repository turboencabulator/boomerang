/**
 * \file
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef SPARCFRONTEND_H
#define SPARCFRONTEND_H

#include "frontend.h"

#include "operator.h"
#include "sparcdecoder.h"

#include <fstream>
#include <list>
#include <string>

#include <cstddef>

class CallStatement;

/**
 * \brief SPARC specific FrontEnd behaviour.
 */
class SparcFrontEnd : public FrontEnd {
	SparcDecoder decoder;

public:
	SparcFrontEnd(BinaryFile *pBF, Prog *prog);

	platform getFrontEndId() const override { return PLAT_SPARC; }
	NJMCDecoder &getDecoder() override { return decoder; }

	std::vector<Exp *> &getDefaultParams() override;
	std::vector<Exp *> &getDefaultReturns() override;

	bool processProc(ADDRESS, UserProc *, bool = false, bool = false) override;

private:

	//static void warnDCTcouple(ADDRESS, ADDRESS);
	bool optimise_DelayCopy(ADDRESS src, ADDRESS dest) const;
	BasicBlock *optimise_CallReturn(CallStatement *call, RTL *rtl, RTL *delay, UserProc *pProc);

	void handleBranch(ADDRESS dest, BasicBlock *&newBB, Cfg *cfg, TargetQueue &tq);
	void handleCall(UserProc *proc, ADDRESS dest, BasicBlock *callBB, Cfg *cfg, ADDRESS address, int offset = 0);

	static void case_unhandled_stub(ADDRESS addr);

	bool case_CALL(ADDRESS &, DecodeResult &, DecodeResult &, std::list<RTL *> *&,
	               UserProc *, std::list<CallStatement *> &, bool = false);

	void case_SD(ADDRESS &, DecodeResult &, DecodeResult &, std::list<RTL *> *&,
	             Cfg *, TargetQueue &);

	bool case_DD(ADDRESS &, DecodeResult &, DecodeResult &, std::list<RTL *> *&,
	             TargetQueue &, UserProc *, std::list<CallStatement *> &);

	bool case_SCD(ADDRESS &, DecodeResult &, DecodeResult &, std::list<RTL *> *&,
	              Cfg *, TargetQueue &);

	bool case_SCDAN(ADDRESS &, DecodeResult &, DecodeResult &, std::list<RTL *> *&,
	                Cfg *, TargetQueue &);

	//static void emitNop(std::list<RTL *> &, ADDRESS);
	static void emitCopyPC(std::list<RTL *> &, ADDRESS);
	static void appendAssignment(std::list<RTL *> &, ADDRESS, Type *, Exp *, Exp *);
	static void quadOperation(std::list<RTL *> &, ADDRESS, OPER);

	bool helperFunc(std::list<RTL *> &, ADDRESS, ADDRESS) override;
	static void gen32op32gives64(std::list<RTL *> &, ADDRESS, OPER);
	static bool helperFuncLong(std::list<RTL *> &, ADDRESS, const std::string &);
	//void setReturnLocations(CalleeEpilogue *epilogue, int iReg);

	/**
	 * This struct represents a single nop instruction.
	 * Used as a substitute delay slot instruction.
	 */
	DecodeResult nop_inst;
};

#endif
