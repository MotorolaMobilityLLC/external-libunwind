/* libunwind - a platform-independent unwind library
   Copyright (C) 2001-2003 Hewlett-Packard Co
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

#include <stdlib.h>

#include "rse.h"
#include "unwind_i.h"
#include "offsets.h"

#ifndef UNW_REMOTE_ONLY

static inline int
local_resume (unw_addr_space_t as, unw_cursor_t *cursor, void *arg)
{
#ifndef __hpux
  unw_word_t val, sol, sof, pri_unat, n, pfs;
  struct cursor *c = (struct cursor *) cursor;
  struct
    {
      unw_word_t r1;
      unw_word_t r4;
      unw_word_t r5;
      unw_word_t r6;
      unw_word_t r7;
      unw_word_t r15;
      unw_word_t r16;
      unw_word_t r17;
      unw_word_t r18;
    }
  extra;
  int ret;
# define GET_NAT(n)						\
  do								\
    {								\
      ret = ia64_access_reg (c, UNW_IA64_NAT + (n), &val, 0);	\
      if (ret < 0)						\
	return ret;						\
      if (val)							\
	pri_unat |= (unw_word_t) 1 << n;			\
    }								\
  while (0)

  /* ensure c->pi is up-to-date: */
  if ((ret = ia64_make_proc_info (c)) < 0)
    return ret;

  /* Copy contents of r4-r7 into "extra", so that their values end up
     contiguous, so we can use a single (primary-) UNaT value.  */
  if ((ret = ia64_get (c, c->r4_loc, &extra.r4)) < 0
      || (ret = ia64_get (c, c->r5_loc, &extra.r5)) < 0
      || (ret = ia64_get (c, c->r6_loc, &extra.r6)) < 0
      || (ret = ia64_get (c, c->r7_loc, &extra.r7)) < 0)
    return ret;

  /* Form the primary UNaT value: */
  pri_unat = 0;
  GET_NAT (4); GET_NAT(5);
  GET_NAT (6); GET_NAT(7);
  n = (((uintptr_t) &extra.r4) / 8 - 4) % 64;
  pri_unat = (pri_unat << n) | (pri_unat >> (64 - n));

  if (unlikely (c->sigcontext_loc))
    {
      struct sigcontext *sc = (struct sigcontext *) c->sigcontext_loc;
#     define PR_SCRATCH		0xffc0	/* p6-p15 are scratch */
#     define PR_PRESERVED	(~(PR_SCRATCH | 1))

      /* We're returning to a frame that was (either directly or
	 indirectly) interrupted by a signal.  We have to restore
	 _both_ "preserved" and "scratch" registers.  That doesn't
	 leave us any registers to work with, and the only way we can
	 achieve this is by doing a sigreturn().

	 Note: it might be tempting to think that we don't have to
	 restore the scratch registers when returning to a frame that
	 was indirectly interrupted by a signal.  However, that is not
	 safe because that frame and its descendants could have been
	 using a special convention that stores "preserved" state in
	 scratch registers.  For example, the Linux fsyscall
	 convention does this with r11 (to save ar.pfs) and b6 (to
	 save "rp"). */

      sc->sc_gr[12] = c->psp;
      c->psp = (c->sigcontext_loc - c->sigcontext_off);

      sof = (c->cfm & 0x7f);
      rbs_cover_and_flush (c, sof);

      sc->sc_ip = c->ip;
      sc->sc_cfm = c->cfm & (((unw_word_t) 1 << 38) - 1);
      sc->sc_pr = (c->pr & ~PR_SCRATCH) | (sc->sc_pr & ~PR_PRESERVED);
      if ((ret = ia64_get (c, c->pfs_loc, &sc->sc_ar_pfs)) < 0
	  || (ret = ia64_get (c, c->fpsr_loc, &sc->sc_ar_fpsr)) < 0
	  || (ret = ia64_get (c, c->unat_loc, &sc->sc_ar_unat)) < 0)
	return ret;

      sc->sc_gr[1] = c->pi.gp;
      if (c->eh_valid_mask & 0x1) sc->sc_gr[15] = c->eh_args[0];
      if (c->eh_valid_mask & 0x2) sc->sc_gr[16] = c->eh_args[1];
      if (c->eh_valid_mask & 0x4) sc->sc_gr[17] = c->eh_args[2];
      if (c->eh_valid_mask & 0x8) sc->sc_gr[18] = c->eh_args[3];
    }
  else
    {
      /* Account for the fact that _Uia64_install_context() will
	 return via br.ret, which will decrement bsp by size-of-locals.  */
      if ((ret = ia64_get (c, c->pfs_loc, &pfs)) < 0)
	return ret;
      sol = (pfs >> 7) & 0x7f;
      rbs_cover_and_flush (c, sol);

      extra.r1 = c->pi.gp;
      extra.r15 = c->eh_args[0];
      extra.r16 = c->eh_args[1];
      extra.r17 = c->eh_args[2];
      extra.r18 = c->eh_args[3];
    }
  _Uia64_install_context (c, pri_unat, (unw_word_t *) &extra);
#else
  return -UNW_EINVAL;
#endif
}

HIDDEN int
ia64_local_resume (unw_addr_space_t as, unw_cursor_t *cursor, void *arg)
{
  return local_resume (as, cursor, arg);
}

#endif /* !UNW_REMOTE_ONLY */

#ifndef UNW_LOCAL_ONLY

static inline int
remote_install_cursor (struct cursor *c)
{
#ifndef __hpux
  int (*access_reg) (unw_addr_space_t, unw_regnum_t, unw_word_t *,
		     int write, void *);
  int (*access_fpreg) (unw_addr_space_t, unw_regnum_t, unw_fpreg_t *,
		       int write, void *);
  unw_fpreg_t fpval;
  unw_word_t val;
  int reg;

  if (c->as == unw_local_addr_space)
    {
      /* Take a short-cut: we directly resume out of the cursor and
	 all we need to do is make sure that all locations point to
	 memory, not registers.  Furthermore, R4-R7 and NAT4-NAT7 are
	 taken care of by ia64_local_resume() so they don't need to be
	 handled here.  */
      unw_word_t *locp;
#     define MEMIFY(preg, reg)					\
      do {							\
	locp = ((unw_word_t *) c) + unw.preg_index[(preg)];	\
	if (IA64_IS_REG_LOC (*locp))				\
	  *locp = (unw_word_t) tdep_uc_addr (c->as_arg, (reg));	\
      } while (0)
      MEMIFY (IA64_REG_PR,	UNW_IA64_PR);
      MEMIFY (IA64_REG_PFS,	UNW_IA64_AR_PFS);
      MEMIFY (IA64_REG_RNAT,	UNW_IA64_AR_RNAT);
      MEMIFY (IA64_REG_UNAT,	UNW_IA64_AR_UNAT);
      MEMIFY (IA64_REG_LC,	UNW_IA64_AR_LC);
      MEMIFY (IA64_REG_FPSR,	UNW_IA64_AR_FPSR);
      MEMIFY (IA64_REG_RP,	UNW_IA64_BR + 0);
      MEMIFY (IA64_REG_B1,	UNW_IA64_BR + 1);
      MEMIFY (IA64_REG_B2,	UNW_IA64_BR + 2);
      MEMIFY (IA64_REG_B3,	UNW_IA64_BR + 3);
      MEMIFY (IA64_REG_B4,	UNW_IA64_BR + 4);
      MEMIFY (IA64_REG_B5,	UNW_IA64_BR + 5);
      MEMIFY (IA64_REG_F2,	UNW_IA64_FR + 2);
      MEMIFY (IA64_REG_F3,	UNW_IA64_FR + 3);
      MEMIFY (IA64_REG_F4,	UNW_IA64_FR + 4);
      MEMIFY (IA64_REG_F5,	UNW_IA64_FR + 5);
      MEMIFY (IA64_REG_F16,	UNW_IA64_FR + 16);
      MEMIFY (IA64_REG_F17,	UNW_IA64_FR + 17);
      MEMIFY (IA64_REG_F18,	UNW_IA64_FR + 18);
      MEMIFY (IA64_REG_F19,	UNW_IA64_FR + 19);
      MEMIFY (IA64_REG_F20,	UNW_IA64_FR + 20);
      MEMIFY (IA64_REG_F21,	UNW_IA64_FR + 21);
      MEMIFY (IA64_REG_F22,	UNW_IA64_FR + 22);
      MEMIFY (IA64_REG_F23,	UNW_IA64_FR + 23);
      MEMIFY (IA64_REG_F24,	UNW_IA64_FR + 24);
      MEMIFY (IA64_REG_F25,	UNW_IA64_FR + 25);
      MEMIFY (IA64_REG_F26,	UNW_IA64_FR + 26);
      MEMIFY (IA64_REG_F27,	UNW_IA64_FR + 27);
      MEMIFY (IA64_REG_F28,	UNW_IA64_FR + 28);
      MEMIFY (IA64_REG_F29,	UNW_IA64_FR + 29);
      MEMIFY (IA64_REG_F30,	UNW_IA64_FR + 30);
      MEMIFY (IA64_REG_F31,	UNW_IA64_FR + 31);
    }
  else
    {
      access_reg = c->as->acc.access_reg;
      access_fpreg = c->as->acc.access_fpreg;

      for (reg = 0; reg < UNW_REG_LAST; ++reg)
	{
	  if (unw_is_fpreg (reg))
	    {
	      if (ia64_access_fpreg (c, reg, &fpval, 0) > 0)
		(*access_fpreg) (c->as, reg, &fpval, 1, c->as_arg);
	    }
	  else
	    {
	      if (ia64_access_reg (c, reg, &val, 0) > 0)
		(*access_reg) (c->as, reg, &val, 1, c->as_arg);
	    }
	}
    }
  return (*c->as->acc.resume) (c->as, (unw_cursor_t *) c, c->as_arg);
#endif
}

#endif

int
unw_resume (unw_cursor_t *cursor)
{
  struct cursor *c = (struct cursor *) cursor;

#ifdef UNW_LOCAL_ONLY
  return local_resume (c->as, cursor, c->as_arg);
#else
  {
    int ret;

    if ((ret = remote_install_cursor (c)) < 0)
      return ret;
    return (*c->as->acc.resume) (c->as, cursor, c->as_arg);
  }
#endif
}
