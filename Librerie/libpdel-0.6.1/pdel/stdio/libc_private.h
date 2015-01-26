/*
 * Selected libc_private.h bits for the stdio hack 
 */

/*
 * File lock contention is difficult to diagnose without knowing
 * where locks were set. Allow a debug library to be built which
 * records the source file and line number of each lock call.
 */
#ifdef  _FLOCK_DEBUG
#define _FLOCKFILE(x)   _flockfile_debug(x, __FILE__, __LINE__)
#else
#define _FLOCKFILE(x)   _flockfile(x)
#endif

/*
 * Macros for locking and unlocking FILEs. These test if the
 * process is threaded to avoid locking when not required.
 */
#define FLOCKFILE(fp)           if (__isthreaded) _FLOCKFILE(fp)
#define FUNLOCKFILE(fp)         if (__isthreaded) _funlockfile(fp)

/*
 * This global flag is non-zero when a process has created one
 * or more threads. It is used to avoid calling locking functions
 * when they are not required.
 */
PD_IMPORT int      __isthreaded;

