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
	friend class XMLProgParser;

protected:
	        std::string name;
	        std::vector<Cluster *> children;
	        Cluster    *parent = nullptr;
	        std::ofstream out;

public:
	typedef std::vector<Cluster *>::const_iterator const_iterator;

	                    Cluster() = default;
	                    Cluster(const std::string &name) : name(name) { }
	virtual            ~Cluster() = default;

	        const std::string &getName() const { return name; }
	        void        setName(const std::string &nam) { name = nam; }
	        Cluster    *find(const std::string &nam);

	        void        addChild(Cluster *n);
	        void        removeChild(Cluster *n);
	        bool        hasChildren() const { return !children.empty(); }
		const_iterator begin() const { return children.begin(); }
		const_iterator end()   const { return children.end(); }

	        Cluster    *getParent() const { return parent; }

	        void        openStream(const std::string &ext);
	        void        openStreams(const std::string &ext);
	        void        closeStreams();
	        std::ofstream &getStream() { return out; }
	        std::string makeDirs() const;
	        std::string getOutPath(const std::string &ext) const;

	virtual bool        isAggregate() const { return false; }

	        void        printTree(std::ostream &out) const;
};

class Module : public Cluster {
public:
	Module(const std::string &name) : Cluster(name) { }
};

class Class : public Cluster {
protected:
	CompoundType *type;

public:
	Class(const std::string &name) : Cluster(name), type(new CompoundType()) { }

	// A Class tends to be aggregated into the parent Module,
	// this isn't the case with Java, but hey, we're not doing that yet.
	bool isAggregate() const override { return true; }
};

#endif
