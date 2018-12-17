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
	virtual ~SparcFrontEnd();

	platform getFrontEndId() const override { return PLAT_SPARC; }
	NJMCDecoder &getDecoder() override { return decoder; }

	std::vector<Exp *> &getDefaultParams() override;
	std::vector<Exp *> &getDefaultReturns() override;

	bool processProc(ADDRESS uAddr, UserProc *pProc, std::ofstream &os, bool frag = false, bool spec = false) override;

private:

	void warnDCTcouple(ADDRESS uAt, ADDRESS uDest);
	bool optimise_DelayCopy(ADDRESS src, ADDRESS dest);
	BasicBlock *optimise_CallReturn(CallStatement *call, RTL *rtl, RTL *delay, UserProc *pProc);

	void handleBranch(ADDRESS dest, BasicBlock *&newBB, Cfg *cfg, TargetQueue &tq);
	void handleCall(UserProc *proc, ADDRESS dest, BasicBlock *callBB, Cfg *cfg, ADDRESS address, int offset = 0);

	void case_unhandled_stub(ADDRESS addr);

	bool case_CALL(ADDRESS &address, DecodeResult &inst, DecodeResult &delay_inst, std::list<RTL *> *&BB_rtls,
	               UserProc *proc, std::list<CallStatement *> &callList, std::ofstream &os, bool isPattern = false);

	void case_SD(ADDRESS &address, DecodeResult &inst, DecodeResult &delay_inst,
	             std::list<RTL *> *&BB_rtls, Cfg *cfg, TargetQueue &tq, std::ofstream &os);

	bool case_DD(ADDRESS &address, DecodeResult &inst, DecodeResult &delay_inst,
	             std::list<RTL *> *&BB_rtls, TargetQueue &tq, UserProc *proc, std::list<CallStatement *> &callList);

	bool case_SCD(ADDRESS &address, DecodeResult &inst, DecodeResult &delay_inst,
	              std::list<RTL *> *&BB_rtls, Cfg *cfg, TargetQueue &tq);

	bool case_SCDAN(ADDRESS &address, DecodeResult &inst, DecodeResult &delay_inst,
	                std::list<RTL *> *&BB_rtls, Cfg *cfg, TargetQueue &tq);

	void emitNop(std::list<RTL *> *pRtls, ADDRESS uAddr);
	void emitCopyPC(std::list<RTL *> *pRtls, ADDRESS uAddr);
	void appendAssignment(Exp *lhs, Exp *rhs, Type *type, ADDRESS addr, std::list<RTL *> *lrtl);
	void quadOperation(ADDRESS addr, std::list<RTL *> *lrtl, OPER op);

	bool helperFunc(ADDRESS dest, ADDRESS addr, std::list<RTL *> *lrtl);
	void gen32op32gives64(OPER op, std::list<RTL *> *lrtl, ADDRESS addr);
	bool helperFuncLong(ADDRESS dest, ADDRESS addr, std::list<RTL *> *lrtl, std::string &name);
	//void setReturnLocations(CalleeEpilogue *epilogue, int iReg);

	/**
	 * This struct represents a single nop instruction.
	 * Used as a substitute delay slot instruction.
	 */
	DecodeResult nop_inst;
};

#endif
