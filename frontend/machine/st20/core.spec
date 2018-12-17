#
# Copyright (C) 2005, Mike Van Emmerik
#
# See the file "LICENSE.TERMS" for information on usage and
# redistribution of this file, and for a DISCLAIMER OF ALL
# WARRANTIES.
#

fields of opcodet (8) fc 4:7 bot 0:3


patterns
	[ j       ldlp    pfix    ldnl    ldc     ldnlp   nfix    ldl
	  adc     call    cj      ajw     eqc     stl     stnl    opr ]
	is fc = { 0 to 15 }

	primary is ldlp | ldnl | ldc | ldnlp | ldl | adc | ajw | eqc | stl | stnl


constructors
	primary oper is primary & bot = oper
	j       oper is j       & bot = oper
	pfix    oper is pfix    & bot = oper
	nfix    oper is nfix    & bot = oper
	call    oper is call    & bot = oper
	cj      oper is cj      & bot = oper
	opr     oper is opr     & bot = oper
