/*
 * Let libc include newer functions (pread in particular), and use
 * GNU-specific versions of some functions (like strerror_r).
 */
#define _GNU_SOURCE