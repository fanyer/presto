#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"
#include "modules/ecmascript/carakan/src/compiler/es_lexer.h"
#include "modules/util/simset.h"

Opera* g_opera;
OpSystemInfo* g_op_system_info;
OpTimeInfo* g_op_time_info;

class TestLineParser
{
public:
    TestLineParser(const char* str);

    void SkipSpaces();
    BOOL EatSpace();
    BOOL EatString(const char* s);
    BOOL AtEnd();
    BOOL FindWord(char* s);
    BOOL IsString();
    BOOL IsNumber();
    void CreateString(uni_char* s);
    double ConvertNumber();

private:
    BOOL IsSpace(char c);
    BOOL CompareWith(const char* s);
    const char* str;
    int i;
};

class TestParser
{
public:
    TestParser(FILE* f);
    BOOL RunTest();

private:
    BOOL NextLine();
    ES_Token::Keyword TranslateKeyword(const char* keyword);
    ES_Token::Punctuator TranslatePunctuator(const char* punctuator);
    BOOL IsLineEmpty();
    BOOL TestLine();
    BOOL IsTestFragment();
    BOOL IsBeginIndata();
    BOOL IsBeginResult();
    BOOL IsEnd();
    BOOL IsTokenEnd();
    BOOL IsTokenInvalid();
    BOOL IsTokenLinebreak();
    BOOL IsTokenKeyword();
    BOOL IsTokenLiteral();
    BOOL IsTokenIdentifier();
    BOOL IsTokenPunctuator();

    BOOL ParseTestFile();
    void AddTestFragment(const char* str);

    FILE* f;
    char* line;
    Head tokens;
    Head fragments;
};

class TestToken : public Link
{
public:
    TestToken();
    ~TestToken();

    ES_Token::Type type;
    ES_Token::Punctuator punctuator;
    ES_Token::Keyword keyword;
    uni_char* id;
    BOOL literalNumber;
    double number;
    uni_char* str;
};

class TestFragment : public Link
{
public:
    TestFragment(const char* s);
    ~TestFragment();

    char* str;
};

uni_char* createUniString(const char* str)
{
    uni_char* uni = new uni_char[strlen(str) + 1];
    for (int i = 0; i < strlen(str); i++)
    {
        uni[i] = str[i];
    }
    uni[strlen(str)] = 0;
    return uni;
}

TestFragment::TestFragment(const char* s)
{
    str = strdup(s);
}

TestFragment::~TestFragment()
{
    delete [] str;
}

void TestParser::AddTestFragment(const char* s)
{
    TestFragment* fragment = new TestFragment(s);
    fragment->Into(&fragments);
}

TestToken::TestToken()
{
    id = NULL;
    str = NULL;
}

TestToken::~TestToken()
{
    delete [] id;
    delete [] str;
}

BOOL TestLineParser::IsString()
{
    return str[i] == '"';
}

BOOL TestLineParser::IsNumber()
{
    return str[i] >= '0' && str[i] <= '9';
}

void TestLineParser::CreateString(uni_char* s)
{
    // First we skip the ".
    // Then we move on until end or ".
    // We can skip advanced stuff for now.
    int pos = 0;
    i++;
    while (str[i] != '\0')
    {
        if (str[i] == '"')
            break;
        s[pos++] = str[i++];
    }
    s[pos] = '\0';
}

double TestLineParser::ConvertNumber()
{
    int num = str[i++] - '0';
    int small = 0;
    int divideby = 1;
    BOOL d = FALSE;
    while (str[i] != '\0')
    {
        if (str[i] == '.')
        {
            d = TRUE;
            i++;
            break;
        }
        if (str[i] < '0' || str[i] > '9')
        {
            break;
        }
        num = num * 10 + (str[i++] - '0');
    }
    if (d)
    {
        while (str[i] != '\0')
        {
            if (str[i] < '0' || str[i] > '9')
            {
                break;
            }
            num = num * 10 + (str[i++] - '0');
            divideby *= 10;
        }
    }
    return num / (double)divideby;
}


ES_Token::Punctuator TestParser::TranslatePunctuator(const char* punctuator)
{
    if (strcmp(punctuator, "LEFT_BRACE") == 0)
        return ES_Token::LEFT_BRACE;
    if (strcmp(punctuator, "RIGHT_BRACE") == 0)
        return ES_Token::RIGHT_BRACE;
    if (strcmp(punctuator, "LEFT_PAREN") == 0)
        return ES_Token::LEFT_PAREN;
    if (strcmp(punctuator, "RIGHT_PAREN") == 0)
        return ES_Token::RIGHT_PAREN;
    if (strcmp(punctuator, "LEFT_BRACKET") == 0)
        return ES_Token::LEFT_BRACKET;
    if (strcmp(punctuator, "RIGHT_BRACKET") == 0)
        return ES_Token::RIGHT_BRACKET;
    if (strcmp(punctuator, "PERIOD") == 0)
        return ES_Token::PERIOD;
    if (strcmp(punctuator, "SEMICOLON") == 0)
        return ES_Token::SEMICOLON;
    if (strcmp(punctuator, "COMMA") == 0)
        return ES_Token::COMMA;
    if (strcmp(punctuator, "LESS_THAN") == 0)
        return ES_Token::LESS_THAN;
    if (strcmp(punctuator, "GREATER_THAN") == 0)
        return ES_Token::GREATER_THAN;
    if (strcmp(punctuator, "LOGICAL_NOT") == 0)
        return ES_Token::LOGICAL_NOT;
    if (strcmp(punctuator, "BITWISE_NOT") == 0)
        return ES_Token::BITWISE_NOT;
    if (strcmp(punctuator, "CONDITIONAL_TRUE") == 0)
        return ES_Token::CONDITIONAL_TRUE;
    if (strcmp(punctuator, "CONDITIONAL_FALSE") == 0)
        return ES_Token::CONDITIONAL_FALSE;
    if (strcmp(punctuator, "ASSIGN") == 0)
        return ES_Token::ASSIGN;
    if (strcmp(punctuator, "ADD") == 0)
        return ES_Token::ADD;
    if (strcmp(punctuator, "SUBTRACT") == 0)
        return ES_Token::SUBTRACT;
    if (strcmp(punctuator, "MULTIPLY") == 0)
        return ES_Token::MULTIPLY;
    if (strcmp(punctuator, "DIVIDE") == 0)
        return ES_Token::DIVIDE;
    if (strcmp(punctuator, "REMAINDER") == 0)
        return ES_Token::REMAINDER;
    if (strcmp(punctuator, "BITWISE_AND") == 0)
        return ES_Token::BITWISE_AND;
    if (strcmp(punctuator, "BITWISE_OR") == 0)
        return ES_Token::BITWISE_OR;
    if (strcmp(punctuator, "BITWISE_XOR") == 0)
        return ES_Token::BITWISE_XOR;
    if (strcmp(punctuator, "LESS_THAN_OR_EQUAL") == 0)
        return ES_Token::LESS_THAN_OR_EQUAL;
    if (strcmp(punctuator, "GREATER_THAN_OR_EQUAL") == 0)
        return ES_Token::GREATER_THAN_OR_EQUAL;
    if (strcmp(punctuator, "EQUAL") == 0)
        return ES_Token::EQUAL;
    if (strcmp(punctuator, "NOT_EQUAL") == 0)
        return ES_Token::NOT_EQUAL;
    if (strcmp(punctuator, "INCREMENT") == 0)
        return ES_Token::INCREMENT;
    if (strcmp(punctuator, "DECREMENT") == 0)
        return ES_Token::DECREMENT;
    if (strcmp(punctuator, "LOGICAL_AND") == 0)
        return ES_Token::LOGICAL_AND;
    if (strcmp(punctuator, "LOGICAL_OR") == 0)
        return ES_Token::LOGICAL_OR;
    if (strcmp(punctuator, "SHIFT_LEFT") == 0)
        return ES_Token::SHIFT_LEFT;
    if (strcmp(punctuator, "SHIFT_SIGNED_RIGHT") == 0)
        return ES_Token::SHIFT_SIGNED_RIGHT;
    if (strcmp(punctuator, "ADD_ASSIGN") == 0)
        return ES_Token::ADD_ASSIGN;
    if (strcmp(punctuator, "SUBTRACT_ASSIGN") == 0)
        return ES_Token::SUBTRACT_ASSIGN;
    if (strcmp(punctuator, "MULTIPLY_ASSIGN") == 0)
        return ES_Token::MULTIPLY_ASSIGN;
    if (strcmp(punctuator, "DIVIDE_ASSIGN") == 0)
        return ES_Token::DIVIDE_ASSIGN;
    if (strcmp(punctuator, "REMAINDER_ASSIGN") == 0)
        return ES_Token::REMAINDER_ASSIGN;
    if (strcmp(punctuator, "BITWISE_AND_ASSIGN") == 0)
        return ES_Token::BITWISE_AND_ASSIGN;
    if (strcmp(punctuator, "BITWISE_OR_ASSIGN") == 0)
        return ES_Token::BITWISE_OR_ASSIGN;
    if (strcmp(punctuator, "BITWISE_XOR_ASSIGN") == 0)
        return ES_Token::BITWISE_XOR_ASSIGN;
    if (strcmp(punctuator, "SINGLE_LINE_COMMENT") == 0)
        return ES_Token::SINGLE_LINE_COMMENT;
    if (strcmp(punctuator, "MULTI_LINE_COMMENT") == 0)
        return ES_Token::MULTI_LINE_COMMENT;
    if (strcmp(punctuator, "STRICT_EQUAL") == 0)
        return ES_Token::STRICT_EQUAL;
    if (strcmp(punctuator, "STRICT_NOT_EQUAL") == 0)
        return ES_Token::STRICT_NOT_EQUAL;
    if (strcmp(punctuator, "SHIFT_UNSIGNED_RIGHT") == 0)
        return ES_Token::SHIFT_UNSIGNED_RIGHT;
    if (strcmp(punctuator, "SHIFT_LEFT_ASSIGN") == 0)
        return ES_Token::SHIFT_LEFT_ASSIGN;
    if (strcmp(punctuator, "SHIFT_SIGNED_RIGHT_ASSIGN") == 0)
        return ES_Token::SHIFT_SIGNED_RIGHT_ASSIGN;
    if (strcmp(punctuator, "SHIFT_UNSIGNED_RIGHT_ASSIGN") == 0)
        return ES_Token::SHIFT_UNSIGNED_RIGHT_ASSIGN;
    return ES_Token::PUNCTUATORS_COUNT;
}

TestLineParser::TestLineParser(const char* str)
{
    this->str = str;
    i = 0;
}

BOOL TestLineParser::IsSpace(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

BOOL TestLineParser::EatSpace()
{
    if (IsSpace(str[i]))
    {
        i++;
        return TRUE;
    }
    return FALSE;
}

void TestLineParser::SkipSpaces()
{
    while (str[i] != '\0')
    {
        if (!IsSpace(str[i]))
            return;
        i++;
    }
}

BOOL TestLineParser::CompareWith(const char* s)
{
    return strncmp(&str[i], s, strlen(s)) == 0;
}

BOOL TestLineParser::EatString(const char* s)
{
    if (CompareWith(s))
    {
        i += strlen(s);
        return TRUE;
    }
    return FALSE;
}

BOOL TestLineParser::AtEnd()
{
    return str[i] == '\0';
}

BOOL TestLineParser::FindWord(char* s)
{
    int start = i;
    while (str[i] != '\0')
    {
        if (IsSpace(str[i]))
            break;
        i++;
    }
    int end = i;
    if (end <= start)
    {
        return FALSE;
    }
    strncpy(s, &str[start], end - start);
    s[end - start] = '\0';
    return TRUE;
}

TestParser::TestParser(FILE* f)
{
    this->f = f;
    line = new char[10000];
}

BOOL TestParser::NextLine()
{
    size_t size = 0;
    int read = getline(&line, &size, f);
    return read > 0;
}

ES_Token::Keyword TestParser::TranslateKeyword(const char* keyword)
{
    if (strcmp(keyword, "break") == 0)
        return ES_Token::KEYWORD_BREAK;
    else if (strcmp(keyword, "case") == 0)
        return ES_Token::KEYWORD_CASE;
    else if (strcmp(keyword, "catch") == 0)
        return ES_Token::KEYWORD_CATCH;
    else if (strcmp(keyword, "continue") == 0)
        return ES_Token::KEYWORD_CONTINUE;
    else if (strcmp(keyword, "default") == 0)
        return ES_Token::KEYWORD_DEFAULT;
    else if (strcmp(keyword, "delete") == 0)
        return ES_Token::KEYWORD_DELETE;
    else if (strcmp(keyword, "do") == 0)
        return ES_Token::KEYWORD_DO;
    else if (strcmp(keyword, "else") == 0)
        return ES_Token::KEYWORD_ELSE;
    else if (strcmp(keyword, "false") == 0)
        return ES_Token::KEYWORD_FALSE;
    else if (strcmp(keyword, "finally") == 0)
        return ES_Token::KEYWORD_FINALLY;
    else if (strcmp(keyword, "for") == 0)
        return ES_Token::KEYWORD_FOR;
    else if (strcmp(keyword, "function") == 0)
        return ES_Token::KEYWORD_FUNCTION;
    else if (strcmp(keyword, "if") == 0)
        return ES_Token::KEYWORD_IF;
    else if (strcmp(keyword, "in") == 0)
        return ES_Token::KEYWORD_IN;
    else if (strcmp(keyword, "instanceof") == 0)
        return ES_Token::KEYWORD_INSTANCEOF;
    else if (strcmp(keyword, "new") == 0)
        return ES_Token::KEYWORD_NEW;
    else if (strcmp(keyword, "null") == 0)
        return ES_Token::KEYWORD_NULL;
    else if (strcmp(keyword, "return") == 0)
        return ES_Token::KEYWORD_RETURN;
    else if (strcmp(keyword, "switch") == 0)
        return ES_Token::KEYWORD_SWITCH;
    else if (strcmp(keyword, "this") == 0)
        return ES_Token::KEYWORD_THIS;
    else if (strcmp(keyword, "throw") == 0)
        return ES_Token::KEYWORD_THROW;
    else if (strcmp(keyword, "true") == 0)
        return ES_Token::KEYWORD_TRUE;
    else if (strcmp(keyword, "try") == 0)
        return ES_Token::KEYWORD_TRY;
    else if (strcmp(keyword, "typeof") == 0)
        return ES_Token::KEYWORD_TYPEOF;
    else if (strcmp(keyword, "var") == 0)
        return ES_Token::KEYWORD_VAR;
    else if (strcmp(keyword, "void") == 0)
        return ES_Token::KEYWORD_VOID;
    else if (strcmp(keyword, "while") == 0)
        return ES_Token::KEYWORD_WHILE;
    else if (strcmp(keyword, "with") == 0)
        return ES_Token::KEYWORD_WITH;
    return ES_Token::KEYWORDS_COUNT;
}

BOOL TestParser::IsLineEmpty()
{
    TestLineParser parser(line);
    parser.SkipSpaces();
    return parser.AtEnd();
}

BOOL TestParser::TestLine()
{
    TestLineParser parser(line);
    parser.SkipSpaces();
    if (!parser.EatString("test"))
        return FALSE;
    if (!parser.EatSpace())
        return FALSE;
    parser.SkipSpaces();
    char* name = new char[strlen(line) + 1];
    BOOL hasname = parser.FindWord(name);
    if (!hasname)
        return FALSE;
    parser.SkipSpaces();
    BOOL atEnd = parser.AtEnd();
    if (!atEnd)
        return FALSE;
    printf("test: %s\n", name);
    delete [] name;
    return TRUE;
}

BOOL TestParser::IsTestFragment()
{
    AddTestFragment(line);
    return TRUE;
}



BOOL TestParser::IsBeginIndata()
{
    TestLineParser parser(line);
    parser.SkipSpaces();
    if (!parser.EatString("begin"))
        return FALSE;
    if (!parser.EatSpace())
        return FALSE;
    parser.SkipSpaces();
    if (!parser.EatString("indata"))
        return FALSE;
    parser.SkipSpaces();
    if (!parser.AtEnd())
        return FALSE;
    return TRUE;
}

BOOL TestParser::IsBeginResult()
{
    TestLineParser parser(line);
    parser.SkipSpaces();
    if (!parser.EatString("begin"))
        return FALSE;
    if (!parser.EatSpace())
        return FALSE;
    parser.SkipSpaces();
    if (!parser.EatString("result"))
        return FALSE;
    parser.SkipSpaces();
    if (!parser.AtEnd())
        return FALSE;
    return TRUE;
}

BOOL TestParser::IsEnd()
{
    TestLineParser parser(line);
    parser.SkipSpaces();
    if (!parser.EatString("end"))
        return FALSE;
    parser.SkipSpaces();
    if (!parser.AtEnd())
        return FALSE;
    return TRUE;
}

BOOL TestParser::IsTokenEnd()
{
    TestLineParser parser(line);
    parser.SkipSpaces();
    if (!parser.EatString("END"))
        return FALSE;
    parser.SkipSpaces();
    if (!parser.AtEnd())
        return FALSE;
    TestToken* token = new TestToken();
    token->type = ES_Token::END;
    token->Into(&tokens);
    return TRUE;
}

BOOL TestParser::IsTokenInvalid()
{
    TestLineParser parser(line);
    parser.SkipSpaces();
    if (!parser.EatString("INVALID"))
        return FALSE;
    parser.SkipSpaces();
    if (!parser.AtEnd())
        return FALSE;
    TestToken* token = new TestToken();
    token->type = ES_Token::INVALID;
    token->Into(&tokens);
    return TRUE;
}

BOOL TestParser::IsTokenLinebreak()
{
    TestLineParser parser(line);
    parser.SkipSpaces();
    if (!parser.EatString("LINEBREAK"))
        return FALSE;
    parser.SkipSpaces();
    if (!parser.AtEnd())
        return FALSE;
    TestToken* token = new TestToken();
    token->type = ES_Token::LINEBREAK;
    token->Into(&tokens);
    return TRUE;
}

BOOL TestParser::IsTokenKeyword()
{
    // Translate keyword string into keyword value.
    TestLineParser parser(line);
    parser.SkipSpaces();
    if (!parser.EatString("KEYWORD"))
        return FALSE;
    parser.SkipSpaces();
    if (!parser.EatString(":"))
        return FALSE;
    parser.SkipSpaces();
    char* name = new char[strlen(line) + 1];
    if (!parser.FindWord(name))
    {
        delete [] name;
        return FALSE;
    }
    parser.SkipSpaces();
    if (!parser.AtEnd())
        return FALSE;
    ES_Token::Keyword keyword = TranslateKeyword(name);
    delete [] name;
    if (keyword >= ES_Token::KEYWORDS_COUNT)
        return FALSE;
    TestToken* token = new TestToken();
    token->type = ES_Token::KEYWORD;
    token->keyword = keyword;
    token->Into(&tokens);
    return TRUE;
}

BOOL TestParser::IsTokenLiteral()
{
    // We need to support strings, and numbers, and booleans.
    // This is the hard part.
    // A string starts with a " or an '.
    // A boolean is true or false.
    // A number is everything else.
    TestLineParser parser(line);
    parser.SkipSpaces();
    if (!parser.EatString("LITERAL"))
        return FALSE;
    parser.SkipSpaces();
    if (!parser.EatString(":"))
        return FALSE;
    parser.SkipSpaces();
    if (parser.IsString())
    {
        uni_char* str = new uni_char[strlen(line) + 1];
        parser.CreateString(str);
        TestToken* token = new TestToken();
        token->type = ES_Token::LITERAL;
        token->literalNumber = FALSE;
        token->str = str;
        token->Into(&tokens);
        // Copy string and translate.
    }
    else if (parser.IsNumber())
    {
        double num = parser.ConvertNumber();
        TestToken* token = new TestToken();
        token->type = ES_Token::LITERAL;
        token->literalNumber = TRUE;
        token->number = num;
        token->Into(&tokens);
    }
    else
        return FALSE;
    /*
      We always give a number that is in the form (0-9)+[.(0-9)+]
      A string can only include simple things. What?
     */
    return TRUE;
}

BOOL TestParser::IsTokenIdentifier()
{
    // We need to support unicode character probably.
    // What about translating everything to unicode?
    // Maybe we should expect file to be encoded in UTF8?
    TestLineParser parser(line);
    parser.SkipSpaces();
    if (!parser.EatString("IDENTIFIER"))
        return FALSE;
    parser.SkipSpaces();
    if (!parser.EatString(":"))
        return FALSE;
    parser.SkipSpaces();
    char* name = new char[strlen(line) + 1];
    if (!parser.FindWord(name))
    {
        delete [] name;
        return FALSE;
    }
    parser.SkipSpaces();
    if (!parser.AtEnd())
        return FALSE;
    int len = strlen(name);
    uni_char* id = createUniString(name);
    delete [] name;
    TestToken* token = new TestToken();
    token->type = ES_Token::IDENTIFIER;
    token->id = id;
    token->Into(&tokens);
    return TRUE;
}

BOOL TestParser::IsTokenPunctuator()
{
    // We need to translate punctuator into something that we can use.
    TestLineParser parser(line);
    parser.SkipSpaces();
    if (!parser.EatString("PUNCTUATOR"))
        return FALSE;
    parser.SkipSpaces();
    if (!parser.EatString(":"))
        return FALSE;
    parser.SkipSpaces();
    char* name = new char[strlen(line) + 1];
    if (!parser.FindWord(name))
    {
        delete [] name;
        return FALSE;
    }
    parser.SkipSpaces();
    if (!parser.AtEnd())
    {
        delete [] name;
        return FALSE;
    }
    ES_Token::Punctuator punctuator = TranslatePunctuator(name);
    delete [] name;
    if (punctuator >= ES_Token::PUNCTUATORS_COUNT)
        return FALSE;
    TestToken* token = new TestToken();
    token->type = ES_Token::PUNCTUATOR;
    token->punctuator = punctuator;
    token->Into(&tokens);
    return TRUE;
}

BOOL TestParser::ParseTestFile()
{
    size_t size = 0;
    while (TRUE) // Find test line.
    {
        if (!NextLine())
            return FALSE;
        if (!IsLineEmpty())
        {
            // Check if test something
            if (!TestLine())
            {
                return FALSE;
            }
            break;
        }
    }
    while (TRUE) // Find begin indata
    {
        if (!NextLine())
            return FALSE;
        if (!IsLineEmpty())
        {
            if (!IsBeginIndata())
            {
                return FALSE;
            }
            break;
        }
    }
    while (TRUE) // Add all lines until end
    {
        if (!NextLine())
            return FALSE;
        if (IsEnd())
            break;
        if (!IsTestFragment())
            break;
    }
    while (TRUE) // Find begin result
    {
        if (!NextLine())
            return FALSE;
        if (!IsLineEmpty())
        {
            if (!IsBeginResult())
            {
                return FALSE;
            }
            break;
        }
    }
    while (TRUE) // Add all tokens until end
    {
        if (!NextLine())
            return FALSE;
        if (IsEnd())
            break;
        if (IsTokenEnd() ||
            IsTokenInvalid() ||
            IsTokenLinebreak() ||
            IsTokenKeyword() ||
            IsTokenLiteral() ||
            IsTokenIdentifier() ||
            IsTokenPunctuator())
            continue;
        return FALSE;
    }

    return TRUE;
}

BOOL TestParser::RunTest()
{
    if (!ParseTestFile())
    {
        printf("Failed to parse test file\n");
        return FALSE;
    }

    ES_Fragments program_fragments;
    program_fragments.fragments = new const uni_char*[fragments.Cardinal()];
    program_fragments.fragment_lengths = new unsigned[fragments.Cardinal()];
    program_fragments.fragments_count = fragments.Cardinal();

    TestFragment* fragment = (TestFragment*)fragments.First();
    for (int i = 0; i < fragments.Cardinal(); i++)
    {
        program_fragments.fragments[i] = createUniString(fragment->str);
        program_fragments.fragment_lengths[i] = strlen(fragment->str);
        fragment = (TestFragment*)fragment->Suc();
    }

    ES_Lexer lexer(&program_fragments, NULL);
    TestToken* testtoken = (TestToken*)tokens.First();

    while (TRUE)
    {
        lexer.NextToken();
        ES_Token token = lexer.GetToken();
        if (token.type != testtoken->type)
        {
            printf("Different token types\n");
            printf("Token is: %d\n", token.type);
            printf("TestToken is: %d\n", testtoken->type);
            return FALSE;
        }
        if (token.type == ES_Token::PUNCTUATOR)
        {
            if (token.punctuator != testtoken->punctuator)
            {
                printf("Different punctuators\n");
                printf("Token is: %d\n", token.punctuator);
                printf("TestToken is: %d\n", testtoken->punctuator);
                return FALSE;
            }
        }
        else if (token.type == ES_Token::KEYWORD)
        {
            if (token.keyword != testtoken->keyword)
            {
                printf("Different keywords\n");
                printf("Token is: %d\n", token.keyword);
                printf("TestToken is: %d\n", testtoken->keyword);
                return FALSE;
            }
        }
        else if (token.type == ES_Token::IDENTIFIER)
        {
            if (uni_strlen(testtoken->id) != token.identifier->length ||
                uni_strncmp(testtoken->id, token.identifier->name,
                            token.identifier->length))
            {
                printf("Identifiers differ\n");
                return FALSE;
            }
        }
        else if (token.type == ES_Token::LITERAL)
        {
            if (token.value.IsNumber())
            {
                if (!testtoken->literalNumber)
                {
                    printf("token is number, testtoken not\n");
                    return FALSE;
                }
                if (token.value.GetNumAsDouble() != testtoken->number)
                {
                    printf("token and testtoken has different numbers\n");
                    return FALSE;
                }
            }
            else if (token.value.IsString())
            {
                if (testtoken->literalNumber)
                {
                    printf("token is string, testtoken not\n");
                    return FALSE;
                }
                JString* jstring = token.value.GetString();
                uni_char* actualstring = StorageZ(jstring);
                if (uni_strcmp(actualstring, testtoken->str))
                {
                    printf("token and testtoken has different strings\n");
                    return FALSE;
                }
            }
        }
        testtoken = (TestToken*)testtoken->Suc();
        if (token.type == ES_Token::INVALID || token.type == ES_Token::END)
        {
            break;
        }
        if (testtoken == NULL)
        {
            printf("Test has more tokens, but supposed to end.\n");
            return FALSE;
        }
    }

    if (testtoken != NULL)
    {
        printf("Test is supposed to generate more tokens.\n");
        return FALSE;
    }

    return TRUE;
}

static BOOL runTest(FILE* f)
{
    TestParser parser(f);
    return parser.RunTest();
    // Get the lines from the test and feed the lexer.
    // Take one token at a time, and compare.
    // TestParser has a NextToken, which returns something that can be compared.
    // We also need to know when we have no more.
    // We need to be able to comare the to with isSameToken.
}

static void runTests(const char* testfile)
{
    FILE* f = fopen(testfile, "r");
    if (!f)
    {
        printf("Could not open test file: %s\n", testfile);
        exit(1);
    }
    while (runTest(f));
    fclose(f);
}

void printPunctuator(ES_Token token)
{
    switch (token.punctuator)
    {
    case ES_Token::LEFT_BRACE:
        printf("punctuator: LEFT_BRACE\n");
        break;
    case ES_Token::RIGHT_BRACE:
        printf("punctuator: RIGHT_BRACE\n");
        break;
    case ES_Token::LEFT_PAREN:
        printf("punctuator: LEFT_PAREN\n");
        break;
    case ES_Token::RIGHT_PAREN:
        printf("punctuator: RIGHT_PAREN\n");
        break;
    case ES_Token::LEFT_BRACKET:
        printf("punctuator: LEFT_BRACKET\n");
        break;
    case ES_Token::RIGHT_BRACKET:
        printf("punctuator: RIGHT_BRACKET\n");
        break;
    case ES_Token::PERIOD:
        printf("punctuator: PERIOD\n");
        break;
    case ES_Token::SEMICOLON:
        printf("punctuator: SEMICOLON\n");
        break;
    case ES_Token::COMMA:
        printf("punctuator: COMMA\n");
        break;
    case ES_Token::LESS_THAN:
        printf("punctuator: LESS_THAN\n");
        break;
    case ES_Token::GREATER_THAN:
        printf("punctuator: GREATER_THAN\n");
        break;
    case ES_Token::LOGICAL_NOT:
        printf("punctuator: LOGICAL_NOT\n");
        break;
    case ES_Token::BITWISE_NOT:
        printf("punctuator: BITWISE_NOT\n");
        break;
    case ES_Token::CONDITIONAL_TRUE:
        printf("punctuator: CONDITIONAL_TRUE\n");
        break;
    case ES_Token::CONDITIONAL_FALSE:
        printf("punctuator: CONDITIONAL_FALSE\n");
        break;
    case ES_Token::ASSIGN:
        printf("punctuator: ASSIGN\n");
        break;
    case ES_Token::ADD:
        printf("punctuator: ADD\n");
        break;
    case ES_Token::SUBTRACT:
        printf("punctuator: SUBTRACT\n");
        break;
    case ES_Token::MULTIPLY:
        printf("punctuator: MULTIPLY\n");
        break;
    case ES_Token::DIVIDE:
        printf("punctuator: DIVIDE\n");
        break;
    case ES_Token::REMAINDER:
        printf("punctuator: REMAINDER\n");
        break;
    case ES_Token::BITWISE_AND:
        printf("punctuator: BITWISE_AND\n");
        break;
    case ES_Token::BITWISE_OR:
        printf("punctuator: BITWISE_OR\n");
        break;
    case ES_Token::BITWISE_XOR:
        printf("punctuator: BITWISE_XOR\n");
        break;
    case ES_Token::LESS_THAN_OR_EQUAL:
        printf("punctuator: LESS_THAN_OR_EQUAL\n");
        break;
    case ES_Token::GREATER_THAN_OR_EQUAL:
        printf("punctuator: GREATER_THAN_OR_EQUAL\n");
        break;
    case ES_Token::EQUAL:
        printf("punctuator: EQUAL\n");
        break;
    case ES_Token::NOT_EQUAL:
        printf("punctuator: NOT_EQUAL\n");
        break;
    case ES_Token::INCREMENT:
        printf("punctuator: INCREMENT\n");
        break;
    case ES_Token::DECREMENT:
        printf("punctuator: DECREMENT\n");
        break;
    case ES_Token::LOGICAL_AND:
        printf("punctuator: LOGICAL_AND\n");
        break;
    case ES_Token::LOGICAL_OR:
        printf("punctuator: LOGICAL_OR\n");
        break;
    case ES_Token::SHIFT_LEFT:
        printf("punctuator: SHIFT_LEFT\n");
        break;
    case ES_Token::SHIFT_SIGNED_RIGHT:
        printf("punctuator: SHIFT_SIGNED_RIGHT\n");
        break;
    case ES_Token::ADD_ASSIGN:
        printf("punctuator: ADD_ASSIGN\n");
        break;
    case ES_Token::SUBTRACT_ASSIGN:
        printf("punctuator: SUBTRACT_ASSIGN\n");
        break;
    case ES_Token::MULTIPLY_ASSIGN:
        printf("punctuator: MULTIPLY_ASSIGN\n");
        break;
    case ES_Token::DIVIDE_ASSIGN:
        printf("punctuator: DIVIDE_ASSIGN\n");
        break;
    case ES_Token::REMAINDER_ASSIGN:
        printf("punctuator: REMAINDER_ASSIGN\n");
        break;
    case ES_Token::BITWISE_AND_ASSIGN:
        printf("punctuator: BITWISE_AND_ASSIGN\n");
        break;
    case ES_Token::BITWISE_OR_ASSIGN:
        printf("punctuator: BITWISE_OR_ASSIGN\n");
        break;
    case ES_Token::BITWISE_XOR_ASSIGN:
        printf("punctuator: BITWISE_XOR_ASSIGN\n");
        break;
    case ES_Token::SINGLE_LINE_COMMENT:
        printf("punctuator: SINGLE_LINE_COMMENT\n");
        break;
    case ES_Token::MULTI_LINE_COMMENT:
        printf("punctuator: MULTI_LINE_COMMENT\n");
        break;
    case ES_Token::STRICT_EQUAL:
        printf("punctuator: STRICT_EQUAL\n");
        break;
    case ES_Token::STRICT_NOT_EQUAL:
        printf("punctuator: STRICT_NOT_EQUAL\n");
        break;
    case ES_Token::SHIFT_UNSIGNED_RIGHT:
        printf("punctuator: SHIFT_UNSIGNED_RIGHT\n");
        break;
    case ES_Token::SHIFT_LEFT_ASSIGN:
        printf("punctuator: SHIFT_LEFT_ASSIGN\n");
        break;
    case ES_Token::SHIFT_SIGNED_RIGHT_ASSIGN:
        printf("punctuator: SHIFT_SIGNED_RIGHT_ASSIGN\n");
        break;
    case ES_Token::SHIFT_UNSIGNED_RIGHT_ASSIGN:
        printf("punctuator: SHIFT_UNSIGNED_RIGHT_ASSIGN\n");
        break;
    default:
        printf("ERROR: unknown punctuator\n");
        break;
    }
}

int main()
{
    char memory[sizeof(Opera)];
    op_memset(memory, 0, sizeof(Opera));
    g_opera = (Opera*)memory;
    g_opera->InitL();
    g_op_system_info = new OpSystemInfo;
    g_op_time_info = new OpTimeInfo;
    ES_Init();
    runTests("modules/ecmascript/carakan/selftest/lexer/testlexer.txt");
    return 0;
}
