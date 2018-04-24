#pragma once

#include <string>
#include <list>
#include <vector>

//#include "../ImGuiGf/IconsFontAwesome.h" // <- note required. just comment if not available

/** Show a file-io dialoge window, e.g. usable as file save and file close dialoge.
*/

struct ImVec2;

bool fileIOWindow(
    std::string& file_path,
    std::vector<std::string>& recently_used_files,
    const std::string& button_text,
    std::vector<std::string> file_filter,
    bool ensure_file_exists,
    ImVec2& size);



/** Helper class to deal with file paths.
*/
class MiniPath
{
private:
    std::string path;
    std::string name;

public:
    MiniPath( const std::string& some_path );
    MiniPath();

    void fromString (const std::string& file_path, char delim);
    void fromNameInCurrentDir( const std::string& file_name );

    std::string filePath()  const;
    std::string prefix()    const;
    std::string extension() const;
    std::string getName()   const;
    std::string getPath()   const;
    std::vector<std::string> getPathTokens() const;

    void setName( const std::string& name );
    bool setPath( const std::string& absolut_path );
    std::string getDelim() const;
   
    static std::string getCurrentDir();
    static std::string getSystemDelim();
    static bool isAbsoluteFilePath( const std::string& s );
    static std::list<std::string> listDirectories( const std::string& s );
    static std::list<std::string> listFiles( const std::string& s, std::string filter = "*.*" );	
    static bool pathExists( const std::string& s );
};


