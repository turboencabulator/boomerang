/*
 * \file
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "hllcode.h"

#include "basicblock.h"
#include "exp.h"
#include "operator.h"

#include <iostream>
#include <fstream>

#include <cstdlib>

static int nodecount = 1000;


#define PRINT_BEFORE_AFTER \
	std::ofstream of("before.dot"); \
	of << "digraph before {\n"; \
	root->printAST(root, of); \
	of << "}\n"; \
	of.close(); \
	std::ofstream of1("after.dot"); \
	of1 << "digraph after {\n"; \
	n->printAST(n, of1); \
	of1 << "}\n"; \
	of1.close(); \
	exit(0);

SyntaxNode::SyntaxNode()
{
	nodenum = nodecount++;
}

SyntaxNode::~SyntaxNode()
{
}

int
SyntaxNode::getScore()
{
	if (score == -1)
		score = evaluate(this);
	return score;
}

bool
SyntaxNode::isGoto() const
{
	return pbb && pbb->getType() == ONEWAY && !notGoto;
}

bool
SyntaxNode::isBranch() const
{
	return pbb && pbb->getType() == TWOWAY;
}

BlockSyntaxNode::BlockSyntaxNode()
{
}

BlockSyntaxNode::~BlockSyntaxNode()
{
	for (const auto &stmt : statements)
		delete stmt;
}

int
BlockSyntaxNode::getNumOutEdges() const
{
	if (pbb)
		return pbb->getNumOutEdges();
	if (!statements.empty())
		return statements.back()->getNumOutEdges();
	return 0;
}

const SyntaxNode *
BlockSyntaxNode::getOutEdge(const SyntaxNode *root, int n) const
{
	if (pbb)
		return root->findNodeFor(pbb->getOutEdge(n));
	if (!statements.empty())
		return statements.back()->getOutEdge(root, n);
	return nullptr;
}

const SyntaxNode *
BlockSyntaxNode::findNodeFor(BasicBlock *bb) const
{
	if (pbb == bb)
		return this;
	const SyntaxNode *n = nullptr;
	for (const auto &stmt : statements) {
		n = stmt->findNodeFor(bb);
		if (n)
			break;
	}
	if (n && n == statements.front())
		return this;
	return n;
}

void
BlockSyntaxNode::printAST(const SyntaxNode *root, std::ostream &os) const
{
	os << "\t" << nodenum
	   << " [label=\"";
	if (pbb) {
		switch (pbb->getType()) {
		case ONEWAY:   os << "Oneway";
			if (notGoto) os << " (ignored)";
			break;
		case TWOWAY:   os << "Twoway";        break;
		case NWAY:     os << "Nway";          break;
		case CALL:     os << "Call";          break;
		case RET:      os << "Ret";           break;
		case FALL:     os << "Fall";          break;
		case COMPJUMP: os << "Computed jump"; break;
		case COMPCALL: os << "Computed call"; break;
		case INVALID:  os << "Invalid";       break;
		}
		os << " " << std::hex << pbb->getLowAddr() << std::dec;
	} else
		os << "block";
	os << "\"];\n";
	if (pbb) {
		for (int i = 0; i < pbb->getNumOutEdges(); ++i) {
			BasicBlock *out = pbb->getOutEdge(i);
			const SyntaxNode *to = root->findNodeFor(out);
			assert(to);
			os << "\t" << nodenum
			   << " -> " << to->getNumber()
			   << " [style=dotted";
			if (pbb->getNumOutEdges() > 1)
				os << ",label=\"" << i << "\"";
			os << "];\n";
		}
	} else {
		for (unsigned i = 0; i < statements.size(); ++i)
			statements[i]->printAST(root, os);
		for (unsigned i = 0; i < statements.size(); ++i)
			os << "\t" << nodenum
			   << " -> " << statements[i]->getNumber()
			   << " [label=\"" << i << "\"];\n";
	}
}

#define DEBUG_EVAL 0

int
BlockSyntaxNode::evaluate(const SyntaxNode *root) const
{
#if DEBUG_EVAL
	if (this == root)
		std::cerr << "begin eval =============" << std::endl;
#endif
	if (pbb)
		return 1;
	int n = 1;
	if (statements.size() == 1) {
		const SyntaxNode *out = statements[0]->getOutEdge(root, 0);
		if (out->getBB() && out->getBB()->getNumInEdges() > 1) {
#if DEBUG_EVAL
			std::cerr << "add 15" << std::endl;
#endif
			n += 15;
		} else {
#if DEBUG_EVAL
			std::cerr << "add 30" << std::endl;
#endif
			n += 30;
		}
	}
	for (unsigned i = 0; i < statements.size(); ++i) {
		n += statements[i]->evaluate(root);
		if (statements[i]->isGoto()) {
			if (i != statements.size() - 1) {
#if DEBUG_EVAL
				std::cerr << "add 100" << std::endl;
#endif
				n += 100;
			} else {
#if DEBUG_EVAL
				std::cerr << "add 50" << std::endl;
#endif
				n += 50;
			}
		} else if (statements[i]->isBranch()) {
			const SyntaxNode *loop = root->getEnclosingLoop(this);
			std::cerr << "branch " << statements[i]->getNumber()
			          << " not in loop" << std::endl;
			if (loop) {
				std::cerr << "branch " << statements[i]->getNumber()
				          << " in loop " << loop->getNumber() << std::endl;
				// this is a bit C specific
				const SyntaxNode *out = loop->getOutEdge(root, 0);
				if (out && statements[i]->getOutEdge(root, 0) == out) {
					std::cerr << "found break" << std::endl;
					n += 10;
				}
				if (statements[i]->getOutEdge(root, 0) == loop) {
					std::cerr << "found continue" << std::endl;
					n += 10;
				}
			} else {
#if DEBUG_EVAL
				std::cerr << "add 50" << std::endl;
#endif
				n += 50;
			}
		} else if (i < statements.size() - 1
		        && statements[i]->getOutEdge(root, 0) != statements[i + 1]) {
#if DEBUG_EVAL
			std::cerr << "add 25" << std::endl;
			std::cerr << statements[i]->getNumber() << " -> "
			          << statements[i]->getOutEdge(root, 0)->getNumber()
			          << " not " << statements[i + 1]->getNumber() << std::endl;
#endif
			n += 25;
		}
	}
#if DEBUG_EVAL
	if (this == root)
		std::cerr << "end eval = " << n << " =============" << std::endl;
#endif
	return n;
}

void
BlockSyntaxNode::addSuccessors(const SyntaxNode *root, std::vector<SyntaxNode *> &successors) const
{
	for (unsigned i = 0; i < statements.size(); ++i) {
		if (statements[i]->isBlock()) {
			//BlockSyntaxNode *b = (BlockSyntaxNode*)statements[i];
			// can move previous statements into this block
			if (i > 0) {
				std::cerr << "successor: move previous statement into block" << std::endl;
				SyntaxNode *n = root->clone();
				n->setDepth(root->getDepth() + 1);
				BlockSyntaxNode *b1 = (BlockSyntaxNode *)this->clone();
				BlockSyntaxNode *nb = (BlockSyntaxNode *)b1->getStatement(i);
				b1 = (BlockSyntaxNode *)b1->replace(statements[i - 1], nullptr);
				nb->prependStatement(statements[i - 1]->clone());
				n = n->replace(this, b1);
				successors.push_back(n);
				//PRINT_BEFORE_AFTER
			}
		} else {
			if (statements.size() != 1) {
				// can replace statement with a block containing that statement
				std::cerr << "successor: replace statement with a block containing the statement" << std::endl;
				auto b = new BlockSyntaxNode();
				b->addStatement(statements[i]->clone());
				SyntaxNode *n = root->clone();
				n->setDepth(root->getDepth() + 1);
				n = n->replace(statements[i], b);
				successors.push_back(n);
				//PRINT_BEFORE_AFTER
			}
		}
		// "jump over" style of if-then
		if (i < statements.size() - 2 && statements[i]->isBranch()) {
			SyntaxNode *b = statements[i];
			if (b->getOutEdge(root, 0) == statements[i + 2]
			 && (statements[i + 1]->getOutEdge(root, 0) == statements[i + 2]
			  || statements[i + 1]->endsWithGoto())) {
				std::cerr << "successor: jump over style if then" << std::endl;
				BlockSyntaxNode *b1 = (BlockSyntaxNode *)this->clone();
				b1 = (BlockSyntaxNode *)b1->replace(statements[i + 1], nullptr);
				auto nif = new IfThenSyntaxNode();
				Exp *cond = b->getBB()->getCond();
				cond = new Unary(opLNot, cond->clone());
				cond = cond->simplify();
				nif->setCond(cond);
				nif->setThen(statements[i + 1]->clone());
				nif->setBB(b->getBB());
				b1->setStatement(i, nif);
				SyntaxNode *n = root->clone();
				n->setDepth(root->getDepth() + 1);
				n = n->replace(this, b1);
				successors.push_back(n);
				//PRINT_BEFORE_AFTER
			}
		}
		// if then else
		if (i < statements.size() - 2 && statements[i]->isBranch()) {
			const SyntaxNode *tThen = statements[i]->getOutEdge(root, 0);
			const SyntaxNode *tElse = statements[i]->getOutEdge(root, 1);

			assert(tThen && tElse);
			if (((tThen == statements[i + 2] && tElse == statements[i + 1])
			  || (tThen == statements[i + 1] && tElse == statements[i + 2]))
			 && tThen->getNumOutEdges() == 1 && tElse->getNumOutEdges() == 1) {
				const SyntaxNode *else_out = tElse->getOutEdge(root, 0);
				const SyntaxNode *then_out = tThen->getOutEdge(root, 0);

				if (else_out == then_out) {
					std::cerr << "successor: if then else" << std::endl;
					SyntaxNode *n = root->clone();
					n->setDepth(root->getDepth() + 1);
					n = n->replace(tThen, nullptr);
					n = n->replace(tElse, nullptr);
					auto nif = new IfThenElseSyntaxNode();
					nif->setCond(statements[i]->getBB()->getCond()->clone());
					nif->setBB(statements[i]->getBB());
					nif->setThen(tThen->clone());
					nif->setElse(tElse->clone());
					n = n->replace(statements[i], nif);
					successors.push_back(n);
					//PRINT_BEFORE_AFTER
				}
			}
		}

		// pretested loop
		if (i < statements.size() - 2 && statements[i]->isBranch()) {
			const SyntaxNode *tBody = statements[i]->getOutEdge(root, 0);
			const SyntaxNode *tFollow =  statements[i]->getOutEdge(root, 1);

			assert(tBody && tFollow);
			if (tBody == statements[i + 1] && tFollow == statements[i + 2]
			 && tBody->getNumOutEdges() == 1
			 && tBody->getOutEdge(root, 0) == statements[i]) {
				std::cerr << "successor: pretested loop" << std::endl;
				SyntaxNode *n = root->clone();
				n->setDepth(root->getDepth() + 1);
				n = n->replace(tBody, nullptr);
				auto nloop = new PretestedLoopSyntaxNode();
				nloop->setCond(statements[i]->getBB()->getCond()->clone());
				nloop->setBB(statements[i]->getBB());
				nloop->setBody(tBody->clone());
				n = n->replace(statements[i], nloop);
				successors.push_back(n);
				//PRINT_BEFORE_AFTER
			}
		}

		// posttested loop
		if (i > 0 && i < statements.size() - 1 && statements[i]->isBranch()) {
			const SyntaxNode *tBody = statements[i]->getOutEdge(root, 0);
			const SyntaxNode *tFollow =  statements[i]->getOutEdge(root, 1);

			assert(tBody && tFollow);
			if (tBody == statements[i - 1] && tFollow == statements[i + 1]
			 && tBody->getNumOutEdges() == 1
			 && tBody->getOutEdge(root, 0) == statements[i]) {
				std::cerr << "successor: posttested loop" << std::endl;
				SyntaxNode *n = root->clone();
				n->setDepth(root->getDepth() + 1);
				n = n->replace(tBody, nullptr);
				auto nloop = new PostTestedLoopSyntaxNode();
				nloop->setCond(statements[i]->getBB()->getCond()->clone());
				nloop->setBB(statements[i]->getBB());
				nloop->setBody(tBody->clone());
				n = n->replace(statements[i], nloop);
				successors.push_back(n);
				//PRINT_BEFORE_AFTER
			}
		}

		// infinite loop
		if (statements[i]->getNumOutEdges() == 1
		 && statements[i]->getOutEdge(root, 0) == statements[i]) {
			std::cerr << "successor: infinite loop" << std::endl;
			SyntaxNode *n = root->clone();
			n->setDepth(root->getDepth() + 1);
			auto nloop = new InfiniteLoopSyntaxNode();
			nloop->setBody(statements[i]->clone());
			n = n->replace(statements[i], nloop);
			successors.push_back(n);
			PRINT_BEFORE_AFTER
		}

		statements[i]->addSuccessors(root, successors);
	}
}

SyntaxNode *
BlockSyntaxNode::clone() const
{
	auto b = new BlockSyntaxNode();
	b->correspond = this;
	if (pbb)
		b->pbb = pbb;
	else
		for (const auto &stmt : statements)
			b->addStatement(stmt->clone());
	return b;
}

SyntaxNode *
BlockSyntaxNode::replace(const SyntaxNode *from, SyntaxNode *to)
{
	if (correspond == from)
		return to;

	if (!pbb) {
		std::vector<SyntaxNode *> news;
		for (const auto &stmt : statements) {
			SyntaxNode *n;
			if (stmt->getCorrespond() == from)
				n = to;
			else
				n = stmt->replace(from, to);
			if (n)
				news.push_back(n);
		}
		statements.swap(news);
	}
	return this;
}

IfThenSyntaxNode::IfThenSyntaxNode()
{
}


IfThenSyntaxNode::~IfThenSyntaxNode()
{
	delete pThen;
}

const SyntaxNode *
IfThenSyntaxNode::getOutEdge(const SyntaxNode *root, int n) const
{
	const SyntaxNode *n1 = root->findNodeFor(pbb->getOutEdge(0));
	assert(n1 != pThen);
	return n1;
}

int
IfThenSyntaxNode::evaluate(const SyntaxNode *root) const
{
	int n = 1;
	n += pThen->evaluate(root);
	return n;
}

void
IfThenSyntaxNode::addSuccessors(const SyntaxNode *root, std::vector<SyntaxNode *> &successors) const
{
	pThen->addSuccessors(root, successors);
}

SyntaxNode *
IfThenSyntaxNode::clone() const
{
	auto b = new IfThenSyntaxNode();
	b->correspond = this;
	b->pbb = pbb;
	b->cond = cond->clone();
	b->pThen = pThen->clone();
	return b;
}

SyntaxNode *
IfThenSyntaxNode::replace(const SyntaxNode *from, SyntaxNode *to)
{
	assert(correspond != from);
	if (pThen->getCorrespond() == from) {
		assert(to);
		pThen = to;
	} else
		pThen = pThen->replace(from, to);
	return this;
}

const SyntaxNode *
IfThenSyntaxNode::findNodeFor(BasicBlock *bb) const
{
	if (pbb == bb)
		return this;
	return pThen->findNodeFor(bb);
}

void
IfThenSyntaxNode::printAST(const SyntaxNode *root, std::ostream &os) const
{
	os << "\t" << nodenum
	   << " [label=\"if " << *cond << "\"];\n";
	pThen->printAST(root, os);
	os << "\t" << nodenum
	   << " -> " << pThen->getNumber()
	   << " [label=\"then\"];\n";
	const SyntaxNode *follows = root->findNodeFor(pbb->getOutEdge(0));
	os << "\t" << nodenum
	   << " -> " << follows->getNumber()
	   << " [style=dotted];\n";
}

IfThenElseSyntaxNode::IfThenElseSyntaxNode()
{
}

IfThenElseSyntaxNode::~IfThenElseSyntaxNode()
{
	delete pThen;
	delete pElse;
}

int
IfThenElseSyntaxNode::evaluate(const SyntaxNode *root) const
{
	int n = 1;
	n += pThen->evaluate(root);
	n += pElse->evaluate(root);
	return n;
}

void
IfThenElseSyntaxNode::addSuccessors(const SyntaxNode *root, std::vector<SyntaxNode *> &successors) const
{
	// at the moment we can always ignore gotos at the end of
	// then and else, because we assume they have the same
	// follow
	if (pThen->getNumOutEdges() == 1 && pThen->endsWithGoto()) {
		std::cerr << "successor: ignoring goto at end of then of if then else" << std::endl;
		SyntaxNode *n = root->clone();
		n->setDepth(root->getDepth() + 1);
		SyntaxNode *nThen = pThen->clone();
		nThen->ignoreGoto();
		n = n->replace(pThen, nThen);
		successors.push_back(n);
	}

	if (pElse->getNumOutEdges() == 1 && pElse->endsWithGoto()) {
		std::cerr << "successor: ignoring goto at end of else of if then else" << std::endl;
		SyntaxNode *n = root->clone();
		n->setDepth(root->getDepth() + 1);
		SyntaxNode *nElse = pElse->clone();
		nElse->ignoreGoto();
		n = n->replace(pElse, nElse);
		successors.push_back(n);
	}

	pThen->addSuccessors(root, successors);
	pElse->addSuccessors(root, successors);
}

SyntaxNode *
IfThenElseSyntaxNode::clone() const
{
	auto b = new IfThenElseSyntaxNode();
	b->correspond = this;
	b->pbb = pbb;
	b->cond = cond->clone();
	b->pThen = pThen->clone();
	b->pElse = pElse->clone();
	return b;
}

SyntaxNode *
IfThenElseSyntaxNode::replace(const SyntaxNode *from, SyntaxNode *to)
{
	assert(correspond != from);
	if (pThen->getCorrespond() == from) {
		assert(to);
		pThen = to;
	} else
		pThen = pThen->replace(from, to);
	if (pElse->getCorrespond() == from) {
		assert(to);
		pElse = to;
	} else
		pElse = pElse->replace(from, to);
	return this;
}

const SyntaxNode *
IfThenElseSyntaxNode::findNodeFor(BasicBlock *bb) const
{
	if (pbb == bb)
		return this;
	const SyntaxNode *n = pThen->findNodeFor(bb);
	if (!n)
		n = pElse->findNodeFor(bb);
	return n;
}

void
IfThenElseSyntaxNode::printAST(const SyntaxNode *root, std::ostream &os) const
{
	os << "\t" << nodenum
	   << " [label=\"if " << *cond << "\"];\n";
	pThen->printAST(root, os);
	pElse->printAST(root, os);
	os << "\t" << nodenum
	   << " -> " << pThen->getNumber()
	   << " [label=\"then\"];\n";
	os << "\t" << nodenum
	   << " -> " << pElse->getNumber()
	   << " [label=\"else\"];\n";
}


PretestedLoopSyntaxNode::PretestedLoopSyntaxNode()
{
}

PretestedLoopSyntaxNode::~PretestedLoopSyntaxNode()
{
	delete pBody;
	delete cond;
}

const SyntaxNode *
PretestedLoopSyntaxNode::getOutEdge(const SyntaxNode *root, int n) const
{
	return root->findNodeFor(pbb->getOutEdge(1));
}

int
PretestedLoopSyntaxNode::evaluate(const SyntaxNode *root) const
{
	int n = 1;
	n += pBody->evaluate(root);
	return n;
}

void
PretestedLoopSyntaxNode::addSuccessors(const SyntaxNode *root, std::vector<SyntaxNode *> &successors) const
{
	// we can always ignore gotos at the end of the body.
	if (pBody->getNumOutEdges() == 1 && pBody->endsWithGoto()) {
		std::cerr << "successor: ignoring goto at end of body of pretested loop" << std::endl;
		const SyntaxNode *out = pBody->getOutEdge(root, 0);
		assert(out->startsWith(this));
		SyntaxNode *n = root->clone();
		n->setDepth(root->getDepth() + 1);
		SyntaxNode *nBody = pBody->clone();
		nBody->ignoreGoto();
		n = n->replace(pBody, nBody);
		successors.push_back(n);
	}

	pBody->addSuccessors(root, successors);
}

SyntaxNode *
PretestedLoopSyntaxNode::clone() const
{
	auto b = new PretestedLoopSyntaxNode();
	b->correspond = this;
	b->pbb = pbb;
	b->cond = cond->clone();
	b->pBody = pBody->clone();
	return b;
}

SyntaxNode *
PretestedLoopSyntaxNode::replace(const SyntaxNode *from, SyntaxNode *to)
{
	assert(correspond != from);
	if (pBody->getCorrespond() == from) {
		assert(to);
		pBody = to;
	} else
		pBody = pBody->replace(from, to);
	return this;
}

const SyntaxNode *
PretestedLoopSyntaxNode::findNodeFor(BasicBlock *bb) const
{
	if (pbb == bb)
		return this;
	return pBody->findNodeFor(bb);
}

void
PretestedLoopSyntaxNode::printAST(const SyntaxNode *root, std::ostream &os) const
{
	os << "\t" << nodenum
	   << " [label=\"loop pretested " << *cond << "\"];\n";
	pBody->printAST(root, os);
	os << "\t" << nodenum
	   << " -> " << pBody->getNumber()
	   << ";\n";
	os << "\t" << nodenum
	   << " -> " << getOutEdge(root, 0)->getNumber()
	   << " [style=dotted];\n";
}

PostTestedLoopSyntaxNode::PostTestedLoopSyntaxNode()
{
}

PostTestedLoopSyntaxNode::~PostTestedLoopSyntaxNode()
{
	delete pBody;
	delete cond;
}

const SyntaxNode *
PostTestedLoopSyntaxNode::getOutEdge(const SyntaxNode *root, int n) const
{
	return root->findNodeFor(pbb->getOutEdge(1));
}

int
PostTestedLoopSyntaxNode::evaluate(const SyntaxNode *root) const
{
	int n = 1;
	n += pBody->evaluate(root);
	return n;
}

void
PostTestedLoopSyntaxNode::addSuccessors(const SyntaxNode *root, std::vector<SyntaxNode *> &successors) const
{
	// we can always ignore gotos at the end of the body.
	if (pBody->getNumOutEdges() == 1 && pBody->endsWithGoto()) {
		std::cerr << "successor: ignoring goto at end of body of posttested loop" << std::endl;
		assert(pBody->getOutEdge(root, 0) == this);
		SyntaxNode *n = root->clone();
		n->setDepth(root->getDepth() + 1);
		SyntaxNode *nBody = pBody->clone();
		nBody->ignoreGoto();
		n = n->replace(pBody, nBody);
		successors.push_back(n);
	}

	pBody->addSuccessors(root, successors);
}

SyntaxNode *
PostTestedLoopSyntaxNode::clone() const
{
	auto b = new PostTestedLoopSyntaxNode();
	b->correspond = this;
	b->pbb = pbb;
	b->cond = cond->clone();
	b->pBody = pBody->clone();
	return b;
}

SyntaxNode *
PostTestedLoopSyntaxNode::replace(const SyntaxNode *from, SyntaxNode *to)
{
	assert(correspond != from);
	if (pBody->getCorrespond() == from) {
		assert(to);
		pBody = to;
	} else
		pBody = pBody->replace(from, to);
	return this;
}

const SyntaxNode *
PostTestedLoopSyntaxNode::findNodeFor(BasicBlock *bb) const
{
	if (pbb == bb)
		return this;
	const SyntaxNode *n = pBody->findNodeFor(bb);
	if (n == pBody)
		return this;
	return n;
}

void
PostTestedLoopSyntaxNode::printAST(const SyntaxNode *root, std::ostream &os) const
{
	os << "\t" << nodenum
	   << " [label=\"loop posttested " << *cond << "\"];\n";
	pBody->printAST(root, os);
	os << "\t" << nodenum
	   << " -> " << pBody->getNumber()
	   << ";\n";
	os << "\t" << nodenum
	   << " -> " << getOutEdge(root, 0)->getNumber()
	   << " [style=dotted];\n";
}

InfiniteLoopSyntaxNode::InfiniteLoopSyntaxNode()
{
}

InfiniteLoopSyntaxNode::~InfiniteLoopSyntaxNode()
{
	delete pBody;
}

int
InfiniteLoopSyntaxNode::evaluate(const SyntaxNode *root) const
{
	int n = 1;
	n += pBody->evaluate(root);
	return n;
}

void
InfiniteLoopSyntaxNode::addSuccessors(const SyntaxNode *root, std::vector<SyntaxNode *> &successors) const
{
	// we can always ignore gotos at the end of the body.
	if (pBody->getNumOutEdges() == 1 && pBody->endsWithGoto()) {
		std::cerr << "successor: ignoring goto at end of body of infinite loop" << std::endl;
		assert(pBody->getOutEdge(root, 0) == this);
		SyntaxNode *n = root->clone();
		n->setDepth(root->getDepth() + 1);
		SyntaxNode *nBody = pBody->clone();
		nBody->ignoreGoto();
		n = n->replace(pBody, nBody);
		successors.push_back(n);
	}

	pBody->addSuccessors(root, successors);
}

SyntaxNode *
InfiniteLoopSyntaxNode::clone() const
{
	auto b = new InfiniteLoopSyntaxNode();
	b->correspond = this;
	b->pbb = pbb;
	b->pBody = pBody->clone();
	return b;
}

SyntaxNode *
InfiniteLoopSyntaxNode::replace(const SyntaxNode *from, SyntaxNode *to)
{
	assert(correspond != from);
	if (pBody->getCorrespond() == from) {
		assert(to);
		pBody = to;
	} else
		pBody = pBody->replace(from, to);
	return this;
}

const SyntaxNode *
InfiniteLoopSyntaxNode::findNodeFor(BasicBlock *bb) const
{
	if (pbb == bb)
		return this;
	const SyntaxNode *n = pBody->findNodeFor(bb);
	if (n == pBody)
		return this;
	return n;
}

void
InfiniteLoopSyntaxNode::printAST(const SyntaxNode *root, std::ostream &os) const
{
	os << "\t" << nodenum
	   << " [label=\"loop infinite\"];\n";
	if (pBody)
		pBody->printAST(root, os);
	if (pBody)
		os << "\t" << nodenum
		   << " -> " << pBody->getNumber()
		   << ";\n";
}
