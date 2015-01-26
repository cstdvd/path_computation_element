/* 
 * pd_funopen.h - Substitude funopen/fopencookie functionality.
 *
 * This supplies a replacement funopen() functions for platforms without
 * this functionality.  The pd_xxx() stdio stubs must be used in place of 
 * the normal functions.  Optionally this header can overrride those functions.
 */

#ifndef __PD_STDIO_H__
#define __PD_STDIO_H__ 1

/* We must have stdio.h included before we do any of this. */
#include <stdio.h>
#include <stdarg.h>

__BEGIN_DECLS

struct __sFILEpd;

#ifndef NEED_FUNOPEN


/*
 * For platforms without our custom stdio define our "public" replacement
 * functions back to the real system functions.
 */
#define  __sFILEpd	__sFILE
#define  FILEpd		FILE

#define pd_clearerr clearerr
#define pd_fclose fclose
#define pd_feof feof
#define pd_ferror ferror
#define pd_fflush fflush
#define pd_fgetc fgetc
#define pd_fgetpos fgetpos
#define pd_fgets fgets
#define pd_fopen fopen
#define pd_fopen fopen
#define pd__fopen fopen
#define pd_fprintf fprintf
#define pd_fputc fputc
#define pd_fputs fputs
#define pd_fread fread
#define pd_freopen freopen
#define pd_freopen freopen
#define pd__freopen freopen
#define pd_fscanf fscanf
#define pd_fseek fseek
#define pd_fsetpos fsetpos
#define pd_ftell ftell
#define pd_fwrite fwrite
#define pd_getc getc
#define pd_rewind rewind
#define pd_setbuf setbuf
#define pd_setvbuf setvbuf
#define pd_ungetc ungetc
#define pd_vfprintf vfprintf
#define pd_vasprintf vasprintf
#define pd_vfscanf vfscanf
#define pd_ftrylockfile ftrylockfile
#define pd_flockfile flockfile
#define pd_funlockfile funlockfile
#define pd_getc_unlocked getc_unlocked
#define pd_clearerr_unlocked clearerr_unlocked
#define pd_feof_unlocked feof_unlocked
#define pd_ferror_unlocked ferror_unlocked
#define pd_fileno_unlocked fileno_unlocked
#define pd_fseeko fseeko
#define pd_ftello ftello
#define pd_getw getw
#define pd_fcloseall fcloseall
#define pd_fgetln fgetln
#define pd_fpurge fpurge
#define pd_setbuffer setbuffer
#define pd_setlinebuf setlinebuf
#define pd_funopen funopen

#else

/*
 * Our optional overrides.  PRIVATE trumps OVERRIDE.
 */
#ifdef PD_STDIO_PRIVATE
#undef PD_STDIO_OVERRIDE
#endif

/*
 * Functions defined in ANSI C standard that we replace.
 */
#ifdef PD_STDIO_OVERRIDE
#undef clearerr
#define clearerr pd_clearerr
#endif
#ifdef PD_STDIO_PRIVATE
#undef clearerr
#undef _clearerr
#define clearerr pd_clearerr_p
#define _clearerr pd_clearerr_p
#endif
void	 pd_clearerr(FILE *);
void	 pd_clearerr_p(struct __sFILEpd *);

#ifdef PD_STDIO_OVERRIDE
#undef fclose
#define fclose pd_fclose
#endif
#ifdef PD_STDIO_PRIVATE
#undef fclose
#undef _fclose
#define fclose pd_fclose_p
#define _fclose pd_fclose_p
#endif
int	 pd_fclose(FILE *);
int	 pd_fclose_p(struct __sFILEpd *, int close_pub);

#ifdef PD_STDIO_OVERRIDE
#undef feof
#define feof pd_feof
#endif
#ifdef PD_STDIO_PRIVATE
#undef feof
#undef _feof
#define feof pd_feof_p
#define _eof pd_feof_p
#endif
int	 pd_feof(FILE *);
int	 pd_feof_p(struct __sFILEpd *);

#ifdef PD_STDIO_OVERRIDE
#undef ferror
#define ferror pd_ferror
#endif
#ifdef PD_STDIO_PRIVATE
#undef ferror
#undef _ferror
#define ferror pd_ferror_p
#define _ferror pd_ferror_p
#endif
int	 pd_ferror(FILE *);
int	 pd_ferror_p(struct __sFILEpd *);

#ifdef PD_STDIO_OVERRIDE
#undef fflush
#define fflush pd_fflush
#endif
#ifdef PD_STDIO_PRIVATE
#undef fflush
#undef _fflush
#define fflush pd_fflush_p
#define _fflush pd_fflush_p
#endif
int	 pd_fflush(FILE *);
int	 pd_fflush_p(struct __sFILEpd *);
#ifdef PD_STDIO_OVERRIDE
#undef fgetc
#define fgetc pd_fgetc
#endif
#ifdef PD_STDIO_PRIVATE
#undef fgetc
#undef _fgetc
#define fgetc pd_fgetc_p
#define _fgetc pd_fgetc_p
#endif
int	 pd_fgetc(FILE *);
int	 pd_fgetc_p(struct __sFILEpd *);
#ifdef PD_STDIO_OVERRIDE
#undef fgetpos
#define fgetpos pd_fgetpos
#endif
#ifdef PD_STDIO_PRIVATE
#undef fgetpos
#undef _fgetpos
#define fgetpos pd_fgetpos_p
#define _fgetpos pd_fgetpos_p
#endif
int	 pd_fgetpos(FILE * __restrict, fpos_t * __restrict);
int	 pd_fgetpos_p(struct __sFILEpd * __restrict, fpos_t * __restrict);
#ifdef PD_STDIO_OVERRIDE
#undef fgets
#define fgets pd_fgets
#endif
#ifdef PD_STDIO_PRIVATE
#undef fgets
#undef _fgets
#define fgets pd_fgets_p
#define _fgets pd_fgets_p
#endif
char	*pd_fgets(char * __restrict, int, FILE * __restrict);
char	*pd_fgets_p(char * __restrict, int, struct __sFILEpd * __restrict);

/* fopen has a wrapper but no private implementation */
#ifdef PD_STDIO_OVERRIDE
#undef fopen
#define fopen pd_fopen
#endif
#ifdef PD_STDIO_PRIVATE
#undef fopen
#undef _fopen
#define fopen pd_fopen
#define _fopen pd_fopen
#endif
FILE	*pd_fopen(const char * __restrict, const char * __restrict);

#ifdef PD_STDIO_OVERRIDE
#undef fprintf
#define fprintf pd_fprintf
#endif
#ifdef PD_STDIO_PRIVATE
#undef fprintf
#undef _fprintf
#define fprintf pd_fprintf_p
#define _fprintf pd_fprintf_p
#endif
int	 pd_fprintf(FILE * __restrict, const char * __restrict, ...);
int	 pd_fprintf_p(struct __sFILEpd * __restrict, const char * __restrict, ...);
#ifdef PD_STDIO_OVERRIDE
#undef fputc
#define fputc pd_fputc
#endif
#ifdef PD_STDIO_PRIVATE
#undef fputc
#undef _fputc
#define fputc pd_fputc_p
#define _fputc pd_fputc_p
#endif
int	 pd_fputc(int, FILE *);
int	 pd_fputc_p(int, struct __sFILEpd *);
#ifdef PD_STDIO_OVERRIDE
#undef fputs
#define fputs pd_fputs
#endif
#ifdef PD_STDIO_PRIVATE
#undef fputs
#undef _fputs
#define fputs pd_fputs_p
#define _fputs pd_fputs_p
#endif
int	 pd_fputs(const char * __restrict, FILE * __restrict);
int	 pd_fputs_p(const char * __restrict, struct __sFILEpd * __restrict);
#ifdef PD_STDIO_OVERRIDE
#undef fread
#define fread pd_fread
#endif
#ifdef PD_STDIO_PRIVATE
#undef fread
#undef _fread
#define fread pd_fread_p
#define _fread pd_fread_p
#endif
size_t	 pd_fread(void * __restrict, size_t, size_t, FILE * __restrict);
size_t	 pd_fread_p(void * __restrict, size_t, size_t, struct __sFILEpd * __restrict);

/* freopen has a wrapper but no private implementation */
#ifdef PD_STDIO_OVERRIDE
#undef freopen
#define freopen pd_freopen
#endif
#ifdef PD_STDIO_PRIVATE
#undef freopen
#undef _freopen
#define freopen pd_freopen
#define _freopen pd_freopen
#endif
FILE	*pd_freopen(const char * __restrict, const char * __restrict, FILE * __restrict);

#ifdef PD_STDIO_OVERRIDE
#undef fscanf
#define fscanf pd_fscanf
#endif
#ifdef PD_STDIO_PRIVATE
#undef fscanf
#undef _fscanf
#define fscanf pd_fscanf_p
#define _fscanf pd_fscanf_p
#endif
int	 pd_fscanf(FILE * __restrict, const char * __restrict, ...);
int	 pd_fscanf_p(struct __sFILEpd * __restrict, const char * __restrict, ...);
#ifdef PD_STDIO_OVERRIDE
#undef fseek
#define fseek pd_fseek
#endif
#ifdef PD_STDIO_PRIVATE
#undef fseek
#undef _fseek
#define fseek pd_fseek_p
#define _fseek pd_fseek_p
#endif
int	 pd_fseek(FILE *, long, int);
int	 pd_fseek_p(struct __sFILEpd *, long, int);

#ifdef PD_STDIO_OVERRIDE
#undef fsetpos
#define fsetpos pd_fsetpos
#endif
#ifdef PD_STDIO_PRIVATE
#undef fsetpos
#undef _fsetpos
#define fsetpos pd_fsetpos_p
#define _fsetpos pd_fsetpos_p
#endif
int	 pd_fsetpos(FILE *, const fpos_t *);
int	 pd_fsetpos_p(struct __sFILEpd *, const fpos_t *);
#ifdef PD_STDIO_OVERRIDE
#undef ftell
#define ftell pd_ftell
#endif
#ifdef PD_STDIO_PRIVATE
#undef ftell
#undef _ftell
#define ftell pd_ftell_p
#define _ftell pd_ftell_p
#endif
long	 pd_ftell(FILE *);
long	 pd_ftell_p(struct __sFILEpd *);
#ifdef PD_STDIO_OVERRIDE
#undef fwrite
#define fwrite pd_fwrite
#endif
#ifdef PD_STDIO_PRIVATE
#undef fwrite
#undef _fwrite
#define fwrite pd_fwrite_p
#define _fwrite pd_fwrite_p
#endif
size_t	 pd_fwrite(const void * __restrict, size_t, size_t, FILE * __restrict);
size_t	 pd_fwrite_p(const void * __restrict, size_t, size_t, struct __sFILEpd * __restrict);
#ifdef PD_STDIO_OVERRIDE
#undef getc
#define getc pd_getc
#endif
#ifdef PD_STDIO_PRIVATE
#undef getc
#undef _getc
#define getc pd_getc_p
#define _getc pd_getc_p
#endif
int	 pd_getc(FILE *);
int	 pd_getc_p(struct __sFILEpd *);
#ifdef PD_STDIO_OVERRIDE
#undef putc
#define putc pd_putc
#endif
#ifdef PD_STDIO_PRIVATE
#undef putc
#undef _putc
#define putc pd_putc_p
#define _putc pd_putc_p
#endif
int	 pd_putc(int, FILE *);
int	 pd_putc_p(int, struct __sFILEpd *);
#ifdef PD_STDIO_OVERRIDE
#undef rewind
#define rewind pd_rewind
#endif
#ifdef PD_STDIO_PRIVATE
#undef rewind
#undef _rewind
#define rewind pd_rewind_p
#define _rewind pd_rewind_p
#endif
void	 pd_rewind(FILE *);
void	 pd_rewind_p(struct __sFILEpd *);
#ifdef PD_STDIO_OVERRIDE
#undef setbuf
#define setbuf pd_setbuf
#endif
#ifdef PD_STDIO_PRIVATE
#undef setbuf
#undef _setbuf
#define setbuf pd_setbuf_p
#define _setbuf pd_setbuf_p
#endif
void	 pd_setbuf(FILE * __restrict, char * __restrict);
void	 pd_setbuf_p(struct __sFILEpd * __restrict, char * __restrict);
#ifdef PD_STDIO_OVERRIDE
#undef setvbuf
#define setvbuf pd_setvbuf
#endif
#ifdef PD_STDIO_PRIVATE
#undef setvbuf
#undef _setvbuf
#define setvbuf pd_setvbuf_p
#define _setvbuf pd_setvbuf_p
#endif
int	 pd_setvbuf(FILE * __restrict, char * __restrict, int, size_t);
int	 pd_setvbuf_p(struct __sFILEpd * __restrict, char * __restrict, int, size_t);
#ifdef PD_STDIO_OVERRIDE
#undef ungetc
#define ungetc pd_ungetc
#endif
#ifdef PD_STDIO_PRIVATE
#undef ungetc
#undef _ungetc
#define ungetc pd_ungetc_p
#define _ungetc pd_ungetc_p
#endif
int	 pd_ungetc(int, FILE *);
int	 pd_ungetc_p(int, struct __sFILEpd *);

#ifdef PD_STDIO_OVERRIDE
#undef vfprintf
#define vfprintf pd_vfprintf
#endif
#ifdef PD_STDIO_PRIVATE
#undef vfprintf
#undef _vfprintf
#define vfprintf pd_vfprintf_p
#define _vfprintf pd_vfprintf_p
#endif
int	 pd_vfprintf(FILE * __restrict, const char * __restrict,
	    va_list);
int	 pd_vfprintf_p(struct __sFILEpd * __restrict, const char * __restrict,
	    va_list);

#ifdef PD_STDIO_OVERRIDE
#undef vasprintf
#define vasprintf pd_vasprintf
#endif
#ifdef PD_STDIO_PRIVATE
#undef vasprintf
#undef _vasfprintf
#define vasprintf pd_vsprintf_p
#define _vasprintf pd_vsprintf_p
#endif
int	 pd_vasprintf(char ** __restrict, const char * __restrict,
	    va_list);
int	 pd_vasprintf_p(char ** __restrict, const char * __restrict,
	    va_list);

#if __ISO_C_VISIBLE >= 1999


#ifdef PD_STDIO_OVERRIDE
#undef vfscanf
#define vfscanf pd_vfscanf
#endif
#ifdef PD_STDIO_PRIVATE
#undef vfscanf
#undef _vfscanf
#define vfscanf pd_vfscanf_p
#define _vfscanf pd_vfscanf_p
#endif
int	 pd_vfscanf(FILE * __restrict, const char * __restrict, __va_list);
int	 pd_vfscanf_p(struct __sFILEpd * __restrict, const char * __restrict, __va_list);

#endif

/*
 * Functions defined in all versions of POSIX 1003.1.
 */


#if __POSIX_VISIBLE >= 199506

#ifdef PD_STDIO_OVERRIDE
#undef ftrylockfile
#define ftrylockfile pd_ftrylockfile
#endif
#ifdef PD_STDIO_PRIVATE
#undef ftrylockfile
#undef _ftrylockfile
#define ftrylockfile pd_ftrylockfile_p
#define _ftrylockfile pd_ftrylockfile_p
#endif
int	 pd_ftrylockfile(FILE *);
int	 pd_ftrylockfile_p(struct __sFILEpd *);
#ifdef PD_STDIO_OVERRIDE
#undef flockfile
#define flockfile pd_flockfile
#endif
#ifdef PD_STDIO_PRIVATE
#undef flockfile
#undef _flockfile
#define flockfile pd_flockfile_p
#define _flockfile pd_flockfile_p
#endif
void	 pd_flockfile(FILE *);
void	 pd_flockfile_p(struct __sFILEpd *);
#ifdef PD_STDIO_OVERRIDE
#undef funlockfile
#define funlockfile pd_funlockfile
#endif
#ifdef PD_STDIO_PRIVATE
#undef funlockfile
#undef _funlockfile
#define funlockfile pd_funlockfile_p
#define _funlockfile pd_funlockfile_p
#endif
void	 pd_funlockfile(FILE *);
void	 pd_funlockfile_p(struct __sFILEpd *);

/*
 * These are normally used through macros as defined below, but POSIX
 * requires functions as well.
 */
#ifdef PD_STDIO_OVERRIDE
#undef getc_unlocked
#define getc_unlocked pd_getc_unlocked
#endif
#ifdef PD_STDIO_PRIVATE
#undef getc_unlocked
#undef _getc_unlocked
#define getc_unlocked pd_getc_unlocked_p
#define _getc_unlocked pd_getc_unlocked_p
#endif
int	 pd_getc_unlocked(FILE *);
int	 pd_getc_unlocked_p(struct __sFILEpd *);
#ifdef PD_STDIO_OVERRIDE
#undef putc_unlocked
#define putc_unlocked pd_putc_unlocked
#endif
#ifdef PD_STDIO_PRIVATE
#undef putc_unlocked
#undef _putc_unlocked
#define putc_unlocked pd_putc_unlocked_p
#define _putc_unlocked pd_putc_unlocked_p
#endif
int	 pd_putc_unlocked(int, FILE *);
int	 pd_putc_unlocked_p(int, struct __sFILEpd *);

#endif

#if __BSD_VISIBLE

#ifdef PD_STDIO_OVERRIDE
#undef clearerr_unlocked
#define clearerr_unlocked pd_clearerr_unlocked
#endif
#ifdef PD_STDIO_PRIVATE
#undef clearerr_unlocked
#undef _clearerr_unlocked
#define clearerr_unlocked pd_clearerr_unlocked_p
#define _clearerr_unlocked pd_clearerr_unlocked_p
#endif
void	pd_clearerr_unlocked(FILE *);
void	pd_clearerr_unlocked_p(struct __sFILEpd *);
#ifdef PD_STDIO_OVERRIDE
#undef feof_unlocked
#define feof_unlocked pd_feof_unlocked
#endif
#ifdef PD_STDIO_PRIVATE
#undef feof_unlocked
#undef _feof_unlocked
#define feof_unlocked pd_feof_unlocked_p
#define _feof_unlocked pd_feof_unlocked_p
#endif
int	pd_feof_unlocked(FILE *);
int	pd_feof_unlocked_p(struct __sFILEpd *);
#ifdef PD_STDIO_OVERRIDE
#undef ferror_unlocked
#define ferror_unlocked pd_ferror_unlocked
#endif
#ifdef PD_STDIO_PRIVATE
#undef ferror_unlocked
#undef _ferror_unlocked
#define ferror_unlocked pd_ferror_unlocked_p
#define _ferror_unlocked pd_ferror_unlocked_p
#endif
int	pd_ferror_unlocked(FILE *);
int	pd_ferror_unlocked_p(struct __sFILEpd *);
#ifdef PD_STDIO_OVERRIDE
#undef fileno_unlocked
#define fileno_unlocked pd_fileno_unlocked
#endif
#ifdef PD_STDIO_PRIVATE
#undef fileno_unlocked
#undef _fileno_unlocked
#define fileno_unlocked pd_fileno_unlocked_p
#define _fileno_unlocked pd_fileno_unlocked_p
#endif
int	pd_fileno_unlocked(FILE *);
int	pd_fileno_unlocked_p(struct __sFILEpd *);
#endif

#if __POSIX_VISIBLE >= 200112

#ifdef PD_STDIO_OVERRIDE
#undef fseeko
#define fseeko pd_fseeko
#endif

/* seeko/tello are special as the foo and _foo versions are different */
#ifdef PD_STDIO_PRIVATE
#undef fseeko
#undef _fseeko
#define fseeko pd_fseeko_p
#define _fseeko _pd_fseeko_p
#endif
int	 pd_fseeko(FILE *, off_t, int);
int	 pd_fseeko_p(struct __sFILEpd *, off_t, int);
int	 _pd_fseeko_p(struct __sFILEpd *, off_t, int, int);

#ifdef PD_STDIO_OVERRIDE
#undef ftello
#define ftello pd_ftello
#endif
#ifdef PD_STDIO_PRIVATE
#undef ftello
#undef _ftello
#define ftello pd_ftello_p
#define _ftello _pd_ftello_p
#endif
off_t	 pd_ftello(FILE *);
off_t	 pd_ftello_p(struct __sFILEpd *);
int	 _pd_ftello_p(struct __sFILEpd *, fpos_t *);

#endif

#if __BSD_VISIBLE || __XSI_VISIBLE > 0 && __XSI_VISIBLE < 600

#ifdef PD_STDIO_OVERRIDE
#undef getw
#define getw pd_getw
#endif
#ifdef PD_STDIO_PRIVATE
#undef getw
#undef _getw
#define getw pd_getw_p
#define _getw pd_getw_p
#endif
int	 pd_getw(FILE *);
int	 pd_getw_p(struct __sFILEpd *);
#ifdef PD_STDIO_OVERRIDE
#undef putw
#define putw pd_putw
#endif
#ifdef PD_STDIO_PRIVATE
#undef putw
#undef _putw
#define putw pd_putw_p
#define _putw pd_putw_p
#endif
int	 pd_putw(int, FILE *);
int	 pd_putw_p(int, struct __sFILEpd *);

#endif /* BSD or X/Open before issue 6 */

/*
 * Routines that are purely local.
 */
#if __BSD_VISIBLE

#ifdef PD_STDIO_OVERRIDE
#undef fcloseall
#define fcloseall pd_fcloseall
#endif
#ifdef PD_STDIO_PRIVATE
#undef fcloseall
#undef _fcloseall
#define fcloseall pd_fcloseall_p
#define _fcloseall pd_fcloseall_p
#endif
void	pd_fcloseall(void);
void	pd_fcloseall_p(void);

#ifdef PD_STDIO_OVERRIDE
#undef fgetln
#define fgetln pd_fgetln
#endif
#ifdef PD_STDIO_PRIVATE
#undef fgetln
#undef _fgetln
#define fgetln pd_fgetln_p
#define _fgetln pd_fgetln_p
#endif
char	*pd_fgetln(FILE *, size_t *);
char	*pd_fgetln_p(struct __sFILEpd *, size_t *);

#ifdef PD_STDIO_OVERRIDE
#undef fpurge
#define fpurge pd_fpurge
#endif
#ifdef PD_STDIO_PRIVATE
#undef fpurge
#undef _fpurge
#define fpurge pd_fpurge_p
#define _fpurge pd_fpurge_p
#endif
int	 pd_fpurge(FILE *);
int	 pd_fpurge_p(struct __sFILEpd *);

#ifdef PD_STDIO_OVERRIDE
#undef setbuffer
#define setbuffer pd_setbuffer
#endif
#ifdef PD_STDIO_PRIVATE
#undef setbuffer
#undef _setbuffer
#define setbuffer pd_setbuffer_p
#define _setbuffer pd_setbuffer_p
#endif
void	 pd_setbuffer(FILE *, char *, int);
void	 pd_setbuffer_p(struct __sFILEpd *, char *, int);
#ifdef PD_STDIO_OVERRIDE
#undef setlinebuf
#define setlinebuf pd_setlinebuf
#endif
#ifdef PD_STDIO_PRIVATE
#undef setlinebuf
#undef _setlinebuf
#define setlinebuf pd_setlinebuf_p
#define _setlinebuf pd_setlinebuf_p
#endif
int	 pd_setlinebuf(FILE *);
int	 pd_setlinebuf_p(struct __sFILEpd *);

/*
 * Stdio function-access interface.
 */
#ifdef PD_STDIO_OVERRIDE
#undef funopen
#undef fropen
#undef fwopen
#define funopen pd_funopen
#define	fropen(cookie, fn) pd_funopen(cookie, fn, 0, 0, 0)
#define	fwopen(cookie, fn) pd_funopen(cookie, 0, fn, 0, 0)
#endif
#ifdef PD_STDIO_PRIVATE
#undef funopen
#undef _funopen
#undef fropen
#undef fwopen
#define	fropen(cookie, fn) pd_funopen_p(cookie, fn, 0, 0, 0)
#define	fwopen(cookie, fn) pd_funopen_p(cookie, 0, fn, 0, 0)
#define funopenpd_funopen_p
#define _funopen pd_funopen_p
#endif

#define	pd_fropen(cookie, fn) pd_funopen(cookie, fn, 0, 0, 0)
#define	pd_fwopen(cookie, fn) pd_funopen(cookie, 0, fn, 0, 0)
FILE	*pd_funopen(const void *,
	    int (*)(void *, char *, int),
	    int (*)(void *, const char *, int),
	    fpos_t (*)(void *, fpos_t, int),
	    int (*)(void *));
struct __sFILEpd	*pd_funopen_p(const void *,
	    int (*)(void *, char *, int),
	    int (*)(void *, const char *, int),
	    fpos_t (*)(void *, fpos_t, int),
	    int (*)(void *));

void *	 pd_fgetcookie(FILE *rfp);



#endif /* BSD_VISABLE */

#if 0
/*
 * These are the functions we don't replace because they either 
 * don't touch a FILE at all or they act on the std streams which are 
 * never used/created by funopen.
 *
 * In the case of fileno it's meaningless on a custom stream but it will 
 * work okay as the implementation forces opening of a real /dev/null.
 */

int	 asprintf(char **, const char *, ...) __printflike(2, 3);
char	*ctermid(char *);
char	*ctermid_r(char *);
FILE	*fdopen(int, const char *);
int	 fileno(FILE *);
__const char *fmtcheck(const char *, const char *) __format_arg(2);
int	 getchar(void);
int	 getchar_unlocked(void);
char	*gets(char *);
int	 printf(const char * __restrict, ...);
int	 putchar(int);
int	 putchar_unlocked(int);
int	 puts(const char *);
void	 perror(const char *);
int	 remove(const char *);
int	 rename(const char *, const char *);
int	 scanf(const char * __restrict, ...);
int	 snprintf(char * __restrict, size_t, const char * __restrict, ...) __printflike(3, 4);
int	 sprintf(char * __restrict, const char * __restrict, ...);
int	 sscanf(const char * __restrict, const char * __restrict, ...);
FILE	*tmpfile(void);
char	*tmpnam(char *);
int	 vprintf(const char * __restrict, __va_list);
int	 vscanf(const char * __restrict, __va_list) __scanflike(1, 0);
int	 vsnprintf(char * __restrict, size_t, const char * __restrict,  __va_list) __printflike(3, 0);
int	 vsprintf(char * __restrict, const char * __restrict, __va_list);
int	 vsscanf(const char * __restrict, const char * __restrict, __va_list)	    __scanflike(2, 0);
#endif

__END_DECLS
#endif

#endif
