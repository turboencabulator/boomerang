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

	platform getFrontEndId() const override { return PLAT_PENTIUM; }
	NJMCDecoder &getDecoder() override { return decoder; }

	std::vector<Exp *> &getDefaultParams() override;
	std::vector<Exp *> &getDefaultReturns() override;

	bool processProc(ADDRESS, UserProc *, bool = false, bool = false) override;

	ADDRESS getMainEntryPoint(bool &gotMain) override;

private:
#if PROCESS_FNSTSW
	/**
	 * Process an F(n)STSW instruction.
	 */
	bool processStsw(std::list<RTL *>::iterator &rit, std::list<RTL *> *pRtls, BasicBlock *pBB, Cfg *pCfg);
	static bool isStoreFsw(Statement *s);
#endif

	//static void emitSet(std::list<RTL *> &, std::list<RTL *>::iterator &, ADDRESS, Exp *, Exp *);

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

	bool helperFunc(std::list<RTL *> &, ADDRESS, ADDRESS) override;

	//static bool isDecAh(RTL *r);
	//static bool isSetX(Statement *e);
	//static bool isAssignFromTern(Statement *s);
	static void bumpRegisterAll(Exp *e, int min, int max, int delta, int mask);

protected:
	DecodeResult &decodeInstruction(ADDRESS pc) override;
	void extraProcessCall(CallStatement *call, std::list<RTL *> *BB_rtls) override;
};

#endif
