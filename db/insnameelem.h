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

#include <deque>
#include <map>
#include <string>

class InsNameElem {
public:
	InsNameElem(const std::string &name);
	virtual ~InsNameElem();
	virtual int ntokens() const;
	virtual std::string getinstruction() const;
	virtual std::string getinspattern() const;
	virtual void getrefmap(std::map<std::string, const InsNameElem *> &m) const;

	int ninstructions() const;
	void append(InsNameElem *next);
	bool increment();
	void reset();
	int getvalue() const;

protected:
	InsNameElem *nextelem = nullptr;
	std::string elemname;
	int value = 0;
};

class InsOptionElem : public InsNameElem {
public:
	InsOptionElem(const std::string &name);
	int ntokens() const override;
	std::string getinstruction() const override;
	std::string getinspattern() const override;
};

class InsListElem : public InsNameElem {
public:
	InsListElem(const std::string &name, const std::deque<std::string> *t, const std::string &idx);
	int ntokens() const override;
	std::string getinstruction() const override;
	std::string getinspattern() const override;
	void getrefmap(std::map<std::string, const InsNameElem *> &m) const override;

	std::string getindex() const;

protected:
	std::string indexname;
	const std::deque<std::string> *thetable;
};

#endif
