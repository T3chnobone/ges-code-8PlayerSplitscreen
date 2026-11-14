// The prebuilt lib/common/curl.lib was compiled against the pre-VS2015 CRT, which exported
// __iob_func (behind stdin/stdout/stderr). The Universal CRT removed it; this shim provides it.
#include <stdio.h>

extern "C" FILE * __cdecl __iob_func( void )
{
	static FILE s_iob[3];
	s_iob[0] = *stdin;
	s_iob[1] = *stdout;
	s_iob[2] = *stderr;
	return s_iob;
}
