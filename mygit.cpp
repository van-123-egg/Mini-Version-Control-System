#include <iostream>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <cstring>
#include <sstream>
#include <openssl/sha.h>
#include <zlib.h>

//------------------creating the directory------------------------

bool initiate_directory()
{
           try {
            // Create necessary directories
            std::filesystem::create_directory(".git");
            std::filesystem::create_directory(".git/objects");
            std::filesystem::create_directory(".git/refs");
            std::filesystem::create_directory(".git/refs/heads");
            std::filesystem::create_directory(".git/logs");
            std::filesystem::create_directory(".git/branches");
            std::filesystem::create_directory(".git/hooks");
            std::filesystem::create_directory(".git/info");

            // Create necessary files
            std::ofstream headFile(".git/HEAD");
            if (headFile.is_open()) {
                headFile << "ref: refs/heads/main\n";
                headFile.close();
            } else {
                std::cerr << "Failed to create .git/HEAD file.\n";
                return EXIT_FAILURE;
            }

            // Create the index file as a binary file
            std::ofstream indexFile(".git/index", std::ios::binary);
            if (!indexFile.is_open()) {
                std::cerr << "Failed to create .git/index file.\n";
                return EXIT_FAILURE;
            }
            // Write any necessary header or initialization for the index here
            indexFile.close();

            std::ofstream commitFile(".git/COMMIT_EDITMSG");
            if (!commitFile.is_open()) {
                std::cerr << "Failed to create .git/COMMIT_EDITMSG file.\n";
                return EXIT_FAILURE;
            }
            commitFile.close();

            std::ofstream configFile(".git/config");
            if (!configFile.is_open()) {
                std::cerr << "Failed to create .git/config file.\n";
                return EXIT_FAILURE;
            }
            configFile.close();

            std::ofstream descriptionFile(".git/description");
            if (!descriptionFile.is_open()) {
                std::cerr << "Failed to create .git/description file.\n";
                return EXIT_FAILURE;
            }
            descriptionFile.close();

            // Create packed-refs file, but leave it empty initially
            std::ofstream packed_refFile(".git/packed-refs");
            if (!packed_refFile.is_open()) {
                std::cerr << "Failed to create .git/packed-refs file.\n";
                return EXIT_FAILURE;
            }
            packed_refFile.close();

            std::cout << "Initialized git directory\n";
            return EXIT_SUCCESS;
        } catch (const std::filesystem::filesystem_error& e) {
            std::cerr << e.what() << '\n';
            return EXIT_FAILURE;
        }
}
//--------------------------computing the sha-----------------------------------------------------------------
std::string compute_sha1(const std::string& input) {
    unsigned char hash[SHA_DIGEST_LENGTH]; // SHA_DIGEST_LENGTH is 20
    SHA1(reinterpret_cast<const unsigned char*>(input.c_str()), input.size(), hash);

    std::stringstream ss;
    ss << std::hex << std::setfill('0'); // Set fill character for hex output

    // Convert each byte to a two-digit hex number
    for (int i = 0; i < SHA_DIGEST_LENGTH; ++i) {
        ss << std::setw(2) << static_cast<int>(hash[i]); // Use setw(2) to ensure two characters
    }
    
    return ss.str();
}


//-------------------------compression and decompression--------------------------------------------------------
std::vector<char> compress_data(const std::string& data) {
    uLongf compressedSize = compressBound(data.size());
    std::vector<char> compressedData(compressedSize);
    if (compress(reinterpret_cast<Bytef*>(compressedData.data()), &compressedSize,
                  reinterpret_cast<const Bytef*>(data.data()), data.size()) != Z_OK) {
        throw std::runtime_error("Compression failed.");
    }
    compressedData.resize(compressedSize);
    return compressedData;
}

std::string decompress_data(const std::string& compressedData) {
    uLongf uncompressedSize = compressedData.size() * 10; // Rough estimate
    std::vector<char> uncompressedData(uncompressedSize);

    int ret = uncompress(reinterpret_cast<Bytef*>(uncompressedData.data()), &uncompressedSize,
                         reinterpret_cast<const Bytef*>(compressedData.data()), compressedData.size());
    
    if (ret != Z_OK) {
        std::cerr << "Failed to decompress data. Error code: " << ret << '\n';
        switch (ret) {
            case Z_MEM_ERROR:
                std::cerr << "Memory error during decompression.\n";
                break;
            case Z_BUF_ERROR:
                std::cerr << "Buffer error during decompression.\n";
                break;
            case Z_DATA_ERROR:
                std::cerr << "Data error during decompression. Check if the data is corrupt.\n";
                break;
        }
        return "";
    }

    // Resize the uncompressed data vector to the actual size
    uncompressedData.resize(uncompressedSize);
    return std::string(uncompressedData.data(), uncompressedData.size());
}


//-------------------------------handling the ls-tree--------------------------
// Helper function to display parsed entries
void display_entries(const std::vector<std::tuple<std::string, std::string, std::string, std::string>>& entries, bool nameOnly) {
    for (const auto& entry : entries) {
        const auto& [mode, type, sha, name] = entry;
        if (nameOnly) {
            // std::cout<<"Only printing names!\n";
            std::cout << name << "\n";
        } else {
            std::cout << mode << " " << type << " " << sha << " " << name << "\n";
        }
    }
}


std::vector<std::tuple<std::string, std::string, std::string, std::string>> parse_tree_content(const std::string& content) {
    // std::cout << content << "\n";
    std::vector<std::tuple<std::string, std::string, std::string, std::string>> entries;

    // Find the position of the null character to separate the header
    size_t nullPos = content.find('\0');
    if (nullPos == std::string::npos) {
        return entries; // No valid header found
    }

    // Start parsing after the null character
    size_t pos = nullPos + 1;
    // std::cout<<content[pos]<<"\n";

    while (pos < content.size()) {
        // Extract mode (e.g., "100644" or "40000")
        size_t spacePos = content.find(' ', pos);
        if (spacePos == std::string::npos) break; // No more entries

        std::string mode = content.substr(pos, spacePos - pos);
        pos = spacePos + 1;
        // std::cout<<mode<<"\n";

        // Extract name (up to null terminator)
        size_t nextNullPos = content.find('\0', pos);
        if (nextNullPos == std::string::npos) break; // No more entries

        std::string name = content.substr(pos, nextNullPos - pos);
        pos = nextNullPos + 1;

        // Extract SHA-1 (20 bytes)
        if (pos + 40 > content.size()) break; // Not enough data for SHA
        std::string sha = content.substr(pos, 40);
        pos += 40;
        // std::cout<<"SHA: "<<sha<<"\n";

        // Determine type based on mode (file or directory)
        std::string type = (mode == "40000") ? "tree" : "blob";

        entries.emplace_back(mode, type, sha, name);
    }
    // std::cout<<"parsed data successfully\n";

    return entries;
}
void handle_ls_tree(const std::string& tree_sha, bool nameOnly) {
    // Construct the path to the tree object file
    std::string dir = ".git/objects/" + tree_sha.substr(0, 2);
    std::string fileName = dir + "/" + tree_sha.substr(2);
    
    // Open the object file to read the compressed data
    std::ifstream tree_objectFile(fileName, std::ios::binary);
    if (!tree_objectFile.is_open()) {
        std::cerr << "Could not open tree object file: " << fileName << "\n";
        return;
    }

    // Read the compressed file content
    std::string compressedData((std::istreambuf_iterator<char>(tree_objectFile)), std::istreambuf_iterator<char>());
    tree_objectFile.close();

    // Decompress tree object using the compressed data
    std::string decompressedContent = decompress_data(compressedData);
    if (decompressedContent.empty()) {
        std::cerr << "Failed to decompress tree object for SHA: " << tree_sha << "\n";
        return;
    }
    // std::cout<<"Decompressed data successfully!\n";

    // Parse tree content
    auto entries = parse_tree_content(decompressedContent);

    // Display entries
    display_entries(entries, nameOnly);
}

//-------------------hashing blob object---------------------------------------------------------------------------
std::string hashing_object(const std::string& filePath, bool write) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Could not open file: " << filePath << '\n';
        return "";
    }

    // Read the content of the file
    std::ostringstream oss;
    oss << file.rdbuf();
    std::string content = oss.str();
    file.close();

    // Format the content
    std::string formatted = "blob " + std::to_string(content.size()) + '\0' + content;

    // Compress the formatted content
    std::vector<char> compressed_vContent = compress_data(formatted);
    if (compressed_vContent.empty()) {
        return ""; // Compression failed
    }
    std::string compressedContent(compressed_vContent.data(), compressed_vContent.size());
    // std::cout<<compressedContent<<"\n";
    // Compute the SHA-1 hash of the formatted content (before compression)
    std::string sha1_hash = compute_sha1(formatted);
    // std::cout << sha1_hash.size()<<"\n";
    
    if (write) {
        // Create directory based on first 2 characters of SHA-1
        std::string dir = ".git/objects/" + sha1_hash.substr(0, 2);
        std::filesystem::create_directory(dir);

        // Create blob file name from last 38 characters of SHA-1
        std::string blobFileName = dir + "/" + sha1_hash.substr(2);

        // Write compressed content to blob file
        std::ofstream blobFile(blobFileName, std::ios::binary);
        if (!blobFile.is_open()) {
            std::cerr << "Failed to create blob file.\n";
            return "";
        }
        blobFile.write(compressedContent.c_str(), compressedContent.size());
        blobFile.close();

        std::cout << "Blob object created at: " << blobFileName << '\n';
    }

    return sha1_hash;
}

//----------------------- and handling write tree  ,hashing the tree object-----------------------------------------------------------------------

std::map<std::string, std::pair<std::string, std::string>> read_index(const std::string& indexFilePath);
struct IndexEntry {
    std::string mode;     // File mode (e.g., 100644)
    std::string sha;      // SHA-1 hash of the file
    std::string filename; // Extracted filename from the filepath
};
std::vector<IndexEntry> extractIndexEntries(const std::string& indexFilePath) {
    std::vector<IndexEntry> entries;
    std::ifstream indexFile(indexFilePath);

    if (!indexFile.is_open()) {
        std::cerr << "Failed to open index file: " << indexFilePath << '\n';
        return entries;
    }

    std::string line;
    while (std::getline(indexFile, line)) {
        std::istringstream iss(line);
        IndexEntry entry;

        // Extract mode, SHA, and ignore the stage
        iss >> entry.mode >> entry.sha;

        // Skip the stage (third column)
        std::string stage;
        iss >> stage;

        // Get the remaining part of the line as the filepath
        std::getline(iss, entry.filename);
        entry.filename = entry.filename.substr(1); // Remove leading space

        // Extract filename from the full filepath
        entry.filename = std::filesystem::path(entry.filename).filename().string();

        entries.push_back(entry);
    }

    indexFile.close();
    return entries;
}

void handle_write() {
    std::string indexFilePath = ".git/index"; // Path to the index file
    std::ifstream indexFile(indexFilePath);
    if (!indexFile.is_open()) {
        std::cerr << "Failed to open index file: " << indexFilePath << '\n';
        return;
    }
    // Check if the index file is empty
    if (indexFile.peek() == std::ifstream::traits_type::eof()) {
        // Handle empty index file: print SHA of null data
        std::string nullData = ""; // Null data is represented as an empty string
        std::string nullHash = compute_sha1(nullData);
        std::cout << "Nothing is in the index file so, null hash printed: " << nullHash << '\n';
        return; // Exit since there's nothing to process
    }

    std::ostringstream treeContent;

    // Read entries from the index file
    std::string line;
    while (std::getline(indexFile, line)) {
        std::istringstream iss(line);
        std::string mode, sha1, stage, path;
        iss >> mode >> sha1 >> stage >> path;

        // // Extract the filename from the path
        // std::string filename = std::filesystem::path(path).filename().string();
        // Extract filename after "./"
        std::string filename = path.substr(path.find("./") + 2); // Get content after "./"

        // Append to tree content in the required format
        treeContent << mode << ' ' << filename << '\0' << sha1; // Append mode, filename, and SHA1
    }
    indexFile.close();

    // Create the tree object in the required format
    std::string treeString = treeContent.str();
    std::string treeHeader = "tree " + std::to_string(treeString.size()) + '\0';
    std::string treeObjectContent = treeHeader + treeString;

    // Compute the SHA-1 hash of the tree object content
    std::string treeHash = compute_sha1(treeObjectContent);

    // Optionally, compress the tree object before writing it to the object store
    std::vector<char> compressedTreeObject = compress_data(treeObjectContent);
    if (compressedTreeObject.empty()) {
        std::cerr << "Failed to compress tree object.\n";
        return;
    }

    // Create directory for the tree object if it doesn't exist
    std::string dir = ".git/objects/" + treeHash.substr(0, 2);
    std::filesystem::create_directories(dir);

    // Write the compressed tree object to a file
    std::ofstream treeFile(dir + "/" + treeHash.substr(2), std::ios::binary);
    treeFile.write(compressedTreeObject.data(), compressedTreeObject.size());
    treeFile.close();

    // Print the SHA-1 hash of the tree
    std::cout << "Tree SHA-1: " << treeHash << '\n';
}


//-------------------handling the cat file-----------------------------------------------------------------------

void cat_file(const std::string& sha, const std::string& flag) {
    std::string dir = ".git/objects/" + sha.substr(0, 2);
    std::string fileName = dir + "/" + sha.substr(2);
    
    std::ifstream blob_objectFile(fileName, std::ios::binary);
    if (!blob_objectFile.is_open()) {
        std::cerr << "Could not open object file: " << fileName << '\n';
        return;
    }

    // Read the file content
    std::string compressedData((std::istreambuf_iterator<char>(blob_objectFile)), std::istreambuf_iterator<char>());
    blob_objectFile.close();

    // Decompress data using the new function
    std::string content = decompress_data(compressedData);
    if (content.empty()) {
        return; // Decompression failed
    }

    // Extract size and type
    std::string::size_type nullPos = content.find('\0');
    if (nullPos == std::string::npos) {
        std::cerr << "Invalid object format.\n";
        return;
    }
    std::string::size_type spacepos = content.find(' ');
     if (spacepos == std::string::npos) {
        std::cerr << "Invalid object format.\n";
        return;
    }

    std::string type = content.substr(0, spacepos); // should be "blob"
    std::string sizeStr = content.substr(5, nullPos - 5); // after "blob "
    size_t size = std::stoul(sizeStr);
    
    // Handle flags
    if (flag == "-p") {
        std::cout << content.substr(nullPos + 1); // actual content
    } else if (flag == "-s") {
        std::cout << size << '\n'; // size of the file
    } else if (flag == "-t") {
        std::cout << type << '\n'; // type of the file
    } else {
        std::cerr << "Unknown flag: " << flag << '\n';
    }
}


//-----------------------handling the git add------------------------------------------------------------------


std::string get_file_mode(const std::filesystem::path& filePath) {
    std::filesystem::perms permissions = std::filesystem::status(filePath).permissions();
    return ((permissions & std::filesystem::perms::owner_exec) != std::filesystem::perms::none) ? "100755" : "100644";
}
std::string tree_object(const std::string& dirPath) {
    std::ostringstream treeContent;

    for (const auto& entry : std::filesystem::directory_iterator(dirPath)) {
        std::string filePath = entry.path().string();
        std::string name = entry.path().filename().string();

        // Skip the .git directory
        if (name == ".git") {
            continue;
        }

        if (entry.is_regular_file()) {
            // Determine the mode (executable or non-executable)
            std::filesystem::perms permissions = std::filesystem::status(filePath).permissions();
            std::string blobHash = hashing_object(filePath, true);
            if ((permissions & std::filesystem::perms::owner_exec) != std::filesystem::perms::none) {
                // File is executable
                treeContent << "100755 " << name << '\0' << blobHash; // Executable mode
            } else {
                // File is non-executable
                treeContent << "100644 " << name << '\0' << blobHash; // Non-executable mode
            }
        } else if (entry.is_directory()) {
            // Recursive call for subdirectory
            std::string subTreeHash = tree_object(filePath);
            treeContent << "40000 " << name << '\0' << subTreeHash; // Directory mode
        }
    }

    std::string treeString = treeContent.str();
    std::string treeHeader = "tree " + std::to_string(treeString.size()) + '\0';
    std::string treeObjectContent = treeHeader + treeString;
std::cout << "Formatted string (with null character): ";
    for (char c : treeObjectContent) {
        if (c == '\0') {
            std::cout << "\\0";  // Display the null character as \0
        } else {
            std::cout << c;
        }
    }
    // std::cout << std::endl;
    // std::cout<<"Tree Header: "<<treeHeader<<"\n";
    // std::cout<<"Tree String: "<<treeString<<"\n";
    // std::cout<<"Full Tree Object content Size: "<<treeObjectContent.size()<<"\n";

    // Compress the tree object content
    std::vector<char> compressedTreeObject = compress_data(treeObjectContent);
    if (compressedTreeObject.empty()) {
        std::cerr << "Failed to compress tree object.\n";
        return "";
    }

    // Compute the SHA-1 hash of the compressed tree object content
    std::string treeHash = compute_sha1(std::string(compressedTreeObject.data(), compressedTreeObject.size()));

    // Create the directory for the tree object if it doesn't exist
    std::string dir = ".git/objects/" + treeHash.substr(0, 2);
    std::filesystem::create_directories(dir);

    // Write the compressed tree object to a file
    std::ofstream treeFile(dir + "/" + treeHash.substr(2), std::ios::binary);
    treeFile.write(compressedTreeObject.data(), compressedTreeObject.size());
    treeFile.close();

    return treeHash; // Return the SHA-1 hash of the tree object
}


void update_index(const std::string& indexFilePath, const std::filesystem::path& execDir) {
    std::map<std::string, std::pair<std::string, std::string>> indexEntries; // To hold mode and SHA1 by path

    // Load existing index entries into memory
    std::ifstream indexFile(indexFilePath);
    if (indexFile.is_open()) {
        std::string line;
        while (std::getline(indexFile, line)) {
            std::istringstream iss(line);
            std::string mode, sha1, stage, path;
            iss >> mode >> sha1 >> stage >> path;
            indexEntries[path] = {mode, sha1}; // Store mode and SHA1
        }
        indexFile.close();
    }

    // Traverse files and directories
    for (const auto& entry : std::filesystem::recursive_directory_iterator(execDir)) {
        if (entry.path().string().find(".git") != std::string::npos) {
            continue;
        }
        std::string mode;
        std::string filePath = entry.path().string();
        std::string name = entry.path().filename().string();
        
        if (entry.is_regular_file()) { // Process only files
            // Read file content to compute SHA-1
            mode = get_file_mode(entry.path()); // Assume this function exists

            std::ifstream file(entry.path(), std::ios::binary);
            std::ostringstream oss;
            oss << file.rdbuf();
            std::string content = oss.str();
            std::string formattedBlob = "blob " + std::to_string(content.size()) + '\0' + content;
            std::string sha1_hash = compute_sha1(formattedBlob);

            // Check if the SHA-1 already exists in the index
            auto it = indexEntries.find(filePath);
            if (it == indexEntries.end() || it->second.second != sha1_hash) {
                // New or different SHA-1, create blob object if it does not exist
                // Compress the formatted content
                std::vector<char> compressed_vContent = compress_data(formattedBlob);
                if (compressed_vContent.empty()) {
                    return; // Compression failed
                }
                std::string compressedContent(compressed_vContent.data(), compressed_vContent.size());
                // std::cout<<compressedContent<<"\n";
                // Compute the SHA-1 hash of the formatted content (before compression)
                 // Create directory based on first 2 characters of SHA-1
                std::string dir = ".git/objects/" + sha1_hash.substr(0, 2);
                std::filesystem::create_directory(dir);

                // Create blob file name from last 38 characters of SHA-1
                std::string blobFileName = dir + "/" + sha1_hash.substr(2);

                // Write compressed content to blob file
                std::ofstream blobFile(blobFileName, std::ios::binary);
                if (!blobFile.is_open()) {
                    std::cerr << "Failed to create blob file.\n";
                    return;
                }
                blobFile.write(compressedContent.c_str(), compressedContent.size());
                blobFile.close();

                indexEntries[filePath] = {mode, sha1_hash}; // Add or update entry
                std::cout << "Added/Updated to index: " << filePath << " with SHA-1: " << sha1_hash << '\n';
            }
        } else if (entry.is_directory()) { // Process directories
            // Create a tree object
            std::string treeHash = tree_object(entry.path().string());
            mode = "040755"; 

            // Check if the treeHash already exists in the index
            auto it = indexEntries.find(filePath);
            if (it == indexEntries.end() || it->second.second != treeHash) {
                // New or different SHA-1, create tree object if it does not exist
                indexEntries[filePath] = {mode, treeHash}; // Add or update entry
                std::cout << "Added/Updated directory to index: " << filePath << " with tree SHA-1: " << treeHash << '\n';
            }
        }
    }

    // Write updated index back to file in text format
    std::ofstream outIndexFile(indexFilePath, std::ios::trunc);
    for (const auto& [path, entry] : indexEntries) {
        const auto& [mode, sha1] = entry;
        outIndexFile << mode << " " << sha1 << " 0 " << path << '\n'; // Write in the specified format
    }
    outIndexFile.close();
}

void add_files_to_index(const std::vector<std::string>& filesToAdd, const std::string& indexFilePath, const std::filesystem::path& execDir) {

     std::map<std::string, std::pair<std::string, std::string>> indexEntries; // To hold mode and SHA1 by full path

    // Load existing index entries into memory
    std::ifstream indexFile(indexFilePath);
    if (indexFile.is_open()) {
        std::string line;
        while (std::getline(indexFile, line)) {
            std::istringstream iss(line);
            std::string mode, sha1, stage, path;
            iss >> mode >> sha1 >> stage >> path;
            indexEntries[path] = {mode, sha1}; // Store mode and SHA1 using full path
        }
        indexFile.close();
    }

    // Process specified files
    for (const auto& file : filesToAdd) {
        std::filesystem::path filePath = execDir / file; // Create full path
        if (!std::filesystem::exists(filePath)) {
            std::cout << "File not found: " << filePath << '\n';
            continue; // Skip if the file does not exist
        }

        // Get full path
        std::string fullFilePath = filePath.string();

        // Get file mode and calculate SHA-1
        std::string mode = get_file_mode(filePath);
        std::ifstream fileStream(filePath, std::ios::binary);
        std::ostringstream oss;
        oss << fileStream.rdbuf();
        std::string content = oss.str();
        std::string formattedBlob = "blob " + std::to_string(content.size()) + '\0' + content;
        std::string sha1 = compute_sha1(formattedBlob);

        // Create the blob object file
        std::vector<char> compressed_vContent = compress_data(formattedBlob);
        if (compressed_vContent.empty()) {
            std::cerr << "Compression failed for file: " << fullFilePath << '\n';
            return; // Compression failed
        }
        std::string compressedContent(compressed_vContent.data(), compressed_vContent.size());

        // Create directory based on first 2 characters of SHA-1
        std::string dir = ".git/objects/" + sha1.substr(0, 2);
        std::filesystem::create_directories(dir); // Ensure the directory exists

        // Create blob file name from last 38 characters of SHA-1
        std::string blobFileName = dir + "/" + sha1.substr(2);

        // Write compressed content to blob file
        std::ofstream blobFile(blobFileName, std::ios::binary);
        if (!blobFile.is_open()) {
            std::cerr << "Failed to create blob file for: " << fullFilePath << '\n';
            return;
        }
        blobFile.write(compressedContent.c_str(), compressedContent.size());
        blobFile.close();

        // Check if the SHA-1 already exists in the index
        auto it = indexEntries.find(fullFilePath);
        if (it == indexEntries.end() || it->second.second != sha1) {
            // New or different SHA-1
            indexEntries[fullFilePath] = {mode, sha1}; // Add or update entry with full path
            std::cout << "Added/Updated to index: " << fullFilePath << " with SHA-1: " << sha1 << '\n';
        }
    }

    // Write updated index back to file in text format
    std::ofstream outIndexFile(indexFilePath, std::ios::trunc);
    for (const auto& [path, entry] : indexEntries) {
        const auto& [mode, sha1] = entry;
        outIndexFile << mode << " " << sha1 << " 0 " << path << '\n'; // Write in the specified format
    }
    outIndexFile.close();
}

void handle_add(int argc,char* argv[])
{
   if (argc < 2) {
        std::cerr << "No files or directories specified.\n";
        return; 
   }
      // Get the executable directory
    std::filesystem::path execDirPath = std::filesystem::absolute(argv[0]).parent_path();

    // Construct the index file path
    std::string indexFilePath = (execDirPath / ".git" / "index").string();

   std::string option = argv[2];
    if(option == ".")
    {
     
    // Now you can call update_index with these paths
    update_index(indexFilePath, execDirPath.string());
    
    }
    else {
        // Collect the files to add to the index
        std::vector<std::string> filesToAdd;
        for (int i = 2; i < argc; ++i) {
            std::string filePath = argv[i];
            if (std::filesystem::exists(filePath)) {
                filesToAdd.push_back(filePath);
            } else {
                std::cerr << "File not found: " << filePath << '\n';
            }
        }

        // Call add_files_to_index with the collected files
        add_files_to_index(filesToAdd, indexFilePath, execDirPath);
    }
}



//------------------------------------------------------------------------------------------------------------
//-----------------------------------------------handling the git commit------------------------------------------------------

//function to get last line so that previous tree hash can be made as parent hash
void getLastLine(const std::string& filePath, std::string& lastLine) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "Could not open the file: " << filePath << std::endl;
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        lastLine = line; // Update lastLine with the current line
    }

    file.close();
}
void prependToLogFile(const std::string& logFilePath, const std::string& commitLog) {
    // Check if the log file exists
    if (!std::filesystem::exists(logFilePath)) {
        // Create the log file if it does not exist
        std::ofstream logFile(logFilePath);
        logFile.close();
    }

    // Read existing contents of the log file
    std::ifstream inFile(logFilePath);
    std::string existingContents;

    if (inFile) {
        existingContents.assign((std::istreambuf_iterator<char>(inFile)),
                                 std::istreambuf_iterator<char>());
        inFile.close();
    }

    // Open the log file in write mode to overwrite it
    std::ofstream outFile(logFilePath, std::ios::trunc);
    if (outFile) {
        // Write the new commit log followed by existing contents
        outFile << commitLog << "\n" << existingContents;
        outFile.close();
    } else {
        std::cerr << "Error opening log file for writing.\n";
    }
}

// Function to handle the commit command

void write_object(const std::string& objectHash, const std::string& content) {
    // Compress the data before writing
    std::vector<char> compressedData = compress_data(content);
    
    // Write the object to .git/objects/<two_chars>/<rest_of_hash>
    std::string dir = ".git/objects/" + objectHash.substr(0, 2);
    std::filesystem::create_directories(dir);
    
    std::ofstream outFile(dir + "/" + objectHash.substr(2), std::ios::binary);
    outFile.write(compressedData.data(), compressedData.size());
    outFile.close();
}

void handle_commit(const std::string& commitMessage, const std::string& indexFilePath) {
    // Define paths
    std::filesystem::path gitDir = ".git";
    std::filesystem::path logsDir = ".git/logs";
    std::filesystem::path logFilePath = gitDir / "logs/log";
    std::filesystem::path headFilePath = logsDir / "HEAD";
    std::filesystem::path objectsDir = gitDir / "objects";

    // // Ensure logs and heads directories exist
    // std::filesystem::create_directories(headsDir);

    

    // Read existing index entries
    auto indexEntries = read_index(indexFilePath);
    if (indexEntries.empty()) {
        std::cerr << "No files to commit.\n";
        return;
    }

    // Create tree content
    std::ostringstream treeContent;
    // for (const auto& entry : indexEntries) {
    //     const auto& [mode, sha1] = entry.second; // mode and sha1
    //     const std::string& filename = entry.first; // filename

    //     treeContent << mode << " " << filename << '\0' << sha1; // Tree entry
    // }
    std::vector<IndexEntry> entries = extractIndexEntries(indexFilePath);

    // Print the extracted entries
    for (const auto& entry : entries) {
        treeContent << entry.mode << " " << entry.filename <<'\0' << entry.sha;
    }

    std::string treeData = treeContent.str();
    std::string treeHeader = "tree " + std::to_string(treeData.size()) + '\0';
    std::string treeObjectContent = treeHeader + treeData;

    // Compute SHA for the tree
    std::string treeHash = compute_sha1(treeObjectContent);
    // Compress the formatted content
                std::vector<char> compressed_vContent = compress_data(treeObjectContent);
                if (compressed_vContent.empty()) {
                    return; // Compression failed
                }
                std::string compressedContent(compressed_vContent.data(), compressed_vContent.size());
                // std::cout<<compressedContent<<"\n";
                // Compute the SHA-1 hash of the formatted content (before compression)
                 // Create directory based on first 2 characters of SHA-1
                std::string dir = ".git/objects/" + treeHash.substr(0, 2);
                std::filesystem::create_directory(dir);

                // Create blob file name from last 38 characters of SHA-1
                std::string blobFileName = dir + "/" + treeHash.substr(2);

                // Write compressed content to blob file
                std::ofstream blobFile(blobFileName, std::ios::binary);
                if (!blobFile.is_open()) {
                    std::cerr << "Failed to create blob file.\n";
                    return;
                }
                blobFile.write(compressedContent.c_str(), compressedContent.size());
                blobFile.close();

    //adding information to the heads file in logs directory to track the parent hash.
    std::string parentHash = "000000000000000000000000000000000000000000"; // Default to zero for first commit
    std::string extractparentHash;
    
    // Create HEAD file if it does not exist
        if (!std::filesystem::exists(headFilePath)) {
            std::ofstream headFile(headFilePath);
            headFile.close();
        } 
        else {
            // Open the HEAD file in read mode to get the last commit's parent hash
            // std::ifstream headFile(headFilePath);
            std::string lastLine;
            getLastLine(headFilePath,lastLine);
            // std::cout<<"ateast headfile is not empty!\n";
            // headFile.close();

            // If there's a last line, extract the parent hash from it
            if (!lastLine.empty()) {
                std::istringstream iss(lastLine);
                std::string commitType, commitHash, author, date, commitMessage;

                // Read the commit type and hash
                iss >> commitType >> commitHash;

                // Now read the parent hash, author, date, and commit message
                std::string line;
                std::getline(iss, line); // Get the remaining line for author and message

                // Assume the parent hash is the commitHash extracted earlier
                parentHash = commitHash;

                // Now we can also extract author and message if needed
                // std::cout << "Parent Hash: " << parentHash << "\n";
                // std::cout << "Commit Line: " << line << "\n";
            }
        }
            // parentHash = extractparentHash;

            // Open the HEAD file in append mode
            std::ofstream headFile(headFilePath, std::ios::app);
            if (!headFile.is_open()) {
                std::cerr << "Failed to open HEAD file for appending.\n";
                return;
            }

            // Get current timestamp
            auto now = std::chrono::system_clock::now();
            std::time_t currentTime = std::chrono::system_clock::to_time_t(now);
            std::string timestamp = std::to_string(currentTime);
            std::string timezone = "+0530"; // Modify this if you need dynamic timezone handling
            std::string authorName = "Vanshika Singh <vanshisingh2502@gmail.com>";

    // Create the commit information string
    headFile << parentHash << " " 
             << treeHash << " " 
             << authorName << " " 
             << timestamp << " " 
             << timezone << "\tcommit: " << commitMessage << '\n';

    headFile.close();

    //creating the commit hash
    // Create the formatted content string
    std::ostringstream content;
    content << "tree " << treeHash << "\n"
            << "parent " << parentHash << "\n"
            << "author " << authorName << " " << timestamp << " " << timezone << "\n"
            << commitMessage << "\n";
    std::string formatted = content.str();
    std::cout<<formatted<<"\n";
    std::string commitHash = compute_sha1(formatted);
    //writing this commit object in the objects directory 
    write_object(commitHash, formatted);


    // Get current timestamp
    std::tm* localTime = std::localtime(&currentTime);
    
    // Format the date string
    std::ostringstream dateStream;
    dateStream << std::put_time(localTime, "%a %b %d %H:%M:%S %Y %z");

    // Create formatted commit log entry
    std::ostringstream commitLog;
    commitLog << "commit " << commitHash << "\n"
              << "Author: " << authorName << "\n"
              << "Date:   " << dateStream.str() << "\n\n"
              << "    " << commitMessage << "\n\n";


    std::cout<<"Now going to print the log file content!\n";
    std::cout<<commitLog.str()<<"\n";
    // //creating log file for keeping commit history 
    prependToLogFile(logFilePath,commitLog.str());

    std::cout << "Committed with hash: " << commitHash << '\n';
}

        // Read the index and return a map of filename to (mode, sha1)
        std::map<std::string, std::pair<std::string, std::string>> read_index(const std::string& indexFilePath) {
            std::map<std::string, std::pair<std::string, std::string>> indexEntries;
            std::ifstream indexFile(indexFilePath);
            if (indexFile.is_open()) {
                std::string line;
                while (std::getline(indexFile, line)) {
                    std::istringstream iss(line);
                    std::string mode, sha1, stage, path;
                    iss >> mode >> sha1 >> stage >> path;
                    indexEntries[path] = {mode, sha1}; // Store mode and SHA1 by path
                }
                indexFile.close();
            }
            return indexEntries;
}
//-------------------------------------------------------handling checkout-------------------------------------------------------
void handle_checkout(const std::string& commitSha) {
    // Path to the commit object file
    std::string objectFilePath = ".git/objects/" + commitSha.substr(0, 2) + "/" + commitSha.substr(2);

    // Read the compressed commit object
    std::ifstream commitFile(objectFilePath, std::ios::binary);
    if (!commitFile.is_open())
    {
        std::cerr << "Failed to open commit object file: " << objectFilePath << '\n';
        return;
    }

    // Read the entire file into a string
    std::string compressedData((std::istreambuf_iterator<char>(commitFile)),
                               std::istreambuf_iterator<char>());
    commitFile.close();

    // Decompress the data
    std::string decompressedData = decompress_data(compressedData);

    // Parse the decompressed data
    std::istringstream iss(decompressedData);
    std::string line;
    std::string keyword, treeHash;

    // Read the first line to get the tree hash
    if (std::getline(iss, line))
    {
        std::istringstream firstLineStream(line);
        firstLineStream >> keyword >> treeHash; // Expecting format: "tree treehash"

        if (keyword == "tree")
        {
            std::cout << "Tree SHA-1: " << treeHash << '\n';
        }
        else
        {
            std::cerr << "Unexpected format in commit object: " << line << '\n';
        }
    }
    else
    {
        std::cerr << "Commit object is empty or not valid.\n";
    }
    std::string parentHash = treeHash; 

    // Construct the path to the blob object files
    std::string blobDir = ".git/objects/";
    
    // Read the object associated with the parent hash
    std::string parentBlobFilePath = blobDir + parentHash.substr(0, 2) + "/" + parentHash.substr(2);
    std::ifstream parentBlobFile(parentBlobFilePath, std::ios::binary);
    
    if (!parentBlobFile.is_open()) {
        std::cerr << "Could not open tree object file for hash: " << parentHash << '\n';
        return;
    }

        // Read and decompress the parent blob object 
        std::ostringstream compressedStream;
        compressedStream << parentBlobFile.rdbuf();
        std::string compressedContent = compressedStream.str();
        std::string decompressedContent = decompress_data(compressedContent); // Assuming this returns std::string

        // Parse the decompressed tree content to extract entries
        std::vector<std::tuple<std::string, std::string, std::string, std::string>> entries = parse_tree_content(decompressedContent);

        for (const auto& [mode, type, sha, name] : entries) {
            if (type == "blob") {
            // Construct the path to the blob file
            std::string blobFilePath = blobDir + sha.substr(0, 2) + "/" + sha.substr(2);
            std::ifstream blobFile(blobFilePath, std::ios::binary);

            if (!blobFile.is_open()) {
                std::cerr << "Could not open blob file for hash: " << sha << '\n';
                continue;
            }

            // Read and decompress the blob object
            std::ostringstream blobStream;
            blobStream << blobFile.rdbuf();
            std::string compressedBlobContent = blobStream.str();
            std::string blobDecompressedContent = decompress_data(compressedBlobContent); // Now returns std::string

            // Find the null character and extract content after it
            size_t nullPos = blobDecompressedContent.find('\0');
            std::string actualContent;
            if (nullPos != std::string::npos) {
                actualContent = blobDecompressedContent.substr(nullPos + 1); // Get content after the null character
            } else {
                actualContent = blobDecompressedContent; // If no null character, use the whole content
            }

            // Write the decompressed content back to the actual file
            std::ofstream outputFile(name, std::ios::binary);
            if (outputFile.is_open()) {
                outputFile.write(actualContent.c_str(), actualContent.size()); // Using c_str() for string
                outputFile.close();
                std::cout << "Restored file: " << name << " from hash: " << sha << '\n';
            } else {
                std::cerr << "Could not write to file: " << name << '\n';
            }
}
}

}

//--------------------------------------------------------------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    // Flush after every std::cout / std::cerr
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    if (argc < 2) {
        std::cerr << "No command provided.\n";
        return EXIT_FAILURE;
    }

    std::string command = argv[1];

    if (command == "init") {
 
        bool status = initiate_directory();
        if(status == EXIT_FAILURE)
        {
            std::cerr<<"Error in creating directory!\n";
        }

    }
    else if(command == "hash-object")
    {
        bool write = (argv[2]!= nullptr && std::string(argv[2]) == "-w");
        std::string filePath = argv[write ? 3 : 2]; 
        std::cout<<"SHA-1 hash: "<<hashing_object(filePath,write)<<"\n";

    }
    else if (command == "cat-file") {
        if (argc < 4) {
            std::cerr << "Usage: ./mygit cat-file <sha> <-p|-s|-t>\n";
            return EXIT_FAILURE;
        }
        std::string sha = argv[3];
        std::string flag = argv[2];
        cat_file(sha, flag);
    }
    else if (command == "write-tree") {
        handle_write();
    }
    else if (command == "ls-tree")
    {
        bool name_only = (argv[2]!=nullptr && std::string(argv[2]) == "--name-only");
        std:: string hash = argv[name_only ? 3 : 2];
        handle_ls_tree(hash,name_only);
    }
    else if(command == "add")
    {
        handle_add(argc,argv);
    }
    else if(command == "commit")
    {
        std::string commitMessage = "new commit done"; // Default commit message
        // Check for -m flag
        for (int i = 2; i < argc; ++i) {
            if (std::string(argv[i]) == "-m" && i + 1 < argc) {
                commitMessage = argv[i + 1]; // Get the commit message
                break;
            }
        }

        // Assuming the index file path is known (you might need to adjust this based on your structure)
        std::string indexFilePath = ".git/index"; // Adjust as needed
        handle_commit(commitMessage, indexFilePath);
    }
    else if(command == "log")
    {
        // Define the path to the log file
    std::string logFilePath = ".git/logs/log"; // Adjust this path if needed

    // Open the log file
    std::ifstream logFile(logFilePath);
    
    // Check if the log file was opened successfully
    if (!logFile.is_open()) {
        std::cerr << "Error: Could not open log file: " << logFilePath << "\n";
    }

    // Read and print the contents of the log file
    std::string line;
    while (std::getline(logFile, line)) {
        std::cout << line << '\n'; // Output each line
    }

    // Close the log file
    logFile.close();
    }
    else if(command == "checkout")
    {
        std::string headFilePath = ".git/logs/HEAD";

        // Check if a commit SHA was provided
        if (argc < 3)
        {
            std::cerr << "Error: Commit SHA is required.\n";
        }

        std::string commitSha = argv[2]; // Get the commit SHA from command line arguments
        handle_checkout(commitSha);
    }
    return EXIT_SUCCESS;
}
