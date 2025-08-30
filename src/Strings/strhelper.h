#pragma once

int CompareLiteral(const char*, const char*);
int BeginsWith(const char*, const char*);
int Contains(const char*, const char*);
int is_space(unsigned char);
char* ltrim(char*);
void rtrim(char*);
char* trim(char*);
char* trim_after_prefix(char*, const char*);
const char* after_first_space_inplace(char*);
const char* first_word_inplace(char*);
int has_text(const char*);