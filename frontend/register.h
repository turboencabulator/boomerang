/**
 * \file
 * \brief Header information for the Register class.
 *
 * \authors
 * Copyright (C) 2000-2001, The University of Queensland
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef REGISTER_H
#define REGISTER_H

#include <string>

class Type;

/**
 * The Register class summarises one line of the @REGISTERS section of an SSL
 * file.  This class is used extensively in sslparser.ypp, and there is a
 * public member of RTLInstDict called DetRegMap which gives a Register object
 * from a register index (register indices may not always be sequential, hence
 * it's not just an array of Register objects).
 *
 * This class plays a more active role in the Interpreter, which is not yet
 * integrated into uqbt.
 */
class Register {
public:

	bool operator ==(const Register &r2) const;
	bool operator <(const Register &r2) const;

	// access and set functins
	void s_name(const std::string &s) { name = s; }
	void s_size(int s) { size = s; }
	void s_float(bool f) { flt = f; }

	/* These are only used in the interpreter */
	const std::string &g_name() const { return name; }
	int g_size() const { return size; }
	bool g_float() const { return flt; }

	Type *g_type() const;

	/* Set the mapped index. For COVERS registers, this is the lower register
	 * of the set that this register covers. For example, if the current register
	 * is f28to31, i would be the index for register f28
	 * For SHARES registers, this is the "parent" register, e.g. if the current
	 * register is %al, the parent is %ax (note: not %eax)
	 */
	void s_mappedIndex(int i) { mappedIndex = i; }
	/* Set the mapped offset. This is the bit number where this register starts,
	   e.g. for register %ah, this is 8. For COVERS regisers, this is 0 */
	void s_mappedOffset(int i) { mappedOffset = i; }
	/* Get the mapped index (see above) */
	int g_mappedIndex() const { return mappedIndex; }
	/* Get the mapped offset (see above) */
	int g_mappedOffset() const { return mappedOffset; }

private:
	std::string name;
	short size;
	bool flt = false;  // True if this is a floating point register
	int mappedIndex = -1;
	int mappedOffset = -1;
};

#endif