/* DOS16/32 execution for FD32
 * by Luca Abeni & Hanzac Chen
 *
 * This is free software; see GPL.txt
 */

#include <ll/i386/hw-data.h>
#include <ll/i386/hw-func.h>
#include <ll/i386/error.h>
#include <ll/i386/x-bios.h>
#include <ll/string.h>
#include <devices.h>
#include <filesys.h>
#include <format.h>
#include <fcntl.h>
#include <kmem.h>
#include <exec.h>
#include <kernel.h>
#include <logger.h>
#include "dpmi.h"
#include "dpmimem.h"
#include "dosmem.h"
#include "dosexec.h"
#include "ldtmanag.h"

#define __DOS_EXEC_DEBUG__

#define ENV_SIZE 0x100

static void _set_psp_commandline(struct psp *ppsp, char *args)
{
  if (args != NULL) {
    ppsp->command_line_len = strlen(args);
    memcpy(ppsp->command_line, args, ppsp->command_line_len);
    ppsp->command_line[ppsp->command_line_len] = '\r';
  } else {
    ppsp->command_line_len = 0;
    ppsp->command_line[0] = '\r';
  }
}

/* TODO: Re-consider the fcb1 and fcb2 to support multi-tasking */
static DWORD g_fcb1 = 0, g_fcb2 = 0, g_env_segment, g_env_segtmp = 0;
static void _set_psp(process_info_t *ppi, struct psp *ppsp, WORD ps_size, WORD ps_parent, WORD env_sel, WORD stubinfo_sel, DWORD fcb_1, DWORD fcb_2, char *filename, char *args)
{
  DWORD i;
  char *env_data = (char *)(g_env_segment<<4);

  /* Set the PSP */
  ppi->psp = ppsp;
  /* Init PSP */
  ppsp->ps_size = ps_size; /* segment of first byte beyond memory allocated to program  */
  ppsp->ps_parent = ps_parent;
  if (fcb_1 != NULL) {
    fcb_1 = (fcb_1>>12)+(fcb_1&0x0000FFFF);
    memcpy(ppsp->def_fcb_1, (void *)fcb_1, 16);
  }
  if (fcb_2 != NULL) {
    fcb_2 = (fcb_2>>12)+(fcb_2&0x0000FFFF);
    memcpy(ppsp->def_fcb_2, (void *)fcb_2, 20);
  }

  ppsp->ps_environ = env_sel;
  ppsp->stubinfo_sel = stubinfo_sel;
  ppsp->indos_flag = 0; /* Allow reentrance */
  ppsp->dta = &ppsp->command_line_len;
  /* And now... Set the arg list!!! */
  _set_psp_commandline(ppsp, args);

  /* Find the end of the env list */
  for (i = 0; env_data[i] || env_data[i+1]; i++);
  /* Append filename in the env (NOTE: filename is only the filepath) */
  strcpy(env_data + i + 4, filename);
}

struct threadinfo {
  DWORD pvExcept;
  DWORD pvStackUserTop; /* start stackpointer */
  DWORD pvStackUserBase;
  DWORD SubSystemTib;
  DWORD FiberData;
  DWORD pvArbitrary;
  DWORD ptibSelf; /* Linear address of TIB structure */

  DWORD unknown2;
  DWORD processID;
  DWORD threadID;
  DWORD unknown3;

  DWORD pvTLSArray;
  DWORD pProcess;
  DWORD staticload;

  BYTE res[8]; /* adjust to make it 40h bytes in size */
};

/* NOTE: Move the structure here, Correct? */
struct stubinfo {
  char magic[16];
  DWORD size;
  DWORD minstack;
  DWORD memory_handle;
  DWORD initial_size;
  WORD minkeep;
  WORD ds_selector;
  WORD ds_segment;
  WORD psp_selector;
  WORD cs_selector;
  WORD env_size;
  char basename[8];
  char argv0[16];
  char dpmi_server[16];
  /* FD/32 items */
  DWORD dosbuf_handler;
};

typedef struct stubinfo_ret {
  WORD stubinfo_sel;
  WORD env_sel;
} stubinfo_ret_t;

/* NOTE: FD32 module can't export ?_init name so make it internal with prefix `_' */
static stubinfo_ret_t _stubinfo_init(struct stubinfo *info, struct psp *newpsp, char *filename, DWORD env_size, DWORD initial_size, WORD cs_sel, WORD ds_sel)
{
  stubinfo_ret_t ret = {0, 0};
  int stubinfo_selector, psp_selector, env_selector;

  stubinfo_selector = fd32_allocate_descriptors(1);
#ifdef __DOS_EXEC_DEBUG__
  fd32_log_printf("[DOSEXEC] StubInfo Selector Allocated: = 0x%x\n", stubinfo_selector);
#endif
  if (stubinfo_selector == ERROR_DESCRIPTOR_UNAVAILABLE) {
    return ret;
  }
  fd32_set_segment_base_address(stubinfo_selector, (DWORD)info);
  fd32_set_segment_limit(stubinfo_selector, sizeof(struct stubinfo));

  psp_selector = fd32_allocate_descriptors(1);
#ifdef __DOS_EXEC_DEBUG__
  fd32_log_printf("[DOSEXEC] PSP Selector Allocated: = 0x%x\n", psp_selector);
#endif
  if (psp_selector == ERROR_DESCRIPTOR_UNAVAILABLE) {
    fd32_free_descriptor(stubinfo_selector);
    return ret;
  }
  fd32_set_segment_base_address(psp_selector, (DWORD)newpsp);
  fd32_set_segment_limit(psp_selector, sizeof(struct psp));

  /* Allocate Environment Selector */
  env_selector = fd32_allocate_descriptors(1);
#ifdef __DOS_EXEC_DEBUG__
  fd32_log_printf("[DOSEXEC] Environment Selector Allocated: = 0x%x\n", env_selector);
#endif
  if (env_selector == ERROR_DESCRIPTOR_UNAVAILABLE) {
    fd32_free_descriptor(stubinfo_selector);
    fd32_free_descriptor(psp_selector);
    return ret;
  }
  fd32_set_segment_base_address(env_selector, g_env_segment<<4);
  fd32_set_segment_limit(env_selector, ENV_SIZE);

  strcpy(info->magic, "go32stub, v 2.02");
  info->size = sizeof(struct stubinfo);
  info->minstack = 0x80000;
  info->memory_handle = 0;
  /* Memory pre-allocated by the kernel... */
  info->initial_size = initial_size; /* align? */
  info->minkeep = 0x1000;        /* DOS buffer size... */
  info->dosbuf_handler = dosmem_get(0x1010);
  info->ds_segment = info->dosbuf_handler >> 4;
  info->ds_selector = ds_sel;
  info->cs_selector = cs_sel;
  info->psp_selector = psp_selector;
  info->env_size = env_size;
  strncpy(info->basename, filename, 8);
  info->argv0[0] = '\0';
  strcpy(info->dpmi_server, "FD32.DPMI");

  ret.env_sel = env_selector;
  ret.stubinfo_sel = stubinfo_selector;
  return ret;
}

static void _restore_psp(void)
{
  WORD stubinfo_sel;
  DWORD base, base1;
  struct stubinfo *info;
  process_info_t *cur_P = fd32_get_current_pi();
  struct psp *curpsp = cur_P->psp;

  /* Free memory & selectors... */
  stubinfo_sel = curpsp->stubinfo_sel;
  fd32_get_segment_base_address(stubinfo_sel, &base);
#ifdef __DOS_EXEC_DEBUG__
  fd32_log_printf("[DOSEXEC] Stubinfo Sel: 0x%x --- 0x%lx\n", stubinfo_sel, base);
#endif
  info = (struct stubinfo *)base;
  fd32_get_segment_base_address(info->psp_selector, &base1);
  if (base1 != (DWORD)curpsp) {
    error("Restore PSP: paranoia check failed...\n");
    message("Stubinfo Sel: 0x%x; Base: 0x%lx\n", stubinfo_sel, base);
    message("Current psp address: 0x%lx; Base1: 0x%lx\n", (DWORD)curpsp, base1);
    fd32_abort();
  }
  fd32_free_descriptor(stubinfo_sel);
  fd32_free_descriptor(info->psp_selector);
  fd32_free_descriptor(curpsp->ps_environ);

  fd32_free_jft(cur_P->jft, cur_P->jft_size);

  if (dosmem_free(info->dosbuf_handler, 0x1010) != 0) {
    error("Restore PSP panic while freeing DOS memory...\n");
    fd32_abort();
  }

  if (mem_free(base, sizeof(struct stubinfo) + sizeof(struct psp)) != 0) {
    error("Restore PSP panic while freeing memory...\n");
    fd32_abort();
  }
}

static DWORD funky_base;
static int wrapper_alloc_region(DWORD base, DWORD size)
{
  message("Wrapper allocating region: 0x%lx, 0x%lx...\n",
		  base, size);
  funky_base = base;

  return -1;
}

static DWORD wrapper_alloc(DWORD size)
{
  DWORD tmp;
  DWORD funky_size;
  
  if (funky_base == 0)
    return mem_get(size);

  /* Store the size to allocate */
  funky_size = size + funky_base;

  message("Wrapper allocating 0x%lx = 0x%lx + 0x%lx...\n", funky_size, size, funky_base);
  /* NOTE: DJLibc doesn't need the memory address to be above the image base anymore
   *	So use normal memory allocation ...
   */
  tmp = funky_base;
  if ((funky_base = mem_get(funky_size)) == 0)
    return 0;
  tmp += funky_base;

  funky_base = 0;

  return tmp;
}

static int _go32_init(process_info_t *ppi, DWORD init_size, char *filename, char *args, WORD cs_sel, WORD ds_sel)
{
  DWORD mem;
  DWORD mem_size, env_size;
  struct stubinfo *info;
  struct psp *newpsp;
  stubinfo_ret_t init_ret;

  mem_size = sizeof(struct stubinfo) + sizeof(struct psp);
  /* Always allocate a fixed amount of memory... */
  if ((mem = mem_get(mem_size)) == NULL)
    return -1;
  info = (struct stubinfo *)mem;
  newpsp = (struct psp *)(mem + sizeof(struct stubinfo));
  /*Environment lenght + 2 zeros + 1 word + program name... */
  env_size = 2 + 2 + strlen(filename) + 1;

  init_ret = _stubinfo_init(info, newpsp, filename, env_size, init_size, cs_sel, ds_sel);
  if (init_ret.stubinfo_sel == 0) {
    mem_free(mem, mem_size);
    error("No stubinfo!!!\n");
    return -1;
  }

  _set_psp(ppi, newpsp, 0xFFFF/* fake, above 1M */, 0, init_ret.env_sel, init_ret.stubinfo_sel, g_fcb1, g_fcb2, filename, args);
  return init_ret.stubinfo_sel;
}


extern int wrap_run(DWORD, WORD, DWORD, WORD, WORD, DWORD);
extern void wrap_restore_sp(int res) __attribute__ ((noreturn));

/* NOTE: Simplify ---> user_stack allocated at funcky_base ! */
static int wrapper_create_process(process_info_t *ppi, DWORD entry, DWORD base, DWORD size, DWORD user_stack, char *filename, char *args)
{
  int res;
  int data_selector, code_selector;

  /* Create new running segments */
  /* NOTE: The DS access rights is different with CS */
  if ((data_selector = fd32_allocate_descriptors(1)) == ERROR_DESCRIPTOR_UNAVAILABLE) {
    message("Cannot allocate data selector!\n");
    return 0;
  } else {
    fd32_set_segment_base_address(data_selector, base);
    fd32_set_segment_limit(data_selector, size);
    fd32_set_descriptor_access_rights(data_selector, 0xC092);
  }

  if ((code_selector = fd32_allocate_descriptors(1)) == ERROR_DESCRIPTOR_UNAVAILABLE) {
    fd32_free_descriptor(data_selector);
    message("Cannot allocate code selector!\n");
    return 0;
  } else {
    fd32_set_segment_base_address(code_selector, base);
    fd32_set_segment_limit(code_selector, size);
    fd32_set_descriptor_access_rights(code_selector, 0xC09A);
  }

  fd32_log_printf("User CS, DS = 0x%x, 0x%x\n", code_selector, data_selector);

#ifdef __DOS_EXEC_DEBUG__
  fd32_log_printf("[WRAP] Going to run 0x%lx, size 0x%lx\n",
		entry, size);
#endif
  if ((res = _go32_init(ppi, size, filename, args, code_selector, data_selector)) == -1)
    return -1;
#ifdef __DOS_EXEC_DEBUG__
  fd32_log_printf("[WRAP] Calling run 0x%lx 0x%lx (0x%x 0x%x) --- 0x%lx\n",
		entry, size, code_selector, data_selector, user_stack);
#endif

  res = wrap_run(entry, res, 0, code_selector, data_selector, user_stack);
#ifdef __DOS_EXEC_DEBUG__
  fd32_log_printf("[WRAP] Returned 0x%x: now restoring PSP\n", res);
#endif

  _restore_psp();

  return res;
}

static int wrapper_exec_process(struct kern_funcs *kf, int f, struct read_funcs *rf,
		char *filename, char *args)
{
  process_info_t *ppi;
  int retval;
  DWORD exec_mem;
  DWORD entry, user_stack;
  executable_info_t info;
  char current_drive[2];
  char current_path[FD32_LFNPMAX];

  /* Use wrapper memory allocation to create separate segments */
  kf->mem_alloc = wrapper_alloc;
  kf->mem_alloc_region = wrapper_alloc_region;

  entry = fd32_load_executable(kf, f, rf, &info);
  if (entry == -1) {
    return -1;
  }

  /* Create and set process information */
  ppi = fd32_new_process(filename, args, MAX_OPEN_FILES);
  ppi->_exit = wrap_restore_sp;

  /* Total memory allocated for wrapper execution */
  exec_mem = info.exec_space-info.image_base;
  info.size += info.image_base;
  /* HACK: Make use of the space before image base */
  user_stack = info.image_base;

  /* Inherit the current path from the previous process */
  current_drive[0] = fd32_get_default_drive();
  current_drive[1] = '\0';
  fd32_getcwd(current_drive, current_path);
  fd32_set_current_pi(ppi);
  fd32_chdir(current_path);
  retval = wrapper_create_process(ppi, entry, exec_mem, info.size, user_stack, filename, args);
  /* Back to the previous process NOTE: TSR native programs? */
  fd32_stop_process(ppi);
  message("Returned: %d!!!\n", retval);

  mem_free(exec_mem, info.size);

  return retval;
}


static int direct_exec_process(struct kern_funcs *kf, int f, struct read_funcs *rf,
		char *filename, char *args)
{
  process_info_t *ppi;
  process_params_t params;
  int retval;
  DWORD offset;
  executable_info_t info;

  params.normal.entry = fd32_load_executable(kf, f, rf, &info);
  params.normal.base = info.image_base;
  params.normal.size = info.size;

  if (params.normal.entry != -1 && info.exec_space != 0 && info.exec_space != -1) {
  	ppi = fd32_new_process(filename, args, MAX_OPEN_FILES);
    ppi->type = NORMAL_PROCESS;
#ifdef __DOS_EXEC_DEBUG__
    fd32_log_printf("[DOSEXEC] Before calling 0x%lx...\n", params.normal.entry);
#endif
    if ((retval = _go32_init(ppi, params.normal.size, filename, args, get_cs(), get_ds())) == -1)
      return -1;
    offset = info.exec_space - params.normal.base;
    params.normal.entry += offset;
    params.normal.base = info.exec_space;
    params.normal.fs_sel = retval;
    retval = fd32_start_process(ppi, &params);
    /* Back to the previous process NOTE: TSR native programs? */
    fd32_stop_process(ppi);
#ifdef __DOS_EXEC_DEBUG__
    fd32_log_printf("[DOSEXEC] Returned 0x%x: now restoring PSP\n", retval);
#endif
    if (!(ppi->type&RESIDENT)) {
      _restore_psp();
      mem_free(info.exec_space, params.normal.size);
    }
    return retval;
  } else {
    return fd32_exec_process(kf, f, rf, filename, args);
  }
}

static int vm86_exec_process(struct kern_funcs *kf, int f, struct read_funcs *rf,
		char *filename, char *args)
{
  process_info_t *ppi;
  process_params_t params;
  struct dpmimem_info dminfo;
  struct dos_header hdr;
  struct psp *ppsp;
  X_REGS16 in, out;
  X_SREGS16 s;
  BYTE *exec_text;
  DWORD exec_size;
  WORD load_seg, exec_seg;

  kf->file_read(f, &hdr, sizeof(struct dos_header));

  exec_size = hdr.e_cp*0x20+hdr.e_minalloc+0x400;
  load_seg = dos_alloc((sizeof(struct psp)>>4)+exec_size);

  ppsp = (struct psp *)(load_seg<<4);
  exec_seg = load_seg+(sizeof(struct psp)>>4);
  exec_text = (BYTE *)ppsp+sizeof(struct psp);

  /* NOTE: the last paragraph size */
  if (hdr.e_cblp != 0)
    hdr.e_cblp = 0x200-hdr.e_cblp;

  kf->file_seek(f, hdr.e_cparhdr*0x10, kf->seek_set);
  kf->file_read(f, exec_text, (exec_size-hdr.e_cparhdr)*0x10-hdr.e_cblp);

  /* Relocation */
  if (hdr.e_crlc != 0) {
    DWORD i;
    WORD *p, seg = exec_seg;
    struct dos_reloc *rel = (struct dos_reloc *)mem_get(sizeof(struct dos_reloc)*hdr.e_crlc);
    kf->file_seek(f, hdr.e_lfarlc, kf->seek_set);
    kf->file_read(f, rel, sizeof(struct dos_reloc)*hdr.e_crlc);

    for (i = 0; i < hdr.e_crlc; i++) {
      p = (WORD *)(((seg+rel[i].segment)<<4)+rel[i].offset);
      *p += seg;
    }

    mem_free((DWORD)rel, sizeof(struct dos_reloc)*hdr.e_crlc);
  }

  s.cs = exec_seg+hdr.e_cs;
  s.ss = exec_seg+hdr.e_ss;
  s.es = s.ds = load_seg;
  in.x.ax = 0;
  in.x.bx = 0;
  in.x.dx = s.ds;
  in.x.di = hdr.e_sp;
  in.x.si = hdr.e_ip;
  fd32_log_printf("[DPMI] VM86 execution, ES: %x DS: %x CS: %x IP: %x\n", s.es, s.ds, s.cs, hdr.e_ip);
  ppi = fd32_new_process(filename, args, MAX_OPEN_FILES);
  _set_psp(ppi, ppsp, exec_seg+exec_size, 0, g_env_segment, 0, g_fcb1, g_fcb2, filename, args);
  ppi->type = VM86_PROCESS;
  /* Init dpmimem info */
  dminfo.signature = MEMSIG;
  dminfo.size = 0;
  dminfo.next = NULL;
  ppi->mem_info = &dminfo;
  ppi->_context = (void *)mem_get(sizeof(struct tss));
  params.vm86.ip = hdr.e_ip;
  params.vm86.sp = hdr.e_sp;
  params.vm86.in_regs = &in;
  params.vm86.out_regs = &out;
  params.vm86.seg_regs = &s;
  params.vm86.prev_cpu_context = ppi->_context;
  /* Call in VM86 mode */
  out.x.ax = fd32_start_process(ppi, &params);
  /* Back to the previous process */
  fd32_stop_process(ppi);
  mem_free((DWORD)ppi->_context, sizeof(struct tss));

  if (!(ppi->type & RESIDENT))
    dos_free(load_seg);

  return out.x.ax; /* Return value */
}

static int pei_exec_process(struct kern_funcs *kf, int f, struct read_funcs *rf,
		char *filename, char *args)
{
  process_info_t *ppi;
  process_params_t params;
  executable_info_t info;
  int retval = -1;
  DWORD offset;

  params.normal.entry = fd32_load_executable(kf, f, rf, &info);
  params.normal.base = info.image_base;
  params.normal.size = info.size;

  if (params.normal.entry != -1) {
    ppi = fd32_new_process(filename, args, MAX_OPEN_FILES);
    if (info.exec_space == 0) { /* Normal PEI */
      struct threadinfo *tib = (struct threadinfo *)mem_get(sizeof(struct threadinfo));
      ppi->type = NORMAL_PROCESS;
      offset = info.exec_space - params.normal.base;
      params.normal.entry += offset;
      params.normal.base = info.exec_space;
      params.normal.fs_sel = fd32_allocate_descriptors(1);
      fd32_set_segment_base_address(params.normal.fs_sel, (DWORD)tib);
      fd32_set_segment_limit(params.normal.fs_sel, sizeof(struct stubinfo));
      tib->ptibSelf = (DWORD)tib;
    } else { /* DLL PEI */
      ppi->type = DLL_PROCESS|RESIDENT;
      info.exec_space = params.normal.base;
    }
fd32_log_printf("%x\n", fd32_start_process);
    retval = fd32_start_process(ppi, &params);
    if (!(ppi->type&RESIDENT))
      mem_free(info.exec_space, params.normal.size);
    /* Back to the previous process */
    fd32_stop_process(ppi);
  }

  return retval;
}

/* MZ format handling for VM86 */
static int isMZ(struct kern_funcs *kf, int f, struct read_funcs *rf)
{
  WORD magic;

  kf->file_seek(f, kf->file_offset, kf->seek_set);
  kf->file_read(f, &magic, 2);
  kf->file_seek(f, kf->file_offset, kf->seek_set);

  if (magic != 0x5A4D) { /* "MZ" */
    return 0;
  } else {
    return 1;
  }
}

/* MZ format handling for direct execution */
static int (*p_isMZ)(struct kern_funcs *kf, int f, struct read_funcs *rf) = NULL;
extern void _fd32_vm86_to_pmode();
extern void _fd32_vm86_to_pmode_end();
void *fd32_vm86_to_pmode = NULL;

int dos_exec_switch(int option)
{
  int res = 1;

  if (g_env_segtmp == 0) {
    char *env_data;
    g_env_segtmp = dosmem_get(ENV_SIZE);
    g_env_segment = g_env_segtmp>>4;
    /* The initial environment setup */
    env_data = (char *)(g_env_segment<<4);
    strcpy(env_data, "PATH=.");
    env_data[7] = 0;
    *((WORD *)(env_data+8)) = 1; /* Count of the env-strings */
  }

  fd32_set_binfmt("pei", NULL, pei_exec_process);

  switch(option)
  {
    case DOS_VM86_EXEC:
      /* Install the fd32_vm86_to_pmode */
      if (fd32_vm86_to_pmode == NULL) {
      	const DWORD psize = (DWORD)_fd32_vm86_to_pmode_end - (DWORD)_fd32_vm86_to_pmode;
        BYTE *p = (BYTE *)dosmem_get(psize);
        /* ".code16" "mov $0xfd32, %ax;" "int $0x2f;" "lret;" */
        memcpy(p, _fd32_vm86_to_pmode, psize);
        /* p[0] = 0xB8, p[1] = 0x32, p[2] = 0xFD;
           p[3] = 0xCD, p[4] = 0x2F, p[5] = 0xCB; */
        fd32_vm86_to_pmode = p;
      }
      /* Store the previous check */
      if (p_isMZ == NULL) {
        DWORD i;
        struct bin_format *binfmt = fd32_get_binfmt();
        for (i = 0; binfmt[i].name != NULL; i++)
          if (strcmp(binfmt[i].name, "mz") == 0) {
            p_isMZ = binfmt[i].check;
            break;
          }
      }
      fd32_set_binfmt("mz", isMZ, vm86_exec_process);
      break;
    case DOS_DIRECT_EXEC:
      fd32_set_binfmt("mz", p_isMZ, direct_exec_process);
      break;
    case DOS_WRAPPER_EXEC:
      fd32_set_binfmt("mz", p_isMZ, wrapper_exec_process);
      break;
    default:
      res = 0;
      break;
  }
  
  return res;
}

int dos_exec(char *filename, DWORD env_segment, char *args,
		DWORD fcb1, DWORD fcb2, WORD *return_val)
{
  struct kern_funcs kf;
  struct read_funcs rf;
  struct bin_format *binfmt;
  struct kernel_file f;
  DWORD i;

  if (fd32_kernel_open(filename, O_RDONLY, 0, 0, &f) < 0)
    return -1;

#ifdef __DOS_EXEC_DEBUG__
  fd32_log_printf("FileId = 0x%lx (0x%lx)\n", (DWORD)f.file_id, (DWORD)&f);
#endif
  kf.file_read = fd32_kernel_read;
  kf.file_seek = fd32_kernel_seek;
  kf.mem_alloc = mem_get;
  kf.mem_alloc_region = mem_get_region;
  kf.mem_free = mem_free;
  kf.message = message;
  kf.log = fd32_log_printf;
  kf.error = message;
  kf.get_dll_table = get_dll_table;
  kf.add_dll_table = add_dll_table;
  kf.seek_set = FD32_SEEKSET;
  kf.seek_cur = FD32_SEEKCUR;
  kf.file_offset = 0;

  /* Get the binary format object table, ending with NULL name */
  binfmt = fd32_get_binfmt();
  
  /* Load different modules in various binary format */
  for (i = 0; binfmt[i].name != NULL; i++)
  {
    if (binfmt[i].check(&kf, (int)(&f), &rf)) {
      g_env_segment = env_segment;
      g_fcb1 = fcb1;
      g_fcb2 = fcb2;
      *return_val = binfmt[i].exec(&kf, (int)(&f), &rf, filename, args);
      break;
    }
#ifdef __DOS_EXEC_DEBUG__
    else {
      fd32_log_printf("[MOD] Not '%s' format\n", binfmt[i].name);
    }
#endif
    /* p->file_seek(file, p->file_offset, p->seek_set); */
  }

  fd32_kernel_close((int)&f);
  return 0;
}
