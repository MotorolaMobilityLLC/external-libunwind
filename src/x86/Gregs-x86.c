/* libunwind - a platform-independent unwind library
   Copyright (C) 2002 Hewlett-Packard Co
	Contributed by David Mosberger-Tang <davidm@hpl.hp.com>

This file is part of libunwind.

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.  */

#include "unwind_i.h"

HIDDEN int
x86_access_reg (struct cursor *c, unw_regnum_t reg, unw_word_t *valp,
		int write)
{
  struct x86_loc loc = X86_LOC (0, 0);

  switch (reg)
    {
    case UNW_X86_EIP:
      if (write)
	c->eip = *valp;		/* also update the EIP cache */
      loc = c->eip_loc;
      break;

    case UNW_X86_ESP:
      if (write)
	return -UNW_EREADONLYREG;
      *valp = c->esp;
      return 0;

    default:
      debug (1, "%s: bad register number %u\n", __FUNCTION__, reg);
      return -UNW_EBADREG;
    }

  if (write)
    return x86_put (c, loc, *valp);
  else
    return x86_get (c, loc, valp);
}
