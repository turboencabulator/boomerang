/**
 * \file
 * \brief Interface for the program object.
 *
 * \authors
 * Copyright (C) 1998-2001, The University of Queensland
 * \authors
 * Copyright (C) 2001, Sun Microsystems, Inc
 * \authors
 * Copyright (C) 2002, Trent Waddington
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef PROG_H
#define PROG_H

#include "BinaryFile.h"
#include "cluster.h"
#include "frontend.h"
#include "type.h"

#include <fstream>
#include <ostream>
#include <list>
#include <map>
#include <set>
#include <string>
#include <vector>

class LibProc;
class Proc;
class Signature;
class Statement;
class StatementSet;
class UserProc;

typedef std::map<ADDRESS, Proc *> PROGMAP;

/**
 * Holds information about a global variable.
 */
class Global {
private:
	        Type       *type = nullptr;
	        ADDRESS     uaddr = 0;
	        std::string nam;

public:
	                    Global(Type *type, ADDRESS uaddr, const std::string &nam) : type(type), uaddr(uaddr), nam(nam) { }
	virtual            ~Global() = default;

	        Type       *getType() const { return type; }
	        void        setType(Type *ty) { type = ty; }
	        void        meetType(Type *ty);
	        ADDRESS     getAddress() const { return uaddr; }
	        const std::string &getName() const { return nam; }
	        Exp        *getInitialValue(Prog *prog) const;
	        void        print(std::ostream &os, Prog *prog) const;

protected:
	                    Global() = default;
	friend class XMLProgParser;
};

/**
 * Holds information of interest to the whole program.
 */
class Prog {
public:
	                    Prog();
	virtual            ~Prog();
	                    Prog(const std::string &);
	static  Prog       *open(const char *);
	        FrontEnd   *getFrontEnd() const { return pFE; }
	        void        setFrontEnd(FrontEnd *);
	        void        setName(const std::string &);
	        Proc       *setNewProc(ADDRESS uNative);
	        Proc       *newProc(const std::string &, ADDRESS, bool = false);
	        //void        remProc(UserProc *proc);
	        void        removeProc(const std::string &);
	        const std::string &getName() const { return m_name; }  ///< Get the name of this program.
	        const std::string &getPath() const { return m_path; }
	        std::string getPathAndName() const { return m_path + m_name; }
	        int         getNumUserProcs() const;
	        Proc       *findProc(ADDRESS) const;
	        Proc       *findProc(const std::string &) const;
	        Proc       *findContainingProc(ADDRESS uAddr) const;
	        bool        isProcLabel(ADDRESS addr);
	        std::string getNameNoPath() const;
	        std::string getNameNoPathNoExt() const;
	// This pair of functions allows the user to iterate through all the procs
	// The procs will appear in order of native address
	        Proc       *getFirstProc(PROGMAP::const_iterator &it);
	        Proc       *getNextProc(PROGMAP::const_iterator &it);

	// This pair of functions allows the user to iterate through all the UserProcs
	// The procs will appear in topdown order
	        UserProc   *getFirstUserProc(std::list<Proc *>::iterator &it);
	        UserProc   *getNextUserProc(std::list<Proc *>::iterator &it);

	// list of UserProcs for entry point(s)
	        std::list<UserProc *> entryProcs;

	        void        decodeEntryPoint(ADDRESS a);
	        void        setEntryPoint(ADDRESS a);
	        void        decodeEverythingUndecoded();

	        void        reDecode(UserProc *proc);

	        bool        wellForm();

	        void        finishDecode();

	        void        decompile();

	        void        removeUnusedGlobals();
	        void        removeUnusedLocals();

	        void        globalTypeAnalysis();

	        bool        removeUnusedReturns();

	        void        fromSSAform();

	        void        conTypeAnalysis();

	//        void        rangeAnalysis();

	        void        generateDot(std::ostream &os) const;

	        void        generateCode(std::ostream &os);
	        void        generateCode(Cluster *cluster = nullptr, UserProc *proc = nullptr, bool intermixRTL = false);
	        void        generateRTL(Cluster *cluster = nullptr, UserProc *proc = nullptr) const;

	        void        print(std::ostream &out) const;

	        LibProc    *getLibraryProc(const std::string &);

	        Signature  *getLibSignature(const std::string &) const;
	        void        rereadLibSignatures();

	//        Statement  *getStmtAtLex(Cluster *cluster, unsigned int begin, unsigned int end) const;

	        platform    getFrontEndId() const;

	        const std::map<ADDRESS, std::string> &getSymbols() const;

	        Signature  *getDefaultSignature(const std::string &) const;

	        //std::vector<Exp *> &getDefaultParams() const;
	        //std::vector<Exp *> &getDefaultReturns() const;

	        bool        isWin32() const;

	        const char *getGlobalName(ADDRESS) const;
	        ADDRESS     getGlobalAddr(const std::string &) const;
	        Global     *getGlobal(const std::string &) const;

		std::string newGlobalName(ADDRESS);

	        Type       *guessGlobalType(const std::string &, ADDRESS) const;

	        ArrayType  *makeArrayType(ADDRESS u, Type *t);

	        bool        globalUsed(ADDRESS uaddr, Type *knownType = nullptr);

	        Type       *getGlobalType(const std::string &) const;

	        void        setGlobalType(const std::string &, Type *);

	        const char *getStringConstant(ADDRESS uaddr, bool knownString = false) const;
	        double      getFloatConstant(ADDRESS uaddr, bool &ok, int bits = 64) const;

	// Hacks for Mike
	        MACHINE     getMachine() const { return pBF->getMachine(); }  ///< Get a code for the machine e.g. MACHINE_SPARC.
	        const SectionInfo *getSectionInfoByAddr(ADDRESS a) const { return pBF->getSectionInfoByAddr(a); }
	        ADDRESS     getLimitTextLow() const { return pBF->getLimitTextLow(); }
	        ADDRESS     getLimitTextHigh() const { return pBF->getLimitTextHigh(); }
	        bool        isReadOnly(ADDRESS a) const { return pBF->isReadOnly(a); }
	// Read 1, 2, or 4 bytes given a native address
	        int         readNative1(ADDRESS a) const { return pBF->readNative1(a); }
	        int         readNative2(ADDRESS a) const { return pBF->readNative2(a); }
	        int         readNative4(ADDRESS a) const { return pBF->readNative4(a); }
	        Exp        *readNativeAs(ADDRESS uaddr, Type *type) const;

	        bool        isDynamicLinkedProcPointer(ADDRESS dest) const { return pBF->isDynamicLinkedProcPointer(dest); }
	        const char *getDynamicProcName(ADDRESS uNative) const { return pBF->getDynamicProcName(uNative); }

	        void        readSymbolFile(const std::string &);

	        void        printSymbolsToFile() const;
	        void        printCallGraph() const;

	        Cluster    *getRootCluster() const { return m_rootCluster; }
	        Cluster    *findCluster(const std::string &name) const { return m_rootCluster->find(name); }
	        Cluster    *getDefaultCluster(const std::string &) const;
	        bool        clusterUsed(Cluster *c) const;

	/**
	 * Add the given RTL to the front end's map from address to
	 * already-decoded-RTL.
	 */
	        void        addDecodedRtl(ADDRESS a, RTL *rtl) { pFE->addDecodedRtl(a, rtl); }

	        Exp        *addReloc(Exp *e, ADDRESS lc);

protected:
	        BinaryFile *pBF = nullptr;      ///< Pointer to the BinaryFile object for the program.
	        FrontEnd   *pFE = nullptr;      ///< Pointer to the FrontEnd object for the project.

	/* Persistent state */
	        std::string m_name;             ///< Name of the program.
	        std::string m_path;             ///< The program's full path.
	        std::list<Proc *> m_procs;      ///< List of procedures.
	        PROGMAP     m_procLabels;       ///< Map from address to Proc*.
	// FIXME: is a set of Globals the most appropriate data structure? Surely not.
	        std::set<Global *> globals;     ///< Globals to print at code generation time.
	        //std::map<ADDRESS, const char *> *globalMap; ///< Map of addresses to global symbols.
	        DataIntervalMap globalMap;      ///< Map from address to DataInterval (has size, name, type).
	        int         m_iNumberedProc = 1;///< Next numbered proc will use this.
	        Cluster    *m_rootCluster;      ///< Root of the cluster tree.

	friend class XMLProgParser;
};

#endif
