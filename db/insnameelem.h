/**
 * \file
 * \brief Class declarations for class InsNameElem.
 *
 * \authors
 * Copyright (C) 2001, The University of Queensland
 * \authors
 * Copyright (C) 2016, Kyle Guinn
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef INSNAMEELEM_H
#define INSNAMEELEM_H

#include <map>
#include <string>

class Table;

class InsNameElem {
public:
	InsNameElem(const char *name);
	virtual ~InsNameElem(void);
	virtual int ntokens(void);
	virtual std::string getinstruction(void);
	virtual std::string getinspattern(void);
	virtual void getrefmap(std::map<std::string, InsNameElem *> &m);

	int ninstructions(void);
	void append(InsNameElem *next);
	bool increment(void);
	void reset(void);
	int getvalue(void);

protected:
	InsNameElem *nextelem = NULL;
	std::string elemname;
	int value = 0;
};

class InsOptionElem : public InsNameElem {
public:
	InsOptionElem(const char *name);
	int ntokens(void) override;
	std::string getinstruction(void) override;
	std::string getinspattern(void) override;
};

class InsListElem : public InsNameElem {
public:
	InsListElem(const char *name, Table *t, const char *idx);
	int ntokens(void) override;
	std::string getinstruction(void) override;
	std::string getinspattern(void) override;
	void getrefmap(std::map<std::string, InsNameElem *> &m) override;

	std::string getindex(void);

protected:
	std::string indexname;
	Table *thetable;
};

#endif
