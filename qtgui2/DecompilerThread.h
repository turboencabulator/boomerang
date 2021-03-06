/**
 * \file
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef DECOMPILERTHREAD_H
#define DECOMPILERTHREAD_H

#include "boomerang.h"
#include "types.h"

#include <QThread>
#include <QString>
#include <QTableWidget>

#include <vector>

class FrontEnd;
class Proc;
class UserProc;
class Prog;
class Cluster;
class QTableWidget;
class QObject;

class Decompiler : public QObject, public Watcher {
	Q_OBJECT

public:
	Decompiler() { }

	void alert_decompile_debug_point(UserProc *p, const std::string &description) override;
	void alert_considering(Proc *parent, Proc *p) override;
	void alert_decompiling(UserProc *p) override;
	void alert_new(Proc *p) override;
	void alert_remove(Proc *p) override;
	void alert_update_signature(Proc *p) override;

	bool getRtlForProc(const QString &name, QString &rtl);
	QString getSigFile(const QString &name);
	QString getClusterFile(const QString &name);
	void renameProc(const QString &oldName, const QString &newName);
	void rereadLibSignatures();
	void getCompoundMembers(const QString &name, QTableWidget *tbl);

	void setDebugging(bool d) { debugging = d; }
	void setUseDFTA(bool d);
	void setNoDecodeChildren(bool d);

	void addEntryPoint(ADDRESS a, const QString &);
	void removeEntryPoint(ADDRESS a);

public slots:
	void changeInputFile(const QString &f);
	void changeOutputPath(const QString &path);
	void load();
	void decode();
	void decompile();
	void generateCode();
	void stopWaiting();

signals:
	void loading();
	void decoding();
	void decompiling();
	void generatingCode();
	void loadCompleted();
	void decodeCompleted();
	void decompileCompleted();
	void generateCodeCompleted();

	void consideringProc(const QString &parent, const QString &name);
	void decompilingProc(const QString &name);
	void newUserProc(const QString &name, unsigned int addr);
	void newLibProc(const QString &name, const QString &params);
	void removeUserProc(const QString &name, unsigned int addr);
	void removeLibProc(const QString &name);
	void newCluster(const QString &name);
	void newProcInCluster(const QString &name, const QString &cluster);
	void newEntrypoint(unsigned int addr, const QString &name);
	void newSection(const QString &name, unsigned int start, unsigned int end);

	void machineType(const QString &machine);

	void debuggingPoint(const QString &name, const QString &description);

protected:

	bool debugging = false, waiting = false;

	FrontEnd *fe;
	Prog *prog;

	QString filename;

	const char *procStatus(UserProc *p);
	void emitClusterAndChildren(Cluster *root);

	std::vector<ADDRESS> user_entrypoints;
};

class DecompilerThread : public QThread {
	Q_OBJECT

public:
	DecompilerThread() { }

	Decompiler *getDecompiler();

protected:
	void run();

	Decompiler *decompiler = nullptr;
};

#endif
