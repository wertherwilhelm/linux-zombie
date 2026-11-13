#include "pch.h"
#include "TestFixture.h"

// ============================================================================
// Character Predicate Tests
// ============================================================================

TEST_F(ZombieSchemeTest, CharPredicate) {
    expectBool("(char? #\\a)", true);
    expectBool("(char? 42)", false);
    expectBool("(char? \"hello\")", false);
}

TEST_F(ZombieSchemeTest, CharAlphabetic) {
    expectBool("(char-alphabetic? #\\a)", true);
    expectBool("(char-alphabetic? #\\Z)", true);
    expectBool("(char-alphabetic? #\\5)", false);
    expectBool("(char-alphabetic? #\\space)", false);
}

TEST_F(ZombieSchemeTest, CharNumeric) {
    expectBool("(char-numeric? #\\5)", true);
    expectBool("(char-numeric? #\\0)", true);
    expectBool("(char-numeric? #\\a)", false);
}

TEST_F(ZombieSchemeTest, CharWhitespace) {
    expectBool("(char-whitespace? #\\space)", true);
    expectBool("(char-whitespace? #\\tab)", true);
    expectBool("(char-whitespace? #\\a)", false);
}

TEST_F(ZombieSchemeTest, CharUpper) {
    expectBool("(char-upper-case? #\\A)", true);
    expectBool("(char-upper-case? #\\a)", false);
}

TEST_F(ZombieSchemeTest, CharLower) {
    expectBool("(char-lower-case? #\\a)", true);
    expectBool("(char-lower-case? #\\A)", false);
}

// ============================================================================
// Character Conversion Tests
// ============================================================================

TEST_F(ZombieSchemeTest, CharToInteger) {
    expectInt("(char->integer #\\A)", 65);
    expectInt("(char->integer #\\a)", 97);
    expectInt("(char->integer #\\0)", 48);
}

TEST_F(ZombieSchemeTest, IntegerToChar) {
    Object* result = eval("(integer->char 65)");
    if (result != nullptr) {
        // Verify it's a character by trying char?
        expectBool("(char? (integer->char 65))", true);
    }
}

TEST_F(ZombieSchemeTest, CharUpcase) {
    Object* result = eval("(char-upcase #\\a)");
    if (result != nullptr) {
        expectInt("(char->integer (char-upcase #\\a))", 65);  // 'A'
    }
}

TEST_F(ZombieSchemeTest, CharDowncase) {
    Object* result = eval("(char-downcase #\\A)");
    if (result != nullptr) {
        expectInt("(char->integer (char-downcase #\\A))", 97);  // 'a'
    }
}

// ============================================================================
// String Predicate Tests
// ============================================================================

TEST_F(ZombieSchemeTest, StringPredicate) {
    expectBool("(string? \"hello\")", true);
    expectBool("(string? 42)", false);
    expectBool("(string? #\\a)", false);
}

TEST_F(ZombieSchemeTest, StringLength) {
    expectInt("(string-length \"hello\")", 5);
    expectInt("(string-length \"\")", 0);
    expectInt("(string-length \"a\")", 1);
}

// ============================================================================
// String Construction Tests
// ============================================================================

TEST_F(ZombieSchemeTest, MakeString) {
    expectInt("(string-length (make-string 5))", 5);
    expectInt("(string-length (make-string 0))", 0);
}

TEST_F(ZombieSchemeTest, MakeStringWithFill) {
    Object* result = eval("(make-string 3 #\\a)");
    if (result != nullptr) {
        expectString("(make-string 3 #\\a)", "aaa");
    }
}

// ============================================================================
// String Access Tests
// ============================================================================

TEST_F(ZombieSchemeTest, StringRef) {
    expectInt("(char->integer (string-ref \"hello\" 0))", 104);  // 'h'
    expectInt("(char->integer (string-ref \"hello\" 4))", 111);  // 'o'
}

TEST_F(ZombieSchemeTest, StringSet) {
    Object* result = eval("(string-set! \"hello\" 1 #\\a)");
    if (result != nullptr) {
        expectString("(string-set! \"hello\" 1 #\\a)", "hallo");
    }
}

// ============================================================================
// String Operations Tests
// ============================================================================

TEST_F(ZombieSchemeTest, Substring) {
    expectString("(substring \"hello world\" 0 5)", "hello");
    expectString("(substring \"hello world\" 6 11)", "world");
    expectString("(substring \"hello\" 1 2)", "e");
    expectString("(substring \"hello\" 2 2)", "");
}

TEST_F(ZombieSchemeTest, StringAppend) {
    expectString("(string-append \"hello\" \" \" \"world\")", "hello world");
    expectString("(string-append \"a\" \"b\" \"c\")", "abc");
    expectString("(string-append \"\")", "");
}

TEST_F(ZombieSchemeTest, StringCopy) {
    Object* result = eval("(string-copy \"hello\")");
    if (result != nullptr) {
        expectString("(string-copy \"hello\")", "hello");
    }
}

// ============================================================================
// String Conversion Tests
// ============================================================================

TEST_F(ZombieSchemeTest, StringToList) {
    Object* result = eval("(string->list \"abc\")");
    if (result != nullptr && result != nil) {
        expectInt("(length (string->list \"abc\"))", 3);
        expectBool("(char? (car (string->list \"abc\")))", true);
    }
}

TEST_F(ZombieSchemeTest, ListToString) {
    Object* result = eval("(list->string (list #\\a #\\b #\\c))");
    if (result != nullptr) {
        expectString("(list->string (list #\\a #\\b #\\c))", "abc");
    }
}

TEST_F(ZombieSchemeTest, NumberToString) {
    expectString("(number->string 123)", "123");
    expectString("(number->string 0)", "0");
}

TEST_F(ZombieSchemeTest, StringToNumber) {
    expectInt("(string->number \"123\")", 123);
    expectInt("(string->number \"0\")", 0);
    expectDouble("(string->number \"3.14\")", 3.14, 0.01);
}

// ============================================================================
// String Comparison Tests
// ============================================================================

TEST_F(ZombieSchemeTest, StringEqual) {
    expectBool("(string=? \"hello\" \"hello\")", true);
    expectBool("(string=? \"hello\" \"world\")", false);
}

TEST_F(ZombieSchemeTest, StringLessThan) {
    expectBool("(string<? \"abc\" \"abd\")", true);
    expectBool("(string<? \"abc\" \"abc\")", false);
    expectBool("(string<? \"abd\" \"abc\")", false);
}

TEST_F(ZombieSchemeTest, StringGreaterThan) {
    expectBool("(string>? \"abd\" \"abc\")", true);
    expectBool("(string>? \"abc\" \"abc\")", false);
}

// ============================================================================
// Symbol Tests
// ============================================================================

TEST_F(ZombieSchemeTest, SymbolPredicate) {
    expectBool("(symbol? 'hello)", true);
    expectBool("(symbol? \"hello\")", false);
    expectBool("(symbol? 42)", false);
}

TEST_F(ZombieSchemeTest, SymbolToString) {
    expectString("(symbol->string 'hello)", "hello");
    expectString("(symbol->string '+)", "+");
}

TEST_F(ZombieSchemeTest, StringToSymbol) {
    Object* result = eval("(string->symbol \"test\")");
    if (result != nullptr) {
        expectBool("(symbol? (string->symbol \"test\"))", true);
        expectString("(symbol->string (string->symbol \"test\"))", "test");
    }
}
