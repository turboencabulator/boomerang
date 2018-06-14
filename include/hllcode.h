/**
 * \file
 * \brief Interface for a high-level language code base class.
 *
 * This class provides methods which are generic of procedural languages like
 * C, Pascal, Fortran, etc.  Included in the base class are the follow and
 * goto sets which are used during code generation.  Concrete implementations
 * of this class provide specific language bindings for a single procedure in
 * the program.
 *
 * \authors
 * Copyright (C) 2002, Trent Waddington
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef HLLCODE_H
#define HLLCODE_H

#include <ostream>
#include <vector>

#include <cassert>

class Assign;
class BasicBlock;
class CallStatement;
class Exp;
class LocationSet;
class Proc;
class ReturnStatement;
class Signature;
class StatementList;
class Type;
class UserProc;

class HLLCode {
protected:
	        UserProc *m_proc;  // Pointer to the enclosing UserProc

public:
	// constructors
	                HLLCode() { }
	                HLLCode(UserProc *p) : m_proc(p) { }

	// destructor
	virtual        ~HLLCode() { }

	// clear the hllcode object (derived classes should call the base)
	virtual void    reset() { }

	// access to proc
	        UserProc *getProc() const { return m_proc; }
	        void    setProc(UserProc *p) { m_proc = p; }

	/*
	 * Functions to add new code, pure virtual.
	 */

	// pretested loops
	virtual void    AddPretestedLoopHeader(int indLevel, Exp *cond) = 0;
	virtual void    AddPretestedLoopEnd(int indLevel) = 0;

	// endless loops
	virtual void    AddEndlessLoopHeader(int indLevel) = 0;
	virtual void    AddEndlessLoopEnd(int indLevel) = 0;

	// posttested loops
	virtual void    AddPosttestedLoopHeader(int indLevel) = 0;
	virtual void    AddPosttestedLoopEnd(int indLevel, Exp *cond) = 0;

	// case conditionals "nways"
	virtual void    AddCaseCondHeader(int indLevel, Exp *cond) = 0;
	virtual void    AddCaseCondOption(int indLevel, Exp *opt) = 0;
	virtual void    AddCaseCondOptionEnd(int indLevel) = 0;
	virtual void    AddCaseCondElse(int indLevel) = 0;
	virtual void    AddCaseCondEnd(int indLevel) = 0;

	// if conditions
	virtual void    AddIfCondHeader(int indLevel, Exp *cond) = 0;
	virtual void    AddIfCondEnd(int indLevel) = 0;

	// if else conditions
	virtual void    AddIfElseCondHeader(int indLevel, Exp *cond) = 0;
	virtual void    AddIfElseCondOption(int indLevel) = 0;
	virtual void    AddIfElseCondEnd(int indLevel) = 0;

	// goto, break, continue, etc
	virtual void    AddGoto(int indLevel, int ord) = 0;
	virtual void    AddBreak(int indLevel) = 0;
	virtual void    AddContinue(int indLevel) = 0;

	// labels
	virtual void    AddLabel(int indLevel, int ord) = 0;
	virtual void    RemoveLabel(int ord) = 0;
	virtual void    RemoveUnusedLabels(int maxOrd) = 0;

	// sequential statements
	virtual void    AddAssignmentStatement(int indLevel, Assign *s) = 0;
	virtual void    AddCallStatement(int indLevel, Proc *proc, const char *name, const StatementList &args, StatementList *results) = 0;
	virtual void    AddIndCallStatement(int indLevel, Exp *exp, const StatementList &args, StatementList *results) = 0;
	virtual void    AddReturnStatement(int indLevel, StatementList *rets) = 0;

	// procedure related
	virtual void    AddProcStart(UserProc *proc) = 0;
	virtual void    AddProcEnd() = 0;
	virtual void    AddLocal(const char *name, Type *type, bool last = false) = 0;
	virtual void    AddGlobal(const char *name, Type *type, Exp *init = nullptr) = 0;
	virtual void    AddPrototype(UserProc *proc) = 0;

	// comments
	virtual void    AddLineComment(const char *cmt) = 0;

	/*
	 * output functions, pure virtual.
	 */
	virtual void    print(std::ostream &os) const = 0;
};

class SyntaxNode {
protected:
	        BasicBlock *pbb = nullptr;
	        int     nodenum;
	        int     score = -1;
	        const SyntaxNode *correspond = nullptr; // corresponding node in previous state
	        bool    notGoto = false;
	        int     depth;

public:
	                SyntaxNode();
	virtual        ~SyntaxNode();

	virtual bool    isBlock() const { return false; }
	virtual bool    isGoto() const;
	virtual bool    isBranch() const;

	virtual void    ignoreGoto() { };

	virtual int     getNumber() const { return nodenum; }

	        BasicBlock *getBB() const { return pbb; }
	        void    setBB(BasicBlock *bb) { pbb = bb; }

	virtual int     getNumOutEdges() const = 0;
	virtual const SyntaxNode *getOutEdge(const SyntaxNode *root, int n) const = 0;
	virtual bool    endsWithGoto() const = 0;
	virtual bool    startsWith(const SyntaxNode *node) const { return this == node; }

	virtual const SyntaxNode *getEnclosingLoop(const SyntaxNode *pFor, const SyntaxNode *cur = nullptr) const = 0;

	        int     getScore();
	        void    addToScore(int n) { score = getScore() + n; }
	        void    setDepth(int n) { depth = n; }
	        int     getDepth() const { return depth; }

	virtual SyntaxNode *clone() const = 0;
	virtual SyntaxNode *replace(const SyntaxNode *from, SyntaxNode *to) = 0;
	        const SyntaxNode *getCorrespond() const { return correspond; }

	virtual const SyntaxNode *findNodeFor(BasicBlock *bb) const = 0;
	virtual void    printAST(const SyntaxNode *root, std::ostream &os) const = 0;
	virtual int     evaluate(const SyntaxNode *root) const = 0;
	virtual void    addSuccessors(const SyntaxNode *root, std::vector<SyntaxNode *> &successors) const { }
};

class BlockSyntaxNode : public SyntaxNode {
private:
	std::vector<SyntaxNode *> statements;

public:
	BlockSyntaxNode();
	virtual ~BlockSyntaxNode();

	bool isBlock() const override { return !pbb; }

	void ignoreGoto() override {
		if (pbb)
			notGoto = true;
		else if (!statements.empty())
			statements.back()->ignoreGoto();
	}

	int getNumStatements() const {
		return pbb ? 0 : statements.size();
	}
	SyntaxNode *getStatement(int n) const {
		assert(!pbb);
		return statements[n];
	}
	void prependStatement(SyntaxNode *n) {
		assert(!pbb);
		statements.insert(statements.begin(), n);
	}
	void addStatement(SyntaxNode *n) {
		assert(!pbb);
		statements.push_back(n);
	}
	void setStatement(int i, SyntaxNode *n) {
		assert(!pbb);
		statements[i] = n;
	}

	int getNumOutEdges() const override;
	const SyntaxNode *getOutEdge(const SyntaxNode *root, int n) const override;
	bool endsWithGoto() const override {
		if (pbb) return isGoto();
		if (!statements.empty())
			return statements.back()->endsWithGoto();
		return false;
	}
	bool startsWith(const SyntaxNode *node) const override {
		return this == node || (!statements.empty() && statements.front()->startsWith(node));
	}
	const SyntaxNode *getEnclosingLoop(const SyntaxNode *pFor, const SyntaxNode *cur = nullptr) const override {
		if (this == pFor) return cur;
		for (const auto &stmt : statements) {
			const SyntaxNode *n = stmt->getEnclosingLoop(pFor, cur);
			if (n) return n;
		}
		return nullptr;
	}

	SyntaxNode *clone() const override;
	SyntaxNode *replace(const SyntaxNode *from, SyntaxNode *to) override;

	const SyntaxNode *findNodeFor(BasicBlock *bb) const override;
	void    printAST(const SyntaxNode *root, std::ostream &os) const override;
	int     evaluate(const SyntaxNode *root) const override;
	void    addSuccessors(const SyntaxNode *root, std::vector<SyntaxNode *> &successors) const override;
};

class IfThenSyntaxNode : public SyntaxNode {
protected:
	SyntaxNode *pThen = nullptr;
	Exp *cond = nullptr;

public:
	IfThenSyntaxNode();
	virtual ~IfThenSyntaxNode();

	bool    isGoto() const override { return false; }
	bool    isBranch() const override { return false; }

	int     getNumOutEdges() const override { return 1; }
	const SyntaxNode *getOutEdge(const SyntaxNode *root, int n) const override;
	bool    endsWithGoto() const override { return false; }

	SyntaxNode *clone() const override;
	SyntaxNode *replace(const SyntaxNode *from, SyntaxNode *to) override;

	const SyntaxNode *getEnclosingLoop(const SyntaxNode *pFor, const SyntaxNode *cur = nullptr) const override {
		if (this == pFor) return cur;
		return pThen->getEnclosingLoop(pFor, cur);
	}

	void    setCond(Exp *e) { cond = e; }
	Exp    *getCond() const { return cond; }
	void    setThen(SyntaxNode *n) { pThen = n; }

	const SyntaxNode *findNodeFor(BasicBlock *bb) const override;
	void    printAST(const SyntaxNode *root, std::ostream &os) const override;
	int     evaluate(const SyntaxNode *root) const override;
	void    addSuccessors(const SyntaxNode *root, std::vector<SyntaxNode *> &successors) const override;
};

class IfThenElseSyntaxNode : public SyntaxNode {
protected:
	SyntaxNode *pThen = nullptr;
	SyntaxNode *pElse = nullptr;
	Exp *cond = nullptr;

public:
	IfThenElseSyntaxNode();
	virtual ~IfThenElseSyntaxNode();

	bool    isGoto() const override { return false; }
	bool    isBranch() const override { return false; }

	int     getNumOutEdges() const override { return 1; }
	const SyntaxNode *getOutEdge(const SyntaxNode *root, int n) const override {
		const SyntaxNode *o = pThen->getOutEdge(root, 0);
		assert(o == pElse->getOutEdge(root, 0));
		return o;
	}
	bool    endsWithGoto() const override { return false; }

	const SyntaxNode *getEnclosingLoop(const SyntaxNode *pFor, const SyntaxNode *cur = nullptr) const override {
		if (this == pFor) return cur;
		const SyntaxNode *n = pThen->getEnclosingLoop(pFor, cur);
		if (n) return n;
		return pElse->getEnclosingLoop(pFor, cur);
	}

	SyntaxNode *clone() const override;
	SyntaxNode *replace(const SyntaxNode *from, SyntaxNode *to) override;

	void    setCond(Exp *e) { cond = e; }
	void    setThen(SyntaxNode *n) { pThen = n; }
	void    setElse(SyntaxNode *n) { pElse = n; }

	const SyntaxNode *findNodeFor(BasicBlock *bb) const override;
	void    printAST(const SyntaxNode *root, std::ostream &os) const override;
	int     evaluate(const SyntaxNode *root) const override;
	void    addSuccessors(const SyntaxNode *root, std::vector<SyntaxNode *> &successors) const override;
};

class PretestedLoopSyntaxNode : public SyntaxNode {
protected:
	SyntaxNode *pBody = nullptr;
	Exp *cond = nullptr;

public:
	PretestedLoopSyntaxNode();
	virtual ~PretestedLoopSyntaxNode();

	bool    isGoto() const override { return false; }
	bool    isBranch() const override { return false; }

	int     getNumOutEdges() const override { return 1; }
	const SyntaxNode *getOutEdge(const SyntaxNode *root, int n) const override;
	bool    endsWithGoto() const override { return false; }
	const SyntaxNode *getEnclosingLoop(const SyntaxNode *pFor, const SyntaxNode *cur = nullptr) const override {
		if (this == pFor) return cur;
		return pBody->getEnclosingLoop(pFor, this);
	}

	SyntaxNode *clone() const override;
	SyntaxNode *replace(const SyntaxNode *from, SyntaxNode *to) override;

	void    setCond(Exp *e) { cond = e; }
	void    setBody(SyntaxNode *n) { pBody = n; }

	const SyntaxNode *findNodeFor(BasicBlock *bb) const override;
	void    printAST(const SyntaxNode *root, std::ostream &os) const override;
	int     evaluate(const SyntaxNode *root) const override;
	void    addSuccessors(const SyntaxNode *root, std::vector<SyntaxNode *> &successors) const override;
};

class PostTestedLoopSyntaxNode : public SyntaxNode {
protected:
	SyntaxNode *pBody = nullptr;
	Exp *cond = nullptr;

public:
	PostTestedLoopSyntaxNode();
	virtual ~PostTestedLoopSyntaxNode();

	bool    isGoto() const override { return false; }
	bool    isBranch() const override { return false; }

	int     getNumOutEdges() const override { return 1; }
	const SyntaxNode *getOutEdge(const SyntaxNode *root, int n) const override;
	bool    endsWithGoto() const override { return false; }
	const SyntaxNode *getEnclosingLoop(const SyntaxNode *pFor, const SyntaxNode *cur = nullptr) const override {
		if (this == pFor) return cur;
		return pBody->getEnclosingLoop(pFor, this);
	}

	SyntaxNode *clone() const override;
	SyntaxNode *replace(const SyntaxNode *from, SyntaxNode *to) override;

	void    setCond(Exp *e) { cond = e; }
	void    setBody(SyntaxNode *n) { pBody = n; }

	const SyntaxNode *findNodeFor(BasicBlock *bb) const override;
	void    printAST(const SyntaxNode *root, std::ostream &os) const override;
	int     evaluate(const SyntaxNode *root) const override;
	void    addSuccessors(const SyntaxNode *root, std::vector<SyntaxNode *> &successors) const override;
};

class InfiniteLoopSyntaxNode : public SyntaxNode {
protected:
	SyntaxNode *pBody = nullptr;

public:
	InfiniteLoopSyntaxNode();
	virtual ~InfiniteLoopSyntaxNode();

	bool    isGoto() const override { return false; }
	bool    isBranch() const override { return false; }

	int     getNumOutEdges() const override { return 0; }
	const SyntaxNode *getOutEdge(const SyntaxNode *root, int n) const override { return nullptr; }
	bool    endsWithGoto() const override { return false; }
	const SyntaxNode *getEnclosingLoop(const SyntaxNode *pFor, const SyntaxNode *cur = nullptr) const override {
		if (this == pFor) return cur;
		return pBody->getEnclosingLoop(pFor, this);
	}

	SyntaxNode *clone() const override;
	SyntaxNode *replace(const SyntaxNode *from, SyntaxNode *to) override;

	void    setBody(SyntaxNode *n) { pBody = n; }

	const SyntaxNode *findNodeFor(BasicBlock *bb) const override;
	void    printAST(const SyntaxNode *root, std::ostream &os) const override;
	int     evaluate(const SyntaxNode *root) const override;
	void    addSuccessors(const SyntaxNode *root, std::vector<SyntaxNode *> &successors) const override;
};

#endif
