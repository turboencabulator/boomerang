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
	        UserProc *getProc() { return m_proc; }
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
	virtual void    AddCallStatement(int indLevel, Proc *proc, const char *name, StatementList &args, StatementList *results) = 0;
	virtual void    AddIndCallStatement(int indLevel, Exp *exp, StatementList &args, StatementList *results) = 0;
	virtual void    AddReturnStatement(int indLevel, StatementList *rets) = 0;

	// procedure related
	virtual void    AddProcStart(UserProc *proc) = 0;
	virtual void    AddProcEnd() = 0;
	virtual void    AddLocal(const char *name, Type *type, bool last = false) = 0;
	virtual void    AddGlobal(const char *name, Type *type, Exp *init = NULL) = 0;
	virtual void    AddPrototype(UserProc *proc) = 0;

	// comments
	virtual void    AddLineComment(const char *cmt) = 0;

	/*
	 * output functions, pure virtual.
	 */
	virtual void    print(std::ostream &os) = 0;
};

class SyntaxNode {
protected:
	        BasicBlock *pbb = NULL;
	        int     nodenum;
	        int     score = -1;
	        SyntaxNode *correspond = NULL; // corresponding node in previous state
	        bool    notGoto = false;
	        int     depth;

public:
	                SyntaxNode();
	virtual        ~SyntaxNode();

	virtual bool    isBlock() { return false; }
	virtual bool    isGoto();
	virtual bool    isBranch();

	virtual void    ignoreGoto() { };

	virtual int     getNumber() { return nodenum; }

	        BasicBlock *getBB() { return pbb; }
	        void    setBB(BasicBlock *bb) { pbb = bb; }

	virtual int     getNumOutEdges() = 0;
	virtual SyntaxNode *getOutEdge(SyntaxNode *root, int n) = 0;
	virtual bool    endsWithGoto() = 0;
	virtual bool    startsWith(SyntaxNode *node) { return this == node; }

	virtual SyntaxNode *getEnclosingLoop(SyntaxNode *pFor, SyntaxNode *cur = NULL) = 0;

	        int     getScore();
	        void    addToScore(int n) { score = getScore() + n; }
	        void    setDepth(int n) { depth = n; }
	        int     getDepth() { return depth; }

	virtual SyntaxNode *clone() = 0;
	virtual SyntaxNode *replace(SyntaxNode *from, SyntaxNode *to) = 0;
	        SyntaxNode *getCorrespond() { return correspond; }

	virtual SyntaxNode *findNodeFor(BasicBlock *bb) = 0;
	virtual void    printAST(SyntaxNode *root, std::ostream &os) = 0;
	virtual int     evaluate(SyntaxNode *root) = 0;
	virtual void    addSuccessors(SyntaxNode *root, std::vector<SyntaxNode *> &successors) { }
};

class BlockSyntaxNode : public SyntaxNode {
private:
	std::vector<SyntaxNode *> statements;

public:
	BlockSyntaxNode();
	virtual ~BlockSyntaxNode();

	bool isBlock() override { return pbb == NULL; }

	void ignoreGoto() override {
		if (pbb) notGoto = true;
		else if (!statements.empty())
			statements[statements.size() - 1]->ignoreGoto();
	}

	int getNumStatements() {
		return pbb ? 0 : statements.size();
	}
	SyntaxNode *getStatement(int n) {
		assert(pbb == NULL);
		return statements[n];
	}
	void prependStatement(SyntaxNode *n) {
		assert(pbb == NULL);
		statements.resize(statements.size() + 1);
		for (int i = statements.size() - 1; i > 0; i--)
			statements[i] = statements[i - 1];
		statements[0] = n;
	}
	void addStatement(SyntaxNode *n) {
		assert(pbb == NULL);
		statements.push_back(n);
	}
	void setStatement(int i, SyntaxNode *n) {
		assert(pbb == NULL);
		statements[i] = n;
	}

	int getNumOutEdges() override;
	SyntaxNode *getOutEdge(SyntaxNode *root, int n) override;
	bool endsWithGoto() override {
		if (pbb) return isGoto();
		bool last = false;
		if (!statements.empty())
			last = statements[statements.size() - 1]->endsWithGoto();
		return last;
	}
	bool startsWith(SyntaxNode *node) override {
		return this == node || (!statements.empty() && statements[0]->startsWith(node));
	}
	SyntaxNode *getEnclosingLoop(SyntaxNode *pFor, SyntaxNode *cur = NULL) override {
		if (this == pFor) return cur;
		for (unsigned i = 0; i < statements.size(); i++) {
			SyntaxNode *n = statements[i]->getEnclosingLoop(pFor, cur);
			if (n) return n;
		}
		return NULL;
	}

	SyntaxNode *clone() override;
	SyntaxNode *replace(SyntaxNode *from, SyntaxNode *to) override;

	SyntaxNode *findNodeFor(BasicBlock *bb) override;
	void    printAST(SyntaxNode *root, std::ostream &os) override;
	int     evaluate(SyntaxNode *root) override;
	void    addSuccessors(SyntaxNode *root, std::vector<SyntaxNode *> &successors) override;
};

class IfThenSyntaxNode : public SyntaxNode {
protected:
	SyntaxNode *pThen = NULL;
	Exp *cond = NULL;

public:
	IfThenSyntaxNode();
	virtual ~IfThenSyntaxNode();

	bool    isGoto() override { return false; }
	bool    isBranch() override { return false; }

	int     getNumOutEdges() override { return 1; }
	SyntaxNode *getOutEdge(SyntaxNode *root, int n) override;
	bool    endsWithGoto() override { return false; }

	SyntaxNode *clone() override;
	SyntaxNode *replace(SyntaxNode *from, SyntaxNode *to) override;

	SyntaxNode *getEnclosingLoop(SyntaxNode *pFor, SyntaxNode *cur = NULL) override {
		if (this == pFor) return cur;
		return pThen->getEnclosingLoop(pFor, cur);
	}

	void    setCond(Exp *e) { cond = e; }
	Exp    *getCond() { return cond; }
	void    setThen(SyntaxNode *n) { pThen = n; }

	SyntaxNode *findNodeFor(BasicBlock *bb) override;
	void    printAST(SyntaxNode *root, std::ostream &os) override;
	int     evaluate(SyntaxNode *root) override;
	void    addSuccessors(SyntaxNode *root, std::vector<SyntaxNode *> &successors) override;
};

class IfThenElseSyntaxNode : public SyntaxNode {
protected:
	SyntaxNode *pThen = NULL;
	SyntaxNode *pElse = NULL;
	Exp *cond = NULL;

public:
	IfThenElseSyntaxNode();
	virtual ~IfThenElseSyntaxNode();

	bool    isGoto() override { return false; }
	bool    isBranch() override { return false; }

	int     getNumOutEdges() override { return 1; }
	SyntaxNode *getOutEdge(SyntaxNode *root, int n) override {
		SyntaxNode *o = pThen->getOutEdge(root, 0);
		assert(o == pElse->getOutEdge(root, 0));
		return o;
	}
	bool    endsWithGoto() override { return false; }

	SyntaxNode *getEnclosingLoop(SyntaxNode *pFor, SyntaxNode *cur = NULL) override {
		if (this == pFor) return cur;
		SyntaxNode *n = pThen->getEnclosingLoop(pFor, cur);
		if (n) return n;
		return pElse->getEnclosingLoop(pFor, cur);
	}

	SyntaxNode *clone() override;
	SyntaxNode *replace(SyntaxNode *from, SyntaxNode *to) override;

	void    setCond(Exp *e) { cond = e; }
	void    setThen(SyntaxNode *n) { pThen = n; }
	void    setElse(SyntaxNode *n) { pElse = n; }

	SyntaxNode *findNodeFor(BasicBlock *bb) override;
	void    printAST(SyntaxNode *root, std::ostream &os) override;
	int     evaluate(SyntaxNode *root) override;
	void    addSuccessors(SyntaxNode *root, std::vector<SyntaxNode *> &successors) override;
};

class PretestedLoopSyntaxNode : public SyntaxNode {
protected:
	SyntaxNode *pBody = NULL;
	Exp *cond = NULL;

public:
	PretestedLoopSyntaxNode();
	virtual ~PretestedLoopSyntaxNode();

	bool    isGoto() override { return false; }
	bool    isBranch() override { return false; }

	int     getNumOutEdges() override { return 1; }
	SyntaxNode *getOutEdge(SyntaxNode *root, int n) override;
	bool    endsWithGoto() override { return false; }
	SyntaxNode *getEnclosingLoop(SyntaxNode *pFor, SyntaxNode *cur = NULL) override {
		if (this == pFor) return cur;
		return pBody->getEnclosingLoop(pFor, this);
	}

	SyntaxNode *clone() override;
	SyntaxNode *replace(SyntaxNode *from, SyntaxNode *to) override;

	void    setCond(Exp *e) { cond = e; }
	void    setBody(SyntaxNode *n) { pBody = n; }

	SyntaxNode *findNodeFor(BasicBlock *bb) override;
	void    printAST(SyntaxNode *root, std::ostream &os) override;
	int     evaluate(SyntaxNode *root) override;
	void    addSuccessors(SyntaxNode *root, std::vector<SyntaxNode *> &successors) override;
};

class PostTestedLoopSyntaxNode : public SyntaxNode {
protected:
	SyntaxNode *pBody = NULL;
	Exp *cond = NULL;

public:
	PostTestedLoopSyntaxNode();
	virtual ~PostTestedLoopSyntaxNode();

	bool    isGoto() override { return false; }
	bool    isBranch() override { return false; }

	int     getNumOutEdges() override { return 1; }
	SyntaxNode *getOutEdge(SyntaxNode *root, int n) override;
	bool    endsWithGoto() override { return false; }
	SyntaxNode *getEnclosingLoop(SyntaxNode *pFor, SyntaxNode *cur = NULL) override {
		if (this == pFor) return cur;
		return pBody->getEnclosingLoop(pFor, this);
	}

	SyntaxNode *clone() override;
	SyntaxNode *replace(SyntaxNode *from, SyntaxNode *to) override;

	void    setCond(Exp *e) { cond = e; }
	void    setBody(SyntaxNode *n) { pBody = n; }

	SyntaxNode *findNodeFor(BasicBlock *bb) override;
	void    printAST(SyntaxNode *root, std::ostream &os) override;
	int     evaluate(SyntaxNode *root) override;
	void    addSuccessors(SyntaxNode *root, std::vector<SyntaxNode *> &successors) override;
};

class InfiniteLoopSyntaxNode : public SyntaxNode {
protected:
	SyntaxNode *pBody = NULL;

public:
	InfiniteLoopSyntaxNode();
	virtual ~InfiniteLoopSyntaxNode();

	bool    isGoto() override { return false; }
	bool    isBranch() override { return false; }

	int     getNumOutEdges() override { return 0; }
	SyntaxNode *getOutEdge(SyntaxNode *root, int n) override { return NULL; }
	bool    endsWithGoto() override { return false; }
	SyntaxNode *getEnclosingLoop(SyntaxNode *pFor, SyntaxNode *cur = NULL) override {
		if (this == pFor) return cur;
		return pBody->getEnclosingLoop(pFor, this);
	}

	SyntaxNode *clone() override;
	SyntaxNode *replace(SyntaxNode *from, SyntaxNode *to) override;

	void    setBody(SyntaxNode *n) { pBody = n; }

	SyntaxNode *findNodeFor(BasicBlock *bb) override;
	void    printAST(SyntaxNode *root, std::ostream &os) override;
	int     evaluate(SyntaxNode *root) override;
	void    addSuccessors(SyntaxNode *root, std::vector<SyntaxNode *> &successors) override;
};

#endif
