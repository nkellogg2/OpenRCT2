/*****************************************************************************
 * Copyright (c) 2014-2020 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "TitleSequence.h"

#include "../common.h"
#include "../core/Collections.hpp"
#include "../core/Console.hpp"
#include "../core/File.h"
#include "../core/FileScanner.h"
#include "../core/FileStream.h"
#include "../core/Guard.hpp"
#include "../core/Memory.hpp"
#include "../core/MemoryStream.h"
#include "../core/Path.hpp"
#include "../core/String.hpp"
#include "../core/StringBuilder.h"
#include "../core/Zip.h"
#include "../scenario/ScenarioRepository.h"
#include "../scenario/ScenarioSources.h"
#include "../util/Util.h"

#include <algorithm>
#include <memory>
#include <vector>

static std::vector<std::string> GetSaves(const std::string& path);
static std::vector<std::string> GetSaves(IZipArchive* zip);
static std::vector<TitleCommand> LegacyScriptRead(const std::vector<uint8_t>& script, std::vector<std::string> saves);
static void LegacyScriptGetLine(OpenRCT2::IStream* stream, char* parts);
static std::vector<uint8_t> ReadScriptFile(const std::string& path);
static std::string LegacyScriptWrite(const TitleSequence& seq);

std::unique_ptr<TitleSequence> CreateTitleSequence()
{
    return std::make_unique<TitleSequence>();
}

std::unique_ptr<TitleSequence> LoadTitleSequence(const std::string& path)
{
    std::vector<uint8_t> script;
    std::vector<std::string> saves;
    bool isZip;

    log_verbose("Loading title sequence: %s", path.c_str());

    auto ext = Path::GetExtension(path);
    if (String::Equals(ext, TITLE_SEQUENCE_EXTENSION))
    {
        auto zip = Zip::TryOpen(path, ZIP_ACCESS::READ);
        if (zip == nullptr)
        {
            Console::Error::WriteLine("Unable to open '%s'", path.c_str());
            return nullptr;
        }

        script = zip->GetFileData("script.txt");
        if (script.empty())
        {
            Console::Error::WriteLine("Unable to open script.txt in '%s'", path.c_str());
            return nullptr;
        }

        saves = GetSaves(zip.get());
        isZip = true;
    }
    else
    {
        auto scriptPath = Path::Combine(path, "script.txt");
        script = ReadScriptFile(scriptPath);
        if (script.empty())
        {
            Console::Error::WriteLine("Unable to open '%s'", scriptPath.c_str());
            return nullptr;
        }

        saves = GetSaves(path);
        isZip = false;
    }

    auto commands = LegacyScriptRead(script, saves);

    auto seq = CreateTitleSequence();
    seq->Name = Path::GetFileNameWithoutExtension(path);
    seq->Path = path;
    seq->Saves = saves;
    seq->Commands = commands;
    seq->IsZip = isZip;
    return seq;
}

std::unique_ptr<TitleSequenceParkHandle> TitleSequenceGetParkHandle(const TitleSequence& seq, size_t index)
{
    std::unique_ptr<TitleSequenceParkHandle> handle;
    if (index <= seq.Saves.size())
    {
        const auto& filename = seq.Saves[index];
        if (seq.IsZip)
        {
            auto zip = Zip::TryOpen(seq.Path, ZIP_ACCESS::READ);
            if (zip != nullptr)
            {
                auto data = zip->GetFileData(filename);
                auto ms = std::make_unique<OpenRCT2::MemoryStream>();
                ms->Write(data.data(), data.size());
                ms->SetPosition(0);

                handle = std::make_unique<TitleSequenceParkHandle>();
                handle->Stream = std::move(ms);
                handle->HintPath = filename;
            }
            else
            {
                Console::Error::WriteLine("Failed to open zipped path '%s' from zip '%s'", filename.c_str(), seq.Path.c_str());
            }
        }
        else
        {
            auto absolutePath = Path::Combine(seq.Path, filename);
            std::unique_ptr<OpenRCT2::IStream> fileStream = nullptr;
            try
            {
                fileStream = std::make_unique<OpenRCT2::FileStream>(absolutePath, OpenRCT2::FILE_MODE_OPEN);
            }
            catch (const IOException& exception)
            {
                Console::Error::WriteLine(exception.what());
            }

            if (fileStream != nullptr)
            {
                handle = std::make_unique<TitleSequenceParkHandle>();
                handle->Stream = std::move(fileStream);
                handle->HintPath = filename;
            }
        }
    }
    return handle;
}

bool TitleSequenceSave(const TitleSequence& seq)
{
    try
    {
        auto script = LegacyScriptWrite(seq);
        if (seq.IsZip)
        {
            auto fdata = std::vector<uint8_t>(script.begin(), script.end());
            auto zip = Zip::Open(seq.Path, ZIP_ACCESS::WRITE);
            zip->SetFileData("script.txt", std::move(fdata));
        }
        else
        {
            auto scriptPath = Path::Combine(seq.Path, "script.txt");
            File::WriteAllBytes(scriptPath, script.data(), script.size());
        }
        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

bool TitleSequenceAddPark(TitleSequence& seq, const utf8* path, const utf8* name)
{
    // Get new save index
    auto it = std::find(seq.Saves.begin(), seq.Saves.end(), path);
    if (it == seq.Saves.end())
    {
        seq.Saves.push_back(name);
    }

    if (seq.IsZip)
    {
        try
        {
            auto fdata = File::ReadAllBytes(path);
            auto zip = Zip::TryOpen(seq.Path, ZIP_ACCESS::WRITE);
            if (zip == nullptr)
            {
                Console::Error::WriteLine("Unable to open '%s'", seq.Path.c_str());
                return false;
            }
            zip->SetFileData(name, std::move(fdata));
        }
        catch (const std::exception& ex)
        {
            Console::Error::WriteLine(ex.what());
        }
    }
    else
    {
        // Determine destination path
        auto dstPath = Path::Combine(seq.Path, name);
        if (!File::Copy(path, dstPath, true))
        {
            Console::Error::WriteLine("Unable to copy '%s' to '%s'", path, dstPath.c_str());
            return false;
        }
    }
    return true;
}

bool TitleSequenceRenamePark(TitleSequence& seq, size_t index, const utf8* name)
{
    Guard::Assert(index < seq.Saves.size(), GUARD_LINE);

    auto& oldRelativePath = seq.Saves[index];
    if (seq.IsZip)
    {
        auto zip = Zip::TryOpen(seq.Path, ZIP_ACCESS::WRITE);
        if (zip == nullptr)
        {
            Console::Error::WriteLine("Unable to open '%s'", seq.Path.c_str());
            return false;
        }
        zip->RenameFile(oldRelativePath, name);
    }
    else
    {
        auto srcPath = Path::Combine(seq.Path, oldRelativePath);
        auto dstPath = Path::Combine(seq.Path, name);
        if (!File::Move(srcPath, dstPath))
        {
            Console::Error::WriteLine("Unable to move '%s' to '%s'", srcPath.c_str(), dstPath.c_str());
            return false;
        }
    }
    seq.Saves[index] = name;
    return true;
}

bool TitleSequenceRemovePark(TitleSequence& seq, size_t index)
{
    Guard::Assert(index < seq.Saves.size(), GUARD_LINE);

    // Delete park file
    auto& relativePath = seq.Saves[index];
    if (seq.IsZip)
    {
        auto zip = Zip::TryOpen(seq.Path, ZIP_ACCESS::WRITE);
        if (zip == nullptr)
        {
            Console::Error::WriteLine("Unable to open '%s'", seq.Path.c_str());
            return false;
        }
        zip->DeleteFile(relativePath);
    }
    else
    {
        auto absolutePath = Path::Combine(seq.Path, relativePath);
        if (!File::Delete(absolutePath))
        {
            Console::Error::WriteLine("Unable to delete '%s'", absolutePath.c_str());
            return false;
        }
    }

    // Remove from sequence
    seq.Saves.erase(seq.Saves.begin() + index);

    // Update load commands
    for (auto& command : seq.Commands)
    {
        if (command.Type == TitleScript::Load)
        {
            if (command.SaveIndex == index)
            {
                // Park no longer exists, so reset load command to invalid
                command.SaveIndex = SAVE_INDEX_INVALID;
            }
            else if (command.SaveIndex > index)
            {
                // Park index will have shifted by -1
                command.SaveIndex--;
            }
        }
    }

    return true;
}

static std::vector<std::string> GetSaves(const std::string& directory)
{
    std::vector<std::string> saves;

    auto pattern = Path::Combine(directory, "*.sc6;*.sv6;*.park;*.sv4;*.sc4");
    auto scanner = Path::ScanDirectory(pattern, true);
    while (scanner->Next())
    {
        const utf8* path = scanner->GetPathRelative();
        saves.push_back(path);
    }
    return saves;
}

static std::vector<std::string> GetSaves(IZipArchive* zip)
{
    std::vector<std::string> saves;
    size_t numFiles = zip->GetNumFiles();
    for (size_t i = 0; i < numFiles; i++)
    {
        auto name = zip->GetFileName(i);
        auto ext = Path::GetExtension(name);
        if (String::Equals(ext, ".sv6", true) || String::Equals(ext, ".sc6", true) || String::Equals(ext, ".park", true))
        {
            saves.push_back(std::move(name));
        }
    }
    return saves;
}

static std::vector<TitleCommand> LegacyScriptRead(const std::vector<uint8_t>& script, std::vector<std::string> saves)
{
    std::vector<TitleCommand> commands;
    auto fs = OpenRCT2::MemoryStream(script.data(), script.size());
    do
    {
        char parts[3 * 128], *token, *part1, *part2;
        LegacyScriptGetLine(&fs, parts);

        token = &parts[0 * 128];
        part1 = &parts[1 * 128];
        part2 = &parts[2 * 128];
        TitleCommand command = {};
        command.Type = TitleScript::Undefined;

        if (token[0] != 0)
        {
            if (_stricmp(token, "LOAD") == 0)
            {
                command.Type = TitleScript::Load;
                command.SaveIndex = SAVE_INDEX_INVALID;
                for (size_t i = 0; i < saves.size(); i++)
                {
                    if (String::Equals(part1, saves[i], true))
                    {
                        command.SaveIndex = static_cast<uint8_t>(i);
                        break;
                    }
                }
            }
            else if (_stricmp(token, "LOCATION") == 0)
            {
                command.Type = TitleScript::Location;
                command.X = atoi(part1) & 0xFF;
                command.Y = atoi(part2) & 0xFF;
            }
            else if (_stricmp(token, "ROTATE") == 0)
            {
                command.Type = TitleScript::Rotate;
                command.Rotations = atoi(part1) & 0xFF;
            }
            else if (_stricmp(token, "ZOOM") == 0)
            {
                command.Type = TitleScript::Zoom;
                command.Zoom = atoi(part1) & 0xFF;
            }
            else if (_stricmp(token, "SPEED") == 0)
            {
                command.Type = TitleScript::Speed;
                command.Speed = std::max(1, std::min(4, atoi(part1) & 0xFF));
            }
            else if (_stricmp(token, "FOLLOW") == 0)
            {
                command.Type = TitleScript::Follow;
                command.SpriteIndex = atoi(part1) & 0xFFFF;
                safe_strcpy(command.SpriteName, part2, USER_STRING_MAX_LENGTH);
            }
            else if (_stricmp(token, "WAIT") == 0)
            {
                command.Type = TitleScript::Wait;
                command.Milliseconds = atoi(part1) & 0xFFFF;
            }
            else if (_stricmp(token, "RESTART") == 0)
            {
                command.Type = TitleScript::Restart;
            }
            else if (_stricmp(token, "END") == 0)
            {
                command.Type = TitleScript::End;
            }
            else if (_stricmp(token, "LOADSC") == 0)
            {
                command.Type = TitleScript::LoadSc;
                safe_strcpy(command.Scenario, part1, sizeof(command.Scenario));
            }
        }
        if (command.Type != TitleScript::Undefined)
        {
            commands.push_back(std::move(command));
        }
    } while (fs.GetPosition() < fs.GetLength());
    return commands;
}

static void LegacyScriptGetLine(OpenRCT2::IStream* stream, char* parts)
{
    for (int32_t i = 0; i < 3; i++)
    {
        parts[i * 128] = 0;
    }
    int32_t part = 0;
    int32_t cindex = 0;
    int32_t whitespace = 1;
    int32_t comment = 0;
    bool load = false;
    bool sprite = false;
    for (; part < 3;)
    {
        int32_t c = 0;
        if (stream->TryRead(&c, 1) != 1)
        {
            c = EOF;
        }
        if (c == '\n' || c == '\r' || c == EOF)
        {
            parts[part * 128 + cindex] = 0;
            return;
        }
        if (c == '#')
        {
            parts[part * 128 + cindex] = 0;
            comment = 1;
        }
        else if (c == ' ' && !comment && !load && (!sprite || part != 2))
        {
            if (!whitespace)
            {
                if (part == 0
                    && ((cindex == 4 && _strnicmp(parts, "LOAD", 4) == 0)
                        || (cindex == 6 && _strnicmp(parts, "LOADSC", 6) == 0)))
                {
                    load = true;
                }
                else if (part == 0 && cindex == 6 && _strnicmp(parts, "FOLLOW", 6) == 0)
                {
                    sprite = true;
                }
                parts[part * 128 + cindex] = 0;
                part++;
                cindex = 0;
            }
        }
        else if (!comment)
        {
            whitespace = 0;
            if (cindex < 127)
            {
                parts[part * 128 + cindex] = c;
                cindex++;
            }
            else
            {
                parts[part * 128 + cindex] = 0;
                part++;
                cindex = 0;
            }
        }
    }
}

static std::vector<uint8_t> ReadScriptFile(const std::string& path)
{
    std::vector<uint8_t> result;
    try
    {
        auto fs = OpenRCT2::FileStream(path, OpenRCT2::FILE_MODE_OPEN);
        auto size = static_cast<size_t>(fs.GetLength());
        result.resize(size);
        fs.Read(result.data(), size);
    }
    catch (const std::exception&)
    {
        result.clear();
        result.shrink_to_fit();
    }
    return result;
}

static std::string LegacyScriptWrite(const TitleSequence& seq)
{
    utf8 buffer[128];
    auto sb = StringBuilder(128);

    sb.Append("# SCRIPT FOR ");
    sb.Append(seq.Name.c_str());
    sb.Append("\n");
    for (const auto& command : seq.Commands)
    {
        switch (command.Type)
        {
            case TitleScript::Load:
                if (command.SaveIndex < seq.Saves.size())
                {
                    sb.Append("LOAD ");
                    sb.Append(seq.Saves[command.SaveIndex].c_str());
                }
                else
                {
                    sb.Append("LOAD <No save file>");
                }
                break;
            case TitleScript::LoadSc:
                if (command.Scenario[0] == '\0')
                {
                    sb.Append("LOADSC <No scenario name>");
                }
                else
                {
                    sb.Append("LOADSC ");
                    sb.Append(command.Scenario);
                }
                break;
            case TitleScript::Undefined:
                break;
            case TitleScript::Loop:
                break;
            case TitleScript::EndLoop:
                break;
            case TitleScript::Location:
                String::Format(buffer, sizeof(buffer), "LOCATION %u %u", command.X, command.Y);
                sb.Append(buffer);
                break;
            case TitleScript::Rotate:
                String::Format(buffer, sizeof(buffer), "ROTATE %u", command.Rotations);
                sb.Append(buffer);
                break;
            case TitleScript::Zoom:
                String::Format(buffer, sizeof(buffer), "ZOOM %u", command.Zoom);
                sb.Append(buffer);
                break;
            case TitleScript::Follow:
                String::Format(buffer, sizeof(buffer), "FOLLOW %u ", command.SpriteIndex);
                sb.Append(buffer);
                sb.Append(command.SpriteName);
                break;
            case TitleScript::Speed:
                String::Format(buffer, sizeof(buffer), "SPEED %u", command.Speed);
                sb.Append(buffer);
                break;
            case TitleScript::Wait:
                String::Format(buffer, sizeof(buffer), "WAIT %u", command.Milliseconds);
                sb.Append(buffer);
                break;
            case TitleScript::Restart:
                sb.Append("RESTART");
                break;
            case TitleScript::End:
                sb.Append("END");
        }
        sb.Append("\n");
    }

    return sb.GetBuffer();
}

bool TitleSequenceIsLoadCommand(const TitleCommand& command)
{
    switch (command.Type)
    {
        case TitleScript::Load:
        case TitleScript::LoadSc:
            return true;
        default:
            return false;
    }
}
