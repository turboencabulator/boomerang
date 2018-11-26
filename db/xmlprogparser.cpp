/**
 * \file
 * \brief Implementation of the XMLProgParser and related classes.
 *
 * \authors
 * Copyright (C) 2004, Trent Waddington
 * \authors
 * Copyright (C) 2016, Kyle Guinn
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "xmlprogparser.h"

#include "boomerang.h"
#include "cluster.h"
#include "frontend.h"
#include "log.h"
#include "proc.h"
#include "prog.h"
#include "rtl.h"
#include "sigenum.h"
#include "signature.h"
#include "statement.h"
#include "type.h"

#ifdef ENABLE_XML_LOAD
#include <expat.h>
#endif

#include <fstream>
#include <iostream>

#include <cctype>
#include <cstring>

extern const char *operStrings[];

#ifdef ENABLE_XML_LOAD
typedef enum {
	e_prog, e_procs, e_global, e_cluster, e_libproc, e_userproc, e_local, e_symbol, e_secondexp,
	e_proven_true, e_callee, e_caller, e_defines,
	e_signature, e_param, e_return, e_rettype, e_prefreturn, e_prefparam,
	e_cfg, e_bb, e_inedge, e_outedge, e_livein, e_order, e_revorder,
	e_rtl, e_stmt, e_assign, e_assignment, e_phiassign, e_lhs, e_rhs,
	e_callstmt, e_dest, e_argument, e_returnexp, e_returntype,
	e_returnstmt, e_returns, e_modifieds,
	e_gotostmt, e_branchstmt, e_cond,
	e_casestmt,
	e_boolasgn,
	e_type, e_exp,
	e_voidtype, e_integertype, e_pointertype, e_chartype, e_namedtype, e_arraytype, e_basetype, e_sizetype,
	e_location, e_unary, e_binary, e_ternary, e_const, e_terminal, e_typedexp, e_refexp, e_def,
	e_subexp1, e_subexp2, e_subexp3, e_unknown = -1
} xmlElement;

#define TAG(x) { #x, &XMLProgParser::start_##x, &XMLProgParser::addChildTo_##x }

_tag XMLProgParser::tags[] = {
	TAG(prog),
	TAG(procs),
	TAG(global),
	TAG(cluster),
	TAG(libproc),
	TAG(userproc),
	TAG(local),
	TAG(symbol),
	TAG(secondexp),
	TAG(proven_true),
	TAG(callee),
	TAG(caller),
	TAG(defines),
	TAG(signature),
	TAG(param),
	TAG(return),
	TAG(rettype),
	TAG(prefreturn),
	TAG(prefparam),
	TAG(cfg),
	TAG(bb),
	TAG(inedge),
	TAG(outedge),
	TAG(livein),
	TAG(order),
	TAG(revorder),
	TAG(rtl),
	TAG(stmt),
	TAG(assign),
	TAG(assignment),
	TAG(phiassign),
	TAG(lhs),
	TAG(rhs),
	TAG(callstmt),
	TAG(dest),
	TAG(argument),
	TAG(returnexp),
	TAG(returntype),
	TAG(returnstmt),
	TAG(returns),
	TAG(modifieds),
	TAG(gotostmt),
	TAG(branchstmt),
	TAG(cond),
	TAG(casestmt),
	TAG(boolasgn),
	TAG(type),
	TAG(exp),
	TAG(voidtype),
	TAG(integertype),
	TAG(pointertype),
	TAG(chartype),
	TAG(namedtype),
	TAG(arraytype),
	TAG(basetype),
	TAG(sizetype),
	TAG(location),
	TAG(unary),
	TAG(binary),
	TAG(ternary),
	TAG(const),
	TAG(terminal),
	TAG(typedexp),
	TAG(refexp),
	TAG(def),
	TAG(subexp1),
	TAG(subexp2),
	TAG(subexp3),
	{ 0, 0, 0 }
};

class Context {
public:
	int tag;
	int n;
	std::string str;
	Prog *prog = nullptr;
	Global *global;
	Cluster *cluster;
	Proc *proc = nullptr;
	Signature *signature = nullptr;
	Cfg *cfg = nullptr;
	BasicBlock *bb = nullptr;
	RTL *rtl = nullptr;
	Statement *stmt = nullptr;
	Parameter *param = nullptr;
	// ImplicitParameter *implicitParam = nullptr;
	Return *ret = nullptr;
	Type *type = nullptr;
	Exp *exp = nullptr, *symbol;
	std::list<Proc *> procs;

	Context(int tag) : tag(tag) { }
};

static void XMLCALL
start(void *data, const char *el, const char **attr)
{
	((XMLProgParser *)data)->handleElementStart(el, attr);
}

static void XMLCALL
end(void *data, const char *el)
{
	((XMLProgParser *)data)->handleElementEnd(el);
}

static void XMLCALL
text(void *data, const char *s, int len)
{
	// Strip leading/trailing whitespace
	while (len && std::isspace(*s)) { ++s; --len; }
	if (!len) return;
	while (std::isspace(s[len - 1])) --len;

	std::cerr << "error: text in document " << len << " bytes (";
	std::cerr.write(s, len);  // s is not NUL-terminated
	std::cerr << ")\n";
}

const char *
XMLProgParser::getAttr(const char **attr, const char *name)
{
	for (int i = 0; attr[i]; i += 2)
		if (!strcmp(attr[i], name))
			return attr[i + 1];
	return nullptr;
}

void
XMLProgParser::handleElementStart(const char *el, const char **attr)
{
	for (int i = 0; tags[i].tag; ++i)
		if (!strcmp(el, tags[i].tag)) {
			//std::cerr << "got tag: " << el << "\n";
			stack.push(new Context(i));
			(this->*tags[i].start)(stack.top(), attr);
			return;
		}
	std::cerr << "got unknown tag: " << el << "\n";
	stack.push(new Context(e_unknown));
}

void
XMLProgParser::handleElementEnd(const char *el)
{
	//std::cerr << "end tag: " << el << " tos: " << stack.top()->tag << "\n";
	if (stack.size() >= 2) {
		Context *child = stack.top();
		stack.pop();
		Context *node = stack.top();
		if (node->tag != e_unknown) {
			//std::cerr << " second: " << node->tag << "\n";
			(this->*tags[node->tag].addChildTo)(node, child);
		}
	}
}

void
XMLProgParser::addChildStub(Context *node, const Context *child) const
{
	if (child->tag == e_unknown)
		std::cerr << "unknown tag";
	else
		std::cerr << "need to handle tag " << tags[child->tag].tag;
	std::cerr << " in context " << tags[node->tag].tag << "\n";
}

Prog *
XMLProgParser::parse(const char *filename)
{
	auto f = std::ifstream(filename);
	if (!f)
		return nullptr;
	f.close();

	while (!stack.empty())
		stack.pop();
	Prog *prog = nullptr;
	for (phase = 0; phase < 2; ++phase) {
		parseFile(filename);
		if (stack.top()->prog) {
			prog = stack.top()->prog;
			parseChildren(prog->getRootCluster());
		}
	}
	if (!prog)
		return nullptr;
	//auto pFE = FrontEnd::open(prog->getPath(), prog);  // Path is usually empty!?
	auto pFE = FrontEnd::open(prog->getPathAndName(), prog);
	return prog;
}

void
XMLProgParser::parseFile(const char *filename)
{
	auto f = std::ifstream(filename);
	if (!f)
		return;
	XML_Parser p = XML_ParserCreate(nullptr);
	if (!p) {
		std::cerr << "Couldn't allocate memory for parser\n";
		return;
	}

	XML_SetUserData(p, this);
	XML_SetElementHandler(p, start, end);
	XML_SetCharacterDataHandler(p, text);

	char Buff[8192];

	for (;;) {
		f.read(Buff, sizeof Buff);
		if (f.bad()) {
			std::cerr << "Read error\n";
			break;
		}

		auto len = f.gcount();
		auto done = f.eof();
		if (XML_Parse(p, Buff, len, done) == XML_STATUS_ERROR) {
			if (XML_GetErrorCode(p) != XML_ERROR_NO_ELEMENTS)
				std::cerr << "Parse error at line " << XML_GetCurrentLineNumber(p)
				          << " of file " << filename << ":\n"
				          << XML_ErrorString(XML_GetErrorCode(p)) << "\n";
			break;
		}

		if (done)
			break;
	}
	f.close();
}

void
XMLProgParser::parseChildren(Cluster *c)
{
	std::string path = c->makeDirs();
	for (const auto &child : c->children) {
		std::string d = path + "/" + child->getName() + ".xml";
		parseFile(d.c_str());
		parseChildren(child);
	}
}

int
XMLProgParser::operFromString(const char *s)
{
	for (int i = 0; i < opNumOf; ++i)
		if (!strcmp(s, operStrings[i]))
			return i;
	return -1;
}

void
XMLProgParser::addId(const char **attr, void *x)
{
	const char *val = getAttr(attr, "id");
	if (val) {
		//std::cerr << "map id " << val << " to " << std::hex << (int)x << std::dec << "\n";
		idToX[atoi(val)] = x;
	}
}

void *
XMLProgParser::findId(const char *id) const
{
	if (!id)
		return nullptr;
	int n = atoi(id);
	if (n == 0)
		return nullptr;
	auto it = idToX.find(n);
	if (it == idToX.cend()) {
		std::cerr << "findId could not find \"" << id << "\"\n";
		assert(false);
		return nullptr;
	}
	return it->second;
}


void
XMLProgParser::start_prog(Context *node, const char **attr)
{
	if (phase == 1) {
		return;
	}
	node->prog = new Prog();
	const char *name = getAttr(attr, "name");
	if (name)
		node->prog->setName(name);
	name = getAttr(attr, "path");
	if (name)
		node->prog->m_path = name;
	const char *iNumberedProc = getAttr(attr, "iNumberedProc");
	node->prog->m_iNumberedProc = atoi(iNumberedProc);
}

void
XMLProgParser::addChildTo_prog(Context *node, const Context *child) const
{
	if (phase == 1) {
		switch (child->tag) {
		case e_libproc:
		case e_userproc:
			Boomerang::get()->alert_load(child->proc);
			break;
		}
		return;
	}
	switch (child->tag) {
	case e_libproc:
		child->proc->setProg(node->prog);
		node->prog->m_procs.push_back(child->proc);
		node->prog->m_procLabels[child->proc->getNativeAddress()] = child->proc;
		break;
	case e_userproc:
		child->proc->setProg(node->prog);
		node->prog->m_procs.push_back(child->proc);
		node->prog->m_procLabels[child->proc->getNativeAddress()] = child->proc;
		break;
	case e_procs:
		for (const auto &proc : child->procs) {
			node->prog->m_procs.push_back(proc);
			node->prog->m_procLabels[proc->getNativeAddress()] = proc;
			Boomerang::get()->alert_load(proc);
		}
		break;
	case e_cluster:
		node->prog->m_rootCluster = child->cluster;
		break;
	case e_global:
		node->prog->globals.insert(child->global);
		break;
	default:
		addChildStub(node, child);
		break;
	}
}

void
XMLProgParser::start_procs(Context *node, const char **attr)
{
	if (phase == 1) {
		return;
	}
	node->procs.clear();
}

void
XMLProgParser::addChildTo_procs(Context *node, const Context *child) const
{
	if (phase == 1) {
		return;
	}
	switch (child->tag) {
	case e_libproc:
		node->procs.push_back(child->proc);
		break;
	case e_userproc:
		node->procs.push_back(child->proc);
		break;
	default:
		addChildStub(node, child);
		break;
	}
}

void
XMLProgParser::start_global(Context *node, const char **attr)
{
	if (phase == 1) {
		return;
	}
	node->global = new Global();
	const char *name = getAttr(attr, "name");
	if (name)
		node->global->nam = name;
	const char *uaddr = getAttr(attr, "uaddr");
	if (uaddr)
		node->global->uaddr = atoi(uaddr);
}

void
XMLProgParser::addChildTo_global(Context *node, const Context *child) const
{
	if (phase == 1) {
		return;
	}
	switch (child->tag) {
	case e_type:
		node->global->type = child->type;
		break;
	default:
		addChildStub(node, child);
		break;
	}
}

void
XMLProgParser::start_cluster(Context *node, const char **attr)
{
	if (phase == 1) {
		node->cluster = (Cluster *)findId(getAttr(attr, "id"));
		return;
	}
	node->cluster = new Cluster();
	addId(attr, node->cluster);
	const char *name = getAttr(attr, "name");
	if (name)
		node->cluster->setName(name);
}

void
XMLProgParser::addChildTo_cluster(Context *node, const Context *child) const
{
	if (phase == 1) {
		return;
	}
	switch (child->tag) {
	case e_cluster:
		node->cluster->addChild(child->cluster);
		break;
	default:
		addChildStub(node, child);
		break;
	}
}

void
XMLProgParser::start_libproc(Context *node, const char **attr)
{
	if (phase == 1) {
		node->proc = (Proc *)findId(getAttr(attr, "id"));
		Proc *p = (Proc *)findId(getAttr(attr, "firstCaller"));
		if (p)
			node->proc->m_firstCaller = p;
		Cluster *c = (Cluster *)findId(getAttr(attr, "cluster"));
		if (c)
			node->proc->cluster = c;
		return;
	}
	node->proc = new LibProc();
	addId(attr, node->proc);
	const char *address = getAttr(attr, "address");
	if (address)
		node->proc->address = atoi(address);
	address = getAttr(attr, "firstCallerAddress");
	if (address)
		node->proc->m_firstCallerAddr = atoi(address);
}

void
XMLProgParser::addChildTo_libproc(Context *node, const Context *child) const
{
	if (phase == 1) {
		switch (child->tag) {
		case e_caller:
			auto call = dynamic_cast<CallStatement *>(child->stmt);
			assert(call);
			node->proc->addCaller(call);
			break;
		}
		return;
	}
	switch (child->tag) {
	case e_signature:
		node->proc->setSignature(child->signature);
		break;
	case e_proven_true:
		node->proc->setProvenTrue(child->exp);
		break;
	case e_caller:
		break;
	default:
		addChildStub(node, child);
		break;
	}
}

void
XMLProgParser::start_userproc(Context *node, const char **attr)
{
	if (phase == 1) {
		node->proc = (Proc *)findId(getAttr(attr, "id"));
		auto u = dynamic_cast<UserProc *>(node->proc);
		assert(u);
		Proc *p = (Proc *)findId(getAttr(attr, "firstCaller"));
		if (p)
			u->m_firstCaller = p;
		Cluster *c = (Cluster *)findId(getAttr(attr, "cluster"));
		if (c)
			u->cluster = c;
		ReturnStatement *r = (ReturnStatement *)findId(getAttr(attr, "retstmt"));
		if (r)
			u->theReturnStatement = r;
		return;
	}
	auto proc = new UserProc();
	node->proc = proc;
	addId(attr, proc);

	const char *address = getAttr(attr, "address");
	if (address)
		proc->address = atoi(address);
	address = getAttr(attr, "status");
	if (address)
		proc->status = (ProcStatus)atoi(address);
	address = getAttr(attr, "firstCallerAddress");
	if (address)
		proc->m_firstCallerAddr = atoi(address);
}

void
XMLProgParser::addChildTo_userproc(Context *node, const Context *child) const
{
	auto userproc = dynamic_cast<UserProc *>(node->proc);
	assert(userproc);
	if (phase == 1) {
		switch (child->tag) {
		case e_caller:
			{
				auto call = dynamic_cast<CallStatement *>(child->stmt);
				assert(call);
				node->proc->addCaller(call);
			}
			break;
		case e_callee:
			userproc->addCallee(child->proc);
			break;
		}
		return;
	}
	switch (child->tag) {
	case e_signature:
		node->proc->setSignature(child->signature);
		break;
	case e_proven_true:
		node->proc->setProvenTrue(child->exp);
		break;
	case e_caller:
		break;
	case e_callee:
		break;
	case e_cfg:
		userproc->setCFG(child->cfg);
		break;
	case e_local:
		userproc->locals[child->str.c_str()] = child->type;
		break;
	case e_symbol:
		userproc->mapSymbolTo(child->exp, child->symbol);
		break;
	default:
		addChildStub(node, child);
		break;
	}
}

void
XMLProgParser::start_local(Context *node, const char **attr)
{
	if (phase == 1) {
		return;
	}
	node->str = getAttr(attr, "name");
}

void
XMLProgParser::addChildTo_local(Context *node, const Context *child) const
{
	if (phase == 1) {
		return;
	}
	switch (child->tag) {
	case e_type:
		node->type = child->type;
		break;
	default:
		addChildStub(node, child);
		break;
	}
}

void
XMLProgParser::start_symbol(Context *node, const char **attr)
{
	if (phase == 1) {
		return;
	}
}

void
XMLProgParser::addChildTo_symbol(Context *node, const Context *child) const
{
	if (phase == 1) {
		return;
	}
	switch (child->tag) {
	case e_exp:
		node->exp = child->exp;
		break;
	case e_secondexp:
		node->symbol = child->exp;
		break;
	default:
		addChildStub(node, child);
		break;
	}
}

void
XMLProgParser::start_secondexp(Context *node, const char **attr)
{
}

void
XMLProgParser::addChildTo_secondexp(Context *node, const Context *child) const
{
	node->exp = child->exp;
}

void
XMLProgParser::start_proven_true(Context *node, const char **attr)
{
}

void
XMLProgParser::addChildTo_proven_true(Context *node, const Context *child) const
{
	node->exp = child->exp;
}

void
XMLProgParser::start_callee(Context *node, const char **attr)
{
	if (phase == 1) {
		node->proc = (Proc *)findId(getAttr(attr, "proc"));
	}
}

void
XMLProgParser::addChildTo_callee(Context *node, const Context *child) const
{
}

void
XMLProgParser::start_caller(Context *node, const char **attr)
{
	if (phase == 1) {
		node->stmt = (Statement *)findId(getAttr(attr, "call"));
	}
}

void
XMLProgParser::addChildTo_caller(Context *node, const Context *child) const
{
}

void
XMLProgParser::start_defines(Context *node, const char **attr)
{
}

void
XMLProgParser::addChildTo_defines(Context *node, const Context *child) const
{
	node->exp = child->exp;
}

void
XMLProgParser::start_signature(Context *node, const char **attr)
{
	if (phase == 1) {
		node->signature = (Signature *)findId(getAttr(attr, "id"));
		return;
	}
	const char *plat = getAttr(attr, "platform");
	const char *convention = getAttr(attr, "convention");
	const char *name = getAttr(attr, "name");

	Signature *sig;
	if (plat && convention) {
		platform p;
		callconv c;
		if (!strcmp(plat, "pentium"))
			p = PLAT_PENTIUM;
		else if (!strcmp(plat, "sparc"))
			p = PLAT_SPARC;
		else if (!strcmp(plat, "ppc"))
			p = PLAT_PPC;
		else if (!strcmp(plat, "st20"))
			p = PLAT_ST20;
		else {
			std::cerr << "unknown platform: " << plat << "\n";
			assert(false);
			p = PLAT_PENTIUM;
		}
		if (!strcmp(convention, "stdc"))
			c = CONV_C;
		else if (!strcmp(convention, "pascal"))
			c = CONV_PASCAL;
		else if (!strcmp(convention, "thiscall"))
			c = CONV_THISCALL;
		else {
			std::cerr << "unknown convention: " << convention << "\n";
			assert(false);
			c = CONV_C;
		}
		sig = Signature::instantiate(p, c, name);
	} else
		sig = new Signature(name);
	sig->params.clear();
	// sig->implicitParams.clear();
	sig->returns.clear();
	node->signature = sig;
	addId(attr, sig);
	const char *n = getAttr(attr, "ellipsis");
	if (n)
		sig->ellipsis = atoi(n) > 0;
	n = getAttr(attr, "preferedName");
	if (n)
		sig->preferedName = n;
}

void
XMLProgParser::addChildTo_signature(Context *node, const Context *child) const
{
	if (phase == 1) {
		return;
	}
	switch (child->tag) {
	case e_param:
		node->signature->appendParameter(child->param);
		break;
	case e_return:
		node->signature->appendReturn(child->ret);
		break;
	case e_rettype:
		node->signature->setRetType(child->type);
		break;
	case e_prefparam:
		node->signature->preferedParams.push_back(child->n);
		break;
	case e_prefreturn:
		node->signature->preferedReturn = child->type;
		break;
	default:
		addChildStub(node, child);
		break;
	}
}

void
XMLProgParser::start_param(Context *node, const char **attr)
{
	if (phase == 1) {
		node->param = (Parameter *)findId(getAttr(attr, "id"));
		return;
	}
	node->param = new Parameter();
	addId(attr, node->param);
	const char *n = getAttr(attr, "name");
	if (n)
		node->param->setName(n);
}

void
XMLProgParser::addChildTo_param(Context *node, const Context *child) const
{
	if (phase == 1) {
		return;
	}
	switch (child->tag) {
	case e_type:
		node->param->setType(child->type);
		break;
	case e_exp:
		node->param->setExp(child->exp);
		break;
	default:
		addChildStub(node, child);
		break;
	}
}

void
XMLProgParser::start_return(Context *node, const char **attr)
{
	if (phase == 1) {
		node->ret = (Return *)findId(getAttr(attr, "id"));
		return;
	}
	node->ret = new Return();
	addId(attr, node->ret);
}

void
XMLProgParser::addChildTo_return(Context *node, const Context *child) const
{
	if (phase == 1) {
		return;
	}
	switch (child->tag) {
	case e_type:
		node->ret->type = child->type;
		break;
	case e_exp:
		node->ret->exp = child->exp;
		break;
	default:
		addChildStub(node, child);
		break;
	}
}

void
XMLProgParser::start_rettype(Context *node, const char **attr)
{
}

void
XMLProgParser::addChildTo_rettype(Context *node, const Context *child) const
{
	node->type = child->type;
}

void
XMLProgParser::start_prefreturn(Context *node, const char **attr)
{
	if (phase == 1) {
		return;
	}
}

void
XMLProgParser::addChildTo_prefreturn(Context *node, const Context *child) const
{
	if (phase == 1) {
		return;
	}
	node->type = child->type;
}

void
XMLProgParser::start_prefparam(Context *node, const char **attr)
{
	if (phase == 1) {
		return;
	}
	const char *n = getAttr(attr, "index");
	assert(n);
	node->n = atoi(n);
}

void
XMLProgParser::addChildTo_prefparam(Context *node, const Context *child) const
{
	if (phase == 1) {
		return;
	}
	//switch (child->tag) {
	//default:
		addChildStub(node, child);
		//break;
	//}
}

void
XMLProgParser::start_cfg(Context *node, const char **attr)
{
	if (phase == 1) {
		node->cfg = (Cfg *)findId(getAttr(attr, "id"));
		BasicBlock *entryBB = (BasicBlock *)findId(getAttr(attr, "entryBB"));
		if (entryBB)
			node->cfg->setEntryBB(entryBB);
		BasicBlock *exitBB = (BasicBlock *)findId(getAttr(attr, "exitBB"));
		if (exitBB)
			node->cfg->setExitBB(exitBB);
		return;
	}
	auto cfg = new Cfg();
	node->cfg = cfg;
	addId(attr, cfg);

	const char *str = getAttr(attr, "wellformed");
	if (str)
		cfg->m_bWellFormed = atoi(str) > 0;
	str = getAttr(attr, "lastLabel");
	if (str)
		cfg->lastLabel = atoi(str);
}

void
XMLProgParser::addChildTo_cfg(Context *node, const Context *child) const
{
	if (phase == 1) {
		switch (child->tag) {
		case e_order:
			node->cfg->Ordering.push_back(child->bb);
			break;
		case e_revorder:
			node->cfg->revOrdering.push_back(child->bb);
			break;
		}
		return;
	}
	switch (child->tag) {
	case e_bb:
		node->cfg->addBB(child->bb);
		break;
	case e_order:
		break;
	case e_revorder:
		break;
	default:
		addChildStub(node, child);
		break;
	}
}

void
XMLProgParser::start_bb(Context *node, const char **attr)
{
	BasicBlock *bb;
	if (phase == 1) {
		bb = node->bb = (BasicBlock *)findId(getAttr(attr, "id"));
		BasicBlock *h = (BasicBlock *)findId(getAttr(attr, "m_loopHead"));
		if (h)
			bb->m_loopHead = h;
		h = (BasicBlock *)findId(getAttr(attr, "m_caseHead"));
		if (h)
			bb->m_caseHead = h;
		h = (BasicBlock *)findId(getAttr(attr, "m_condFollow"));
		if (h)
			bb->m_condFollow = h;
		h = (BasicBlock *)findId(getAttr(attr, "m_loopFollow"));
		if (h)
			bb->m_loopFollow = h;
		h = (BasicBlock *)findId(getAttr(attr, "m_latchNode"));
		if (h)
			bb->m_latchNode = h;
		// note the ridiculous duplication here
		h = (BasicBlock *)findId(getAttr(attr, "immPDom"));
		if (h)
			bb->immPDom = h;
		h = (BasicBlock *)findId(getAttr(attr, "loopHead"));
		if (h)
			bb->loopHead = h;
		h = (BasicBlock *)findId(getAttr(attr, "caseHead"));
		if (h)
			bb->caseHead = h;
		h = (BasicBlock *)findId(getAttr(attr, "condFollow"));
		if (h)
			bb->condFollow = h;
		h = (BasicBlock *)findId(getAttr(attr, "loopFollow"));
		if (h)
			bb->loopFollow = h;
		h = (BasicBlock *)findId(getAttr(attr, "latchNode"));
		if (h)
			bb->latchNode = h;
		return;
	}
	bb = new BasicBlock();
	node->bb = bb;
	addId(attr, bb);

	const char *str = getAttr(attr, "nodeType");
	if (str)
		bb->m_nodeType = (BBTYPE)atoi(str);
	str = getAttr(attr, "labelNum");
	if (str)
		bb->m_iLabelNum = atoi(str);
	str = getAttr(attr, "label");
	if (str)
		bb->m_labelStr = strdup(str);
	str = getAttr(attr, "labelneeded");
	if (str)
		bb->m_labelneeded = atoi(str) > 0;
	str = getAttr(attr, "incomplete");
	if (str)
		bb->m_bIncomplete = atoi(str) > 0;
	str = getAttr(attr, "jumpreqd");
	if (str)
		bb->m_bJumpReqd = atoi(str) > 0;
	str = getAttr(attr, "m_traversed");
	if (str)
		bb->m_iTraversed = atoi(str) > 0;
	str = getAttr(attr, "DFTfirst");
	if (str)
		bb->m_DFTfirst = atoi(str);
	str = getAttr(attr, "DFTlast");
	if (str)
		bb->m_DFTlast = atoi(str);
	str = getAttr(attr, "DFTrevfirst");
	if (str)
		bb->m_DFTrevfirst = atoi(str);
	str = getAttr(attr, "DFTrevlast");
	if (str)
		bb->m_DFTrevlast = atoi(str);
	str = getAttr(attr, "structType");
	if (str)
		bb->m_structType = (SBBTYPE)atoi(str);
	str = getAttr(attr, "loopCondType");
	if (str)
		bb->m_loopCondType = (SBBTYPE)atoi(str);
	str = getAttr(attr, "ord");
	if (str)
		bb->ord = atoi(str);
	str = getAttr(attr, "revOrd");
	if (str)
		bb->revOrd = atoi(str);
	str = getAttr(attr, "inEdgesVisited");
	if (str)
		bb->inEdgesVisited = atoi(str);
	str = getAttr(attr, "numForwardInEdges");
	if (str)
		bb->numForwardInEdges = atoi(str);
	str = getAttr(attr, "loopStamp1");
	if (str)
		bb->loopStamps[0] = atoi(str);
	str = getAttr(attr, "loopStamp2");
	if (str)
		bb->loopStamps[1] = atoi(str);
	str = getAttr(attr, "revLoopStamp1");
	if (str)
		bb->revLoopStamps[0] = atoi(str);
	str = getAttr(attr, "revLoopStamp2");
	if (str)
		bb->revLoopStamps[1] = atoi(str);
	str = getAttr(attr, "traversed");
	if (str)
		bb->traversed = (travType)atoi(str);
	str = getAttr(attr, "hllLabel");
	if (str)
		bb->hllLabel = atoi(str) > 0;
	str = getAttr(attr, "labelStr");
	if (str)
		bb->labelStr = strdup(str);
	str = getAttr(attr, "indentLevel");
	if (str)
		bb->indentLevel = atoi(str);
	str = getAttr(attr, "sType");
	if (str)
		bb->sType = (structType)atoi(str);
	str = getAttr(attr, "usType");
	if (str)
		bb->usType = (unstructType)atoi(str);
	str = getAttr(attr, "lType");
	if (str)
		bb->lType = (loopType)atoi(str);
	str = getAttr(attr, "cType");
	if (str)
		bb->cType = (condType)atoi(str);
}

void
XMLProgParser::addChildTo_bb(Context *node, const Context *child) const
{
	if (phase == 1) {
		switch (child->tag) {
		case e_inedge:
			node->bb->addInEdge(child->bb);
			break;
		case e_outedge:
			node->bb->addOutEdge(child->bb);
			break;
		}
		return;
	}
	switch (child->tag) {
	case e_inedge:
		break;
	case e_outedge:
		break;
	case e_livein:
		node->bb->addLiveIn((Location *)child->exp);
		break;
	case e_rtl:
		node->bb->addRTL(child->rtl);
		break;
	default:
		addChildStub(node, child);
		break;
	}
}

void
XMLProgParser::start_inedge(Context *node, const char **attr)
{
	if (phase == 1)
		node->bb = (BasicBlock *)findId(getAttr(attr, "bb"));
	else
		node->bb = nullptr;
}

void
XMLProgParser::addChildTo_inedge(Context *node, const Context *child) const
{
	//switch (child->tag) {
	//default:
		addChildStub(node, child);
		//break;
	//}
}

void
XMLProgParser::start_outedge(Context *node, const char **attr)
{
	if (phase == 1)
		node->bb = (BasicBlock *)findId(getAttr(attr, "bb"));
	else
		node->bb = nullptr;
}

void
XMLProgParser::addChildTo_outedge(Context *node, const Context *child) const
{
	//switch (child->tag) {
	//default:
		addChildStub(node, child);
		//break;
	//}
}

void
XMLProgParser::start_livein(Context *node, const char **attr)
{
}

void
XMLProgParser::addChildTo_livein(Context *node, const Context *child) const
{
	node->exp = child->exp;
}

void
XMLProgParser::start_order(Context *node, const char **attr)
{
	if (phase == 1)
		node->bb = (BasicBlock *)findId(getAttr(attr, "bb"));
	else
		node->bb = nullptr;
}

void
XMLProgParser::addChildTo_order(Context *node, const Context *child) const
{
	//switch (child->tag) {
	//default:
		addChildStub(node, child);
		//break;
	//}
}

void
XMLProgParser::start_revorder(Context *node, const char **attr)
{
	if (phase == 1)
		node->bb = (BasicBlock *)findId(getAttr(attr, "bb"));
	else
		node->bb = nullptr;
}

void
XMLProgParser::addChildTo_revorder(Context *node, const Context *child) const
{
	//switch (child->tag) {
	//default:
		addChildStub(node, child);
		//break;
	//}
}

void
XMLProgParser::start_rtl(Context *node, const char **attr)
{
	if (phase == 1) {
		node->rtl = (RTL *)findId(getAttr(attr, "id"));
		return;
	}
	node->rtl = new RTL();
	addId(attr, node->rtl);
	const char *a = getAttr(attr, "addr");
	if (a)
		node->rtl->nativeAddr = atoi(a);
}

void
XMLProgParser::addChildTo_rtl(Context *node, const Context *child) const
{
	if (phase == 1) {
		return;
	}
	switch (child->tag) {
	case e_stmt:
		node->rtl->appendStmt(child->stmt);
		break;
	default:
		addChildStub(node, child);
		break;
	}
}

void
XMLProgParser::start_stmt(Context *node, const char **attr)
{
}

void
XMLProgParser::addChildTo_stmt(Context *node, const Context *child) const
{
	node->stmt = child->stmt;
}

void
XMLProgParser::start_assign(Context *node, const char **attr)
{
	if (phase == 1) {
		node->stmt = (Statement *)findId(getAttr(attr, "id"));
		UserProc *p = (UserProc *)findId(getAttr(attr, "proc"));
		if (p)
			node->stmt->setProc(p);
		Statement *parent = (Statement *)findId(getAttr(attr, "parent"));
		if (parent)
			node->stmt->parent = parent;
		return;
	}
	node->stmt = new Assign();
	addId(attr, node->stmt);
	const char *n = getAttr(attr, "number");
	if (n)
		node->stmt->number = atoi(n);
}

void
XMLProgParser::addChildTo_assign(Context *node, const Context *child) const
{
	if (phase == 1) {
		return;
	}
	auto assign = dynamic_cast<Assign *>(node->stmt);
	assert(assign);
	switch (child->tag) {
	case e_lhs:
		assign->setLeft(child->exp);
		break;
	case e_rhs:
		assign->setRight(child->exp);
		break;
	case e_type:
		assert(child->type);
		assign->setType(child->type);
		break;
	default:
		addChildStub(node, child);
		break;
	}
}

void
XMLProgParser::start_assignment(Context *node, const char **attr)
{
}

void
XMLProgParser::addChildTo_assignment(Context *node, const Context *child) const
{
	node->stmt = child->stmt;
}

void
XMLProgParser::start_phiassign(Context *node, const char **attr)
{
	// FIXME: TBC
}

void
XMLProgParser::addChildTo_phiassign(Context *node, const Context *child) const
{
	if (phase == 1) {
		return;
	}
	auto pa = dynamic_cast<PhiAssign *>(node->stmt);
	assert(pa);
	switch (child->tag) {
	case e_lhs:
		pa->setLeft(child->exp);
		break;
	// FIXME: More required
	default:
		addChildStub(node, child);
		break;
	}
}

void
XMLProgParser::start_lhs(Context *node, const char **attr)
{
}

void
XMLProgParser::addChildTo_lhs(Context *node, const Context *child) const
{
	node->exp = child->exp;
}

void
XMLProgParser::start_rhs(Context *node, const char **attr)
{
}

void
XMLProgParser::addChildTo_rhs(Context *node, const Context *child) const
{
	node->exp = child->exp;
}

void
XMLProgParser::start_callstmt(Context *node, const char **attr)
{
	if (phase == 1) {
		node->stmt = (Statement *)findId(getAttr(attr, "id"));
		UserProc *p = (UserProc *)findId(getAttr(attr, "proc"));
		if (p)
			((Statement *)node->stmt)->setProc(p);
		Statement *s = (Statement *)findId(getAttr(attr, "parent"));
		if (s)
			((Statement *)node->stmt)->parent = s;
		return;
	}
	auto call = new CallStatement();
	node->stmt = call;
	addId(attr, call);
	const char *n = getAttr(attr, "number");
	if (n)
		call->number = atoi(n);
	n = getAttr(attr, "computed");
	if (n)
		call->m_isComputed = atoi(n) > 0;
	n = getAttr(attr, "returnAftercall");
	if (n)
		call->returnAfterCall = atoi(n) > 0;
}

void
XMLProgParser::addChildTo_callstmt(Context *node, const Context *child) const
{
	auto call = dynamic_cast<CallStatement *>(node->stmt);
	assert(call);
	if (phase == 1) {
		switch (child->tag) {
		case e_dest:
			if (child->proc)
				call->setDestProc(child->proc);
			break;
		}
		return;
	}
	Exp *returnExp = nullptr;
	switch (child->tag) {
	case e_dest:
		call->setDest(child->exp);
		break;
	case e_argument:
		call->appendArgument((Assignment *)child->stmt);
		break;
	case e_returnexp:
		// Assume that the corresponding return type will appear next
		returnExp = child->exp;
		break;
	default:
		addChildStub(node, child);
		break;
	}
}

void
XMLProgParser::start_dest(Context *node, const char **attr)
{
	if (phase == 1) {
		Proc *p = (Proc *)findId(getAttr(attr, "proc"));
		if (p)
			node->proc = p;
		return;
	}
}

void
XMLProgParser::addChildTo_dest(Context *node, const Context *child) const
{
	node->exp = child->exp;
}

void
XMLProgParser::start_argument(Context *node, const char **attr)
{
}

void
XMLProgParser::addChildTo_argument(Context *node, const Context *child) const
{
	node->exp = child->exp;
}

void
XMLProgParser::start_returnexp(Context *node, const char **attr)
{
}

void
XMLProgParser::addChildTo_returnexp(Context *node, const Context *child) const
{
	node->exp = child->exp;
}

void
XMLProgParser::start_returntype(Context *node, const char **attr)
{
}

void
XMLProgParser::addChildTo_returntype(Context *node, const Context *child) const
{
	node->type = child->type;
}

void
XMLProgParser::start_returnstmt(Context *node, const char **attr)
{
	if (phase == 1) {
		node->stmt = (Statement *)findId(getAttr(attr, "id"));
		UserProc *p = (UserProc *)findId(getAttr(attr, "proc"));
		if (p)
			((Statement *)node->stmt)->setProc(p);
		Statement *s = (Statement *)findId(getAttr(attr, "parent"));
		if (s)
			((Statement *)node->stmt)->parent = s;
		return;
	}
	auto ret = new ReturnStatement();
	node->stmt = ret;
	addId(attr, ret);
	const char *n = getAttr(attr, "number");
	if (n)
		ret->number = atoi(n);
	n = getAttr(attr, "retAddr");
	if (n)
		ret->retAddr = atoi(n);
}

void
XMLProgParser::addChildTo_returnstmt(Context *node, const Context *child) const
{
	auto ret = dynamic_cast<ReturnStatement *>(node->stmt);
	assert(ret);
	if (phase == 1) {
		return;
	}
	switch (child->tag) {
	case e_modifieds:
		ret->modifieds.append((Assignment *)child->stmt);
		break;
	case e_returns:
		ret->returns.append((Assignment *)child->stmt);
		break;
	default:
		addChildStub(node, child);
		break;
	}
}

void
XMLProgParser::start_returns(Context *node, const char **attr)
{
}

void
XMLProgParser::addChildTo_returns(Context *node, const Context *child) const
{
}

void
XMLProgParser::start_modifieds(Context *node, const char **attr)
{
}

void
XMLProgParser::addChildTo_modifieds(Context *node, const Context *child) const
{
}

void
XMLProgParser::start_gotostmt(Context *node, const char **attr)
{
	if (phase == 1) {
		node->stmt = (Statement *)findId(getAttr(attr, "id"));
		UserProc *p = (UserProc *)findId(getAttr(attr, "proc"));
		if (p)
			((Statement *)node->stmt)->setProc(p);
		Statement *s = (Statement *)findId(getAttr(attr, "parent"));
		if (s)
			((Statement *)node->stmt)->parent = s;
		return;
	}
	auto branch = new GotoStatement();
	node->stmt = branch;
	addId(attr, branch);
	const char *n = getAttr(attr, "number");
	if (n)
		branch->number = atoi(n);
	n = getAttr(attr, "computed");
	if (n)
		branch->m_isComputed = atoi(n) > 0;
}

void
XMLProgParser::addChildTo_gotostmt(Context *node, const Context *child) const
{
	auto branch = dynamic_cast<GotoStatement *>(node->stmt);
	assert(branch);
	if (phase == 1) {
		return;
	}
	switch (child->tag) {
	case e_dest:
		branch->setDest(child->exp);
		break;
	default:
		addChildStub(node, child);
		break;
	}
}

void
XMLProgParser::start_branchstmt(Context *node, const char **attr)
{
	if (phase == 1) {
		node->stmt = (Statement *)findId(getAttr(attr, "id"));
		UserProc *p = (UserProc *)findId(getAttr(attr, "proc"));
		if (p)
			((Statement *)node->stmt)->setProc(p);
		Statement *s = (Statement *)findId(getAttr(attr, "parent"));
		if (s)
			((Statement *)node->stmt)->parent = s;
		return;
	}
	auto branch = new BranchStatement();
	node->stmt = branch;
	addId(attr, branch);
	const char *n = getAttr(attr, "number");
	if (n)
		branch->number = atoi(n);
	n = getAttr(attr, "computed");
	if (n)
		branch->m_isComputed = atoi(n) > 0;
	n = getAttr(attr, "jtcond");
	if (n)
		branch->jtCond = (BRANCH_TYPE)atoi(n);
	n = getAttr(attr, "float");
	if (n)
		branch->bFloat = atoi(n) > 0;
}

void
XMLProgParser::addChildTo_branchstmt(Context *node, const Context *child) const
{
	auto branch = dynamic_cast<BranchStatement *>(node->stmt);
	assert(branch);
	if (phase == 1) {
		return;
	}
	switch (child->tag) {
	case e_cond:
		branch->setCondExpr(child->exp);
		break;
	case e_dest:
		branch->setDest(child->exp);
		break;
	default:
		addChildStub(node, child);
		break;
	}
}

void
XMLProgParser::start_cond(Context *node, const char **attr)
{
}

void
XMLProgParser::addChildTo_cond(Context *node, const Context *child) const
{
	node->exp = child->exp;
}

void
XMLProgParser::start_casestmt(Context *node, const char **attr)
{
	if (phase == 1) {
		node->stmt = (Statement *)findId(getAttr(attr, "id"));
		UserProc *p = (UserProc *)findId(getAttr(attr, "proc"));
		if (p)
			((Statement *)node->stmt)->setProc(p);
		Statement *s = (Statement *)findId(getAttr(attr, "parent"));
		if (s)
			((Statement *)node->stmt)->parent = s;
		return;
	}
	auto cas = new CaseStatement();
	node->stmt = cas;
	addId(attr, cas);
	const char *n = getAttr(attr, "number");
	if (n)
		cas->number = atoi(n);
	n = getAttr(attr, "computed");
	if (n)
		cas->m_isComputed = atoi(n) > 0;
}

void
XMLProgParser::addChildTo_casestmt(Context *node, const Context *child) const
{
	auto cas = dynamic_cast<CaseStatement *>(node->stmt);
	assert(cas);
	if (phase == 1) {
		return;
	}
	switch (child->tag) {
	case e_dest:
		cas->setDest(child->exp);
		break;
	default:
		addChildStub(node, child);
		break;
	}
}

void
XMLProgParser::start_boolasgn(Context *node, const char **attr)
{
	if (phase == 1) {
		node->stmt = (Statement *)findId(getAttr(attr, "id"));
		UserProc *p = (UserProc *)findId(getAttr(attr, "proc"));
		if (p)
			((Statement *)node->stmt)->setProc(p);
		Statement *s = (Statement *)findId(getAttr(attr, "parent"));
		if (s)
			((Statement *)node->stmt)->parent = s;
		return;
	}
	const char *n = getAttr(attr, "size");
	assert(n);
	auto boo = new BoolAssign(atoi(n));
	node->stmt = boo;
	addId(attr, boo);
	n = getAttr(attr, "number");
	if (n)
		boo->number = atoi(n);
	n = getAttr(attr, "jtcond");
	if (n)
		boo->jtCond = (BRANCH_TYPE)atoi(n);
	n = getAttr(attr, "float");
	if (n)
		boo->bFloat = atoi(n) > 0;
}

void
XMLProgParser::addChildTo_boolasgn(Context *node, const Context *child) const
{
	auto boo = dynamic_cast<BoolAssign *>(node->stmt);
	assert(boo);
	if (phase == 1) {
		return;
	}
	switch (child->tag) {
	case e_cond:
		boo->pCond = child->exp;
		break;
	case e_lhs:
		boo->lhs = child->exp;
		break;
	default:
		addChildStub(node, child);
		break;
	}
}

void
XMLProgParser::start_type(Context *node, const char **attr)
{
}

void
XMLProgParser::addChildTo_type(Context *node, const Context *child) const
{
	node->type = child->type;
}

void
XMLProgParser::start_exp(Context *node, const char **attr)
{
}

void
XMLProgParser::addChildTo_exp(Context *node, const Context *child) const
{
	node->exp = child->exp;
}

void
XMLProgParser::start_voidtype(Context *node, const char **attr)
{
	if (phase == 1) {
		node->type = (Type *)findId(getAttr(attr, "id"));
		return;
	}
	node->type = new VoidType();
	addId(attr, node->type);
}

void
XMLProgParser::addChildTo_voidtype(Context *node, const Context *child) const
{
	//switch (child->tag) {
	//default:
		addChildStub(node, child);
		//break;
	//}
}

void
XMLProgParser::start_integertype(Context *node, const char **attr)
{
	if (phase == 1) {
		node->type = (Type *)findId(getAttr(attr, "id"));
		return;
	}
	auto ty = new IntegerType();
	node->type = ty;
	addId(attr, ty);
	const char *n = getAttr(attr, "size");
	if (n)
		ty->size = atoi(n);
	n = getAttr(attr, "signedness");
	if (n)
		ty->signedness = atoi(n);
}

void
XMLProgParser::addChildTo_integertype(Context *node, const Context *child) const
{
	//switch (child->tag) {
	//default:
		addChildStub(node, child);
		//break;
	//}
}

void
XMLProgParser::start_pointertype(Context *node, const char **attr)
{
	if (phase == 1) {
		node->type = (Type *)findId(getAttr(attr, "id"));
		return;
	}
	node->type = new PointerType(nullptr);
	addId(attr, node->type);
}

void
XMLProgParser::addChildTo_pointertype(Context *node, const Context *child) const
{
	auto p = dynamic_cast<PointerType *>(node->type);
	assert(p);
	if (phase == 1) {
		return;
	}
	p->setPointsTo(child->type);
}

void
XMLProgParser::start_chartype(Context *node, const char **attr)
{
	if (phase == 1) {
		node->type = (Type *)findId(getAttr(attr, "id"));
		return;
	}
	node->type = new CharType();
	addId(attr, node->type);
}

void
XMLProgParser::addChildTo_chartype(Context *node, const Context *child) const
{
	//switch (child->tag) {
	//default:
		addChildStub(node, child);
		//break;
	//}
}

void
XMLProgParser::start_namedtype(Context *node, const char **attr)
{
	if (phase == 1) {
		node->type = (Type *)findId(getAttr(attr, "id"));
		return;
	}
	node->type = new NamedType(getAttr(attr, "name"));
	addId(attr, node->type);
}

void
XMLProgParser::addChildTo_namedtype(Context *node, const Context *child) const
{
	//switch (child->tag) {
	//default:
		addChildStub(node, child);
		//break;
	//}
}

void
XMLProgParser::start_arraytype(Context *node, const char **attr)
{
	if (phase == 1) {
		node->type = (Type *)findId(getAttr(attr, "id"));
		return;
	}
	auto a = new ArrayType();
	node->type = a;
	addId(attr, a);
	const char *len = getAttr(attr, "length");
	if (len)
		a->length = atoi(len);
}

void
XMLProgParser::addChildTo_arraytype(Context *node, const Context *child) const
{
	auto a = dynamic_cast<ArrayType *>(node->type);
	assert(a);
	switch (child->tag) {
	case e_basetype:
		a->base_type = child->type;
		break;
	default:
		addChildStub(node, child);
		break;
	}
}

void
XMLProgParser::start_basetype(Context *node, const char **attr)
{
}

void
XMLProgParser::addChildTo_basetype(Context *node, const Context *child) const
{
	node->type = child->type;
}

void
XMLProgParser::start_sizetype(Context *node, const char **attr)
{
	if (phase == 1) {
		node->type = (Type *)findId(getAttr(attr, "id"));
		return;
	}
	auto ty = new SizeType();
	node->type = ty;
	addId(attr, ty);
	const char *n = getAttr(attr, "size");
	if (n)
		ty->size = atoi(n);
}

void
XMLProgParser::addChildTo_sizetype(Context *node, const Context *child) const
{
	//switch (child->tag) {
	//default:
		addChildStub(node, child);
		//break;
	//}
}

void
XMLProgParser::start_location(Context *node, const char **attr)
{
	if (phase == 1) {
		node->exp = (Exp *)findId(getAttr(attr, "id"));
		UserProc *p = (UserProc *)findId(getAttr(attr, "proc"));
		if (p)
			((Location *)node->exp)->setProc(p);
		return;
	}
	OPER op = (OPER)operFromString(getAttr(attr, "op"));
	assert(op != -1);
	node->exp = new Location(op);
	addId(attr, node->exp);
}

void
XMLProgParser::addChildTo_location(Context *node, const Context *child) const
{
	if (phase == 1) {
		return;
	}
	auto l = dynamic_cast<Location *>(node->exp);
	assert(l);
	switch (child->tag) {
	case e_subexp1:
		node->exp->setSubExp1(child->exp);
		break;
	default:
		addChildStub(node, child);
		break;
	}
}

void
XMLProgParser::start_unary(Context *node, const char **attr)
{
	if (phase == 1) {
		node->exp = (Exp *)findId(getAttr(attr, "id"));
		return;
	}
	OPER op = (OPER)operFromString(getAttr(attr, "op"));
	assert(op != -1);
	node->exp = new Unary(op);
	addId(attr, node->exp);
}

void
XMLProgParser::addChildTo_unary(Context *node, const Context *child) const
{
	if (phase == 1) {
		return;
	}
	switch (child->tag) {
	case e_subexp1:
		node->exp->setSubExp1(child->exp);
		break;
	default:
		addChildStub(node, child);
		break;
	}
}

void
XMLProgParser::start_binary(Context *node, const char **attr)
{
	if (phase == 1) {
		node->exp = (Exp *)findId(getAttr(attr, "id"));
		return;
	}
	OPER op = (OPER)operFromString(getAttr(attr, "op"));
	assert(op != -1);
	node->exp = new Binary(op);
	addId(attr, node->exp);
}

void
XMLProgParser::addChildTo_binary(Context *node, const Context *child) const
{
	if (phase == 1) {
		return;
	}
	switch (child->tag) {
	case e_subexp1:
		node->exp->setSubExp1(child->exp);
		break;
	case e_subexp2:
		node->exp->setSubExp2(child->exp);
		break;
	default:
		addChildStub(node, child);
		break;
	}
}

void
XMLProgParser::start_ternary(Context *node, const char **attr)
{
	if (phase == 1) {
		node->exp = (Exp *)findId(getAttr(attr, "id"));
		return;
	}
	OPER op = (OPER)operFromString(getAttr(attr, "op"));
	assert(op != -1);
	node->exp = new Ternary(op);
	addId(attr, node->exp);
}

void
XMLProgParser::addChildTo_ternary(Context *node, const Context *child) const
{
	if (phase == 1) {
		return;
	}
	switch (child->tag) {
	case e_subexp1:
		node->exp->setSubExp1(child->exp);
		break;
	case e_subexp2:
		node->exp->setSubExp2(child->exp);
		break;
	case e_subexp3:
		node->exp->setSubExp3(child->exp);
		break;
	default:
		addChildStub(node, child);
		break;
	}
}

void
XMLProgParser::start_const(Context *node, const char **attr)
{
	if (phase == 1) {
		node->exp = (Exp *)findId(getAttr(attr, "id"));
		return;
	}
	double d;
	const char *value = getAttr(attr, "value");
	const char *opstring = getAttr(attr, "op");
	assert(value);
	assert(opstring);
	//std::cerr << "got value=" << value << " opstring=" << opstring << "\n";
	OPER op = (OPER)operFromString(opstring);
	assert(op != -1);
	switch (op) {
	case opIntConst:
		node->exp = new Const(atoi(value));
		addId(attr, node->exp);
		break;
	case opStrConst:
		node->exp = new Const(strdup(value));
		addId(attr, node->exp);
		break;
	case opFltConst:
		sscanf(value, "%lf", &d);
		node->exp = new Const(d);
		addId(attr, node->exp);
		break;
	default:
		LOG << "unknown Const op " << op << "\n";
		assert(false);
	}
	//std::cerr << "end of start const\n";
}

void
XMLProgParser::addChildTo_const(Context *node, const Context *child) const
{
	//switch (child->tag) {
	//default:
		addChildStub(node, child);
		//break;
	//}
}

void
XMLProgParser::start_terminal(Context *node, const char **attr)
{
	if (phase == 1) {
		node->exp = (Exp *)findId(getAttr(attr, "id"));
		return;
	}
	OPER op = (OPER)operFromString(getAttr(attr, "op"));
	assert(op != -1);
	node->exp = new Terminal(op);
	addId(attr, node->exp);
}

void
XMLProgParser::addChildTo_terminal(Context *node, const Context *child) const
{
	//switch (child->tag) {
	//default:
		addChildStub(node, child);
		//break;
	//}
}

void
XMLProgParser::start_typedexp(Context *node, const char **attr)
{
	if (phase == 1) {
		node->exp = (Exp *)findId(getAttr(attr, "id"));
		return;
	}
	node->exp = new TypedExp();
	addId(attr, node->exp);
}

void
XMLProgParser::addChildTo_typedexp(Context *node, const Context *child) const
{
	auto t = dynamic_cast<TypedExp *>(node->exp);
	assert(t);
	switch (child->tag) {
	case e_type:
		t->type = child->type;
		break;
	case e_subexp1:
		node->exp->setSubExp1(child->exp);
		break;
	default:
		addChildStub(node, child);
		break;
	}
}

void
XMLProgParser::start_refexp(Context *node, const char **attr)
{
	if (phase == 1) {
		node->exp = (Exp *)findId(getAttr(attr, "id"));
		auto r = dynamic_cast<RefExp *>(node->exp);
		assert(r);
		r->def = (Statement *)findId(getAttr(attr, "def"));
		return;
	}
	node->exp = new RefExp();
	addId(attr, node->exp);
}

void
XMLProgParser::addChildTo_refexp(Context *node, const Context *child) const
{
	switch (child->tag) {
	case e_subexp1:
		node->exp->setSubExp1(child->exp);
		break;
	default:
		addChildStub(node, child);
		break;
	}
}

void
XMLProgParser::start_def(Context *node, const char **attr)
{
	if (phase == 1) {
		node->stmt = (Statement *)findId(getAttr(attr, "stmt"));
		return;
	}
}

void
XMLProgParser::addChildTo_def(Context *node, const Context *child) const
{
	//switch (child->tag) {
	//default:
		addChildStub(node, child);
		//break;
	//}
}

void
XMLProgParser::start_subexp1(Context *node, const char **attr)
{
}

void
XMLProgParser::addChildTo_subexp1(Context *node, const Context *child) const
{
	node->exp = child->exp;
}

void
XMLProgParser::start_subexp2(Context *node, const char **attr)
{
}

void
XMLProgParser::addChildTo_subexp2(Context *node, const Context *child) const
{
	node->exp = child->exp;
}

void
XMLProgParser::start_subexp3(Context *node, const char **attr)
{
}

void
XMLProgParser::addChildTo_subexp3(Context *node, const Context *child) const
{
	node->exp = child->exp;
}
#endif


void
XMLProgParser::persistToXML(Prog *prog)
{
	prog->m_rootCluster->openStreams("xml");
	std::ofstream &os = prog->m_rootCluster->getStream();
	os << "<prog path=\"" << prog->getPath()
	   << "\" name=\"" << prog->getName()
	   << "\" iNumberedProc=\"" << prog->m_iNumberedProc
	   << "\">\n";
	for (const auto &global : prog->globals)
		persistToXML(os, global);
	persistToXML(os, prog->m_rootCluster);
	for (const auto &proc : prog->m_procs)
		persistToXML(proc->getCluster()->getStream(), proc);
	os << "</prog>\n";
	os.close();
	prog->m_rootCluster->closeStreams();
}

void
XMLProgParser::persistToXML(std::ostream &out, Global *g)
{
	out << "<global name=\"" << g->nam
	    << "\" uaddr=\"" << (int)g->uaddr
	    << "\">\n";
	out << "<type>\n";
	persistToXML(out, g->type);
	out << "</type>\n";
	out << "</global>\n";
}

void
XMLProgParser::persistToXML(std::ostream &out, Cluster *c)
{
	out << "<cluster id=\"" << (int)c
	    << "\" name=\"" << c->name
	    << "\">\n";
	for (const auto &child : c->children) {
		persistToXML(out, child);
	}
	out << "</cluster>\n";
}

void
XMLProgParser::persistToXML(std::ostream &out, Proc *proc)
{
	if (proc->isLib())
		persistToXML(out, (LibProc *)proc);
	else
		persistToXML(out, (UserProc *)proc);
}

void
XMLProgParser::persistToXML(std::ostream &out, LibProc *proc)
{
	out << "<libproc id=\"" << (int)proc
	    << "\" address=\"" << (int)proc->address
	    << "\" firstCallerAddress=\"" << proc->m_firstCallerAddr;
	if (proc->m_firstCaller)
		out << "\" firstCaller=\"" << (int)proc->m_firstCaller;
	if (proc->cluster)
		out << "\" cluster=\"" << (int)proc->cluster;
	out << "\">\n";

	persistToXML(out, proc->signature);

	for (const auto &caller : proc->callerSet)
		out << "<caller call=\"" << (int)caller << "\"/>\n";
	for (const auto &pt : proc->provenTrue) {
		out << "<proven_true>\n";
		persistToXML(out, pt.first);
		persistToXML(out, pt.second);
		out << "</proven_true>\n";
	}
	out << "</libproc>\n";
}

void
XMLProgParser::persistToXML(std::ostream &out, UserProc *proc)
{
	out << "<userproc id=\"" << (int)proc
	    << "\" address=\"" << (int)proc->address
	    << "\" status=\"" << (int)proc->status
	    << "\" firstCallerAddress=\"" << proc->m_firstCallerAddr;
	if (proc->m_firstCaller)
		out << "\" firstCaller=\"" << (int)proc->m_firstCaller;
	if (proc->cluster)
		out << "\" cluster=\"" << (int)proc->cluster;
	if (proc->theReturnStatement)
		out << "\" retstmt=\"" << (int)proc->theReturnStatement;
	out << "\">\n";

	persistToXML(out, proc->signature);

	for (const auto &caller : proc->callerSet)
		out << "<caller call=\"" << (int)caller << "\"/>\n";
	for (const auto &pt : proc->provenTrue) {
		out << "<proven_true>\n";
		persistToXML(out, pt.first);
		persistToXML(out, pt.second);
		out << "</proven_true>\n";
	}

	for (const auto &local : proc->locals) {
		out << "<local name=\"" << local.first << "\">\n";
		out << "<type>\n";
		persistToXML(out, local.second);
		out << "</type>\n";
		out << "</local>\n";
	}

	for (const auto &symbol : proc->symbolMap) {
		out << "<symbol>\n";
		out << "<exp>\n";
		persistToXML(out, symbol.first);
		out << "</exp>\n";
		out << "<secondexp>\n";
		persistToXML(out, symbol.second);
		out << "</secondexp>\n";
		out << "</symbol>\n";
	}

	for (const auto &callee : proc->calleeList)
		out << "<callee proc=\"" << (int)callee << "\"/>\n";

	persistToXML(out, proc->cfg);

	out << "</userproc>\n";
}

void
XMLProgParser::persistToXML(std::ostream &out, Signature *sig)
{
	out << "<signature id=\"" << (int)sig
	    << "\" name=\"" << sig->name
	    << "\" ellipsis=\"" << (int)sig->ellipsis
	    << "\" preferedName=\"" << sig->preferedName;
	if (sig->getPlatform() != PLAT_GENERIC)
		out << "\" platform=\"" << sig->platformName(sig->getPlatform());
	if (sig->getConvention() != CONV_NONE)
		out << "\" convention=\"" << sig->conventionName(sig->getConvention());
	out << "\">\n";
	for (const auto &param : sig->params) {
		out << "<param id=\"" << (int)param
		    << "\" name=\"" << param->getName()
		    << "\">\n";
		out << "<type>\n";
		persistToXML(out, param->getType());
		out << "</type>\n";
		out << "<exp>\n";
		persistToXML(out, param->getExp());
		out << "</exp>\n";
		out << "</param>\n";
	}
	for (const auto &ret : sig->returns) {
		out << "<return id=\"" << (int)ret
		    << "\">\n";
		out << "<type>\n";
		persistToXML(out, ret->type);
		out << "</type>\n";
		out << "<exp>\n";
		persistToXML(out, ret->exp);
		out << "</exp>\n";
		out << "</return>\n";
	}
	if (sig->rettype) {
		out << "<rettype>\n";
		persistToXML(out, sig->rettype);
		out << "</rettype>\n";
	}
	if (sig->preferedReturn) {
		out << "<prefreturn>\n";
		persistToXML(out, sig->preferedReturn);
		out << "</prefreturn>\n";
	}
	for (const auto &pp : sig->preferedParams)
		out << "<prefparam index=\"" << pp << "\"/>\n";
	out << "</signature>\n";
}

void
XMLProgParser::persistToXML(std::ostream &out, Type *ty)
{
	if (auto v = dynamic_cast<VoidType *>(ty)) {
		out << "<voidtype id=\"" << (int)ty << "\"/>\n";
		return;
	}
	if (auto f = dynamic_cast<FuncType *>(ty)) {
		out << "<functype id=\"" << (int)ty << "\">\n";
		persistToXML(out, f->signature);
		out << "</functype>\n";
		return;
	}
	if (auto i = dynamic_cast<IntegerType *>(ty)) {
		out << "<integertype id=\"" << (int)ty
		    << "\" size=\"" << i->size
		    << "\" signedness=\"" << i->signedness
		    << "\"/>\n";
		return;
	}
	if (auto fl = dynamic_cast<FloatType *>(ty)) {
		out << "<floattype id=\"" << (int)ty
		    << "\" size=\"" << fl->size << "\"/>\n";
		return;
	}
	if (auto b = dynamic_cast<BooleanType *>(ty)) {
		out << "<booleantype id=\"" << (int)ty << "\"/>\n";
		return;
	}
	if (auto c = dynamic_cast<CharType *>(ty)) {
		out << "<chartype id=\"" << (int)ty << "\"/>\n";
		return;
	}
	if (auto p = dynamic_cast<PointerType *>(ty)) {
		out << "<pointertype id=\"" << (int)ty << "\">\n";
		persistToXML(out, p->points_to);
		out << "</pointertype>\n";
		return;
	}
	if (auto a = dynamic_cast<ArrayType *>(ty)) {
		out << "<arraytype id=\"" << (int)ty
		    << "\" length=\"" << (int)a->length
		    << "\">\n";
		out << "<basetype>\n";
		persistToXML(out, a->base_type);
		out << "</basetype>\n";
		out << "</arraytype>\n";
		return;
	}
	if (auto n = dynamic_cast<NamedType *>(ty)) {
		out << "<namedtype id=\"" << (int)ty
		    << "\" name=\"" << n->name
		    << "\"/>\n";
		return;
	}
	if (auto co = dynamic_cast<CompoundType *>(ty)) {
		out << "<compoundtype id=\"" << (int)ty << "\">\n";
		for (unsigned i = 0; i < co->names.size(); ++i) {
			out << "<member name=\"" << co->names[i] << "\">\n";
			persistToXML(out, co->types[i]);
			out << "</member>\n";
		}
		out << "</compoundtype>\n";
		return;
	}
	if (auto sz = dynamic_cast<SizeType *>(ty)) {
		out << "<sizetype id=\"" << (int)ty
		    << "\" size=\"" << sz->getSize()
		    << "\"/>\n";
		return;
	}
	std::cerr << "unknown type in persistToXML\n";
	assert(false);
}

void
XMLProgParser::persistToXML(std::ostream &out, Exp *e)
{
	if (auto t = dynamic_cast<TypeVal *>(e)) {
		out << "<typeval id=\"" << (int)e
		    << "\" op=\"" << operStrings[t->op]
		    << "\">\n";
		out << "<type>\n";
		persistToXML(out, t->val);
		out << "</type>\n";
		out << "</typeval>\n";
		return;
	}
	if (auto te = dynamic_cast<Terminal *>(e)) {
		out << "<terminal id=\"" << (int)e
		    << "\" op=\"" << operStrings[te->op]
		    << "\"/>\n";
		return;
	}
	if (auto c = dynamic_cast<Const *>(e)) {
		out << "<const id=\"" << (int)e
		    << "\" op=\"" << operStrings[c->op]
		    << "\" conscript=\"" << c->conscript;
		if (c->op == opIntConst)
			out << "\" value=\"" << c->u.i;
		else if (c->op == opFuncConst)
			out << "\" value=\"" << c->u.a;
		else if (c->op == opFltConst)
			out << "\" value=\"" << c->u.d;
		else if (c->op == opStrConst)
			out << "\" value=\"" << c->u.p;
		else {
			// TODO
			// uint64_t ll;
			// Proc *pp;
			assert(false);
		}
		out << "\"/>\n";
		return;
	}
	if (auto l = dynamic_cast<Location *>(e)) {
		out << "<location id=\"" << (int)e;
		if (l->proc)
			out << "\" proc=\"" << (int)l->proc;
		out << "\" op=\"" << operStrings[l->op]
		    << "\">\n";
		out << "<subexp1>\n";
		persistToXML(out, l->subExp1);
		out << "</subexp1>\n";
		out << "</location>\n";
		return;
	}
	if (auto r = dynamic_cast<RefExp *>(e)) {
		out << "<refexp id=\"" << (int)e;
		if (r->def)
			out << "\" def=\"" << (int)r->def;
		out << "\" op=\"" << operStrings[r->op]
		    << "\">\n";
		out << "<subexp1>\n";
		persistToXML(out, r->subExp1);
		out << "</subexp1>\n";
		out << "</refexp>\n";
		return;
	}
	if (auto f = dynamic_cast<FlagDef *>(e)) {
		out << "<flagdef id=\"" << (int)e;
		if (f->rtl)
			out << "\" rtl=\"" << (int)f->rtl;
		out << "\" op=\"" << operStrings[f->op]
		    << "\">\n";
		out << "<subexp1>\n";
		persistToXML(out, f->subExp1);
		out << "</subexp1>\n";
		out << "</flagdef>\n";
		return;
	}
	if (auto ty = dynamic_cast<TypedExp *>(e)) {
		out << "<typedexp id=\"" << (int)e
		    << "\" op=\"" << operStrings[ty->op]
		    << "\">\n";
		out << "<subexp1>\n";
		persistToXML(out, ty->subExp1);
		out << "</subexp1>\n";
		out << "<type>\n";
		persistToXML(out, ty->type);
		out << "</type>\n";
		out << "</typedexp>\n";
		return;
	}
	if (auto tn = dynamic_cast<Ternary *>(e)) {
		out << "<ternary id=\"" << (int)e
		    << "\" op=\"" << operStrings[tn->op]
		    << "\">\n";
		out << "<subexp1>\n";
		persistToXML(out, tn->subExp1);
		out << "</subexp1>\n";
		out << "<subexp2>\n";
		persistToXML(out, tn->subExp2);
		out << "</subexp2>\n";
		out << "<subexp3>\n";
		persistToXML(out, tn->subExp3);
		out << "</subexp3>\n";
		out << "</ternary>\n";
		return;
	}
	if (auto b = dynamic_cast<Binary *>(e)) {
		out << "<binary id=\"" << (int)e
		    << "\" op=\"" << operStrings[b->op]
		    << "\">\n";
		out << "<subexp1>\n";
		persistToXML(out, b->subExp1);
		out << "</subexp1>\n";
		out << "<subexp2>\n";
		persistToXML(out, b->subExp2);
		out << "</subexp2>\n";
		out << "</binary>\n";
		return;
	}
	if (auto u = dynamic_cast<Unary *>(e)) {
		out << "<unary id=\"" << (int)e
		    << "\" op=\"" << operStrings[u->op]
		    << "\">\n";
		out << "<subexp1>\n";
		persistToXML(out, u->subExp1);
		out << "</subexp1>\n";
		out << "</unary>\n";
		return;
	}
	std::cerr << "unknown exp in persistToXML\n";
	assert(false);
}

void
XMLProgParser::persistToXML(std::ostream &out, Cfg *cfg)
{
	out << "<cfg id=\"" << (int)cfg
	    << "\" wellformed=\"" << (int)cfg->m_bWellFormed
	    << "\" lastLabel=\"" << cfg->lastLabel
	    << "\" entryBB=\"" << (int)cfg->entryBB
	    << "\" exitBB=\"" << (int)cfg->exitBB
	    << "\">\n";

	for (const auto &bb : cfg->m_listBB)
		persistToXML(out, bb);

	for (const auto &bb : cfg->Ordering)
		out << "<order bb=\"" << (int)bb << "\"/>\n";

	for (const auto &bb : cfg->revOrdering)
		out << "<revorder bb=\"" << (int)bb << "\"/>\n";

	// TODO
	// MAPBB m_mapBB;
	// std::set<CallStatement *> callSites;
	// std::vector<BasicBlock *> BBs;        // Pointers to BBs from indices
	// std::map<BasicBlock *, int> indices;  // Indices from pointers to BBs
	// more
	out << "</cfg>\n";
}

void
XMLProgParser::persistToXML(std::ostream &out, BasicBlock *bb)
{
	out << "<bb id=\"" << (int)bb
	    << "\" nodeType=\"" << bb->m_nodeType
	    << "\" labelNum=\"" << bb->m_iLabelNum
	    << "\" label=\"" << bb->m_labelStr
	    << "\" labelneeded=\"" << (int)bb->m_labelneeded
	    << "\" incomplete=\"" << (int)bb->m_bIncomplete
	    << "\" jumpreqd=\"" << (int)bb->m_bJumpReqd
	    << "\" m_traversed=\"" << bb->m_iTraversed
	    << "\" DFTfirst=\"" << bb->m_DFTfirst
	    << "\" DFTlast=\"" << bb->m_DFTlast
	    << "\" DFTrevfirst=\"" << bb->m_DFTrevfirst
	    << "\" DFTrevlast=\"" << bb->m_DFTrevlast
	    << "\" structType=\"" << bb->m_structType
	    << "\" loopCondType=\"" << bb->m_loopCondType;
	if (bb->m_loopHead)
		out << "\" m_loopHead=\"" << (int)bb->m_loopHead;
	if (bb->m_caseHead)
		out << "\" m_caseHead=\"" << (int)bb->m_caseHead;
	if (bb->m_condFollow)
		out << "\" m_condFollow=\"" << (int)bb->m_condFollow;
	if (bb->m_loopFollow)
		out << "\" m_loopFollow=\"" << (int)bb->m_loopFollow;
	if (bb->m_latchNode)
		out << "\" m_latchNode=\"" << (int)bb->m_latchNode;
	out << "\" ord=\"" << bb->ord
	    << "\" revOrd=\"" << bb->revOrd
	    << "\" inEdgesVisited=\"" << bb->inEdgesVisited
	    << "\" numForwardInEdges=\"" << bb->numForwardInEdges
	    << "\" loopStamp1=\"" << bb->loopStamps[0]
	    << "\" loopStamp2=\"" << bb->loopStamps[1]
	    << "\" revLoopStamp1=\"" << bb->revLoopStamps[0]
	    << "\" revLoopStamp2=\"" << bb->revLoopStamps[1]
	    << "\" traversed=\"" << (int)bb->traversed
	    << "\" hllLabel=\"" << (int)bb->hllLabel;
	if (bb->labelStr)
		out << "\" labelStr=\"" << bb->labelStr;
	out << "\" indentLevel=\"" << bb->indentLevel;
	// note the ridiculous duplication here
	if (bb->immPDom)
		out << "\" immPDom=\"" << (int)bb->immPDom;
	if (bb->loopHead)
		out << "\" loopHead=\"" << (int)bb->loopHead;
	if (bb->caseHead)
		out << "\" caseHead=\"" << (int)bb->caseHead;
	if (bb->condFollow)
		out << "\" condFollow=\"" << (int)bb->condFollow;
	if (bb->loopFollow)
		out << "\" loopFollow=\"" << (int)bb->loopFollow;
	if (bb->latchNode)
		out << "\" latchNode=\"" << (int)bb->latchNode;
	out << "\" sType=\"" << (int)bb->sType
	    << "\" usType=\"" << (int)bb->usType
	    << "\" lType=\"" << (int)bb->lType
	    << "\" cType=\"" << (int)bb->cType
	    << "\">\n";

	for (const auto &edge : bb->m_InEdges)
		out << "<inedge bb=\"" << (int)edge << "\"/>\n";
	for (const auto &edge : bb->m_OutEdges)
		out << "<outedge bb=\"" << (int)edge << "\"/>\n";

	for (const auto &li : bb->liveIn) {
		out << "<livein>\n";
		persistToXML(out, li);
		out << "</livein>\n";
	}

	if (bb->m_pRtls) {
		for (const auto &rtl : *bb->m_pRtls)
			persistToXML(out, rtl);
	}
	out << "</bb>\n";
}

void
XMLProgParser::persistToXML(std::ostream &out, RTL *rtl)
{
	out << "<rtl id=\"" << (int)rtl
	    << "\" addr=\"" << (int)rtl->nativeAddr
	    << "\">\n";
	for (const auto &stmt : rtl->stmtList) {
		out << "<stmt>\n";
		persistToXML(out, stmt);
		out << "</stmt>\n";
	}
	out << "</rtl>\n";
}

void
XMLProgParser::persistToXML(std::ostream &out, Statement *stmt)
{
	if (auto b = dynamic_cast<BoolAssign *>(stmt)) {
		out << "<boolasgn id=\"" << (int)stmt
		    << "\" number=\"" << b->number;
		if (b->parent)
			out << "\" parent=\"" << (int)b->parent;
		if (b->proc)
			out << "\" proc=\"" << (int)b->proc;
		out << "\" jtcond=\"" << b->jtCond
		    << "\" float=\"" << (int)b->bFloat
		    << "\" size=\"" << b->size
		    << "\">\n";
		if (b->pCond) {
			out << "<cond>\n";
			persistToXML(out, b->pCond);
			out << "</cond>\n";
		}
		out << "</boolasgn>\n";
		return;
	}
	if (auto r = dynamic_cast<ReturnStatement *>(stmt)) {
		out << "<returnstmt id=\"" << (int)stmt
		    << "\" number=\"" << r->number;
		if (r->parent)
			out << "\" parent=\"" << (int)r->parent;
		if (r->proc)
			out << "\" proc=\"" << (int)r->proc;
		out << "\" retAddr=\"" << (int)r->retAddr
		    << "\">\n";

		for (const auto &mod : r->modifieds) {
			out << "<modifieds>\n";
			persistToXML(out, mod);
			out << "</modifieds>\n";
		}
		for (const auto &ret : r->returns) {
			out << "<returns>\n";
			persistToXML(out, ret);
			out << "</returns>\n";
		}

		out << "</returnstmt>\n";
		return;
	}
	if (auto c = dynamic_cast<CallStatement *>(stmt)) {
		out << "<callstmt id=\"" << (int)stmt
		    << "\" number=\"" << c->number
		    << "\" computed=\"" << (int)c->m_isComputed;
		if (c->parent)
			out << "\" parent=\"" << (int)c->parent;
		if (c->proc)
			out << "\" proc=\"" << (int)c->proc;
		out << "\" returnAfterCall=\"" << (int)c->returnAfterCall
		    << "\">\n";

		if (c->pDest) {
			out << "<dest";
			if (c->procDest)
				out << " proc=\"" << (int)c->procDest << "\"";
			out << ">\n";
			persistToXML(out, c->pDest);
			out << "</dest>\n";
		}

		for (const auto &arg : c->arguments) {
			out << "<argument>\n";
			persistToXML(out, arg);
			out << "</argument>\n";
		}

		for (const auto &def : c->defines) {
			out << "<defines>\n";
			persistToXML(out, def);
			out << "</defines>\n";
		}

		out << "</callstmt>\n";
		return;
	}
	if (auto ca = dynamic_cast<CaseStatement *>(stmt)) {
		out << "<casestmt id=\"" << (int)stmt
		    << "\" number=\"" << ca->number
		    << "\" computed=\"" << (int)ca->m_isComputed;
		if (ca->parent)
			out << "\" parent=\"" << (int)ca->parent;
		if (ca->proc)
			out << "\" proc=\"" << (int)ca->proc;
		out << "\">\n";
		if (ca->pDest) {
			out << "<dest>\n";
			persistToXML(out, ca->pDest);
			out << "</dest>\n";
		}
		// TODO
		// SWITCH_INFO *pSwitchInfo;   // Ptr to struct with info about the switch
		out << "</casestmt>\n";
		return;
	}
	if (auto br = dynamic_cast<BranchStatement *>(stmt)) {
		out << "<branchstmt id=\"" << (int)stmt
		    << "\" number=\"" << br->number
		    << "\" computed=\"" << (int)br->m_isComputed
		    << "\" jtcond=\"" << br->jtCond
		    << "\" float=\"" << (int)br->bFloat;
		if (br->parent)
			out << "\" parent=\"" << (int)br->parent;
		if (br->proc)
			out << "\" proc=\"" << (int)br->proc;
		out << "\">\n";
		if (br->pDest) {
			out << "<dest>\n";
			persistToXML(out, br->pDest);
			out << "</dest>\n";
		}
		if (br->pCond) {
			out << "<cond>\n";
			persistToXML(out, br->pCond);
			out << "</cond>\n";
		}
		out << "</branchstmt>\n";
		return;
	}
	if (auto g = dynamic_cast<GotoStatement *>(stmt)) {
		out << "<gotostmt id=\"" << (int)stmt
		    << "\" number=\"" << g->number
		    << "\" computed=\"" << (int) g->m_isComputed;
		if (g->parent)
			out << "\" parent=\"" << (int)g->parent;
		if (g->proc)
			out << "\" proc=\"" << (int)g->proc;
		out << "\">\n";
		if (g->pDest) {
			out << "<dest>\n";
			persistToXML(out, g->pDest);
			out << "</dest>\n";
		}
		out << "</gotostmt>\n";
		return;
	}
	if (auto p = dynamic_cast<PhiAssign *>(stmt)) {
		out << "<phiassign id=\"" << (int)stmt
		    << "\" number=\"" << p->number;
		if (p->parent)
			out << "\" parent=\"" << (int)p->parent;
		if (p->proc)
			out << "\" proc=\"" << (int)p->proc;
		out << "\">\n";
		out << "<lhs>\n";
		persistToXML(out, p->lhs);
		out << "</lhs>\n";
		for (const auto &def : *p)
			out << "<def stmt=\"" << (int)def.def
			    << "\" exp=\"" << (int)def.e
			    << "\"/>\n";
		out << "</phiassign>\n";
		return;
	}
	if (auto a = dynamic_cast<Assign *>(stmt)) {
		out << "<assign id=\"" << (int)stmt
		    << "\" number=\"" << a->number;
		if (a->parent)
			out << "\" parent=\"" << (int)a->parent;
		if (a->proc)
			out << "\" proc=\"" << (int)a->proc;
		out << "\">\n";
		out << "<lhs>\n";
		persistToXML(out, a->lhs);
		out << "</lhs>\n";
		out << "<rhs>\n";
		persistToXML(out, a->rhs);
		out << "</rhs>\n";
		if (a->type) {
			out << "<type>\n";
			persistToXML(out, a->type);
			out << "</type>\n";
		}
		if (a->guard) {
			out << "<guard>\n";
			persistToXML(out, a->guard);
			out << "</guard>\n";
		}
		out << "</assign>\n";
		return;
	}
	std::cerr << "unknown stmt in persistToXML\n";
	assert(false);
}
