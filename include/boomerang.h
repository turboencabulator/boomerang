/**
 * \file
 * \brief Interface for the boomerang singleton object.
 *
 * \authors
 * Copyright (C) 2002, Trent Waddington
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

/**
 * \mainpage Introduction
 * \section Introduction
 *
 * Welcome to the Doxygen generated documentation for the %Boomerang
 * decompiler.  Not all classes and functions have been documented yet, but
 * eventually they will.  If you have figured out what a function is doing
 * please update the documentation and submit it as a patch.  Documentation
 * about a function should be at one place only, so document all functions at
 * the point of implementation (in the .c file).
 *
 * More information on the %Boomerang decompiler can be found at
 * http://boomerang.sourceforge.net.
 */

#ifndef BOOMERANG_H
#define BOOMERANG_H

// Defines to control experimental features
#define USE_DOMINANCE_NUMS 1  // Set true to store a statement number that has dominance properties

#include "types.h"

#include <fstream>
#include <map>
#include <ostream>
#include <set>
#include <string>
#include <vector>

class HLLCode;
class ObjcModule;
class Proc;
class Prog;
class UserProc;

#define DEBUG_RANGE_ANALYSIS 0

/**
 * \brief Virtual class to monitor the decompilation.
 */
class Watcher {
public:
	virtual            ~Watcher() = default;

	virtual void        alert_new(Proc *) { }
	virtual void        alert_load(Proc *) { }
	virtual void        alert_remove(Proc *) { }
	virtual void        alert_update_signature(Proc *) { }
	virtual void        alert_decode_start(ADDRESS, int) { }
	virtual void        alert_decode_bad(ADDRESS) { }
	virtual void        alert_decode_inst(ADDRESS, int) { }
	virtual void        alert_decode_proc(Proc *, ADDRESS, ADDRESS, int) { }
	virtual void        alert_decode_end() { }
	virtual void        alert_considering(Proc *, Proc *) { }
	virtual void        alert_proc_status_change(UserProc *) { }
	virtual void        alert_decompiling(UserProc *) { }
	virtual void        alert_decompile_start(UserProc *) { }
	virtual void        alert_decompile_SSADepth(UserProc *, int) { }
	virtual void        alert_decompile_beforePropagate(UserProc *, int) { }
	virtual void        alert_decompile_afterPropagate(UserProc *, int) { }
	virtual void        alert_decompile_afterRemoveStmts(UserProc *, int) { }
	virtual void        alert_decompile_end(UserProc *) { }
	virtual void        alert_decompile_debug_point(UserProc *, const std::string &) { }
};

/**
 * Controls the loading, decoding, decompilation and code generation for a
 * program.  This is the main class of the decompiler.
 */
class Boomerang {
private:
	static  Boomerang  *boomerang;
	        /// String with the path to the boomerang executable.
	        std::string progPath;
	        /// The path where all output files are created.
	        std::string outputPath;
	        /// Takes care of the log messages.
	        std::ostream *logger = nullptr;
	        /// The watchers which are interested in this decompilation.
	        std::set<Watcher *> watchers;

	static  void        usage();
	static  void        help();
	static  void        helpcmd();
	static  int         splitLine(char *line, const char *argv[]);
	        int         parseCmd(int argc, const char *argv[]);
	        int         cmdLine();

	        // constructor & destructor are private for the singleton pattern.
	                    Boomerang();
	                   ~Boomerang() = default;
public:
	static  Boomerang  &get();

	static  const char *getVersionStr();
	        std::ostream &log();
	        void        setLogger(std::ostream *l) { logger = l; }
	        bool        setOutputDirectory(const std::string &);

	        /// \return The HLLCode for the specified UserProc.
	static  HLLCode    *getHLLCode(UserProc *p = nullptr);

	        int         commandLine(int argc, const char *argv[]);
	        /// Set the path to the %Boomerang executable.
	        void        setProgPath(const std::string &p) { progPath = p; }
	        /// Get the path to the %Boomerang executable.
	        const std::string &getProgPath() { return progPath; }
	        /// Set the path where the output files are saved.
	        void        setOutputPath(const std::string &p) { outputPath = p; }
	        /// Returns the path to where the output files are saved.
	        const std::string &getOutputPath() { return outputPath; }

	        Prog       *loadAndDecode(const char *fname, const char *pname = nullptr);
	        int         decompile(const char *fname, const char *pname = nullptr);
	        /// Add a Watcher to the set of Watchers for this Boomerang object.
	        void        addWatcher(Watcher *watcher) { watchers.insert(watcher); }

	static  void        persistToXML(Prog *prog);
#ifdef ENABLE_XML_LOAD
	static  Prog       *loadFromXML(const std::string &);
#endif

	static  void        objcDecode(const std::map<std::string, ObjcModule> &modules, Prog *prog);

	        void        alert_new(Proc *);
	        void        alert_load(Proc *);
	        void        alert_remove(Proc *);
	        void        alert_update_signature(Proc *);
	        void        alert_decode_start(ADDRESS, int);
	        void        alert_decode_bad(ADDRESS);
	        void        alert_decode_inst(ADDRESS, int);
	        void        alert_decode_proc(Proc *, ADDRESS, ADDRESS, int);
	        void        alert_decode_end();
	        void        alert_considering(Proc *, Proc *);
	        void        alert_proc_status_change(UserProc *);
	        void        alert_decompiling(UserProc *);
	        void        alert_decompile_start(UserProc *);
	        void        alert_decompile_SSADepth(UserProc *, int);
	        void        alert_decompile_beforePropagate(UserProc *, int);
	        void        alert_decompile_afterPropagate(UserProc *, int);
	        void        alert_decompile_afterRemoveStmts(UserProc *, int);
	        void        alert_decompile_end(UserProc *);
	        void        alert_decompile_debug_point(UserProc *, const std::string &);

	        // Command line flags
	        bool        vFlag = false;
	        bool        printRtl = false;
	        bool        noBranchSimplify = false;
	        bool        noRemoveNull = false;
	        bool        noLocals = false;
	        bool        noRemoveLabels = false;
	        bool        noDataflow = false;
	        bool        noDecompile = false;
	        bool        stopBeforeDecompile = false;
	        bool        traceDecoder = false;
	        bool        dotFile = false;
	        int         numToPropagate = -1;
	        bool        noPromote = false;
	        bool        propOnlyToAll = false;
	        bool        debugGen = false;
	        int         maxMemDepth = 99;
	        bool        debugSwitch = false;
	        bool        noParameterNames = false;
	        bool        debugLiveness = false;
	        bool        stopAtDebugPoints = false;
	        bool        debugTA = false;
	        /// A vector which contains all know entrypoints for the Prog.
	        std::vector<ADDRESS> entrypoints;
	        /// A vector containing the names off all symbolfiles to load.
	        std::vector<std::string> symbolFiles;
	        /// A map to find a name by a given address.
	        std::map<ADDRESS, std::string> symbols;
	        /// When true, attempt to decode main, all children, and all procs.
	        /// \a decodeMain is set when there are no -e or -E switches given
	        bool        decodeMain = true;
	        bool        printAST = false;
	        bool        dumpXML = false;
	        bool        noRemoveReturns = false;
	        bool        debugDecoder = false;
	        bool        decodeThruIndCall = false;
	        std::ofstream *ofsIndCallReport = nullptr;
	        bool        noDecodeChildren = false;
	        bool        debugProof = false;
	        bool        debugUnused = false;
#ifdef ENABLE_XML_LOAD
	        bool        loadBeforeDecompile = false;
#endif
	        bool        saveBeforeDecompile = false;
	        bool        noProve = false;
	        bool        noChangeSignatures = false;
	        bool        conTypeAnalysis = false;
	        bool        dfaTypeAnalysis = true;
	        int         propMaxDepth = 3;      ///< Max depth of expression that will be propagated to more than one dest
	        bool        generateCallGraph = false;
	        bool        generateSymbols = false;
	        bool        noGlobals = false;
	        bool        assumeABI = false;     ///< Assume ABI compliance
	        bool        experimental = false;  ///< Activate experimental code. Caution!
	        int         minsToStopAfter = 0;
};

#define LOG                 (Boomerang::get().log())
#define VERBOSE             (Boomerang::get().vFlag)
#define DEBUG_TA            (Boomerang::get().debugTA)
#define DEBUG_PROOF         (Boomerang::get().debugProof)
#define DEBUG_UNUSED        (Boomerang::get().debugUnused)
#define DEBUG_LIVENESS      (Boomerang::get().debugLiveness)
#define DFA_TYPE_ANALYSIS   (Boomerang::get().dfaTypeAnalysis)
#define CON_TYPE_ANALYSIS   (Boomerang::get().conTypeAnalysis)
#define ADHOC_TYPE_ANALYSIS (!DFA_TYPE_ANALYSIS && !CON_TYPE_ANALYSIS)
#define DEBUG_GEN           (Boomerang::get().debugGen)
#define DUMP_XML            (Boomerang::get().dumpXML)
#define DEBUG_SWITCH        (Boomerang::get().debugSwitch)
#define EXPERIMENTAL        (Boomerang::get().experimental)

#endif
