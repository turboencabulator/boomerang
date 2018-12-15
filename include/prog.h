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

class Global {
private:
	        Type       *type = nullptr;
	        ADDRESS     uaddr = 0;
	        std::string nam = "";

public:
	                    Global(Type *type, ADDRESS uaddr, const char *nam) : type(type), uaddr(uaddr), nam(nam) { }
	virtual            ~Global();

	        Type       *getType() const { return type; }
	        void        setType(Type *ty) { type = ty; }
	        void        meetType(Type *ty);
	        ADDRESS     getAddress() const { return uaddr; }
	        const char *getName() const { return nam.c_str(); }
	        Exp        *getInitialValue(Prog *prog) const;  // Get the initial value as an expression (or nullptr if not initialised)
	        void        print(std::ostream &os, Prog *prog) const;  // Print to stream os

protected:
	                    Global() { }
	friend class XMLProgParser;
};

class Prog {
public:
	                    Prog();                         // Default constructor
	virtual            ~Prog();
	                    Prog(const char *name);         // Constructor with name
	        void        setFrontEnd(FrontEnd *fe);
	        void        setName(const std::string &name);  // Set the name of this program
	        Proc       *setNewProc(ADDRESS uNative);    // Set up new proc
	// Return a pointer to a new proc
	        Proc       *newProc(const char *name, ADDRESS uNative, bool bLib = false);
	        void        remProc(UserProc *proc);        // Remove the given UserProc
	        void        removeProc(const char *name);
	        const char *getName() const { return m_name.c_str(); }  // Get the name of this program
	        const char *getPath() const { return m_path.c_str(); }
	        const char *getPathAndName() const { return (m_path + m_name).c_str(); }
	        int         getNumProcs() const;            // # of procedures stored in prog
	        int         getNumUserProcs() const;        // # of user procedures stored in prog
	        Proc       *getProc(int i) const;           // returns pointer to indexed proc
	// Find the Proc with given address, nullptr if none, -1 if deleted
	        Proc       *findProc(ADDRESS uAddr) const;
	// Find the Proc with the given name
	        Proc       *findProc(const char *name) const;
	// Find the Proc that contains the given address
	        Proc       *findContainingProc(ADDRESS uAddr) const;
	        bool        isProcLabel(ADDRESS addr);      // Checks if addr is a label or not
	// get the filename of this program
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

	// Lookup the given native address in the code section, returning a host pointer corresponding to the same
	// address
	        const void *getCodeInfo(ADDRESS uAddr, const char *&last, ptrdiff_t &delta) const;

	        const char *getRegName(int idx) const { return pFE->getRegName(idx); }
	        int         getRegSize(int idx) const { return pFE->getRegSize(idx); }

	        void        decodeEntryPoint(ADDRESS a);
	        void        setEntryPoint(ADDRESS a);  // As per the above, but don't decode
	        void        decodeEverythingUndecoded();
	        void        decodeFragment(UserProc *proc, ADDRESS a);

	// Re-decode this proc from scratch
	        void        reDecode(UserProc *proc);

	// Well form all the procedures/cfgs in this program
	        bool        wellForm();

	// last fixes after decoding everything
	        void        finishDecode();

	// Do the main non-global decompilation steps
	        void        decompile();

	        void        removeUnusedGlobals();
	        void        removeUnusedLocals();

	// Type analysis
	        void        globalTypeAnalysis();

	/// Remove unused return locations
	/// \return true if any returns are removed
	        bool        removeUnusedReturns();

	// Convert from SSA form
	        void        fromSSAform();

	// Type analysis
	        void        conTypeAnalysis();

	// Range analysis
	        void        rangeAnalysis();

	// Generate dotty file
	        void        generateDot(std::ostream &os) const;

	// Generate code
	        void        generateCode(std::ostream &os);
	        void        generateCode(Cluster *cluster = nullptr, UserProc *proc = nullptr, bool intermixRTL = false);
	        void        generateRTL(Cluster *cluster = nullptr, UserProc *proc = nullptr) const;

	// Print this program (primarily for debugging)
	        void        print(std::ostream &out) const;

	// lookup a library procedure by name; create if does not exist
	        LibProc    *getLibraryProc(const char *nam);

	// Get a library signature for a given name (used when creating a new library proc.
	        Signature  *getLibSignature(const char *name) const;
	        void        rereadLibSignatures();

	        Statement  *getStmtAtLex(Cluster *cluster, unsigned int begin, unsigned int end) const;

	// Get the front end id used to make this prog
	        platform    getFrontEndId() const;

	        const std::map<ADDRESS, std::string> &getSymbols() const;

	        Signature  *getDefaultSignature(const char *name) const;

	        std::vector<Exp *> &getDefaultParams() const;
	        std::vector<Exp *> &getDefaultReturns() const;

	// Returns true if this is a win32 program
	        bool        isWin32() const;

	// Get a global variable if possible, looking up the loader's symbol table if necessary
	        const char *getGlobalName(ADDRESS uaddr) const;
	        ADDRESS     getGlobalAddr(const char *nam) const;
	        Global     *getGlobal(const char *nam) const;

	// Make up a name for a new global at address uaddr (or return an existing name if address already used)
	        const char *newGlobalName(ADDRESS uaddr);

	// Guess a global's type based on its name and address
	        Type       *guessGlobalType(const char *nam, ADDRESS u) const;

	// Make an array type for the global array at u. Mainly, set the length sensibly
	        ArrayType  *makeArrayType(ADDRESS u, Type *t);

	// Indicate that a given global has been seen used in the program.
	        bool        globalUsed(ADDRESS uaddr, Type *knownType = nullptr);

	// Get the type of a global variable
	        Type       *getGlobalType(const char *nam) const;

	// Set the type of a global variable
	        void        setGlobalType(const char *name, Type *ty);

	// get a string constant at a give address if appropriate
	        const char *getStringConstant(ADDRESS uaddr, bool knownString = false) const;
	        double      getFloatConstant(ADDRESS uaddr, bool &ok, int bits = 64) const;

	// Hacks for Mike
	        MACHINE     getMachine() const { return pBF->getMachine(); }  // Get a code for the machine e.g. MACHINE_SPARC
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

	        bool        processProc(int addr, UserProc *proc) { std::ofstream os; return pFE->processProc((unsigned)addr, proc, os); }  // Decode a proc

	        void        readSymbolFile(const char *fname);

	// Public booleans that are set if and when a register jump or call is
	// found, respectively
	        bool        bRegisterJump;
	        bool        bRegisterCall;

	        void        printSymbolsToFile() const;
	        void        printCallGraph() const;
	        void        printCallGraphXML() const;

	        Cluster    *getRootCluster() const { return m_rootCluster; }
	        Cluster    *findCluster(const std::string &name) const { return m_rootCluster->find(name); }
	        Cluster    *getDefaultCluster(const std::string &name) const;
	        bool        clusterUsed(Cluster *c) const;

	// Add the given RTL to the front end's map from address to aldready-decoded-RTL
	        void        addDecodedRtl(ADDRESS a, RTL *rtl) { pFE->addDecodedRtl(a, rtl); }

	// This does extra processing on a constant.  The Exp* is expected to be a Const,
	// and the ADDRESS is the native location from which the constant was read.
	        Exp        *addReloc(Exp *e, ADDRESS lc);

protected:
	        BinaryFile *pBF = nullptr;      // Pointer to the BinaryFile object for the program
	        FrontEnd   *pFE = nullptr;      // Pointer to the FrontEnd object for the project

	/* Persistent state */
	        std::string m_name, m_path;     // name of the program and its full path
	        std::list<Proc *> m_procs;      // list of procedures
	        PROGMAP     m_procLabels;       // map from address to Proc*
	// FIXME: is a set of Globals the most appropriate data structure? Surely not.
	        std::set<Global *> globals;     // globals to print at code generation time
	        //std::map<ADDRESS, const char *> *globalMap; // Map of addresses to global symbols
	        DataIntervalMap globalMap;      // Map from address to DataInterval (has size, name, type)
	        int         m_iNumberedProc = 1;// Next numbered proc will use this
	        Cluster    *m_rootCluster;      // Root of the cluster tree

	friend class XMLProgParser;
};

#endif
