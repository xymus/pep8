#include "asm.h"
#include "argument.h"
#include "code.h"

#include <QDebug>

// Regular expressions for lexical analysis
QRegExp Asm::rxAddrMode ("^((,)(\\s*)(i|d|x|n|s(?![fx])|sx(?![f])|sf|sxf){1}){1}");
QRegExp Asm::rxCharConst ("^((\')(?![\'])(([^\'|\\\\]){1}|((\\\\)([\'|b|f|n|r|t|v|\"|\\\\]))|((\\\\)(([x|X])([0-9|A-F|a-f]{2}))))(\'))");
QRegExp Asm::rxComment ("^((;{1})(.)*)");
QRegExp Asm::rxDecConst ("^((([+|-]{0,1})([0-9]+))|^(([1-9])([0-9]*)))");
QRegExp Asm::rxDotCommand ("^((.)(([A-Z|a-z]{1})(\\w)*))");
QRegExp Asm::rxHexConst ("^((0(?![x|X]))|((0)([x|X])([0-9|A-F|a-f])+)|((0)([0-9]+)))");
QRegExp Asm::rxIdentifier ("^((([A-Z|a-z]{1})(\\w*))(:){0,1})");
QRegExp Asm::rxStringConst ("^((\")((([^\"|\\\\])|((\\\\)([\'|b|f|n|r|t|v|\"|\\\\]))|((\\\\)(([x|X])([0-9|A-F|a-f]{2}))))*)(\"))");

// Regular expressions for trace tag analysis


bool Asm::getToken(QString &sourceLine, ELexicalToken &token, QString &tokenString)
{
    sourceLine = sourceLine.trimmed();
    if (sourceLine.length() == 0) {
        token = LT_EMPTY;
        tokenString = "";
        return true;
    }
    QChar firstChar = sourceLine[0];
    rxAddrMode.setCaseSensitivity (Qt::CaseInsensitive);  // Make rxAddrMode not case sensitive
    if (firstChar == ',') {
        if (rxAddrMode.indexIn(sourceLine) == -1) {
            tokenString = ";ERROR: Malformed addressing mode.";
            return false;
        }
        token = LT_ADDRESSING_MODE;
        tokenString = rxAddrMode.capturedTexts()[0];
        sourceLine.remove(0, tokenString.length());
        return true;
    }
    if (firstChar == '\'') {
        if (rxCharConst.indexIn(sourceLine) == -1) {
            tokenString = ";ERROR: Malformed character constant.";
            return false;
        }
        token = LT_CHAR_CONSTANT;
        tokenString = rxCharConst.capturedTexts()[0];
        sourceLine.remove(0, tokenString.length());
        return true;
    }
    if (firstChar == ';') {
        if (rxComment.indexIn(sourceLine) == -1) {
            // This error should not occur, as any characters are allowed in a comment.
            tokenString = ";ERROR: Malformed comment";
            return false;
        }
        token = LT_COMMENT;
        tokenString = rxComment.capturedTexts()[0];
        sourceLine.remove(0, tokenString.length());
        return true;
    }
    if ((firstChar.isDigit() || firstChar == '+' || firstChar == '-') & !startsWithHexPrefix(sourceLine)) {
        if (rxDecConst.indexIn(sourceLine) == -1) {
            tokenString = ";ERROR: Malformed decimal constant.";
            return false;
        }
        token = LT_DEC_CONSTANT;
        tokenString = rxDecConst.capturedTexts()[0];
        sourceLine.remove(0, tokenString.length());
        return true;
    }
    if (firstChar == '.') {
        if (rxDotCommand.indexIn(sourceLine) == -1) {
            tokenString = ";ERROR: Malformed dot command.";
            return false;
        }
        token = LT_DOT_COMMAND;
        tokenString = rxDotCommand.capturedTexts()[0];
        sourceLine.remove(0, tokenString.length());
        return true;
    }
    if (startsWithHexPrefix(sourceLine)) {
        if (rxHexConst.indexIn(sourceLine) == -1) {
            tokenString = ";ERROR: Malformed hex constant.";
            return false;
        }
        token = LT_HEX_CONSTANT;
        tokenString = rxHexConst.capturedTexts()[0];
        sourceLine.remove(0, tokenString.length());
        return true;
    }
    if (firstChar.isLetter() || firstChar == '_') {
        if (rxIdentifier.indexIn(sourceLine) == -1) {
            // This error should not occur, as one-character identifiers are valid.
            tokenString = ";ERROR: Malformed identifier.";
            return false;
        }
        tokenString = rxIdentifier.capturedTexts()[0];
        token = tokenString.endsWith(':') ? LT_SYMBOL_DEF : LT_IDENTIFIER;
        sourceLine.remove(0, tokenString.length());
        return true;
    }
    if (firstChar == '\"') {
        if (rxStringConst.indexIn(sourceLine) == -1) {
            tokenString = ";ERROR: Malformed string constant.";
            return false;
        }
        token = LT_STRING_CONSTANT;
        tokenString = rxStringConst.capturedTexts()[0];
        sourceLine.remove(0, tokenString.length());
        return true;
    }
    tokenString = ";ERROR: Syntax error.";
    return false;
}

QList<QString> Asm::listOfReferencedSymbols;
QList<int> Asm::listOfReferencedSymbolLineNums;


bool Asm::startsWithHexPrefix(QString str)
{
    if (str.length() < 2) return false;
    if (str[0] != '0') return false;
    if (str[1] == 'x' || str[1] == 'X') return true;
    return false;
}

int Asm::stringToAddrMode(QString str)
{
    str.remove(0, 1); // Remove the comma.
    str = str.trimmed().toUpper();
    if (str == "I") return Pep::I;
    if (str == "D") return Pep::D;
    if (str == "N") return Pep::N;
    if (str == "S") return Pep::S;
    if (str == "SF") return Pep::SF;
    if (str == "X") return Pep::X;
    if (str == "SX") return Pep::SX;
    if (str == "SXF") return Pep::SXF;
    return Pep::NONE;
}

int Asm::charStringToInt(QString str)
{
    str.remove(0, 1); // Remove the leftmost single quote.
    str.chop(1); // Remove the rightmost single quote.
    int value;
    Asm::unquotedStringToInt(str, value);
    return value;
}

int Asm::string2ArgumentToInt(QString str) {
    int valueA, valueB;
    str.remove(0, 1); // Remove the leftmost double quote.
    str.chop(1); // Remove the rightmost double quote.
    Asm::unquotedStringToInt(str, valueA);
    if (str.length() == 0) {
        return valueA;
    }
    else {
        Asm::unquotedStringToInt(str, valueB);
        valueA = 256 * valueA + valueB;
        if (valueA < 0) {
            valueA += 65536; // Stored as two-byte unsigned.
        }
        return valueA;
    }
}

void Asm::unquotedStringToInt(QString &str, int &value)
{
    QString s;
    if (str.startsWith("\\x") || str.startsWith("\\X")) {
        str.remove(0, 2); // Remove the leading \x or \X
        s = str.left(2);
        str.remove(0, 2); // Get the two-byte hex number
        bool ok;
        value = s.toInt(&ok, 16);
    }
    else if (str.startsWith("\\")) {
        str.remove(0, 1); // Remove the leading bash
        s = str.left(1);
        str.remove(0,1);
        if (s == "b") { // backspace
            value = 8;
        }
        else if (s == "f") { // form feed
            value = 12;
        }
        else if (s == "n") { // line feed (new line)
            value = 10;
        }
        else if (s == "r") { // carriage return
            value = 13;
        }
        else if (s == "t") { // horizontal tab
            value = 9;
        }
        else if (s == "v") { // vertical tab
            value = 11;
        }
        else {
            value = QChar(s[0]).toAscii();
        }
    }
    else {
        s = str.left(1);
        str.remove(0, 1);
        value = QChar(s[0]).toAscii();
    }
}

int Asm::byteStringLength(QString str)
{
    str.remove(0, 1); // Remove the leftmost double quote.
    str.chop(1); // Remove the rightmost double quote.
    int length = 0;
    while (str.length() > 0) {
        if (str.startsWith("\\x") || str.startsWith("\\X")) {
            str.remove(0, 4); // Remove the \xFF
        }
        else if (str.startsWith("\\")) {
            str.remove(0, 2); // Remove the quoted character
        }
        else {
            str.remove(0, 1); // Remove the single character
        }
        length++;
    }
    return length;
}

bool Asm::processSourceLine(QString sourceLine, int lineNum, Code *&code, QString &errorString, int &byteCount, bool &dotEndDetected)
{
    Asm::ELexicalToken token; // Passed to getToken.
    QString tokenString; // Passed to getToken.
    QString localSymbolDef = ""; // Saves symbol definition for processing in the following state.
    Pep::EMnemonic localEnumMnemonic; // Key to Pep:: table lookups.

    // The concrete code objects asssigned to code.
    UnaryInstruction *unaryInstruction;
    NonUnaryInstruction *nonUnaryInstruction;
    DotAddress *dotAddress;
    DotAscii *dotAscii;
    DotBlock *dotBlock;
    DotBurn *dotBurn;
    DotByte *dotByte;
    DotEnd *dotEnd;
    DotEquate *dotEquate;
    DotWord *dotWord;
    CommentOnly *commentOnly;
    BlankLine *blankLine;

    dotEndDetected = false;
    Asm::ParseState state = Asm::PS_START;
    do {
        if (!getToken(sourceLine, token, tokenString)) {
            errorString = tokenString;
            return false;
        }
        switch (state) {
        case Asm::PS_START:
            if (token == Asm::LT_IDENTIFIER){
                if (Pep::mnemonToEnumMap.contains(tokenString.toUpper())) {
                    localEnumMnemonic = Pep::mnemonToEnumMap.value(tokenString.toUpper());
                    if (Pep::isUnaryMap.value(localEnumMnemonic)) {
                        unaryInstruction = new UnaryInstruction;
                        unaryInstruction->symbolDef = "";
                        unaryInstruction->mnemonic = localEnumMnemonic;
                        code = unaryInstruction;
                        byteCount += 1; // One byte generated for unary instruction.
                        state = Asm::PS_CLOSE;
                    }
                    else {
                        nonUnaryInstruction = new NonUnaryInstruction;
                        nonUnaryInstruction->symbolDef = "";
                        nonUnaryInstruction->mnemonic = localEnumMnemonic;
                        code = nonUnaryInstruction;
                        byteCount += 3; // Three bytes generated for unary instruction.
                        state = Asm::PS_INSTRUCTION;
                    }
                }
                else {
                    errorString = ";ERROR: Invalid mnemonic.";
                    return false;
                }
            }
            else if (token == Asm::LT_DOT_COMMAND) {
                tokenString.remove(0, 1); // Remove the period
                tokenString = tokenString.toUpper();
                if (tokenString == "ADDRSS") {
                    dotAddress = new DotAddress;
                    dotAddress->symbolDef = "";
                    code = dotAddress;
                    state = Asm::PS_DOT_ADDRSS;
                }
                else if (tokenString == "ASCII") {
                    dotAscii = new DotAscii;
                    dotAscii->symbolDef = "";
                    code = dotAscii;
                    state = Asm::PS_DOT_ASCII;
                }
                else if (tokenString == "BLOCK") {
                    dotBlock = new DotBlock;
                    dotBlock->symbolDef = "";
                    code = dotBlock;
                    state = Asm::PS_DOT_BLOCK;
                }
                else if (tokenString == "BURN") {
                    dotBurn = new DotBurn;
                    dotBurn->symbolDef = "";
                    code = dotBurn;
                    state = Asm::PS_DOT_BURN;
                }
                else if (tokenString == "BYTE") {
                    dotByte = new DotByte;
                    dotByte->symbolDef = "";
                    code = dotByte;
                    state = Asm::PS_DOT_BYTE;
                }
                else if (tokenString == "END") {
                    dotEnd = new DotEnd;
                    dotEnd->symbolDef = "";
                    code = dotEnd;
                    dotEndDetected = true;
                    state = Asm::PS_DOT_END;
                }
                else if (tokenString == "EQUATE") {
                    dotEquate = new DotEquate;
                    dotEquate->symbolDef = "";
                    code = dotEquate;
                    state = Asm::PS_DOT_EQUATE;
                }
                else if (tokenString == "WORD") {
                    dotWord = new DotWord;
                    dotWord->symbolDef = "";
                    code = dotWord;
                    state = Asm::PS_DOT_WORD;
                }
                else {
                    errorString = ";ERROR: Invalid dot command.";
                    return false;
                }
            }
            else if (token == Asm::LT_SYMBOL_DEF) {
                tokenString.chop(1); // Remove the colon
                if (tokenString.length() > 8) {
                    errorString = ";ERROR: Symbol " + tokenString + " cannot have more than eight characters.";
                    return false;
                }
                localSymbolDef = tokenString;
                Pep::symbolTable.insert(localSymbolDef, byteCount);
                state = Asm::PS_SYMBOL_DEF;
            }
            else if (token == Asm::LT_COMMENT) {
                commentOnly = new CommentOnly;
                commentOnly->comment = tokenString;
                code = commentOnly;
                state = Asm::PS_COMMENT;
            }
            else if (token == Asm::LT_EMPTY) {
                blankLine = new BlankLine;
                code = blankLine;
                state = Asm::PS_FINISH;
            }
            else {
                errorString = ";ERROR: Line must start with symbol definition, mnemonic, dot command, or comment.";
                return false;
            }
            break;

        case Asm::PS_SYMBOL_DEF:
            if (token == Asm::LT_IDENTIFIER){
                if (Pep::mnemonToEnumMap.contains(tokenString.toUpper())) {
                    localEnumMnemonic = Pep::mnemonToEnumMap.value(tokenString.toUpper());
                    if (Pep::isUnaryMap.value(localEnumMnemonic)) {
                        unaryInstruction = new UnaryInstruction;
                        unaryInstruction->symbolDef = localSymbolDef;
                        unaryInstruction->mnemonic = localEnumMnemonic;
                        code = unaryInstruction;
                        byteCount += 1; // One byte generated for unary instruction.
                        state = Asm::PS_CLOSE;
                    }
                    else {
                        nonUnaryInstruction = new NonUnaryInstruction;
                        nonUnaryInstruction->symbolDef = localSymbolDef;
                        nonUnaryInstruction->mnemonic = localEnumMnemonic;
                        code = nonUnaryInstruction;
                        byteCount += 3; // Three bytes generated for unary instruction.
                        state = Asm::PS_INSTRUCTION;
                    }
                }
                else {
                    errorString = ";ERROR: Invalid mnemonic.";
                    return false;
                }
            }
            else if (token == Asm::LT_DOT_COMMAND) {
                tokenString.remove(0, 1); // Remove the period
                tokenString = tokenString.toUpper();
                if (tokenString == "ADDRSS") {
                    dotAddress = new DotAddress;
                    dotAddress->symbolDef = localSymbolDef;
                    code = dotAddress;
                    state = Asm::PS_DOT_ADDRSS;
                }
                else if (tokenString == "ASCII") {
                    dotAscii = new DotAscii;
                    dotAscii->symbolDef = localSymbolDef;
                    code = dotAscii;
                    state = Asm::PS_DOT_ASCII;
                }
                else if (tokenString == "BLOCK") {
                    dotBlock = new DotBlock;
                    dotBlock->symbolDef = localSymbolDef;
                    code = dotBlock;
                    state = Asm::PS_DOT_BLOCK;
                }
                else if (tokenString == "BURN") {
                    dotBurn = new DotBurn;
                    dotBurn->symbolDef = localSymbolDef;
                    code = dotBurn;
                    state = Asm::PS_DOT_BURN;
                }
                else if (tokenString == "BYTE") {
                    dotByte = new DotByte;
                    dotByte->symbolDef = localSymbolDef;
                    code = dotByte;
                    state = Asm::PS_DOT_BYTE;
                }
                else if (tokenString == "END") {
                    dotEnd = new DotEnd;
                    dotEnd->symbolDef = localSymbolDef;
                    code = dotEnd;
                    dotEndDetected = true;
                    state = Asm::PS_DOT_END;
                }
                else if (tokenString == "EQUATE") {
                    dotEquate = new DotEquate;
                    dotEquate->symbolDef = localSymbolDef;
                    code = dotEquate;
                    state = Asm::PS_DOT_EQUATE;
                }
                else if (tokenString == "WORD") {
                    dotWord = new DotWord;
                    dotWord->symbolDef = localSymbolDef;
                    code = dotWord;
                    state = Asm::PS_DOT_WORD;
                }
                else {
                    errorString = ";ERROR: Invalid dot command.";
                    return false;
                }
            }
            else {
                errorString = ";ERROR: Must have mnemonic or dot command after symbol definition.";
                return false;
            }
            break;

        case Asm::PS_INSTRUCTION:
            if (token == Asm::LT_IDENTIFIER) {
                if (tokenString.length() > 8) {
                    errorString = ";ERROR: Symbol " + tokenString + " cannot have more than eight characters.";
                    return false;
                }
                nonUnaryInstruction->argument = new SymbolRefArgument(tokenString);
                Asm::listOfReferencedSymbols.append(tokenString);
                Asm::listOfReferencedSymbolLineNums.append(lineNum);
                state = Asm::PS_ADDRESSING_MODE;
            }
            else if (token == Asm::LT_STRING_CONSTANT) {
                if (Asm::byteStringLength(tokenString) > 2) {
                    errorString = ";ERROR: String operands must have length at most two.";
                    return false;
                }
                nonUnaryInstruction->argument = new StringArgument(tokenString);
                state = Asm::PS_ADDRESSING_MODE;
            }
            else if (token == Asm::LT_HEX_CONSTANT) {
                tokenString.remove(0, 2); // Remove "0x" prefix.
                bool ok;
                nonUnaryInstruction->argument = new HexArgument(tokenString.toInt(&ok, 16));
                state = Asm::PS_ADDRESSING_MODE;
            }
            else if (token == Asm::LT_DEC_CONSTANT) {
                bool ok;
                int value = tokenString.toInt(&ok, 10);
                if ((-32768 <= value) && (value <= 65535)) {
                    if (value < 0) {
                        value += 65536; // Stored as two-byte unsigned.
                    }
                    nonUnaryInstruction->argument = new DecArgument(value);
                    state = Asm::PS_ADDRESSING_MODE;
                }
                else {
                    errorString = ";ERROR: Decimal constant is out of range (-32768..65535).";
                    return false;
                }
            }
            else if (token == Asm::LT_CHAR_CONSTANT) {
                nonUnaryInstruction->argument = new CharArgument(tokenString);
                state = Asm::PS_ADDRESSING_MODE;
            }
            else {
                errorString = ";ERROR: Operand specifier expected after mnemonic.";
                return false;
            }
            break;

        case Asm::PS_ADDRESSING_MODE:
            if (token == Asm::LT_ADDRESSING_MODE) {
                int addrMode = Asm::stringToAddrMode(tokenString);
                if ((addrMode & Pep::addrModesMap.value(localEnumMnemonic)) == 0) { // Nested parens required.
                    errorString = ";ERROR: Illegal addressing mode for this instruction.";
                    return false;
                }
                nonUnaryInstruction->addressingMode = addrMode;
                state = Asm::PS_CLOSE;
            }
            else if (Pep::addrModeRequiredMap.value(localEnumMnemonic)) {
                errorString = ";ERROR: Addressing mode required for this instruction.";
                return false;
            }
            else { // Must be branch type instruction with no addressing mode. Assign default addressing mode.
                nonUnaryInstruction->addressingMode = Pep::I;
                state = Asm::PS_CLOSE;
            }
            break;

        case Asm::PS_DOT_ADDRSS:
            if (token == Asm::LT_IDENTIFIER) {
                if (tokenString.length() > 8) {
                    errorString = ";ERROR: Symbol " + tokenString + " cannot have more than eight characters.";
                    return false;
                }
                dotAddress->argument = new SymbolRefArgument(tokenString);
                Asm::listOfReferencedSymbols.append(tokenString);
                Asm::listOfReferencedSymbolLineNums.append(lineNum);
                byteCount += 2;
                state = Asm::PS_CLOSE;
            }
            else {
                errorString = ";ERROR: .ADDRSS requires a symbol argument.";
                return false;
            }
            break;

        case Asm::PS_DOT_ASCII:
            if (token == Asm::LT_STRING_CONSTANT) {
                dotAscii->argument = new StringArgument(tokenString);
                byteCount += Asm::byteStringLength(tokenString);
                state = Asm::PS_CLOSE;
            }
            else {
                errorString = ";ERROR: .ASCII requires a string constant argument.";
                return false;
            }
            break;

        case Asm::PS_DOT_BLOCK:
            if (token == Asm::LT_DEC_CONSTANT) {
                bool ok;
                int value = tokenString.toInt(&ok, 10);
                if ((0 <= value) && (value <= 65535)) {
                    if (value < 0) {
                        value += 65536; // value stored as two-byte unsigned.
                    }
                    dotBlock->argument = new DecArgument(value);
                    byteCount += value;
                    state = Asm::PS_CLOSE;
                }
                else {
                    errorString = ";ERROR: Decimal constant is out of range (0..65535).";
                    return false;
                }
            }
            else if (token == Asm::LT_HEX_CONSTANT) {
                tokenString.remove(0, 2); // Remove "0x" prefix.
                bool ok;
                int value = tokenString.toInt(&ok, 16);
                dotBlock->argument = new HexArgument(value);
                byteCount += value;
                state = Asm::PS_CLOSE;
            }
            else {
                errorString = ";ERROR: .BLOCK requires a decimal or hex constant argument.";
                return false;
            }
            break;

        case Asm::PS_DOT_BURN:
            if (token == Asm::LT_HEX_CONSTANT) {
                tokenString.remove(0, 2); // Remove "0x" prefix.
                bool ok;
                int value = tokenString.toInt(&ok, 16);
                dotBurn->argument = new HexArgument(value);
                Pep::burnCount++;
                Pep::dotBurnByteCount = byteCount;
                Pep::dotBurnArgument = value;
                state = Asm::PS_CLOSE;
            }
            else {
                errorString = ";ERROR: .BURN requires a hex constant argument.";
                return false;
            }
            break;

        case Asm::PS_DOT_BYTE:
            if (token == Asm::LT_CHAR_CONSTANT) {
                dotByte->argument = new CharArgument(tokenString);
                byteCount += 1;
                state = Asm::PS_CLOSE;
            }
            else if (token == Asm::LT_DEC_CONSTANT) {
                bool ok;
                int value = tokenString.toInt(&ok, 10);
                if ((-128 <= value) && (value <= 255)) {
                    if (value < 0) {
                        value += 256; // value stored as one-byte unsigned.
                    }
                    dotByte->argument = new DecArgument(value);
                    byteCount += 1;
                    state = Asm::PS_CLOSE;
                }
                else {
                    errorString = ";ERROR: Decimal constant is out of byte range (-128..255).";
                    return false;
                }
            }
            else if (token == Asm::LT_HEX_CONSTANT) {
                tokenString.remove(0, 2); // Remove "0x" prefix.
                if (tokenString.length() <= 2) {
                    bool ok;
                    int value = tokenString.toInt(&ok, 16);
                    dotByte->argument = new HexArgument(value);
                    byteCount += 1;
                    state = Asm::PS_CLOSE;
                }
                else {
                    errorString = ";ERROR: Hex constant is out of byte range (0x00..0xFF).";
                    return false;
                }
            }
            else if (token == Asm::LT_STRING_CONSTANT) {
                if (Asm::byteStringLength(tokenString) > 1) {
                    errorString = ";ERROR: .BYTE string operands must have length one.";
                    return false;
                }
                dotByte->argument = new StringArgument(tokenString);
                byteCount += 1;
                state = Asm::PS_CLOSE;
            }
            else {
                errorString = ";ERROR: .BYTE requires a char, dec, hex, or string constant argument.";
                return false;
            }
            break;

        case Asm::PS_DOT_END:
            if (token == Asm::LT_COMMENT) {
                dotEnd->comment = tokenString;
                state = Asm::PS_FINISH;
            }
            else if (token == Asm::LT_EMPTY) {
                dotEnd->comment = "";
                state = Asm::PS_FINISH;
            }
            else {
                errorString = ";ERROR: Only a comment can follow .END.";
                return false;
            }
            break;

        case Asm::PS_DOT_EQUATE:
            if (dotEquate->symbolDef == "") {
                errorString = ";ERROR: .EQUATE must have a symbol definition.";
                return false;
            }
            else if (token == Asm::LT_DEC_CONSTANT) {
                bool ok;
                int value = tokenString.toInt(&ok, 10);
                if ((-32768 <= value) && (value <= 65535)) {
                    if (value < 0) {
                        value += 65536; // value stored as two-byte unsigned.
                    }
                    dotEquate->argument = new DecArgument(value);
                    Pep::symbolTable.insert(dotEquate->symbolDef, value);
                    state = Asm::PS_CLOSE;
                }
                else {
                    errorString = ";ERROR: Decimal constant is out of range (-32768..65535).";
                    return false;
                }
            }
            else if (token == Asm::LT_HEX_CONSTANT) {
                tokenString.remove(0, 2); // Remove "0x" prefix.
                bool ok;
                int value = tokenString.toInt(&ok, 16);
                dotEquate->argument = new HexArgument(value);
                Pep::symbolTable.insert(dotEquate->symbolDef, value);
                state = Asm::PS_CLOSE;
            }
            else if (token == Asm::LT_STRING_CONSTANT) {
                if (Asm::byteStringLength(tokenString) > 2) {
                    errorString = ";ERROR: .EQUATE string operand must have length at most two.";
                    return false;
                }
                dotEquate->argument = new StringArgument(tokenString);
                Pep::symbolTable.insert(dotEquate->symbolDef, Asm::string2ArgumentToInt(tokenString));
                state = Asm::PS_CLOSE;
            }
            else {
                errorString = ";ERROR: .EQUATE requires a dec, hex, or string constant argument.";
                return false;
            }
            break;

        case Asm::PS_DOT_WORD:
            if (token == Asm::LT_CHAR_CONSTANT) {
                dotWord->argument = new CharArgument(tokenString);
                byteCount += 2;
                state = Asm::PS_CLOSE;
            }
            else if (token == Asm::LT_DEC_CONSTANT) {
                bool ok;
                int value = tokenString.toInt(&ok, 10);
                if ((-32768 <= value) && (value <= 65535)) {
                    if (value < 0) {
                        value += 65536; // value stored as two-byte unsigned.
                    }
                    dotWord->argument = new DecArgument(value);
                    byteCount += 2;
                    state = Asm::PS_CLOSE;
                }
                else {
                    errorString = ";ERROR: Decimal constant is out of range (-32768..65535).";
                    return false;
                }
            }
            else if (token == Asm::LT_HEX_CONSTANT) {
                tokenString.remove(0, 2); // Remove "0x" prefix.
                bool ok;
                int value = tokenString.toInt(&ok, 16);
                dotWord->argument = new HexArgument(value);
                byteCount += 2;
                state = Asm::PS_CLOSE;
            }
            else if (token == Asm::LT_STRING_CONSTANT) {
                if (Asm::byteStringLength(tokenString) > 2) {
                    errorString = ";ERROR: .WORD string operands must have length at most two.";
                    return false;
                }
                dotWord->argument = new StringArgument(tokenString);
                byteCount += 2;
                state = Asm::PS_CLOSE;
            }
            else {
                errorString = ";ERROR: .WORD requires a char, dec, hex, or string constant argument.";
                return false;
            }
            break;

        case Asm::PS_CLOSE:
            if (token == Asm::LT_EMPTY) {
                state = Asm::PS_FINISH;
            }
            else if (token == Asm::LT_COMMENT) {
                code->comment = tokenString;
                state = Asm::PS_COMMENT;
            }
            else {
                errorString = ";ERROR: Comment expected following instruction.";
                return false;
            }
            break;

        case Asm::PS_COMMENT:
            if (token == Asm::LT_EMPTY) {
                state = Asm::PS_FINISH;
            }
            else {
                // This error should not occur, as all characters are allowed in comments.
                errorString = ";ERROR: Problem detected after comment.";
                return false;
            }
            break;

        default:
            break;
        }
    }
    while (state != Asm::PS_FINISH);
    code->memAddress = byteCount;
    return true;
}