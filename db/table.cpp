/**
 * \file
 * \brief Provides the implementation of classes Table, OpTable, and
 *        ExprTable.
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

Table::Table(std::deque<std::string> &recs, TABLE_TYPE t /* = NAMETABLE */) :
	records(recs),
	type(t)
{
}

TABLE_TYPE
Table::getType() const
{
	return type;
}

OpTable::OpTable(std::deque<OPER> &ops) :
	Table(OPTABLE),
	operators(ops)
{
}

ExprTable::ExprTable(std::deque<Exp *> &exprs) :
	Table(EXPRTABLE),
	expressions(exprs)
{
}

ExprTable::~ExprTable()
{
	for (auto loc = expressions.begin(); loc != expressions.end(); loc++)
		delete *loc;
}
