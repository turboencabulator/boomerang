/**
 * \file
 * \brief Declaration of the memo class.
 *
 * \authors
 * Copyright (C) 2004, Trent Waddington
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef MEMO_H
#define MEMO_H

#include <list>

class Memo {
public:
	Memo(int m) : mId(m) { }
	int mId;
	virtual void doNothing() { }
	virtual ~Memo() = default;
};

class Memoisable {
public:
	Memoisable() { cur_memo = memos.begin(); }
	virtual ~Memoisable() = default;

	void takeMemo(int mId);
	void restoreMemo(int mId, bool dec = false);

	virtual Memo *makeMemo(int mId) = 0;
	virtual void readMemo(Memo *m, bool dec) = 0;

	void takeMemo();
	bool canRestore(bool dec = false);
	void restoreMemo(bool dec = false);

protected:
	std::list<Memo *> memos;
	std::list<Memo *>::iterator cur_memo;
};

#endif
