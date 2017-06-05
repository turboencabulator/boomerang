/**
 * \file
 * \brief Implementation of the Transformer and related classes.
 *
 * \authors
 * Copyright (C) 2004, Mike Van Emmerik and Trent Waddington
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "transformer.h"

#include "boomerang.h"
#include "exp.h"
#include "log.h"
#include "transformation-parser.h"

#include <fstream>
#include <iostream>
#include <string>

#include <cstdlib>

std::list<ExpTransformer *> ExpTransformer::transformers;

ExpTransformer::ExpTransformer()
{
	transformers.push_back(this);
}

std::list<Exp *> cache;

Exp *
ExpTransformer::applyAllTo(Exp *p, bool &bMod)
{
	for (std::list<Exp *>::iterator it = cache.begin(); it != cache.end(); it++)
		if (*(*it)->getSubExp1() == *p)
			return (*it)->getSubExp2()->clone();

	Exp *e = p->clone();
	Exp *subs[3];
	subs[0] = e->getSubExp1();
	subs[1] = e->getSubExp2();
	subs[2] = e->getSubExp3();

	for (int i = 0; i < 3; i++)
		if (subs[i]) {
			bool mod = false;
			subs[i] = applyAllTo(subs[i], mod);
			if (mod && i == 0)
				e->setSubExp1(subs[i]);
			if (mod && i == 1)
				e->setSubExp2(subs[i]);
			if (mod && i == 2)
				e->setSubExp3(subs[i]);
			bMod |= mod;
			//if (mod) i--;
		}

#if 0
	LOG << "applyAllTo called on " << e << "\n";
#endif
	bool mod;
	//do {
		mod = false;
		for (std::list<ExpTransformer *>::iterator it = transformers.begin(); it != transformers.end(); it++) {
			e = (*it)->applyTo(e, mod);
			bMod |= mod;
		}
	//} while (mod);

	cache.push_back(new Binary(opEquals, p->clone(), e->clone()));
	return e;
}

void
ExpTransformer::loadAll()
{
	std::string sPath = Boomerang::get()->getProgPath() + "transformations/exp.ts";
	std::ifstream ifs(sPath.c_str());
	if (!ifs.good()) {
		std::cerr << "can't open `" << sPath.c_str() << "'\n";
		exit(1);
	}

	while (!ifs.eof()) {
		std::string sFile;
		ifs >> sFile;
		std::string::size_type j = sFile.find('#');
		if (j != sFile.npos)
			sFile.erase(j);
		if (sFile.size() > 0 && sFile[sFile.size() - 1] == '\n')
			sFile.erase(sFile.size() - 1);
		if (sFile.empty()) continue;

		std::string sPath1 = Boomerang::get()->getProgPath() + "transformations/" + sFile;
		std::ifstream ifs1(sPath1.c_str());
		if (!ifs1.good()) {
			LOG << "can't open `" << sPath1.c_str() << "'\n";
			exit(1);
		}

		TransformationParser p(ifs1, false);
		p.yyparse();
		ifs1.close();
	}
	ifs.close();
}
