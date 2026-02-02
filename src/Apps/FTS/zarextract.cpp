#include "zarextract.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "gamez/zArchive/zar.h"

namespace
{
    struct ExtractOptions
    {
        std::string zar_path;
        std::string out_dir;
        size_t padded_size = 16;
    };

    std::string SanitizeName(const char* name)
    {
        std::string sanitized = name ? name : "unnamed";
        for (char& ch : sanitized)
        {
            if (ch == '/' || ch == '\\' || ch == ':' || ch == '*'
                || ch == '?' || ch == '"' || ch == '<' || ch == '>' || ch == '|')
            {
                ch = '_';
            }
        }
        return sanitized;
    }

    bool ParseArgs(int argc, char** argv, ExtractOptions& options)
    {
        for (int i = 1; i < argc; ++i)
        {
            std::string arg = argv[i];
            if (arg == "--extract-zar" && i + 1 < argc)
            {
                options.zar_path = argv[++i];
            }
            else if (arg == "--out" && i + 1 < argc)
            {
                options.out_dir = argv[++i];
            }
            else if (arg == "--pad" && i + 1 < argc)
            {
                options.padded_size = static_cast<size_t>(std::stoul(argv[++i]));
            }
        }

        return !options.zar_path.empty();
    }

    bool WriteKeyData(zar::CZAR& archive, zar::CKey* key, const std::filesystem::path& out_path)
    {
        const s32 size = key->GetSize();
        if (size <= 0)
        {
            return true;
        }

        std::vector<unsigned char> buffer(static_cast<size_t>(size));
        if (!archive.Fetch(key, buffer.data(), buffer.size()))
        {
            return false;
        }

        std::filesystem::create_directories(out_path.parent_path());
        std::ofstream output(out_path, std::ios::binary);
        if (!output)
        {
            return false;
        }

        output.write(reinterpret_cast<const char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
        return output.good();
    }

    void ExtractKeyTree(zar::CZAR& archive, zar::CKey* key, const std::filesystem::path& base_dir)
    {
        const std::string name = SanitizeName(key->GetName());
        const std::filesystem::path current_path = base_dir / name;
        const bool has_children = key->size() > 0;

        if (has_children)
        {
            std::filesystem::create_directories(current_path);
        }

        if (key->GetSize() > 0)
        {
            const std::filesystem::path data_path = has_children ? (current_path / "__data.bin") : current_path;
            if (!WriteKeyData(archive, key, data_path))
            {
                std::cerr << "Failed to write key: " << name << "\n";
            }
        }

        for (auto it = key->begin(); it != key->end(); ++it)
        {
            ExtractKeyTree(archive, *it, current_path);
        }
    }

    void PrintUsage()
    {
        std::cout << "Usage: fts --extract-zar <file> --out <directory> [--pad <bytes>]\n";
    }
}

bool TryRunZarExtractor(int argc, char** argv)
{
    ExtractOptions options;
    if (!ParseArgs(argc, argv, options))
    {
        return false;
    }

    if (options.out_dir.empty())
    {
        PrintUsage();
        return true;
    }

    zar::CZAR archive;
    if (!archive.Open(options.zar_path.c_str(), 0, 0, options.padded_size))
    {
        std::cerr << "Failed to open ZAR file: " << options.zar_path << "\n";
        return true;
    }

    std::filesystem::create_directories(options.out_dir);
    if (!archive.m_root)
    {
        std::cerr << "ZAR archive has no root directory entries.\n";
        return true;
    }

    for (auto it = archive.m_root->begin(); it != archive.m_root->end(); ++it)
    {
        ExtractKeyTree(archive, *it, options.out_dir);
    }

    std::cout << "Extraction complete to: " << options.out_dir << "\n";
    return true;
}
