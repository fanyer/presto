#ifndef AUSTRINGUTILS_H
#define AUSTRINGUTILS_H

size_t u_strlen(const uni_char* s);

uni_char* u_strcpy(uni_char* dest, const uni_char* src);

uni_char* u_strncpy(uni_char* dest, const uni_char* src, size_t n);

uni_char* u_strchr(const uni_char* s, uni_char c0);

uni_char* u_strrchr(const uni_char* s, uni_char c);

int u_strncmp(const uni_char *s1_0, const uni_char *s2_0, size_t n);

uni_char* u_strcat(uni_char* dest, const uni_char* src);

UINT32 u_str_to_uint32(const uni_char* str);

OP_STATUS u_uint32_to_str(uni_char* str, size_t count, int number);

#endif // AUSTRINGUTILS_H
