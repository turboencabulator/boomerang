/**
 * \file
 * \brief Command line processing for the Boomerang decompiler
 *
 * \authors
 * Copyright (C) 2002-2006, Mike Van Emmerik and Trent Waddington
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "boomerang.h"

#include "cluster.h"
#include "codegen/chllcode.h"
#include "frontend.h"
#include "log.h"
#include "proc.h"
#include "prog.h"
//#include "transformer.h"
#ifdef USE_XML
#include "xmlprogparser.h"
#endif

// For the -nG switch to disable the garbage collector
#ifdef GARBAGE_COLLECTOR
#include <gc/gc.h>
#endif

#include <sys/stat.h>   // For mkdir
#include <sys/types.h>
#include <unistd.h>     // For unlink

#include <iostream>
#include <fstream>
#include <set>
#include <string>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <ctime>

Boomerang *Boomerang::boomerang = NULL;

#ifndef DATADIR
#define DATADIR "."
#endif

#ifndef OUTPUTDIR
#define OUTPUTDIR "./output"
#endif

/**
 * Initializes the Boomerang object.
 * The default settings are:
 * - All options disabled
 * - Infinite propagations
 * - A maximum memory depth of 99
 * - The path to the executable is "./"
 * - The output directory is "./output/"
 */
Boomerang::Boomerang() :
	progPath(DATADIR "/"),
	outputPath(OUTPUTDIR "/")
{
}

/**
 * Returns the Log object associated with the object.
 */
Log &
Boomerang::log()
{
	return *logger;
}

/**
 * Sets the outputfile to be the file "log" in the default output directory.
 */
FileLogger::FileLogger() :
	out((Boomerang::get()->getOutputPath() + "log").c_str())
{
}

/**
 * Returns the HLLCode for the given proc.
 */
HLLCode *
Boomerang::getHLLCode(UserProc *p)
{
	return new CHLLCode(p);
}

/**
 * Prints a short usage statement.
 */
void
Boomerang::usage()
{
	static const char str[] =
		"Usage: boomerang [ switches ] <program>\n"
		"boomerang -h for switch help\n"
	;
	std::cout << str;
	exit(1);
}

/**
 * Prints help for the interactive mode.
 */
void
Boomerang::helpcmd()
{
	static const char str[] =
		//___.____1____.____2____.____3____.____4____.____5____.____6____.____7____.____8
		"Available commands (for use with -k):\n"
		"  decode                             : Loads and decodes the specified binary.\n"
		"  decompile [proc]                   : Decompiles the program or specified proc.\n"
		"  codegen [cluster]                  : Generates code for the program or a\n"
		"                                       specified cluster.\n"
		"  move proc <proc> <cluster>         : Moves the specified proc to the specified\n"
		"                                       cluster.\n"
		"  move cluster <cluster> <parent>    : Moves the specified cluster to the\n"
		"                                       specified parent cluster.\n"
		"  add cluster <cluster> [parent]     : Adds a new cluster to the root/specified\n"
		"                                       cluster.\n"
		"  delete cluster <cluster>           : Deletes an empty cluster.\n"
		"  rename proc <proc> <newname>       : Renames the specified proc.\n"
		"  rename cluster <cluster> <newname> : Renames the specified cluster.\n"
		"  info prog                          : Print info about the program.\n"
		"  info cluster <cluster>             : Print info about a cluster.\n"
		"  info proc <proc>                   : Print info about a proc.\n"
		"  print <proc>                       : Print the RTL for a proc.\n"
		"  help                               : This help.\n"
		"  exit                               : Quit the shell.\n"
	;
	std::cout << str;
}

/**
 * Prints help about the command line switches.
 */
void
Boomerang::help()
{
	static const char str[] =
		"Symbols\n"
		"  -s <addr> <name> : Define a symbol\n"
		"  -sf <filename>   : Read a symbol/signature file\n"
		"\n"
		"Decoding/decompilation options\n"
		"  -e <addr>        : Decode the procedure beginning at addr, and callees\n"
		"  -E <addr>        : Decode the procedure at addr, no callees\n"
		"                     Use -e and -E repeatedly for multiple entry points\n"
		"  -ic              : Decode through type 0 Indirect Calls\n"
		"  -S <min>         : Stop decompilation after specified number of minutes\n"
		"  -t               : Trace (print address of) every instruction decoded\n"
		"  -Tc              : Use old constraint-based type analysis\n"
		"  -Td              : Use data-flow-based type analysis\n"
#ifdef USE_XML
		"  -LD              : Load before decompile (<program> becomes xml input file)\n"
		"  -SD              : Save before decompile\n"
#endif
		"  -a               : Assume ABI compliance\n"
		//"  -pa              : only propagate if can propagate to all\n"
		"\n"
		"Output\n"
		"  -v               : Verbose\n"
		"  -h               : This help\n"
		"  -o <output path> : Where to generate output (defaults to ./output)\n"
		"  -x               : Dump XML files\n"
		"  -r               : Print RTL for each proc to log before code generation\n"
		"  -gd <dot file>   : Generate a dotty graph of the program's CFG and DFG\n"
		"  -gc              : Generate a call graph (callgraph.out and callgraph.dot)\n"
		"  -gs              : Generate a symbol file (symbols.h)\n"
		"  -iw              : Write indirect call report to output/indirect.txt\n"
		"\n"
		"Misc.\n"
		"  -k               : Command mode, for available commands see -h cmd\n"
		"  -P <path>        : Path to Boomerang files\n"
		"  -X               : activate eXperimental code; errors likely\n"
		"  --               : No effect (used for testing)\n"
		"\n"
		"Debug\n"
		"  -da              : Print AST before code generation\n"
		"  -dc              : Debug switch (Case) analysis\n"
		"  -dd              : Debug decoder to stdout\n"
		"  -dg              : Debug code Generation\n"
		"  -dl              : Debug liveness (from SSA) code\n"
		"  -dp              : Debug proof engine\n"
		"  -ds              : Stop at debug points for keypress\n"
		"  -dt              : Debug type analysis\n"
		"  -du              : Debug removing unused statements etc\n"
		"\n"
		"Restrictions\n"
		"  -nb              : No simplifications for branches\n"
		"  -nc              : No decode children in the call graph (callees)\n"
		"  -nd              : No (reduced) dataflow analysis\n"
		"  -nD              : No decompilation (at all!)\n"
		"  -nl              : No creation of local variables\n"
		//"  -nm              : No decoding of the 'main' procedure\n"
		"  -ng              : No replacement of expressions with Globals\n"
#ifdef GARBAGE_COLLECTOR
		"  -nG              : No garbage collection\n"
#endif
		"  -nn              : No removal of NULL and unused statements\n"
		"  -np              : No replacement of expressions with Parameter names\n"
		"  -nP              : No promotion of signatures\n"
		"                     (other than main/WinMain/DriverMain)\n"
		"  -nr              : No removal of unneeded labels\n"
		"  -nR              : No removal of unused Returns\n"
		"  -l <depth>       : Limit multi-propagations to expressions with depth <depth>\n"
		"  -p <num>         : Only do num propagations\n"
		"  -m <num>         : Max memory depth\n"
	;
	std::cout << str;
	exit(1);
}

/**
 * Creates a directory and tests it.
 *
 * \param dir   The name of the directory.
 *
 * \retval true The directory is valid.
 * \retval false The directory is invalid.
 */
static bool
createDirectory(std::string dir)
{
	std::string remainder(dir);
	std::string path;
	std::string::size_type i;
	while ((i = remainder.find('/')) != remainder.npos) {
		path += remainder.substr(0, i + 1);
		remainder = remainder.substr(i + 1);
		mkdir(path.c_str(), 0777);  // Doesn't matter if already exists
	}
	// Now try to create a test file
	path += remainder;
	mkdir(path.c_str(), 0777);  // Make the last dir if needed
	path += "test.file";
	std::ofstream test;
	test.open(path.c_str(), std::ios::out);
	test << "testing\n";
	bool pathOK = !test.bad();
	test.close();
	if (pathOK)
		remove(path.c_str());
	return pathOK;
}

/**
 * Prints a tree graph.
 */
void
Cluster::printTree(std::ostream &out)
{
	out << "\t\t" << name << "\n";
	for (unsigned i = 0; i < children.size(); i++)
		children[i]->printTree(out);
}

/**
 * Splits a string up in different words.
 * use like: argc = splitLine(line, &argv);
 *
 * \param[in] line      the string to parse
 * \param[out] argv     argv array to fill
 *
 * \return The number of words found (argc).
 */
int
Boomerang::splitLine(char *line, const char *argv[])
{
	int argc = 0;
	char *p = strtok(line, " \r\n");
	while (p) {
		argv[argc++] = p;
		p = strtok(NULL, " \r\n");
	}
	return argc;
}

/**
 * Parse and execute a command supplied in interactive mode.
 *
 * \param argc      The number of arguments.
 * \param argv      Pointers to the arguments.
 *
 * \return A value indicating what happened.
 *
 * \retval 0 Success
 * \retval 1 Failure
 * \retval 2 The user exited with \a quit or \a exit
 */
int
Boomerang::parseCmd(int argc, const char *argv[])
{
	static Prog *prog = NULL;
	if (!strcmp(argv[0], "decode")) {
		if (argc <= 1) {
			std::cerr << "not enough arguments for cmd\n";
			return 1;
		}
		const char *fname = argv[1];
		Prog *p = loadAndDecode(fname);
		if (p == NULL) {
			std::cerr << "failed to load " << fname << "\n";
			return 1;
		}
		delete prog;
		prog = p;
#ifdef USE_XML
	} else if (!strcmp(argv[0], "load")) {
		if (argc <= 1) {
			std::cerr << "not enough arguments for cmd\n";
			return 1;
		}
		const char *fname = argv[1];
		Prog *p = loadFromXML(fname);
		if (p == NULL) p = loadFromXML((outputPath + fname + "/" + fname + ".xml").c_str());  // try guessing
		if (p == NULL) {
			std::cerr << "failed to read xml " << fname << "\n";
			return 1;
		}
		delete prog;
		prog = p;
	} else if (!strcmp(argv[0], "save")) {
		if (prog == NULL) {
			std::cerr << "need to load or decode before save!\n";
			return 1;
		}
		persistToXML(prog);
#endif
	} else if (!strcmp(argv[0], "decompile")) {
		if (argc > 1) {
			Proc *proc = prog->findProc(argv[1]);
			if (proc == NULL) {
				std::cerr << "cannot find proc " << argv[1] << "\n";
				return 1;
			}
			if (proc->isLib()) {
				std::cerr << "cannot decompile a lib proc\n";
				return 1;
			}
			int indent = 0;
			((UserProc *)proc)->decompile(new ProcList, indent);
		} else {
			prog->decompile();
		}
	} else if (!strcmp(argv[0], "codegen")) {
		if (argc > 1) {
			Cluster *cluster = prog->findCluster(argv[1]);
			if (cluster == NULL) {
				std::cerr << "cannot find cluster " << argv[1] << "\n";
				return 1;
			}
			prog->generateCode(cluster);
		} else {
			prog->generateCode();
		}
	} else if (!strcmp(argv[0], "move")) {
		if (argc <= 1) {
			std::cerr << "not enough arguments for cmd\n";
			return 1;
		}
		if (!strcmp(argv[1], "proc")) {
			if (argc <= 3) {
				std::cerr << "not enough arguments for cmd\n";
				return 1;
			}

			Proc *proc = prog->findProc(argv[2]);
			if (proc == NULL) {
				std::cerr << "cannot find proc " << argv[2] << "\n";
				return 1;
			}

			Cluster *cluster = prog->findCluster(argv[3]);
			if (cluster == NULL) {
				std::cerr << "cannot find cluster " << argv[3] << "\n";
				return 1;
			}
			proc->setCluster(cluster);
		} else if (!strcmp(argv[1], "cluster")) {
			if (argc <= 3) {
				std::cerr << "not enough arguments for cmd\n";
				return 1;
			}

			Cluster *cluster = prog->findCluster(argv[2]);
			if (cluster == NULL) {
				std::cerr << "cannot find cluster " << argv[2] << "\n";
				return 1;
			}

			Cluster *parent = prog->findCluster(argv[3]);
			if (parent == NULL) {
				std::cerr << "cannot find cluster " << argv[3] << "\n";
				return 1;
			}

			parent->addChild(cluster);
		} else {
			std::cerr << "don't know how to move a " << argv[1] << "\n";
			return 1;
		}
	} else if (!strcmp(argv[0], "add")) {
		if (argc <= 1) {
			std::cerr << "not enough arguments for cmd\n";
			return 1;
		}
		if (!strcmp(argv[1], "cluster")) {
			if (argc <= 2) {
				std::cerr << "not enough arguments for cmd\n";
				return 1;
			}

			Cluster *cluster = new Cluster(argv[2]);
			if (cluster == NULL) {
				std::cerr << "cannot create cluster " << argv[2] << "\n";
				return 1;
			}

			Cluster *parent = prog->getRootCluster();
			if (argc > 3) {
				parent = prog->findCluster(argv[3]);
				if (cluster == NULL) {
					std::cerr << "cannot find cluster " << argv[3] << "\n";
					return 1;
				}
			}

			parent->addChild(cluster);
		} else {
			std::cerr << "don't know how to add a " << argv[1] << "\n";
			return 1;
		}
	} else if (!strcmp(argv[0], "delete")) {
		if (argc <= 1) {
			std::cerr << "not enough arguments for cmd\n";
			return 1;
		}
		if (!strcmp(argv[1], "cluster")) {
			if (argc <= 2) {
				std::cerr << "not enough arguments for cmd\n";
				return 1;
			}

			Cluster *cluster = prog->findCluster(argv[2]);
			if (cluster == NULL) {
				std::cerr << "cannot find cluster " << argv[2] << "\n";
				return 1;
			}

			if (cluster->hasChildren() || cluster == prog->getRootCluster()) {
				std::cerr << "cluster " << argv[2] << " is not empty\n";
				return 1;
			}

			if (prog->clusterUsed(cluster)) {
				std::cerr << "cluster " << argv[2] << " is not empty\n";
				return 1;
			}

			unlink(cluster->getOutPath("xml"));
			unlink(cluster->getOutPath("c"));
			assert(cluster->getParent());
			cluster->getParent()->removeChild(cluster);
		} else {
			std::cerr << "don't know how to delete a " << argv[1] << "\n";
			return 1;
		}
	} else if (!strcmp(argv[0], "rename")) {
		if (argc <= 1) {
			std::cerr << "not enough arguments for cmd\n";
			return 1;
		}
		if (!strcmp(argv[1], "proc")) {
			if (argc <= 3) {
				std::cerr << "not enough arguments for cmd\n";
				return 1;
			}

			Proc *proc = prog->findProc(argv[2]);
			if (proc == NULL) {
				std::cerr << "cannot find proc " << argv[2] << "\n";
				return 1;
			}

			Proc *nproc = prog->findProc(argv[3]);
			if (nproc != NULL) {
				std::cerr << "proc " << argv[3] << " already exists\n";
				return 1;
			}

			proc->setName(argv[3]);
		} else if (!strcmp(argv[1], "cluster")) {
			if (argc <= 3) {
				std::cerr << "not enough arguments for cmd\n";
				return 1;
			}

			Cluster *cluster = prog->findCluster(argv[2]);
			if (cluster == NULL) {
				std::cerr << "cannot find cluster " << argv[2] << "\n";
				return 1;
			}

			Cluster *ncluster = prog->findCluster(argv[3]);
			if (ncluster == NULL) {
				std::cerr << "cluster " << argv[3] << " already exists\n";
				return 1;
			}

			cluster->setName(argv[3]);
		} else {
			std::cerr << "don't know how to rename a " << argv[1] << "\n";
			return 1;
		}
	} else if (!strcmp(argv[0], "info")) {
		if (argc <= 1) {
			std::cerr << "not enough arguments for cmd\n";
			return 1;
		}
		if (!strcmp(argv[1], "prog")) {

			std::cout << "prog " << prog->getName() << ":\n";
			std::cout << "\tclusters:\n";
			prog->getRootCluster()->printTree(std::cout);
			std::cout << "\n\tlibprocs:\n";
			PROGMAP::const_iterator it;
			for (Proc *p = prog->getFirstProc(it); p; p = prog->getNextProc(it))
				if (p->isLib())
					std::cout << "\t\t" << p->getName() << "\n";
			std::cout << "\n\tuserprocs:\n";
			for (Proc *p = prog->getFirstProc(it); p; p = prog->getNextProc(it))
				if (!p->isLib())
					std::cout << "\t\t" << p->getName() << "\n";
			std::cout << "\n";

			return 0;
		} else if (!strcmp(argv[1], "cluster")) {
			if (argc <= 2) {
				std::cerr << "not enough arguments for cmd\n";
				return 1;
			}

			Cluster *cluster = prog->findCluster(argv[2]);
			if (cluster == NULL) {
				std::cerr << "cannot find cluster " << argv[2] << "\n";
				return 1;
			}

			std::cout << "cluster " << cluster->getName() << ":\n";
			if (cluster->getParent())
				std::cout << "\tparent = " << cluster->getParent()->getName() << "\n";
			else
				std::cout << "\troot cluster.\n";
			std::cout << "\tprocs:\n";
			PROGMAP::const_iterator it;
			for (Proc *p = prog->getFirstProc(it); p; p = prog->getNextProc(it))
				if (p->getCluster() == cluster)
					std::cout << "\t\t" << p->getName() << "\n";
			std::cout << "\n";

			return 0;
		} else if (!strcmp(argv[1], "proc")) {
			if (argc <= 2) {
				std::cerr << "not enough arguments for cmd\n";
				return 1;
			}

			Proc *proc = prog->findProc(argv[2]);
			if (proc == NULL) {
				std::cerr << "cannot find proc " << argv[2] << "\n";
				return 1;
			}

			std::cout << "proc " << proc->getName() << ":\n";
			std::cout << "\tbelongs to cluster " << proc->getCluster()->getName() << "\n";
			std::cout << "\tnative address " << std::hex << proc->getNativeAddress() << std::dec << "\n";
			if (proc->isLib())
				std::cout << "\tis a library proc.\n";
			else {
				std::cout << "\tis a user proc.\n";
				UserProc *p = (UserProc *)proc;
				if (p->isDecoded())
					std::cout << "\thas been decoded.\n";
#if 0
				if (p->isAnalysed())
					std::cout << "\thas been analysed.\n";
#endif
			}
			std::cout << "\n";

			return 0;
		} else {
			std::cerr << "don't know how to print info about a " << argv[1] << "\n";
			return 1;
		}
	} else if (!strcmp(argv[0], "print")) {
		if (argc <= 1) {
			std::cerr << "not enough arguments for cmd\n";
			return 1;
		}

		Proc *proc = prog->findProc(argv[1]);
		if (proc == NULL) {
			std::cerr << "cannot find proc " << argv[1] << "\n";
			return 1;
		}
		if (proc->isLib()) {
			std::cerr << "cannot print a libproc.\n";
			return 1;
		}

		((UserProc *)proc)->print(std::cout);
		std::cout << "\n";
		return 0;
	} else if (!strcmp(argv[0], "exit")) {
		return 2;
	} else if (!strcmp(argv[0], "quit")) {
		return 2;
	} else if (!strcmp(argv[0], "help")) {
		helpcmd();
		return 0;
	} else {
		std::cerr << "unknown cmd " << argv[0] << ".\n";
		return 1;
	}

	return 0;
}

/**
 * Displays a command line and processes the commands entered.
 *
 * \retval 0 stdin was closed.
 * \retval 2 The user typed exit or quit.
 */
int
Boomerang::cmdLine()
{
	char line[1024];
	printf("boomerang: ");
	fflush(stdout);
	while (fgets(line, sizeof line, stdin)) {
		const char *argv[100];
		int argc = splitLine(line, argv);
		if (parseCmd(argc, argv) == 2)
			return 2;
		printf("boomerang: ");
		fflush(stdout);
	}
	return 0;
}

/**
 * The main function for the command line mode. Parses switches and runs decompile(filename).
 *
 * \return Zero on success, nonzero on failure.
 */
int
Boomerang::commandLine(int argc, const char *argv[])
{
	printf("%s\n", PACKAGE_STRING);
	if (argc < 2) usage();

	// Parse switches on command line
	if ((argc == 2) && (strcmp(argv[1], "-h") == 0)) {
		help();
		return 1;
	}
	if (argc == 3 && !strcmp(argv[1], "-h") && !strcmp(argv[2], "cmd")) {
		helpcmd();
		return 1;
	}

	int kmd = 0;

	for (int i = 1; i < argc; i++) {
		if (argv[i][0] != '-' && i == argc - 1)
			break;
		if (argv[i][0] != '-')
			usage();
		switch (argv[i][1]) {
		case '-':
			break;  // No effect: ignored
		case 'h':
			help();
			break;
		case 'v':
			vFlag = true;
			break;
		case 'x':
			dumpXML = true;
			break;
		case 'X':
			experimental = true;
			std::cout << "Warning: experimental code active!\n";
			break;
		case 'r':
			printRtl = true;
			break;
		case 't':
			traceDecoder = true;
			break;
		case 'T':
			if (argv[i][2] == 'c') {
				conTypeAnalysis = true;  // -Tc: use old constraint-based type analysis
				dfaTypeAnalysis = false;
			}
			else if (argv[i][2] == 'd')
				dfaTypeAnalysis = true;  // -Td: use data-flow-based type analysis (now default)
			break;
		case 'g':
			if (argv[i][2] == 'd')
				dotFile = argv[++i];
			else if (argv[i][2] == 'c')
				generateCallGraph = true;
			else if (argv[i][2] == 's') {
				generateSymbols = true;
				stopBeforeDecompile = true;
			}
			break;
		case 'o': {
			outputPath = argv[++i];
			if (outputPath[outputPath.length() - 1] != '/')
				outputPath += '/';  // Maintain the convention of a trailing slash
			break;
		}
		case 'p':
			if (argv[i][2] == 'a') {
				propOnlyToAll = true;
				std::cerr << " * * Warning! -pa is not implemented yet!\n";
			}
			else {
				if (++i == argc) {
					usage();
					return 1;
				}
				sscanf(argv[i], "%i", &numToPropagate);
			}
			break;
		case 'n':
			switch (argv[i][2]) {
			case 'b':
				noBranchSimplify = true;
				break;
			case 'c':
				noDecodeChildren = true;
				break;
			case 'd':
				noDataflow = true;
				break;
			case 'D':
				noDecompile = true;
				break;
			case 'l':
				noLocals = true;
				break;
			case 'n':
				noRemoveNull = true;
				break;
			case 'P':
				noPromote = true;
				break;
			case 'p':
				noParameterNames = true;
				break;
			case 'r':
				noRemoveLabels = true;
				break;
			case 'R':
				noRemoveReturns = true;
				break;
			case 'g':
				noGlobals = true;
				break;
			case 'G':
#ifdef GARBAGE_COLLECTOR
				GC_disable();
#endif
				break;
			default:
				help();
			}
			break;
		case 'E':
			noDecodeChildren = true;
			// Fall through
		case 'e':
			{
				ADDRESS addr;
				int n;
				decodeMain = false;
				if (++i == argc) {
					usage();
					return 1;
				}
				if (argv[i][0] == '0' && argv[i + 1][1] == 'x') {
					n = sscanf(argv[i], "0x%x", &addr);
				} else {
					n = sscanf(argv[i], "%i", &addr);
				}
				if (n != 1) {
					std::cerr << "bad address: " << argv[i] << std::endl;
					exit(1);
				}
				entrypoints.push_back(addr);
			}
			break;
		case 's':
			{
				if (argv[i][2] == 'f') {
					symbolFiles.push_back(argv[i + 1]);
					i++;
					break;
				}
				ADDRESS addr;
				int n;
				if (++i == argc) {
					usage();
					return 1;
				}
				if (argv[i][0] == '0' && argv[i + 1][1] == 'x') {
					n = sscanf(argv[i], "0x%x", &addr);
				} else {
					n = sscanf(argv[i], "%i", &addr);
				}
				if (n != 1) {
					std::cerr << "bad address: " << argv[i + 1] << std::endl;
					exit(1);
				}
				const char *nam = argv[++i];
				symbols[addr] = nam;
			}
			break;
		case 'd':
			switch (argv[i][2]) {
			case 'a':
				printAST = true;
				break;
			case 'c':
				debugSwitch = true;
				break;
			case 'd':
				debugDecoder = true;
				break;
			case 'g':
				debugGen = true;
				break;
			case 'l':
				debugLiveness = true;
				break;
			case 'p':
				debugProof = true;
				break;
			case 's':
				stopAtDebugPoints = true;
				break;
			case 't':  // debug type analysis
				debugTA = true;
				break;
			case 'u':  // debug unused locations (including returns and parameters now)
				debugUnused = true;
				break;
			default:
				help();
			}
			break;
		case 'm':
			if (++i == argc) {
				usage();
				return 1;
			}
			sscanf(argv[i], "%i", &maxMemDepth);
			break;
		case 'i':
			if (argv[i][2] == 'c')  // -ic;
				decodeThruIndCall = true;
			if (argv[i][2] == 'w')  // -iw
				if (ofsIndCallReport) {
					std::string fname = getOutputPath() + "indirect.txt";
					ofsIndCallReport = new std::ofstream(fname.c_str());
				}
			break;
		case 'L':
			if (argv[i][2] == 'D')
#ifdef USE_XML
				loadBeforeDecompile = true;
#else
				std::cerr << "LD command not enabled\n";
#endif
			break;
		case 'S':
			if (argv[i][2] == 'D')
#ifdef USE_XML
				saveBeforeDecompile = true;
#else
				std::cerr << "SD command not enabled\n";
#endif
			else {
				sscanf(argv[++i], "%i", &minsToStopAfter);
			}
			break;
		case 'k':
			kmd = 1;
			break;
		case 'P':
			progPath = argv[++i];
			if (progPath[progPath.length() - 1] != '/')
				progPath += "/";
			break;
		case 'a':
			assumeABI = true;
			break;
		case 'l':
			if (++i == argc) {
				usage();
				return 1;
			}
			sscanf(argv[i], "%i", &propMaxDepth);
			break;
		default:
			help();
		}
	}

	setOutputDirectory(outputPath.c_str());

	if (kmd)
		return cmdLine();

	return decompile(argv[argc - 1]);
}

/**
 * Sets the directory in which Boomerang creates its output files.  The directory will be created if it doesn't exist.
 *
 * \param path      the path to the directory
 *
 * \retval true Success.
 * \retval false The directory could not be created.
 */
bool
Boomerang::setOutputDirectory(const char *path)
{
	outputPath = path;
	// Create the output directory, if needed
	if (!createDirectory(outputPath)) {
		std::cerr << "Warning! Could not create path " << outputPath << "!\n";
		return false;
	}
	if (logger == NULL)
		setLogger(new FileLogger());
	return true;
}

/**
 * Adds information about functions and classes from Objective-C modules to the Prog object.
 *
 * \param modules A map from name to the Objective-C modules.
 * \param prog The Prog object to add the information to.
 */
void
Boomerang::objcDecode(std::map<std::string, ObjcModule> &modules, Prog *prog)
{
	if (VERBOSE)
		LOG << "Adding Objective-C information to Prog.\n";
	Cluster *root = prog->getRootCluster();
	for (std::map<std::string, ObjcModule>::iterator it = modules.begin(); it != modules.end(); it++) {
		ObjcModule &mod = (*it).second;
		Module *module = new Module(mod.name.c_str());
		root->addChild(module);
		if (VERBOSE)
			LOG << "\tModule: " << mod.name.c_str() << "\n";
		for (std::map<std::string, ObjcClass>::iterator it1 = mod.classes.begin(); it1 != mod.classes.end(); it1++) {
			ObjcClass &c = (*it1).second;
			Class *cl = new Class(c.name.c_str());
			root->addChild(cl);
			if (VERBOSE)
				LOG << "\t\tClass: " << c.name.c_str() << "\n";
			for (std::map<std::string, ObjcMethod>::iterator it2 = c.methods.begin(); it2 != c.methods.end(); it2++) {
				ObjcMethod &m = (*it2).second;
				// TODO: parse :'s in names
				Proc *p = prog->newProc(m.name.c_str(), m.addr);
				p->setCluster(cl);
				// TODO: decode types in m.types
				if (VERBOSE)
					LOG << "\t\t\tMethod: " << m.name.c_str() << "\n";
			}
		}
	}
	if (VERBOSE)
		LOG << "\n";
}

/**
 * Loads the executable file and decodes it.
 *
 * \param fname The name of the file to load.
 * \param pname How the Prog will be named.
 *
 * \returns A Prog object.
 */
Prog *
Boomerang::loadAndDecode(const char *fname, const char *pname)
{
	std::cout << "loading...\n";
	Prog *prog = new Prog();
	FrontEnd *fe = FrontEnd::open(fname, prog);
	if (fe == NULL) {
		std::cerr << "failed.\n";
		return NULL;
	}

	// Add symbols from -s switch(es)
	for (std::map<ADDRESS, std::string>::iterator it = symbols.begin(); it != symbols.end(); it++) {
		fe->addSymbol((*it).first, (*it).second.c_str());
	}
	fe->readLibraryCatalog();  // Needed before readSymbolFile()

	for (unsigned i = 0; i < symbolFiles.size(); i++) {
		std::cout << "reading symbol file " << symbolFiles[i].c_str() << "\n";
		prog->readSymbolFile(symbolFiles[i].c_str());
	}

	std::map<std::string, ObjcModule> &objcmodules = fe->getBinaryFile()->getObjcModules();
	if (!objcmodules.empty())
		objcDecode(objcmodules, prog);

	// Entry points from -e (and -E) switch(es)
	for (unsigned i = 0; i < entrypoints.size(); i++) {
		std::cout << "decoding specified entrypoint " << std::hex << entrypoints[i] << "\n";
		prog->decodeEntryPoint(entrypoints[i]);
	}

	if (entrypoints.empty()) {  // no -e or -E given
		if (decodeMain)
			std::cout << "decoding entry point...\n";
		fe->decode(prog, decodeMain, pname);

		if (!noDecodeChildren) {
			// this causes any undecoded userprocs to be decoded
			std::cout << "decoding anything undecoded...\n";
			fe->decode(prog, NO_ADDRESS);
		}
	}

	std::cout << "finishing decode...\n";
	prog->finishDecode();

	Boomerang::get()->alert_end_decode();

	std::cout << "found " << std::dec << prog->getNumUserProcs() << " procs\n";

	// GK: The analysis which was performed was not exactly very "analysing", and so it has been moved to
	// prog::finishDecode, UserProc::assignProcsToCalls and UserProc::finalSimplify
	//std::cout << "analysing...\n";
	//prog->analyse();

	if (generateSymbols) {
		prog->printSymbolsToFile();
	}
	if (generateCallGraph) {
		prog->printCallGraph();
		prog->printCallGraphXML();
	}
	return prog;
}

static void
stopProcess(int n)
{
	std::cerr << "\n\n Stopping process, timeout.\n";
	exit(1);
}

/**
 * The program will subsequently be loaded, decoded, decompiled and written to a source file.
 * After decompilation the elapsed time is printed to std::cerr.
 *
 * \param fname The name of the file to load.
 * \param pname The name that will be given to the Proc.
 *
 * \return Zero on success, nonzero on failure.
 */
int
Boomerang::decompile(const char *fname, const char *pname)
{
	Prog *prog = NULL;
	time_t start;
	time(&start);

	if (minsToStopAfter) {
		std::cout << "stopping decompile after " << minsToStopAfter << " minutes.\n";
		signal(SIGALRM, stopProcess);
		alarm(minsToStopAfter * 60);
	}

	//std::cout << "setting up transformers...\n";
	//ExpTransformer::loadAll();

#ifdef USE_XML
	if (loadBeforeDecompile) {
		std::cout << "loading persisted state...\n";
		prog = loadFromXML(fname);
	} else
#endif
	{
		prog = loadAndDecode(fname, pname);
		if (prog == NULL)
			return 1;
	}

#ifdef USE_XML
	if (saveBeforeDecompile) {
		std::cout << "saving persistable state...\n";
		persistToXML(prog);
	}
#endif

	if (stopBeforeDecompile)
		return 0;

	std::cout << "decompiling...\n";
	prog->decompile();

	if (dotFile) {
		std::ofstream of(dotFile);
		prog->generateDot(of);
		of.close();
	}

	if (printAST) {
		std::cout << "printing AST...\n";
		PROGMAP::const_iterator it;
		for (Proc *p = prog->getFirstProc(it); p; p = prog->getNextProc(it))
			if (!p->isLib()) {
				UserProc *u = (UserProc *)p;
				u->getCFG()->compressCfg();
				u->printAST();
			}
	}

	std::cout << "generating code...\n";
	prog->generateCode();

	std::cout << "output written to " << outputPath << prog->getRootCluster()->getName() << "\n";

	delete prog;
	if (Boomerang::get()->ofsIndCallReport)
		ofsIndCallReport->close();

	time_t end;
	time(&end);
	int hours = (int)((end - start) / 60 / 60);
	int mins = (int)((end - start) / 60 - hours * 60);
	int secs = (int)((end - start) - hours * 60 * 60 - mins * 60);
	std::cout << "completed in " << std::dec;
	if (hours)
		std::cout << hours << " hours ";
	if (hours || mins)
		std::cout << mins << " mins ";
	std::cout << secs << " sec" << (secs == 1 ? "" : "s") << ".\n";

	return 0;
}

#ifdef USE_XML
/**
 * Saves the state of the Prog object to a XML file.
 * \param prog The Prog object to save.
 */
void
Boomerang::persistToXML(Prog *prog)
{
	LOG << "saving persistable state...\n";
	XMLProgParser::persistToXML(prog);
}
/**
 * Loads the state of a Prog object from a XML file.
 * \param fname The name of the XML file.
 * \return The loaded Prog object.
 */
Prog *
Boomerang::loadFromXML(const char *fname)
{
	LOG << "loading persisted state...\n";
	XMLProgParser p;
	return p.parse(fname);
}
#endif

void
Boomerang::alert_decompile_debug_point(UserProc *p, const char *description)
{
	if (stopAtDebugPoints) {
		std::cout << "decompiling " << p->getName() << ": " << description << "\n";
		static char *stopAt = NULL;
		static std::set<Statement *> watches;
		if (stopAt == NULL || !strcmp(p->getName(), stopAt)) {
			// This is a mini command line debugger.  Feel free to expand it.
			for (std::set<Statement *>::iterator it = watches.begin(); it != watches.end(); it++) {
				(*it)->print(std::cout);
				std::cout << "\n";
			}
			std::cout << " <press enter to continue> \n";
			char line[1024];
			while (1) {
				*line = 0;
				fgets(line, 1024, stdin);
				if (!strncmp(line, "print", 5))
					p->print(std::cout);
				else if (!strncmp(line, "fprint", 6)) {
					std::ofstream of("out.proc");
					p->print(of);
					of.close();
				} else if (!strncmp(line, "run ", 4)) {
					stopAt = strdup(line + 4);
					if (strchr(stopAt, '\n'))
						*strchr(stopAt, '\n') = 0;
					if (strchr(stopAt, ' '))
						*strchr(stopAt, ' ') = 0;
					break;
				} else if (!strncmp(line, "watch ", 6)) {
					int n = atoi(line + 6);
					StatementList stmts;
					p->getStatements(stmts);
					StatementList::iterator it;
					for (it = stmts.begin(); it != stmts.end(); it++)
						if ((*it)->getNumber() == n) {
							watches.insert(*it);
							std::cout << "watching " << *it << "\n";
						}
				} else
					break;
			}
		}
	}
	for (std::set<Watcher *>::iterator it = watchers.begin(); it != watchers.end(); it++)
		(*it)->alert_decompile_debug_point(p, description);
}

const char *
Boomerang::getVersionStr()
{
	return VERSION;
}
