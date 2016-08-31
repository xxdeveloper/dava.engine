#pragma once
#include <string>

namespace sl
{
class Allocator;

/**
 * This class is used for outputting code. It handles indentation and inserting #line markers
 * to match the desired output line numbers.
 */
class CodeWriter
{
public:
    explicit CodeWriter(std::string* buf = nullptr);

    void BeginLine(int indent, const char* fileName = nullptr, int lineNumber = -1);
    void Write(const char* format, ...);
    void EndLine(const char* text = NULL);

    void WriteLine(int indent, const char* format, ...);
    void WriteLine(int indent, const char* fileName, int lineNumber, const char* format, ...);

    const char* GetResult() const;
    void Reset(std::string* buf = nullptr);
    void EnableLineNumbers(bool enable);

private:
    //    Allocator* m_allocator;
    std::string ownBuffer;
    std::string* m_buffer;
    int m_currentLine;
    const char* m_currentFileName;
    int m_spacesPerIndent;
    bool m_writeLines;
    bool m_writeFileNames;
};

} // namespace sl
