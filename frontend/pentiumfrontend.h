/**
 * \file
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef PENTIUMFRONTEND_H
#define PENTIUMFRONTEND_H

#include "frontend.h"

#include "pentiumdecoder.h"

#include <list>

class Statement;

/**
 * \brief Pentium specific FrontEnd behaviour.
 */
class PentiumFrontEnd : public FrontEnd {
	PentiumDecoder decoder;

public:
	PentiumFrontEnd(BinaryFile *pBF, Prog *prog);
	virtual ~PentiumFrontEnd();

	platform getFrontEndId() const override { return PLAT_PENTIUM; }
	NJMCDecoder &getDecoder() override { return decoder; }

	std::vector<Exp *> &getDefaultParams() override;
	std::vector<Exp *> &getDefaultReturns() override;

	bool processProc(ADDRESS uAddr, UserProc *pProc, std::ofstream &os, bool frag = false, bool spec = false) override;

	ADDRESS getMainEntryPoint(bool &gotMain) override;

private:
#if PROCESS_FNSTSW
	/**
	 * Process an F(n)STSW instruction.
	 */
	bool processStsw(std::list<RTL *>::iterator &rit, std::list<RTL *> *pRtls, BasicBlock *pBB, Cfg *pCfg);
#endif

	void emitSet(std::list<RTL *> *pRtls, std::list<RTL *>::iterator &itRtl, ADDRESS uAddr, Exp *pLHS, Exp *cond);

#if 0 // Cruft?
	/**
	 * Handle the case of being in state 23 and encountering a set instruction.
	 */
	void State25(Exp *pLHS, Exp *pRHS, std::list<RTL *> *pRtls, std::list<RTL *>::iterator &rit, ADDRESS uAddr);
#endif

	int idPF = -1;  ///< Parity flag.

	void processFloatCode(Cfg *pCfg);
	void processFloatCode(BasicBlock *pBB, int &tos, Cfg *pCfg);
	void processStringInst(UserProc *proc);
	void processOverlapped(UserProc *proc);

	bool helperFunc(ADDRESS dest, ADDRESS addr, std::list<RTL *> *lrtl);

	bool isStoreFsw(Statement *s);
	bool isDecAh(RTL *r);
	bool isSetX(Statement *e);
	bool isAssignFromTern(Statement *s);
	void bumpRegisterAll(Exp *e, int min, int max, int delta, int mask);

protected:
	DecodeResult &decodeInstruction(ADDRESS pc) override;
	void extraProcessCall(CallStatement *call, std::list<RTL *> *BB_rtls) override;
};

#endif
