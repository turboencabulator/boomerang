/**
 * \file
 * \brief Provides the definition of class Table and children used by the SSL
 *        parser.
 *
 * \authors
 * Copyright (C) 2001, The University of Queensland
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef TABLE_H
#define TABLE_H

#include "operator.h"

#include <deque>
#include <string>

// Kinds of SSL specification tables
enum TABLE_TYPE {
	NAMETABLE,
	OPTABLE,
	EXPRTABLE
};

class Table {
public:
	Table(TABLE_TYPE t);
	virtual ~Table() = 0;
	TABLE_TYPE getType() const;

private:
	TABLE_TYPE type;
};

class NameTable : public Table {
public:
	NameTable(const std::deque<std::string> &recs);
	std::deque<std::string> records;
};

class OpTable : public Table {
public:
	OpTable(const std::deque<OPER> &ops);
	std::deque<OPER> operators;
};

class Exp;

class ExprTable : public Table {
public:
	ExprTable(const std::deque<Exp *> &exprs);
	~ExprTable();
	std::deque<Exp *> expressions;
};

#endif
