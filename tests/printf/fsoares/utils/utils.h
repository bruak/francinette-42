/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   utils.h                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: fsoares- <fsoares-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2022/01/14 13:40:02 by fsoares-          #+#    #+#             */
/*   Updated: 2022/02/11 18:11:04 by fsoares-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef UTILS_H
#define UTILS_H

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <signal.h>
#include <stdarg.h>
#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <stdbool.h>
#include <time.h>

#include "color.h"

#define MEM_SIZE 0x100
#define REPETITIONS 1000

extern char function[1000];
extern char signature[10000];
extern int g_offset;
extern char escaped[1000];
extern FILE *errors_file;
extern int g_test;
extern int child_pid;

#ifdef STRICT_MEM
#define null_check(fn_call, rst)                                                                    \
	reset_malloc_mock();                                                                            \
	fn_call;                                                                                        \
	int malloc_calls = reset_malloc_mock();                                                         \
	for (int i = 0; i < malloc_calls; i++)                                                          \
	{                                                                                               \
		sprintf(signature + g_offset, MAG " malloc " NC "protection check for %ith malloc", i + 1); \
		malloc_set_null(i);                                                                         \
		void *res = fn_call;                                                                        \
		rst = check_leaks(res) && rst;                                                              \
		if (res != NULL)                                                                            \
			rst = error("Should return NULL\n");                                                    \
	}

#define null_null_check(fn_call, rst)                                                               \
	reset_malloc_mock();                                                                            \
	fn_call;                                                                                        \
	int malloc_calls = reset_malloc_mock();                                                         \
	for (int i = 0; i < malloc_calls; i++)                                                          \
	{                                                                                               \
		sprintf(signature + g_offset, MAG " malloc" NC " protection check for %ith malloc", i + 1); \
		fn_call;                                                                                    \
		malloc_set_null(i);                                                                         \
		rst = check_leaks(NULL) && rst;                                                             \
	}
#else
#define null_check(fn_call, result)
#define null_null_check(fn_call, result)
#endif

/**
 * @brief given a function call that returns an allocated string and the
 * expected return value, this macro will check that the string returned
 * was the one expected as well as that there are no leaks and that it
 * correctly handles allocation errors
 */
#define check_alloc_str_return(fn_call, exp)                 \
	int result = 1;                                          \
	char *res = fn_call;                                     \
	result = same_string(exp, res);                          \
	result = check_mem_size(res, strlen(exp) + 1) && result; \
	result = check_leaks(res) && result;                     \
	null_check(fn_call, result);                             \
	return result;

#define BASE_TEST(title, code)                            \
	{                                                     \
		int status = 0;                                   \
		int test = fork();                                \
		if (test == 0)                                    \
		{                                                 \
			code;                                         \
		}                                                 \
		else                                              \
		{                                                 \
			long total = 0;                               \
			long interval = 50000;                        \
			while (total < TIMEOUT * 1000000)             \
			{                                             \
				usleep(interval);                         \
				int c = waitpid(test, &status, WNOHANG);  \
				if (c != 0 && WIFEXITED(status))          \
				{                                         \
					if (WEXITSTATUS(status) != 0)         \
						add_to_error_file(title);         \
					break;                                \
				}                                         \
				total += interval;                        \
			}                                             \
			if (total >= TIMEOUT * 1000000)               \
			{                                             \
				if (waitpid(test, &status, WNOHANG) == 0) \
				{                                         \
					kill(test, SIGKILL);                  \
					show_timeout();                       \
				}                                         \
			}                                             \
		}                                                 \
	}

/**
 * @brief Macro that wraps a get_next_line_test
 */
#define TEST(title, code)                       \
	BASE_TEST(title, {                          \
		g_test = 1;                             \
		alarm(TIMEOUT);                         \
		char *_title = title;                   \
		printf(BLU "%-20s" NC ": ", _title);    \
		fflush(stdout);                         \
		int res = 1;                            \
		errors_file = fopen("errors.log", "w"); \
		reset_malloc_mock();                    \
		code;                                   \
		res = leak_check() && res;              \
		res = null_check_gnl(_title) && res;    \
		fclose(errors_file);                    \
		printf("\n");                           \
		if (res)                                \
			exit(EXIT_SUCCESS);                 \
		else                                    \
			exit(1);                            \
	})

#define test_gnl(fd, expected) res = test_gnl_func(fd, expected, _title) && res;

void show_timeout();
void handle_signals();

void print_mem(void *ptr, int size);
void print_mem_full(void *ptr, int size);

char *rand_bytes(char *dest, int len);
char *rand_str(char *dest, int len);
char *escape_str(char *src);
char *escape_chr(char ch);
void reset(void *m1, void *m2, int size);
void reset_with(void *m1, void *m2, char *content, int size);

int set_signature(const char *format, ...);
int error(const char *format, ...);
void add_to_error_file();

int same_ptr(void *res, void *res_std);
int same_mem(void *expected, void *result, int size);
int same_value(int expected, int res);
int same_sign(int expected, int res);
int same_offset(void *expected_start, void *expected_res, void *start, void *res);
int same_return(void *expected, void *res);
int same_size(void *ptr, void *ptr_std);
int same_string(char *expected, char *actual);
char *my_strdup(const char *s1);
char *my_strndup(const char *s1, size_t size);
/**
 * @brief In normal mode makes sure that you reserved enough space.
 * In strict makes sure that you reserved the correct amount of space.
 *
 * @param ptr The pointer to check how much memory was allocated
 * @param expected_size The expected allocated size
 * @return If it passes or fails the test
 */
int check_mem_size(void *ptr, size_t expected_size);

int reset_malloc_mock();
size_t get_malloc_size(void *ptr);
void malloc_set_result(void *res);
void malloc_set_null(int nth);
int check_leaks(void *ptr);
void print_mallocs();

/* for file tester */
int check_res(int res, char *prefix);
int check_alloc(char *next, char *expected);
int leak_check();
int test_gnl_func(int fd, char *expected, char *input);
int silent_gnl_test(int fd, char *expected);
int null_check_gnl(char *file);

#ifndef __APPLE__
size_t strlcat(char *dst, const char *src, size_t size);
size_t strlcpy(char *dst, const char *src, size_t size);
char *strnstr(const char *haystack, const char *needle, size_t len);
#endif

#endif