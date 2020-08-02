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

	// access and set functions
	void s_name(const std::string &s) { name = s; }
	void s_size(int s) { size = s; }
	void s_float(bool f) { flt = f; }
	void s_mappedIndex(int i) { mappedIndex = i; }
	void s_mappedOffset(int i) { mappedOffset = i; }

	/* These are only used in the interpreter */
	const std::string &g_name() const { return name; }
	int g_size() const { return size; }
	bool g_float() const { return flt; }
	int g_mappedIndex() const { return mappedIndex; }
	int g_mappedOffset() const { return mappedOffset; }

	Type *g_type() const;

private:
	std::string name;

	/**
	 * Register size, in bits.
	 */
	short size;

	/**
	 * True if this is a floating point register.
	 */
	bool flt = false;

	/**
	 * For COVERS registers, this is the lower register of the set that
	 * this register covers.  For example, if the current register is
	 * f28to31, it would be the index for register f28.
	 *
	 * For SHARES registers, this is the "parent" register, e.g. if the
	 * current register is \%al, the parent is \%ax (note: not \%eax).
	 */
	int mappedIndex = -1;

	/**
	 * This is the bit number where this register starts, e.g. for
	 * register \%ah, this is 8.  For COVERS registers, this is 0.
	 */
	int mappedOffset = -1;
};

#endif
