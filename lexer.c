// Legacy BASIC
// Copyright (c) 2022 Nigel Perks

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <assert.h>
#include "lexer.h"
#include "token.h"
#include "utils.h"

LEX* new_lex(const SOURCE* source, bool recognise_keyword_prefixes) {
  assert(source != NULL);
  LEX* lex = emalloc(sizeof *lex);
  lex->source = source;
  lex->line = 0;
  lex->text = NULL;
  lex->pos = 0;
  lex->token_pos = 0;
  lex->token = TOK_NONE;
  lex->num = 0;
  lex->word[0] = '\0';
  lex->recognise_keyword_prefixes = recognise_keyword_prefixes;
  return lex;
}

void delete_lex(LEX* lex) {
  efree(lex);
}

void lex_line(LEX* lex, unsigned line) {
  assert(lex != NULL);
  assert(line < source_lines(lex->source));
  lex->line = line;
  lex->text = source_text(lex->source, line);
  lex->pos = 0;
  lex_next(lex);
}

static void lex_error(LEX* lex, const char* fmt, ...) {
  assert(lex != NULL && lex->source != NULL);
  fprintf(stderr, "%s(%u): ", source_name(lex->source), lex->line + 1);
  fprintf(stderr, "%u %s\n", source_linenum(lex->source, lex->line), source_text(lex->source, lex->line));
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  exit(EXIT_FAILURE);
}

static int lex_char_pos(LEX* lex, unsigned *pos) {
  assert(lex != NULL);
  assert(pos != NULL);

  if (lex->text == NULL) {
    *pos = 0;
    return EOF;
  }

  *pos = lex->pos;

  if (lex->text[lex->pos] == '\0')
    return '\n';

  return lex->text[lex->pos++];
}

static int lex_char(LEX* lex) {
  assert(lex != NULL);

  if (lex->text == NULL)
    return EOF;

  if (lex->text[lex->pos] == '\0')
    return '\n';

  return lex->text[lex->pos++];
}

static int lex_peek(LEX* lex) {
  assert(lex != NULL);

  if (lex->text == NULL)
    return EOF;

  if (lex->text[lex->pos] == '\0')
    return '\n';

  return lex->text[lex->pos];
}

void lex_discard(LEX* lex) {
  assert(lex != NULL);

  if (lex->text)
    lex->token_pos = lex->pos = (unsigned) strlen(lex->text);
  else
    lex->token_pos = lex->pos = 0;

  lex->token = '\n';
}

static int chr(int c) {
  return isprint(c) ? c : '?';
}

static void pushback(LEX* lex, int c) {
  if (lex->text == NULL)
    lex_error(lex, "internal error: invalid pushback\n");
  if (c == '\n' && lex->text[lex->pos] == '\0')
    return;
  if (lex->pos == 0)
    lex_error(lex, "internal error: invalid pushback\n");
  lex->pos--;
  if (lex->text[lex->pos] != c)
    lex_error(lex, "internal error: pushback: attempted '%c' 0x%02x, found '%c' 0x%02x\n",
        chr(c), c, chr(lex->text[lex->pos]), lex->text[lex->pos]);
}

unsigned lex_line_num(LEX* lex) {
  assert(lex != NULL);
  return source_linenum(lex->source, lex->line);
}

const char* lex_line_text(LEX* lex) {
  assert(lex != NULL);
  return source_text(lex->source, lex->line);
}

static void append(LEX* lex, unsigned *i, int c, const char* descrip) {
  if (*i + 1 >= sizeof lex->word) {
    lex->word[*i] = '\0';
    lex_error(lex, "%s is too long: %s...", descrip, lex->word);
  }
  lex->word[*i] = c;
  ++*i;
}

int lex_next(LEX* lex) {
  unsigned pos;
  int c = lex_char_pos(lex, &pos);

  while (c == ' ' || c == '\t')
    c = lex_char_pos(lex, &pos);

  lex->token_pos = pos;

  if (isalpha(c)) {
    if (lex->recognise_keyword_prefixes) {
      pushback(lex, c);
      const KEYWORD* kw = keyword_prefix(lex->text + lex->pos);
      if (kw) {
        strcpy(lex->word, kw->name);
        lex->pos += kw->len;
        return lex->token = kw->token;
      }
      unsigned i = 0;
      lex->word[i++] = lex_char(lex);
      while (isalnum(c = lex_peek(lex))) {
        if (isalpha(c) && (keyword_prefix(lex->text + lex->pos))) {
          lex->word[i] = '\0';
          return lex->token = TOK_ID;
        }
        if (i + 2 >= sizeof lex->word) {
          lex->word[i] = '\0';
          lex_error(lex, "name is too long: %s...", lex->word);
        }
        lex->word[i++] = lex_char(lex);
      }
      if (c == '$')
        lex->word[i++] = lex_char(lex);
      lex->word[i] = '\0';
      return lex->token = TOK_ID;
    }

    unsigned i = 0;
    lex->word[i++] = c;
    while (isalnum(c = lex_char(lex))) {
      if (i + 2 >= sizeof lex->word) {
        lex->word[i] = '\0';
        lex_error(lex, "a keyword or name is too long: %s...", lex->word);
      }
      lex->word[i++] = c;
    }
    if (c == '$')
      lex->word[i++] = c;
    else
      pushback(lex, c);
    lex->word[i] = '\0';
    return lex->token = identifier_token(lex->word);
  }

  if (isdigit(c) || c == '.') {
    static const char NUMBER[] = "number";
    unsigned i = 0;
    if (isdigit(c)) {
      do {
        append(lex, &i, c, NUMBER);
      } while (isdigit(c = lex_char(lex)));
    }
    if (c == '.') {
      append(lex, &i, c, NUMBER);
      while (isdigit(c = lex_char(lex)))
        append(lex, &i, c, NUMBER);
    }
    if (tolower(c) == 'e') {
      append(lex, &i, c, NUMBER);
      if ((c = lex_char(lex)) == '-') {
        append(lex, &i, c, NUMBER);
        c = lex_char(lex);
      }
      while (isdigit(c)) {
        append(lex, &i, c, NUMBER);
        c = lex_char(lex);
      }
    }
    pushback(lex, c);
    lex->word[i] = '\0';
    char* end = NULL;
    lex->num = strtod(lex->word, &end);
    if (*end)
      lex_error(lex, "invalid number: %s\n", lex->word);
    return lex->token = TOK_NUM;
  }

  if (c == '\"') {
    unsigned i = 0;
    while ((c = lex_char(lex)) != '\"' && c != '\n' && c != EOF) {
      if (i + 1 >= sizeof lex->word) {
        lex->word[i] = '\0';
        lex_error(lex, "a string is too long: \"%s...", lex->word);
      }
      lex->word[i++] = c;
    }
    lex->word[i] = '\0';
    if (c != '\"')
      lex_error(lex, "unterminated string: \"%s...", lex->word);
    return lex->token = TOK_STR;
  }

  if (c == '<') {
    if ((c = lex_char(lex)) == '>')
      return lex->token = TOK_NE;
    if (c == '=')
      return lex->token = TOK_LE;
    pushback(lex, c);
    return lex->token = '<';
  }

  if (c == '>') {
    if ((c = lex_char(lex)) == '=')
      return lex->token = TOK_GE;
    pushback(lex, c);
    return lex->token = '>';
  }

  return lex->token = (c == EOF ? TOK_EOF : c);
}

// Recognise a string token, or read other characters into a string, for untyped DATA.
const char* lex_next_data(LEX* lex) {
  unsigned pos;
  int c = lex_char_pos(lex, &pos);

  while (c == ' ' || c == '\t')
    c = lex_char_pos(lex, &pos);

  lex->token_pos = pos;

  unsigned i = 0;
  if (c == '\"') {
    while ((c = lex_char(lex)) != '\"' && c != '\n' && c != EOF) {
      if (i + 1 >= sizeof lex->word) {
        lex->word[i] = '\0';
        lex_error(lex, "a string is too long: \"%s...", lex->word);
      }
      lex->word[i++] = c;
    }
    if (c != '\"')
      lex_error(lex, "unterminated string: \"%s...", lex->word);
  }
  else {
    while (c != '\"' && c != ',' && c != ':' && c != '\n' && c != EOF) {
      if (i + 1 >= sizeof lex->word) {
        lex->word[i] = '\0';
        lex_error(lex, "data is too long: %s...", lex->word);
      }
      lex->word[i++] = c;
      c = lex_char(lex);
    }
    pushback(lex, c);
    // trim trailing space otherwise it is inconsistent trimming leading space only
    while (i > 0 && (lex->word[i-1] == ' ' || lex->word[i-1] == '\t'))
      i--;
  }
  lex->word[i] = '\0';

  return lex->word;
}

int lex_token(LEX* lex) {
  assert(lex != NULL);
  return lex->token;
}

const char* lex_word(LEX* lex) {
  assert(lex != NULL);
  return lex->word;
}

double lex_num(LEX* lex) {
  assert(lex != NULL);
  return lex->num;
}

void print_lex_token(LEX* lex, FILE* fp) {
  assert(lex != NULL);

  switch (lex->token) {
    case TOK_ID:
      fprintf(fp, "name: %s", lex->word);
      return;
    case TOK_NUM:
      fprintf(fp, "number: %g", lex->num);
      return;
    case TOK_STR:
      fprintf(fp, "string: \"%s\"", lex->word);
      return;
  }

  print_token(lex->token, fp);
}

unsigned lex_token_pos(LEX* lex) {
  assert(lex != NULL);
  return lex->token_pos;
}


#ifdef UNIT_TEST

#include "CuTest.h"

static void test_new(CuTest* tc) {
  SOURCE source;
  LEX* lex;

  memset(&source, 0, sizeof source);

  lex = new_lex(&source, true);
  CuAssertPtrNotNull(tc, lex);
  CuAssertTrue(tc, lex->source == &source);
  CuAssertIntEquals(tc, 0, lex->line);
  CuAssertTrue(tc, lex->text == NULL);
  CuAssertIntEquals(tc, 0, lex->pos);
  CuAssertIntEquals(tc, 0, lex->token_pos);
  CuAssertIntEquals(tc, TOK_NONE, lex->token);
  CuAssertDblEquals(tc, 0, lex->num, 0);
  CuAssertIntEquals(tc, '\0', lex->word[0]);
  CuAssertIntEquals(tc, true, lex->recognise_keyword_prefixes);

  delete_lex(lex);
}

static void test_char(CuTest* tc) {
  static const char CODE[] = "10 abc";
  SOURCE* source = load_source_string(CODE, "test");
  LEX* lex = new_lex(source, false);
  unsigned pos;
  int c;

  lex->line = 0;
  lex->text = source_text(lex->source, 0);
  lex->pos = 0;

  CuAssertIntEquals(tc, 'a', lex_peek(lex));
  c = lex_char_pos(lex, &pos);
  CuAssertIntEquals(tc, 'a', c);
  CuAssertIntEquals(tc, 0, pos);
  CuAssertIntEquals(tc, 1, lex->pos);

  CuAssertIntEquals(tc, 'b', lex_peek(lex));
  c = lex_char_pos(lex, &pos);
  CuAssertIntEquals(tc, 'b', c);
  CuAssertIntEquals(tc, 1, pos);
  CuAssertIntEquals(tc, 2, lex->pos);

  pushback(lex, 'b');
  CuAssertIntEquals(tc, 1, lex->pos);
  CuAssertIntEquals(tc, 'b', lex_char(lex));

  CuAssertIntEquals(tc, 'c', lex_peek(lex));
  c = lex_char(lex);
  CuAssertIntEquals(tc, 'c', c);
  CuAssertIntEquals(tc, 3, lex->pos);

  CuAssertIntEquals(tc, '\n', lex_peek(lex));
  c = lex_char(lex);
  CuAssertIntEquals(tc, '\n', c);
  CuAssertIntEquals(tc, 3, lex->pos);

  CuAssertIntEquals(tc, '\n', lex_peek(lex));
  c = lex_char(lex);
  CuAssertIntEquals(tc, '\n', c);
  CuAssertIntEquals(tc, 3, lex->pos);

  pushback(lex, '\n');
  CuAssertIntEquals(tc, 3, lex->pos);
  CuAssertIntEquals(tc, '\n', lex_char(lex));
  CuAssertIntEquals(tc, 3, lex->pos);

  delete_lex(lex);
  delete_source(source);
}

static void test_lexer(CuTest* tc) {
  static const char CODE[] =
      "10 REM test program\n"
      "20 PRINT 3.14+ab12$and<>data<=>=\"scrambled\":\n"
      ;
  SOURCE* source = load_source_string(CODE, "test");

  LEX* lex = new_lex(source, false);

  CuAssertIntEquals(tc, TOK_NONE, lex_token(lex));

  // append()
  memset(lex->word, 0, sizeof lex->word);
  unsigned i = 0;
  append(lex, &i, '*', "test");
  CuAssertIntEquals(tc, '*', lex->word[0]);
  CuAssertIntEquals(tc, 1, i);

  // 10 REM test program
  lex_line(lex, 0);
  CuAssertIntEquals(tc, 10, lex_line_num(lex));
  CuAssertStrEquals(tc, "REM test program", lex_line_text(lex));
  CuAssertIntEquals(tc, TOK_REM, lex_token(lex));
  CuAssertIntEquals(tc, 0, lex->token_pos);
  lex_discard(lex);
  CuAssertIntEquals(tc, '\n', lex_token(lex));
  CuAssertStrEquals(tc, "REM test program", lex_line_text(lex));

  // 20 PRINT 3.14+...
  lex_line(lex, 1);
  CuAssertIntEquals(tc, 20, lex_line_num(lex));
  CuAssertIntEquals(tc, TOK_PRINT, lex_token(lex));
  CuAssertIntEquals(tc, 0, lex_token_pos(lex));

  CuAssertIntEquals(tc, TOK_NUM, lex_next(lex));
  CuAssertIntEquals(tc, TOK_NUM, lex_token(lex));
  CuAssertDblEquals(tc, 3.14, lex_num(lex), 0);
  CuAssertIntEquals(tc, 6, lex_token_pos(lex));

  CuAssertIntEquals(tc, '+', lex_peek(lex));
  CuAssertIntEquals(tc, '+', lex_peek(lex));
  CuAssertIntEquals(tc, '+', lex_char(lex));
  CuAssertIntEquals(tc, 'a', lex_peek(lex));
  pushback(lex, '+');
  CuAssertIntEquals(tc, '+', lex_peek(lex));
  CuAssertIntEquals(tc, '+', lex_next(lex));

  CuAssertIntEquals(tc, TOK_ID, lex_next(lex));
  CuAssertStrEquals(tc, "ab12$", lex_word(lex));

  CuAssertIntEquals(tc, TOK_AND, lex_next(lex));
  CuAssertIntEquals(tc, TOK_NE, lex_next(lex));
  CuAssertIntEquals(tc, TOK_DATA, lex_next(lex));
  CuAssertIntEquals(tc, TOK_LE, lex_next(lex));
  CuAssertIntEquals(tc, TOK_GE, lex_next(lex));

  CuAssertIntEquals(tc, TOK_STR, lex_next(lex));
  CuAssertStrEquals(tc, "scrambled", lex_word(lex));

  delete_lex(lex);
  delete_source(source);
}

static void test_number(CuTest* tc) {
  static const char CODE[] = "10 PRINT 123,123.,123.45,.45,123e-7,.45e12,123.45e-7\n";
  SOURCE* source = load_source_string(CODE, "test");
  LEX* lex = new_lex(source, false);
  lex_line(lex, 0);

  // PRINT
  CuAssertIntEquals(tc, TOK_PRINT, lex->token);

  // 123
  CuAssertIntEquals(tc, TOK_NUM, lex_next(lex));
  CuAssertDblEquals(tc, 123, lex->num, 0);
  CuAssertIntEquals(tc, ',', lex_next(lex));

  // 123.
  CuAssertIntEquals(tc, TOK_NUM, lex_next(lex));
  CuAssertDblEquals(tc, 123, lex->num, 0);
  CuAssertIntEquals(tc, ',', lex_next(lex));

  // 123.45
  CuAssertIntEquals(tc, TOK_NUM, lex_next(lex));
  CuAssertDblEquals(tc, 123.45, lex->num, 0);
  CuAssertIntEquals(tc, ',', lex_next(lex));

  // .45
  CuAssertIntEquals(tc, TOK_NUM, lex_next(lex));
  CuAssertDblEquals(tc, 0.45, lex->num, 0);
  CuAssertIntEquals(tc, ',', lex_next(lex));

  // 123e-7
  CuAssertIntEquals(tc, TOK_NUM, lex_next(lex));
  CuAssertDblEquals(tc, 123e-7, lex->num, 0);
  CuAssertIntEquals(tc, ',', lex_next(lex));

  // .45e12
  CuAssertIntEquals(tc, TOK_NUM, lex_next(lex));
  CuAssertDblEquals(tc, 0.45e12, lex->num, 0);
  CuAssertIntEquals(tc, ',', lex_next(lex));

  // 123.45e-7
  CuAssertIntEquals(tc, TOK_NUM, lex_next(lex));
  CuAssertDblEquals(tc, 123.45e-7, lex->num, 0);
  CuAssertIntEquals(tc, '\n', lex_next(lex));

  delete_lex(lex);
  delete_source(source);
}

static void test_keyword_recognition(CuTest* tc) {
  static const char CODE[] =
      // line number
      "10 "
      // standalone keyword
      "print "
      // keyword prefix
      "printable "
      // keyword in the middle
      "before "
      // string, numeric identifier
      "abc$abc "
      // keyword + $
      "for$ "
      ;

  SOURCE* source = load_source_string(CODE, "test");

  // recognise keywords only when properly delimited
  LEX* lex = new_lex(source, false);
  lex_line(lex, 0);
  CuAssertIntEquals(tc, TOK_PRINT, lex_token(lex));

  CuAssertIntEquals(tc, TOK_ID, lex_next(lex));
  CuAssertStrEquals(tc, "printable", lex_word(lex));

  CuAssertIntEquals(tc, TOK_ID, lex_next(lex));
  CuAssertStrEquals(tc, "before", lex_word(lex));

  CuAssertIntEquals(tc, TOK_ID, lex_next(lex));
  CuAssertStrEquals(tc, "abc$", lex_word(lex));

  CuAssertIntEquals(tc, TOK_ID, lex_next(lex));
  CuAssertStrEquals(tc, "abc", lex_word(lex));

  CuAssertIntEquals(tc, TOK_ID, lex_next(lex));
  CuAssertStrEquals(tc, "for$", lex_word(lex));

  delete_lex(lex);

  // recognise keywords anywhere
  lex = new_lex(source, true);
  lex_line(lex, 0);
  CuAssertIntEquals(tc, TOK_PRINT, lex_token(lex));

  // printable
  CuAssertIntEquals(tc, TOK_PRINT, lex_next(lex));
  CuAssertIntEquals(tc, TOK_ID, lex_next(lex));
  CuAssertStrEquals(tc, "able", lex_word(lex));

  // before
  CuAssertIntEquals(tc, TOK_ID, lex_next(lex));
  CuAssertStrEquals(tc, "be", lex_word(lex));
  CuAssertIntEquals(tc, TOK_FOR, lex_next(lex));
  CuAssertIntEquals(tc, TOK_ID, lex_next(lex));
  CuAssertStrEquals(tc, "e", lex_word(lex));

  // abc$abc
  CuAssertIntEquals(tc, TOK_ID, lex_next(lex));
  CuAssertStrEquals(tc, "abc$", lex_word(lex));

  CuAssertIntEquals(tc, TOK_ID, lex_next(lex));
  CuAssertStrEquals(tc, "abc", lex_word(lex));

  // for$
  CuAssertIntEquals(tc, TOK_FOR, lex_next(lex));
  CuAssertIntEquals(tc, '$', lex_next(lex));

  delete_lex(lex);

  delete_source(source);
}

static void test_data(CuTest* tc) {
  static const char CODE[] = "10 DATA \"  delimited string  \",  3.14  ,   Cabbages!  \n";
  SOURCE* source = load_source_string(CODE, "test");
  LEX* lex = new_lex(source, false);
  lex_line(lex, 0);
  CuAssertIntEquals(tc, TOK_DATA, lex->token);
  const char* data;

  data = lex_next_data(lex);
  CuAssertTrue(tc, data == lex->word);
  CuAssertStrEquals(tc, "  delimited string  ", data);
  CuAssertIntEquals(tc, 5, lex_token_pos(lex));
  CuAssertIntEquals(tc, ',', lex_next(lex));

  data = lex_next_data(lex);
  CuAssertTrue(tc, data == lex->word);
  CuAssertStrEquals(tc, "3.14", data);
  CuAssertIntEquals(tc, 30, lex_token_pos(lex));
  CuAssertIntEquals(tc, ',', lex_next(lex));

  data = lex_next_data(lex);
  CuAssertTrue(tc, data == lex->word);
  CuAssertStrEquals(tc, "Cabbages!", data);
  CuAssertIntEquals(tc, 40, lex_token_pos(lex));

  delete_lex(lex);
  delete_source(source);
}

CuSuite* lexer_test_suite(void) {
  CuSuite* suite = CuSuiteNew();
  SUITE_ADD_TEST(suite, test_new);
  SUITE_ADD_TEST(suite, test_char);
  SUITE_ADD_TEST(suite, test_lexer);
  SUITE_ADD_TEST(suite, test_number);
  SUITE_ADD_TEST(suite, test_keyword_recognition);
  SUITE_ADD_TEST(suite, test_data);
  return suite;
}

#endif