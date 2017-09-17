/**
 * \file
 * \brief Definition of the classes that describe a Cluster.
 *
 * A cluster is a grouping of functions irrespective of relationship.  For
 * example, the Object Oriented Programming concept of a Class is a Cluster.
 * Clusters can contain other Clusters to form a tree.
 *
 * \authors
 * Copyright (C) 2004, Trent Waddington
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef CLUSTER_H
#define CLUSTER_H

#include "type.h"

#include <fstream>
#include <ostream>
#include <string>
#include <vector>

class Cluster {
protected:
	        std::string name;
	        std::vector<Cluster *> children;
	        Cluster    *parent = nullptr;
	        std::ofstream out;

public:
	                    Cluster() { }
	                    Cluster(const char *name) : name(name) { }
	virtual            ~Cluster() { }
	        const char *getName() const { return name.c_str(); }
	        void        setName(const char *nam) { name = nam; }
	        unsigned int getNumChildren() const { return children.size(); }
	        Cluster    *getChild(int n) const { return children[n]; }
	        void        addChild(Cluster *n);
	        void        removeChild(Cluster *n);
	        Cluster    *getParent() const { return parent; }
	        bool        hasChildren() const { return !children.empty(); }
	        void        openStream(const char *ext);
	        void        openStreams(const char *ext);
	        void        closeStreams();
	        std::ofstream &getStream() { return out; }
	        std::string makeDirs();
	        std::string getOutPath(const char *ext);
	        Cluster    *find(const char *nam);
	virtual bool        isAggregate() const { return false; }

	        void        printTree(std::ostream &out) const;

protected:
	friend class XMLProgParser;
};

class Module : public Cluster {
public:
	Module(const char *name) : Cluster(name) { }
};

class Class : public Cluster {
protected:
	CompoundType *type;

public:
	Class(const char *name) : Cluster(name) { type = new CompoundType(); }

	// A Class tends to be aggregated into the parent Module,
	// this isn't the case with Java, but hey, we're not doing that yet.
	bool isAggregate() const override { return true; }
};

#endif
