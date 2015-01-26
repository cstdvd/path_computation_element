/* 
 * pd_glue.h - Substitude funopen/fopencookie functionality.
 *
 * These are the "public" glue funcs to the private stdio library.
 * If the FILE * passed is a private function, the private bits are 
 * invoked, otherwise the public API is invoked.
 */

#include <stdio.h>
#include <errno.h>
#include <unistd.h>

#include <pd_stdio_p.h>

#undef FILE
#undef __sFILE

/*
 * Functions defined in ANSI C standard that we replace.
 */
#undef clearerr
void
pd_clearerr(FILE *rfp)
{
	FILEpd	*fp;

	fp = file2filepd(rfp);
	if (fp == NULL) {
		clearerr(rfp);
	} else {
		pd_clearerr_p(fp);
	}
	return;
}

#undef fclose
int
pd_fclose(FILE *rfp)
{
	FILEpd	*fp;

	fp = file2filepd(rfp);
	if (fp == NULL) {
		return(fclose(rfp));
	}
	return(pd_fclose_p(fp, 1));
}

#undef feof
int
pd_feof(FILE *rfp)
{
	FILEpd	*fp;

	fp = file2filepd(rfp);
	if (fp == NULL) {
		return(feof(rfp));
	}
	return(pd_feof_p(fp));
}

#undef ferror
int
pd_ferror(FILE *rfp)
{
	FILEpd	*fp;

	fp = file2filepd(rfp);
	if (fp == NULL) {
		return(ferror(rfp));
	}
	return(pd_ferror_p(fp));
}

#undef fflush
int
pd_fflush(FILE *rfp)
{
	FILEpd	*fp;

	fp = file2filepd(rfp);
	if (fp == NULL) {
		return(fflush(rfp));
	}
	return(pd_fflush_p(fp));
}

#undef fgetc
int
pd_fgetc(FILE *rfp)
{
	FILEpd	*fp;

	fp = file2filepd(rfp);
	if (fp == NULL) {
		return(fgetc(rfp));
	}
	return(pd_fgetc_p(fp));
}

#undef fgetpos
int
pd_fgetpos(FILE *rfp, fpos_t *fpos)
{
	FILEpd	*fp;

	fp = file2filepd(rfp);
	if (fp == NULL) {
		return(fgetpos(rfp, fpos));
	}
	return(pd_fgetpos_p(fp, fpos));
}

#undef fgets
char *
pd_fgets(char *buf, int len, FILE *rfp)
{
	FILEpd	*fp;

	fp = file2filepd(rfp);
	if (fp == NULL) {
		return(fgets(buf, len, rfp));
	}
	return(pd_fgets_p(buf, len, fp));
}

/* fopen has a wrapper but no private implementation */
#undef fopen
FILE *
pd_fopen(const char *path, const char *mode)
{
	return(fopen(path, mode));
}

#undef fprintf
#undef vfprintf
int
pd_fprintf(FILE *rfp, const char *fmt0, ...)
{
	FILEpd	*fp;
	va_list	al;
	int	ret;

	va_start(al, fmt0);

	fp = file2filepd(rfp);
	if (fp == NULL) {
		ret = vfprintf(rfp, fmt0, al);
	} else {
		ret = pd_vfprintf_p(fp, fmt0, al);
	}
	va_end(al);
	return(ret);
}

#undef fputc
int
pd_fputc(int ch, FILE *rfp)
{
	FILEpd	*fp;

	fp = file2filepd(rfp);
	if (fp == NULL) {
		return(fputc(ch, rfp));
	}
	return(pd_fputc_p(ch, fp));
}

#undef fputs
int
pd_fputs(const char *buf, FILE *rfp)
{
	FILEpd	*fp;

	fp = file2filepd(rfp);
	if (fp == NULL) {
		return(fputs(buf, rfp));
	}
	return(pd_fputs_p(buf, fp));
}

#undef fread
size_t
pd_fread(void *buf, size_t rlen, size_t num, FILE *rfp)
{
	FILEpd	*fp;

	fp = file2filepd(rfp);
	if (fp == NULL) {
		return(fread(buf, rlen, num, rfp));
	}
	return(pd_fread_p(buf, rlen, num, fp));
}

/*
 * freopen is a bit odd.  We shouldn't really allow reopening a 
 * custom stream but try to be defensive.
 * So in this case close the underlying stream and then let the OS
 * try to reopen the real file.
 */
#undef freopen
FILE *
pd_freopen(const char *path, const char *mode, FILE *rfp)
{
	FILEpd	*fp;

	fp = file2filepd(rfp);
	if (rfp != NULL) {	
		/* Keep pd_fclose_fp from closing the underlying FILE */
		  pd_fclose_p(fp, 0);
	}
	return(freopen(path, mode, rfp));
}

#undef fscanf
#undef vfscanf
int
pd_fscanf(FILE *rfp, const char *fmt0, ...)
{
	FILEpd	*fp;
	va_list	al;
	int	ret;

	va_start(al, fmt0);

	fp = file2filepd(rfp);
	if (fp == NULL) {
#if defined(WIN32) && !defined(__MINGW32__)
		/* Win32 has no vfscanf, MINGW does */
		return(-1);
#else
		ret = vfscanf(rfp, fmt0, al);
#endif
	} else {
		ret = pd_vfscanf_p(fp, fmt0, al);
	}
	va_end(al);
	return(ret);
}

#undef fseek
int
pd_fseek(FILE *rfp, long off, int whence)
{
	FILEpd	*fp;

	fp = file2filepd(rfp);
	if (fp == NULL) {
		return(fseek(rfp, off, whence));
	}
	return(pd_fseek_p(fp, off, whence));
}

#undef fsetpos
int
pd_fsetpos(FILE *rfp, const fpos_t *fpos)
{
	FILEpd	*fp;

	fp = file2filepd(rfp);
	if (fp == NULL) {
		return(fsetpos(rfp, fpos));
	}
	return(pd_fsetpos_p(fp, fpos));
}

#undef ftell
long
pd_ftell(FILE *rfp)
{
	FILEpd	*fp;

	fp = file2filepd(rfp);
	if (fp == NULL) {
		return(ftell(rfp));
	}
	return(pd_ftell_p(fp));
}

#undef fwrite
size_t
pd_fwrite(const void *buf, size_t recs, size_t nrec, FILE *rfp)
{
	FILEpd	*fp;

	fp = file2filepd(rfp);
	if (fp == NULL) {
		return(fwrite(buf, recs, nrec, rfp));
	}
	return(pd_fwrite_p(buf, recs, nrec, fp));
}

#undef getc
int
pd_getc(FILE *rfp)
{
	FILEpd	*fp;

	fp = file2filepd(rfp);
	if (fp == NULL) {
		return(getc(rfp));
	}
	return(pd_getc_p(fp));
}

#undef putc
int
pd_putc(int ch, FILE *rfp)
{
	FILEpd	*fp;

	fp = file2filepd(rfp);
	if (fp == NULL) {
		return(putc(ch, rfp));
	}
	return(pd_putc_p(ch, fp));
}

#undef rewind
void
pd_rewind(FILE *rfp)
{
	FILEpd	*fp;

	fp = file2filepd(rfp);
	if (fp == NULL) {
		rewind(rfp);
	} else {
		pd_rewind_p(fp);
	}
	return;
}

#undef setbuf
void
pd_setbuf(FILE *rfp, char *buf)
{
	FILEpd	*fp;

	fp = file2filepd(rfp);
	if (fp == NULL) {
		setbuf(rfp, buf);
	} else {
		pd_setbuf_p(fp, buf);
	}
	return;
}

#undef setvbuf
int
pd_setvbuf(FILE *rfp, char *buf, int mode, size_t len)
{
	FILEpd	*fp;

	fp = file2filepd(rfp);
	if (fp == NULL) {
		return(setvbuf(rfp, buf, mode, len));
	}
	return(pd_setvbuf_p(fp, buf, mode, len));
}

#undef ungetc
int
pd_ungetc(int ch, FILE *rfp)
{
	FILEpd	*fp;

	fp = file2filepd(rfp);
	if (fp == NULL) {
		return(ungetc(ch, rfp));
	}
	return(pd_ungetc_p(ch, fp));
}

#undef vfprintf
int
pd_vfprintf(FILE *rfp, const char *fmt0, __va_list al)
{
	FILEpd	*fp;

	fp = file2filepd(rfp);
	if (fp == NULL) {
		return(vfprintf(rfp, fmt0, al));
	}
	return(pd_vfprintf_p(fp, fmt0, al));
}

#undef vasprintf
int
pd_vasprintf(char **bufp, const char *fmt0, __va_list al)
{
#ifdef WIN32
	return(pd_vasprintf_p(bufp, fmt0, al));
#else
	return(vasprintf(bufp, fmt0, al));
#endif
}


#undef vfscanf
int
pd_vfscanf(FILE *rfp, const char *fmt0, __va_list al)
{
	FILEpd	*fp;

	fp = file2filepd(rfp);
	if (fp == NULL) {
#if defined(WIN32) && !defined(__MINGW32__)
		/* Win32 has no vfscanf, MINGW does */
		return(-1);
#else
		return(vfscanf(rfp, fmt0, al));
#endif
	}
	return(pd_vfscanf_p(fp, fmt0, al));
}

/*
 * Functions defined in all versions of POSIX 1003.1.
 */
#undef ftrylockfile
int
pd_ftrylockfile(FILE *rfp)
{
	FILEpd	*fp;

	fp = file2filepd(rfp);
	if (fp == NULL) {
#ifdef WIN32
		return(0);
#else
		return(ftrylockfile(rfp));
#endif
	}
	return(pd_ftrylockfile_p(fp));
}

#undef flockfile
void
pd_flockfile(FILE *rfp)
{
	FILEpd	*fp;

	fp = file2filepd(rfp);
	if (fp == NULL) {
#ifndef WIN32
		flockfile(rfp);
#endif
	} else {
		pd_flockfile_p(fp);
	}
	return;
}

#undef funlockfile
void
pd_funlockfile(FILE *rfp)
{
	FILEpd	*fp;

	fp = file2filepd(rfp);
	if (fp == NULL) {
#ifndef WIN32
		funlockfile(rfp);
#endif
	} else {
		pd_funlockfile_p(fp);
	}
	return;
}

/*
 * These are normally used through macros as defined below, but POSIX
 * requires functions as well.
 */
#undef getc_unlocked
#undef getc
int
pd_getc_unlocked(FILE *rfp)
{
	FILEpd	*fp;

	fp = file2filepd(rfp);
	if (fp == NULL) {
#ifdef WIN32
		return(getc(rfp));
#else
		return(getc_unlocked(rfp));
#endif
	}
	return(pd_getc_unlocked_p(fp));
}

#undef putc_unlocked
#undef putc
int
pd_putc_unlocked(int ch, FILE *rfp)
{
	FILEpd	*fp;

	fp = file2filepd(rfp);
	if (fp == NULL) {
#ifdef WIN32
		return(putc(ch, rfp));
#else
		return(putc_unlocked(ch, rfp));
#endif
	}
	return(pd_putc_unlocked_p(ch, fp));

}

#undef clearerr_unlocked
#undef clearerr
void
pd_clearerr_unlocked(FILE *rfp)
{
	FILEpd	*fp;

	fp = file2filepd(rfp);
	if (fp == NULL) {
#if defined(WIN32) || defined(__CYGWIN__)
		clearerr(rfp);
#else
		clearerr_unlocked(rfp);
#endif
	} else {
		pd_clearerr_unlocked_p(fp);
	}
	return;
}

#undef feof_unlocked
#undef feof
int
pd_feof_unlocked(FILE *rfp)
{
	FILEpd	*fp;

	fp = file2filepd(rfp);
	if (fp == NULL) {
#if defined(WIN32) || defined(__CYGWIN__)
		return(feof(rfp));
#else
		return(feof_unlocked(rfp));
#endif
	}
	return(pd_feof_unlocked_p(fp));
}

#undef ferror_unlocked
#undef ferror
int
pd_ferror_unlocked(FILE *rfp)
{
	FILEpd	*fp;

	fp = file2filepd(rfp);
	if (fp == NULL) {
#if defined(WIN32) || defined(__CYGWIN__)
		return(ferror(rfp));
#else
		return(ferror_unlocked(rfp));
#endif
	}
	return(pd_ferror_unlocked_p(fp));
}

#undef fileno_unlocked
#undef fileno
int
pd_fileno_unlocked(FILE *rfp)
{
	FILEpd	*fp;

	fp = file2filepd(rfp);
	if (fp == NULL) {
#if defined(WIN32) || defined(__CYGWIN__)
		return(fileno(rfp));
#else
		return(fileno_unlocked(rfp));
#endif
	}
	return(pd_fileno_unlocked_p(fp));
}

/* seeko/tello are special as the foo and _foo versions are different */
#undef fseeko
int
pd_fseeko(FILE *rfp, off_t off, int whence)
{
	FILEpd	*fp;

	fp = file2filepd(rfp);
	if (fp == NULL) {
#ifdef WIN32
#ifdef __MINGW32__
		return(fseeko64(rfp, off, whence));
#else
		return(fseek(rfp, off, whence));
#endif
#else
		return(fseeko(rfp, off, whence));
#endif
	}
	return(pd_fseeko_p(fp, off, whence));
}

#undef ftello
off_t
pd_ftello(FILE *rfp)
{
	FILEpd	*fp;

	fp = file2filepd(rfp);
	if (fp == NULL) {
#ifdef WIN32
#ifdef __MINGW32__
		return(ftello64(rfp));
#else
		return(ftell(rfp));
#endif
#else
		return(ftello(rfp));
#endif
	}
	return(pd_ftello_p(fp));
}

#undef getw
int
pd_getw(FILE *rfp)
{
	FILEpd	*fp;

	fp = file2filepd(rfp);
	if (fp == NULL) {
		return(getw(rfp));
	}
	return(pd_getw_p(fp));
}

#undef putw
int
pd_putw(int ch, FILE *rfp)
{
	FILEpd	*fp;

	fp = file2filepd(rfp);
	if (fp == NULL) {
		return(putw(ch, rfp));
	}
	return(pd_putw_p(ch, fp));
}

/*
 * Routines that are purely local.
 */


/*
 * Call our closeall then the real one.
 */
#undef fcloseall
void
pd_fcloseall(void)
{
	pd_fcloseall_p();
#ifdef WIN32
	_fcloseall();
#else
	fcloseall();
#endif
}

#undef fgetln
char *
pd_fgetln(FILE *rfp, size_t *len)
{
	FILEpd	*fp;

	fp = file2filepd(rfp);
	if (fp == NULL) {
#if 1
		errno = EDOM;
		return(NULL);
#else
		return(fgetln(rfp, len));
#endif
	}
	return(pd_fgetln_p(fp, len));
}

#undef fpurge
int
pd_fpurge(FILE *rfp)
{
	FILEpd	*fp;

	fp = file2filepd(rfp);
	if (fp != NULL) {
		return(pd_fpurge_p(fp));
	}
	/* Most platforms don't have fpurge, but dealt with in hacks.h */
	return(pdp_fpurge(rfp));

}

#undef setbuffer
void
pd_setbuffer(FILE *rfp, char *buf, int len)
{
	FILEpd	*fp;

	fp = file2filepd(rfp);
	if (fp == NULL) {
		setvbuf(rfp, buf, _IOFBF, len);
	} else {
		pd_setbuffer_p(fp, buf, len);
	}
	return;
}

#undef setlinebuf
int
pd_setlinebuf(FILE *rfp)
{
	FILEpd	*fp;

	fp = file2filepd(rfp);
	if (fp == NULL) {
		return(setvbuf(rfp, NULL, _IOLBF, 0));
	}
	return(pd_setlinebuf_p(fp));
}

/*
 * Stdio function-access interface.  No if here, this code soley exists
 * to provide this function when it's missing.
 */
#undef funopen
FILE	*pd_funopen(const void *cookie,
	    int (*readfn)(void *, char *, int),
	    int (*writefn)(void *, const char *, int),
	    fpos_t (*seekfn)(void *, fpos_t, int),
	    int (*closefn)(void *))
{
	FILEpd	*fp;

	fp = pd_funopen_p(cookie, readfn, writefn, seekfn, closefn);
	return(fp == NULL ? NULL : fp->_real.rfp);
}

#ifndef WIN32
#ifndef __linux__
#define GET_COOKIE(fp)		((fp)->_cookie)
#else
#define GET_COOKIE(fp)		(((void **)(fp + 1))[1])
#endif
#endif

void *
pd_fgetcookie(FILE *rfp)
{
	FILEpd	*fp;

	fp = file2filepd(rfp);
	if (fp == NULL) {
#ifdef WIN32
		return(NULL);
#else
		return(GET_COOKIE(rfp));
#endif
	}
	return(fp->_cookie);
}
