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
	SparcFrontEnd(BinaryFile *, Prog *);

	platform getFrontEndId() const override { return PLAT_SPARC; }
	NJMCDecoder &getDecoder() override { return decoder; }

	std::vector<Exp *> &getDefaultParams() override;
	std::vector<Exp *> &getDefaultReturns() override;

	bool processProc(ADDRESS, UserProc *, bool = false, bool = false) override;

private:

	//static void warnDCTcouple(ADDRESS, ADDRESS);
	bool optimise_DelayCopy(ADDRESS, ADDRESS) const;
	BasicBlock *optimise_CallReturn(CallStatement *, RTL *, RTL *, UserProc *);

	void handleBranch(ADDRESS, BasicBlock *&, Cfg *, TargetQueue &);
	void handleCall(UserProc *, ADDRESS, BasicBlock *callBB, Cfg *, ADDRESS, int = 0);

	static void case_unhandled_stub(ADDRESS);

	bool case_CALL(ADDRESS &, DecodeResult &, const DecodeResult &, std::list<RTL *> *&,
	               UserProc *, std::list<CallStatement *> &, bool = false);

	void case_SD(ADDRESS &, const DecodeResult &, const DecodeResult &, std::list<RTL *> *&,
	             Cfg *, TargetQueue &);

	bool case_DD(ADDRESS &, const DecodeResult &, const DecodeResult &, std::list<RTL *> *&,
	             TargetQueue &, UserProc *, std::list<CallStatement *> &);

	bool case_SCD(ADDRESS &, const DecodeResult &, const DecodeResult &, std::list<RTL *> *&,
	              Cfg *, TargetQueue &);

	bool case_SCDAN(ADDRESS &, const DecodeResult &, const DecodeResult &, std::list<RTL *> *&,
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
