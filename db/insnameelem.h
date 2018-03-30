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

class NameTable;

class InsNameElem {
public:
	InsNameElem(const std::string &name);
	virtual ~InsNameElem();
	virtual int ntokens();
	virtual std::string getinstruction();
	virtual std::string getinspattern();
	virtual void getrefmap(std::map<std::string, InsNameElem *> &m);

	int ninstructions();
	void append(InsNameElem *next);
	bool increment();
	void reset();
	int getvalue();

protected:
	InsNameElem *nextelem = nullptr;
	std::string elemname;
	int value = 0;
};

class InsOptionElem : public InsNameElem {
public:
	InsOptionElem(const std::string &name);
	int ntokens() override;
	std::string getinstruction() override;
	std::string getinspattern() override;
};

class InsListElem : public InsNameElem {
public:
	InsListElem(const std::string &name, NameTable *t, const std::string &idx);
	int ntokens() override;
	std::string getinstruction() override;
	std::string getinspattern() override;
	void getrefmap(std::map<std::string, InsNameElem *> &m) override;

	std::string getindex();

protected:
	std::string indexname;
	NameTable *thetable;
};

#endif
