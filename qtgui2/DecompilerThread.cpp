/**
 * \file
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "DecompilerThread.h"

#include "boomerang.h"
#include "cluster.h"
#include "frontend.h"
#include "proc.h"
#include "prog.h"
#include "signature.h"

#ifdef GARBAGE_COLLECTOR
#include <gc/gc.h>
#endif

#include <QtGui>
#include <QtCore>
#include <QThread>
#include <QString>
#include <QTableWidget>

#include <algorithm>
#include <sstream>

#ifdef GARBAGE_COLLECTOR
static Qt::HANDLE threadToCollect = 0;

void *
operator new(size_t n)
{
	Qt::HANDLE curThreadId = QThread::currentThreadId();
	if (curThreadId == threadToCollect)
		return GC_malloc(n);
	else
		return GC_malloc_uncollectable(n);  // Don't collect, but mark
}

void
operator delete(void *p)
{
	Qt::HANDLE curThreadId = QThread::currentThreadId();
	if (curThreadId != threadToCollect)
		GC_free(p); // Important to call this if you call GC_malloc_uncollectable
}
#endif

void
DecompilerThread::run()
{
#ifdef GARBAGE_COLLECTOR
	threadToCollect = QThread::currentThreadId();
#endif

	Boomerang::get().setOutputDirectory("./output/");
	//Boomerang::get().vFlag = true;
	//Boomerang::get().traceDecoder = true;

	decompiler = new Decompiler();
	decompiler->moveToThread(this);

	Boomerang::get().addWatcher(decompiler);

	this->setPriority(QThread::LowPriority);

	exec();
}

Decompiler *
DecompilerThread::getDecompiler()
{
	while (!decompiler)
		msleep(10);
	return decompiler;
}

void
Decompiler::setUseDFTA(bool d)
{
	Boomerang::get().dfaTypeAnalysis = d;
}

void
Decompiler::setNoDecodeChildren(bool d)
{
	Boomerang::get().noDecodeChildren = d;
}

void
Decompiler::addEntryPoint(ADDRESS a, const QString &name)
{
	user_entrypoints.push_back(a);
	fe->addSymbol(a, name.toStdString());
}

void
Decompiler::removeEntryPoint(ADDRESS a)
{
	auto it = std::find(user_entrypoints.begin(), user_entrypoints.end(), a);
	if (it != user_entrypoints.end())
		user_entrypoints.erase(it);
}

void
Decompiler::changeInputFile(const QString &f)
{
	filename = f;
}

void
Decompiler::changeOutputPath(const QString &path)
{
	Boomerang::get().setOutputDirectory(path.toStdString());
}

void
Decompiler::load()
{
	emit loading();

	prog = Prog::open(strdup(filename.toAscii()));
	if (!prog) {
		emit machineType(QString("unavailable: Load Failed!"));
		return;
	}

	fe = prog->getFrontEnd();
	fe->readLibraryCatalog();

	switch (prog->getMachine()) {
	case MACHINE_PENTIUM:
		emit machineType(QString("pentium"));
		break;
	case MACHINE_SPARC:
		emit machineType(QString("sparc"));
		break;
	case MACHINE_HPRISC:
		emit machineType(QString("hprisc"));
		break;
	case MACHINE_PALM:
		emit machineType(QString("palm"));
		break;
	case MACHINE_PPC:
		emit machineType(QString("ppc"));
		break;
	case MACHINE_ST20:
		emit machineType(QString("st20"));
		break;
	case MACHINE_MIPS:
		emit machineType(QString("mips"));
		break;
	}

	QStringList entrypointStrings;
	std::vector<ADDRESS> entrypoints = fe->getEntryPoints();
	for (const auto &ep : entrypoints) {
		user_entrypoints.push_back(ep);
		emit newEntrypoint(ep, fe->getBinaryFile()->getSymbolByAddress(ep));
	}

	for (size_t i = 1; i < fe->getBinaryFile()->getNumSections(); ++i) {
		const SectionInfo *section = fe->getBinaryFile()->getSectionInfo(i);
		emit newSection(QString::fromStdString(section->name), section->uNativeAddr, section->uNativeAddr + section->uSectionSize);
	}

	emit loadCompleted();
}

void
Decompiler::decode()
{
	emit decoding();

	bool gotMain;
	ADDRESS a = fe->getMainEntryPoint(gotMain);
	auto it = std::find(user_entrypoints.begin(), user_entrypoints.end(), a);
	if (it != user_entrypoints.end())
		fe->decode();

	for (const auto &ep : user_entrypoints)
		prog->decodeEntryPoint(ep);

	if (!Boomerang::get().noDecodeChildren) {
		// decode anything undecoded
		fe->decode(NO_ADDRESS);
	}

	prog->finishDecode();

	emit decodeCompleted();
}

void
Decompiler::decompile()
{
	emit decompiling();

	prog->decompile();

	emit decompileCompleted();
}

void
Decompiler::emitClusterAndChildren(Cluster *root)
{
	emit newCluster(QString::fromStdString(root->getName()));
	for (const auto &child : *root)
		emitClusterAndChildren(child);
}

void
Decompiler::generateCode()
{
	emit generatingCode();

	prog->generateCode();

	Cluster *root = prog->getRootCluster();
	if (root)
		emitClusterAndChildren(root);
	std::list<Proc *>::iterator it;
	for (UserProc *p = prog->getFirstUserProc(it); p; p = prog->getNextUserProc(it)) {
		emit newProcInCluster(QString::fromStdString(p->getName()), QString::fromStdString(p->getCluster()->getName()));
	}

	emit generateCodeCompleted();
}

const char *
Decompiler::procStatus(UserProc *p)
{
	switch (p->getStatus()) {
	case PROC_UNDECODED:
		return "undecoded";
	case PROC_DECODED:
		return "decoded";
	case PROC_SORTED:
		return "sorted";
	case PROC_VISITED:
		return "visited";
	case PROC_INCYCLE:
		return "in cycle";
	case PROC_PRESERVEDS:
		return "preserveds";
	case PROC_EARLYDONE:
		return "early done";
	case PROC_FINAL:
		return "final";
	case PROC_CODE_GENERATED:
		return "code generated";
	}
	return "unknown";
}

void
Decompiler::alert_considering(Proc *parent, Proc *p)
{
	emit consideringProc(parent ? QString::fromStdString(parent->getName()) : QString(""), QString::fromStdString(p->getName()));
}

void
Decompiler::alert_decompiling(UserProc *p)
{
	emit decompilingProc(QString::fromStdString(p->getName()));
}

void
Decompiler::alert_new(Proc *p)
{
	if (auto lp = dynamic_cast<LibProc *>(p)) {
		QString params;
		auto sig = lp->getSignature();
		if (!sig || sig->isUnknown()) {
			params = "<unknown>";
		} else {
			for (unsigned int i = 0; i < sig->getNumParams(); ++i) {
				Type *ty = sig->getParamType(i);
				params.append(ty->getCtype().c_str());
				params.append(" ");
				params.append(sig->getParamName(i));
				if (i != sig->getNumParams() - 1)
					params.append(", ");
			}
		}
		emit newLibProc(QString::fromStdString(lp->getName()), params);
		return;
	}
	if (auto up = dynamic_cast<UserProc *>(p)) {
		emit newUserProc(QString::fromStdString(up->getName()), up->getNativeAddress());
		return;
	}
}

void
Decompiler::alert_remove(Proc *p)
{
	if (auto lp = dynamic_cast<LibProc *>(p)) {
		emit removeLibProc(QString::fromStdString(lp->getName()));
		return;
	}
	if (auto up = dynamic_cast<UserProc *>(p)) {
		emit removeUserProc(QString::fromStdString(up->getName()), up->getNativeAddress());
		return;
	}
}

void
Decompiler::alert_update_signature(Proc *p)
{
	alert_new(p);
}

bool
Decompiler::getRtlForProc(const QString &name, QString &rtl)
{
	auto p = prog->findProc(name.toStdString());
	auto up = dynamic_cast<UserProc *>(p);
	if (!up)
		return false;
	std::ostringstream os;
	up->print(os, true);
	rtl = os.str().c_str();
	return true;
}

void
Decompiler::alert_decompile_debug_point(UserProc *p, const std::string &description)
{
	LOG << p->getName() << ": " << description << "\n";
	if (debugging) {
		waiting = true;
		emit debuggingPoint(QString::fromStdString(p->getName()), QString::fromStdString(description));
		while (waiting) {
			thread()->wait(10);
		}
	}
}

void
Decompiler::stopWaiting()
{
	waiting = false;
}

QString
Decompiler::getSigFile(const QString &name)
{
	if (auto p = prog->findProc(name.toStdString()))
		if (auto lp = dynamic_cast<LibProc *>(p))
			if (auto sig = lp->getSignature())
				return QString::fromStdString(sig->getSigFile());
	return QString();
}

QString
Decompiler::getClusterFile(const QString &name)
{
	Cluster *c = prog->findCluster(name.toStdString());
	if (c)
		return QString::fromStdString(c->getOutPath("c"));
	return QString();
}

void
Decompiler::rereadLibSignatures()
{
	prog->rereadLibSignatures();
}

void
Decompiler::renameProc(const QString &oldName, const QString &newName)
{
	Proc *p = prog->findProc(oldName.toStdString());
	if (p)
		p->setName(newName.toStdString());
}

void
Decompiler::getCompoundMembers(const QString &name, QTableWidget *tbl)
{
	Type *ty = Type::getNamedType(name.toStdString());
	tbl->setRowCount(0);
	if (!ty || !ty->resolvesToCompound())
		return;
	CompoundType *c = ty->asCompound();
	for (auto it = c->cbegin(); it != c->cend(); ++it) {
		tbl->setRowCount(tbl->rowCount() + 1);
		tbl->setItem(tbl->rowCount() - 1, 0, new QTableWidgetItem(tr("%1").arg(c->getOffsetTo(it))));
		tbl->setItem(tbl->rowCount() - 1, 1, new QTableWidgetItem(tr("%1").arg(c->getOffsetTo(it) / 8)));
		tbl->setItem(tbl->rowCount() - 1, 2, new QTableWidgetItem(QString(c->getName(it))));
		tbl->setItem(tbl->rowCount() - 1, 3, new QTableWidgetItem(tr("%1").arg(c->getType(it)->getSize())));
	}
}
