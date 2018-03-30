/**
 * \file
 * \brief Provides the implementation of classes Table, NameTable, OpTable,
 *        and ExprTable.
 *
 * \authors
 * Copyright (C) 2001, The University of Queensland
 * \authors
 * Copyright (C) 2002, Trent Waddington
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "table.h"

#include "exp.h"

Table::Table(TABLE_TYPE t) :
	type(t)
{
}

Table::~Table()
{
}

TABLE_TYPE
Table::getType() const
{
	return type;
}

NameTable::NameTable(const std::deque<std::string> &recs) :
	Table(NAMETABLE),
	records(recs)
{
}

OpTable::OpTable(const std::deque<OPER> &ops) :
	Table(OPTABLE),
	operators(ops)
{
}

ExprTable::ExprTable(const std::deque<Exp *> &exprs) :
	Table(EXPRTABLE),
	expressions(exprs)
{
}

ExprTable::~ExprTable()
{
	for (auto loc = expressions.begin(); loc != expressions.end(); ++loc)
		delete *loc;
}
