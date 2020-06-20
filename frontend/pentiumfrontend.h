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
	NJMCDecoder &getDecoder() override { return decoder; }

	platform getFrontEndId() const override { return PLAT_PENTIUM; }
	//std::vector<Exp *> &getDefaultParams() override;
	//std::vector<Exp *> &getDefaultReturns() override;

public:
	PentiumFrontEnd(BinaryFile *, Prog *);

	bool processProc(ADDRESS, UserProc *, bool = false) override;

	ADDRESS getMainEntryPoint(bool &) override;

private:
#if PROCESS_FNSTSW
	/**
	 * Process an F(n)STSW instruction.
	 */
	bool processStsw(std::list<RTL *>::iterator &, std::list<RTL *> *, BasicBlock *, Cfg *);
	static bool isStoreFsw(Statement *);
#endif

	//static void emitSet(std::list<RTL *> &, std::list<RTL *>::iterator &, ADDRESS, Exp *, Exp *);

#if 0 // Cruft?
	/**
	 * Handle the case of being in state 23 and encountering a set instruction.
	 */
	void State25(Exp *lhs, Exp *rhs, std::list<RTL *> *rtls, std::list<RTL *>::iterator &rit, ADDRESS addr);
#endif

	int idPF = -1;  ///< Parity flag.

	void processFloatCode(Cfg *);
	void processFloatCode(BasicBlock *, int &, Cfg *);
	void processStringInst(UserProc *);
	void processOverlapped(UserProc *);

	bool helperFunc(std::list<RTL *> &, ADDRESS, ADDRESS) override;

	//static bool isDecAh(RTL *);
	//static bool isSetX(Statement *);
	//static bool isAssignFromTern(Statement *);
	static void bumpRegisterAll(Exp *, int, int, int, int);

protected:
	DecodeResult &decodeInstruction(ADDRESS) override;
	void extraProcessCall(CallStatement *, std::list<RTL *> *) override;
};

#endif
