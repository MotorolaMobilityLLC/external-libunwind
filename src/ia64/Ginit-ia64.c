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

#ifdef HAVE_SYS_UC_ACCESS_H
# include <sys/uc_access.h>
#endif

#ifdef UNW_DEBUG

HIDDEN const char *
ia64_strloc (ia64_loc_t loc)
{
  static char buf[128];

  if (IA64_IS_NULL_LOC (loc))
    return "<null>";

  buf[0] = '\0';

  if (IA64_IS_MEMSTK_NAT (loc))
    strcat (buf, "memstk_nat(");
  if (IA64_IS_UC_LOC (loc))
    strcat (buf, "uc(");
  if (IA64_IS_FP_LOC (loc))
    strcat (buf, "fp(");

  if (IA64_IS_REG_LOC (loc))
    sprintf (buf + strlen (buf), "%s", unw_regname (IA64_GET_REG (loc)));
  else
    sprintf (buf + strlen (buf), "0x%llx",
	     (unsigned long long) IA64_GET_ADDR (loc));

  if (IA64_IS_FP_LOC (loc))
    strcat (buf, ")");
  if (IA64_IS_UC_LOC (loc))
    strcat (buf, ")");
  if (IA64_IS_MEMSTK_NAT (loc))
    strcat (buf, ")");

  return buf;
}

#endif /* UNW_DEBUG */

#ifdef UNW_REMOTE_ONLY

/* unw_local_addr_space is a NULL pointer in this case.  */
unw_addr_space_t unw_local_addr_space;

#else /* !UNW_REMOTE_ONLY */

static struct unw_addr_space local_addr_space;

unw_addr_space_t unw_local_addr_space = &local_addr_space;

#ifdef HAVE_SYS_UC_ACCESS_H

#else /* !HAVE_SYS_UC_ACCESS_H */

static inline void *
uc_addr (ucontext_t *uc, int reg)
{
  void *addr;

  switch (reg)
    {
    case UNW_IA64_GR + 0:	addr = &unw.r0; break;
    case UNW_IA64_NAT + 0:	addr = &unw.r0; break;
    case UNW_IA64_IP:		addr = &uc->uc_mcontext.sc_br[0]; break;
    case UNW_IA64_CFM:		addr = &uc->uc_mcontext.sc_ar_pfs; break;
    case UNW_IA64_AR_RNAT:	addr = &uc->uc_mcontext.sc_ar_rnat; break;
    case UNW_IA64_AR_UNAT:	addr = &uc->uc_mcontext.sc_ar_unat; break;
    case UNW_IA64_AR_LC:	addr = &uc->uc_mcontext.sc_ar_lc; break;
    case UNW_IA64_AR_FPSR:	addr = &uc->uc_mcontext.sc_ar_fpsr; break;
    case UNW_IA64_PR:		addr = &uc->uc_mcontext.sc_pr; break;
      /* This may look confusing, but it's correct: AR_BSPSTORE needs
         to return the address past the last word written, which is
         stored in sc_ar_bsp.  On the other hand, AR_BSP needs to
         return the address that was in ar.bsp at the time the context
         was captured.  As described in unw_init_local(), sc_ar_bsp is
         (ab-)used for this purpose.  */
    case UNW_IA64_AR_BSP:	addr = &uc->uc_mcontext.sc_rbs_base; break;
    case UNW_IA64_AR_BSPSTORE:	addr = &uc->uc_mcontext.sc_ar_bsp; break;

    case UNW_IA64_GR + 4 ... UNW_IA64_GR + 7:
    case UNW_IA64_GR + 12:
      addr = &uc->uc_mcontext.sc_gr[reg - UNW_IA64_GR];
      break;

    case UNW_IA64_BR + 1 ... UNW_IA64_BR + 5:
      addr = &uc->uc_mcontext.sc_br[reg - UNW_IA64_BR];
      break;

    case UNW_IA64_FR+ 2 ... UNW_IA64_FR+ 5:
    case UNW_IA64_FR+16 ... UNW_IA64_FR+31:
      addr = &uc->uc_mcontext.sc_fr[reg - UNW_IA64_FR];
      break;

    default:
      addr = NULL;
    }
  return addr;
}

# ifdef UNW_LOCAL_ONLY

void *
tdep_uc_addr (ucontext_t *uc, int reg)
{
  return uc_addr (uc, reg);
}

# endif /* UNW_LOCAL_ONLY */
#endif /* !HAVE_SYS_UC_ACCESS_H */

static void
put_unwind_info (unw_addr_space_t as, unw_proc_info_t *proc_info, void *arg)
{
  /* it's a no-op */
}

static int
get_dyn_info_list_addr (unw_addr_space_t as, unw_word_t *dyn_info_list_addr,
			void *arg)
{
  *dyn_info_list_addr = (unw_word_t) &_U_dyn_info_list;
  return 0;
}

static int
access_mem (unw_addr_space_t as, unw_word_t addr, unw_word_t *val, int write,
	    void *arg)
{
  if (write)
    {
      debug (100, "%s: mem[%lx] <- %lx\n", __FUNCTION__, addr, *val);
      *(unw_word_t *) addr = *val;
    }
  else
    {
      *val = *(unw_word_t *) addr;
      debug (100, "%s: mem[%lx] -> %lx\n", __FUNCTION__, addr, *val);
    }
  return 0;
}

#ifdef HAVE_SYS_UC_ACCESS_H

#define SYSCALL_CFM_SAVE_REG	11 /* on a syscall, ar.pfs is saved in r11 */
#define REASON_SYSCALL		0

static int
access_reg (unw_addr_space_t as, unw_regnum_t reg, unw_word_t *val, int write,
	    void *arg)
{
  ucontext_t *uc = arg;
  unsigned int nat, mask;
  unw_word_t cfm, sol; 
  uint64_t value;
  uint16_t reason;
  int ret;

  __uc_get_reason (uc, &reason);

  switch (reg)
    {
    case UNW_IA64_GR  ... UNW_IA64_GR + 31:
      if ((ret = __uc_get_grs (uc, (reg - UNW_IA64_GR), 1, &value, &nat)))
	break;

      if (write)
	ret = __uc_set_grs (uc, (reg - UNW_IA64_GR), 1, val, nat);
      else
	*val = value;
      break;

    case UNW_IA64_NAT ... UNW_IA64_NAT + 31:
      if ((ret = __uc_get_grs (uc, (reg - UNW_IA64_GR), 1, &value, &nat)))
	break;

      mask = 1 << (reg - UNW_IA64_GR);

      if (write)
	{
	  if (*val)
	    nat |= mask;
	  else
	    nat &= ~mask;
	  ret = __uc_set_grs (uc, (reg - UNW_IA64_GR), 1, &value, nat);
	}
      else
	*val = (nat & mask) != 0;
      break;

    case UNW_IA64_AR  ... UNW_IA64_AR + 127:
      if (reg == UNW_IA64_AR_BSP)
	{
	  if (reason == REASON_SYSCALL)
	    ret = __uc_get_grs (uc, SYSCALL_CFM_SAVE_REG, 1, &cfm, &nat);
	  else
	    ret = __uc_get_cfm (uc, &cfm);
	  if (ret)
	    break;
	  sol = (cfm >> 7) & 0x7f;

	  if (write)
	    ret = __uc_set_ar (uc, (reg - UNW_IA64_AR),
			       ia64_rse_skip_regs (*val, sol));
	  else
	    {
	      ret = __uc_get_ar (uc, (reg - UNW_IA64_AR), val);
	      *val = ia64_rse_skip_regs (*val, -sol);
	    }
	}
      else
	{
	  if (write)
	    ret = __uc_set_ar (uc, (reg - UNW_IA64_AR), *val);
	  else
	    ret = __uc_get_ar (uc, (reg - UNW_IA64_AR), val);
	}
      break;

    case UNW_IA64_BR  ... UNW_IA64_BR + 7:
      if (write)
	ret = __uc_set_brs (uc, (reg - UNW_IA64_BR), 1, val);
      else
	ret = __uc_get_brs (uc, (reg - UNW_IA64_BR), 1, val);
      break;

    case UNW_IA64_PR:
      if (write)
	ret = __uc_set_prs (uc, *val);
      else
	ret = __uc_get_prs (uc, val);
      break;

    case UNW_IA64_IP:
      if (reason == REASON_SYSCALL)
	{
	  if (write)
	    ret = __uc_set_brs (uc, 0, 1, val);
	  else
	    ret = __uc_get_brs (uc, 0, 1, val);
	}
      else
	{
	  if (write)
	    ret = __uc_set_ip (uc, *val);
	  else
	    ret = __uc_get_ip (uc, val);
	}
      break;

    case UNW_IA64_CFM:
      if (reason == REASON_SYSCALL)
	{
	  ret = __uc_get_grs (uc, SYSCALL_CFM_SAVE_REG, 1, &cfm, &nat);
	  if (write)
	    {
	      if (ret)
		break;
	      ret = __uc_set_grs (uc, SYSCALL_CFM_SAVE_REG, 1, &cfm, nat);
	    }
	}
      else
	{
	  if (write)
	    ret = __uc_set_cfm (uc, *val);
	  else
	    ret = __uc_get_cfm (uc, val);
	}
      break;

    case UNW_IA64_FR  ... UNW_IA64_FR + 127:
    default:
      ret = EINVAL;
      break;
    }

  if (ret != 0)
    return -UNW_EBADREG;

  if (write)
    debug (100, "%s: %s <- %lx\n", __FUNCTION__, unw_regname (reg), *val);
  else
    debug (100, "%s: %s -> %lx\n", __FUNCTION__, unw_regname (reg), *val);
  return 0;
}

static int
access_fpreg (unw_addr_space_t as, unw_regnum_t reg, unw_fpreg_t *val,
	      int write, void *arg)
{
  ucontext_t *uc = arg;
  fp_regval_t fp_regval;
  int ret;

  switch (reg)
    {
    case UNW_IA64_FR  ... UNW_IA64_FR + 127:
      if (write)
	{
	  memcpy (&fp_regval, val, sizeof (fp_regval));
	  ret = __uc_set_frs (uc, (reg - UNW_IA64_FR), 1, &fp_regval);
	}
      else
	{
	  ret = __uc_get_frs (uc, (reg - UNW_IA64_FR), 1, &fp_regval);
	  memcpy (val, &fp_regval, sizeof (*val));
	}
      break;

    default:
      ret = EINVAL;
      break;
    }
  if (ret != 0)
    return -UNW_EBADREG;

  return 0;
}

#else /* !HAVE_SYS_UC_ACCESS_H */

static int
access_reg (unw_addr_space_t as, unw_regnum_t reg, unw_word_t *val, int write,
	    void *arg)
{
  unw_word_t *addr, mask;
  ucontext_t *uc = arg;

  if (reg >= UNW_IA64_FR && reg < UNW_IA64_FR + 128)
    goto badreg;

  if (reg >= UNW_IA64_NAT + 4 && reg <= UNW_IA64_NAT + 7)
    {
      mask = ((unw_word_t) 1) << (reg - UNW_IA64_NAT);
      if (write)
	{
	  if (*val)
	    uc->uc_mcontext.sc_nat |= mask;
	  else
	    uc->uc_mcontext.sc_nat &= ~mask;
	}
      else
	*val = (uc->uc_mcontext.sc_nat & mask) != 0;

      if (write)
	debug (100, "%s: %s <- %lx\n", __FUNCTION__, unw_regname (reg), *val);
      else
	debug (100, "%s: %s -> %lx\n", __FUNCTION__, unw_regname (reg), *val);
      return 0;
    }

  addr = uc_addr (uc, reg);
  if (!addr)
    goto badreg;

  if (write)
    {
      *(unw_word_t *) addr = *val;
      debug (100, "%s: %s <- %lx\n", __FUNCTION__, unw_regname (reg), *val);
    }
  else
    {
      *val = *(unw_word_t *) addr;
      debug (100, "%s: %s -> %lx\n", __FUNCTION__, unw_regname (reg), *val);
    }
  return 0;

 badreg:
  debug (1, "%s: bad register number %u\n", __FUNCTION__, reg);
  return -UNW_EBADREG;
}

static int
access_fpreg (unw_addr_space_t as, unw_regnum_t reg, unw_fpreg_t *val,
	      int write, void *arg)
{
  ucontext_t *uc = arg;
  unw_fpreg_t *addr;

  if (reg < UNW_IA64_FR || reg >= UNW_IA64_FR + 128)
    goto badreg;

  addr = uc_addr (uc, reg);
  if (!addr)
    goto badreg;

  if (write)
    {
      debug (100, "%s: %s <- %016lx.%016lx\n", __FUNCTION__,
	     unw_regname (reg), val->raw.bits[1], val->raw.bits[0]);
      *(unw_fpreg_t *) addr = *val;
    }
  else
    {
      *val = *(unw_fpreg_t *) addr;
      debug (100, "%s: %s -> %016lx.%016lx\n", __FUNCTION__,
	     unw_regname (reg), val->raw.bits[1], val->raw.bits[0]);
    }
  return 0;

 badreg:
  debug (1, "%s: bad register number %u\n", __FUNCTION__, reg);
  /* attempt to access a non-preserved register */
  return -UNW_EBADREG;
}

#endif /* !HAVE_SYS_UC_ACCESS_H */

static int
get_static_proc_name (unw_addr_space_t as, unw_word_t ip,
		      char *buf, size_t buf_len, unw_word_t *offp,
		      void *arg)
{
  return _Uelf64_get_proc_name (getpid (), ip, buf, buf_len, offp);
}

HIDDEN void
ia64_local_addr_space_init (void)
{
  memset (&local_addr_space, 0, sizeof (local_addr_space));
  ia64_script_cache_init (&local_addr_space.global_cache);
  local_addr_space.big_endian = (__BYTE_ORDER == __BIG_ENDIAN);
  local_addr_space.caching_policy = UNW_CACHE_GLOBAL;
  local_addr_space.acc.find_proc_info = UNW_ARCH_OBJ (find_proc_info);
  local_addr_space.acc.put_unwind_info = put_unwind_info;
  local_addr_space.acc.get_dyn_info_list_addr = get_dyn_info_list_addr;
  local_addr_space.acc.access_mem = access_mem;
  local_addr_space.acc.access_reg = access_reg;
  local_addr_space.acc.access_fpreg = access_fpreg;
  local_addr_space.acc.resume = ia64_local_resume;
  local_addr_space.acc.get_proc_name = get_static_proc_name;
}

#endif /* !UNW_REMOTE_ONLY */
