/**
 * \file
 * \brief Skeleton for MIPS disassembly.
 *
 * \authors
 * Markus Gothe <nietzsche@lysator.liu.se>
 */

#include "exp.h"
#include "register.h"
#include "rtl.h"
#include "cfg.h"
#include "proc.h"
#include "prog.h"
#include "decoder.h"
#include "mipsdecoder.h"
#include "BinaryFile.h"
#include "frontend.h"
#include "mipsfrontend.h"
#include "BinaryFile.h"     // E.g. IsDynamicallyLinkedProc
#include "boomerang.h"
#include "signature.h"

#include <iomanip>          // For setfill etc
#include <sstream>

MIPSFrontEnd::MIPSFrontEnd(BinaryFile *pBF, Prog *prog, BinaryFileFactory *pbff) : FrontEnd(pBF, prog, pbff)
{
	decoder = new MIPSDecoder(prog);
}

MIPSFrontEnd::~MIPSFrontEnd()
{
}

std::vector<Exp *> &MIPSFrontEnd::getDefaultParams()
{
	static std::vector<Exp *> params;
	if (params.size() == 0) {
		for (int r = 31; r >= 0; r--) {
			params.push_back(Location::regOf(r));
		}
	}
	return params;
}

std::vector<Exp *> &MIPSFrontEnd::getDefaultReturns()
{
	static std::vector<Exp *> returns;
	if (returns.size() == 0) {
		for (int r = 31; r >= 0; r--) {
			returns.push_back(Location::regOf(r));
		}
	}
	return returns;
}

ADDRESS MIPSFrontEnd::getMainEntryPoint(bool &gotMain)
{
	gotMain = true;
	ADDRESS start = pBF->getMainEntryPoint();
	if (start != NO_ADDRESS) return start;

	start = pBF->getEntryPoint();
	gotMain = false;
	if (start == NO_ADDRESS) return NO_ADDRESS;

	gotMain = true;
	return start;
}

bool MIPSFrontEnd::processProc(ADDRESS uAddr, UserProc *pProc, std::ofstream &os, bool frag /* = false */, bool spec /* = false */)
{
	// Call the base class to do most of the work
	if (!FrontEnd::processProc(uAddr, pProc, os, frag, spec))
		return false;
	// This will get done twice; no harm
	pProc->setEntryBB();

	return true;
}
