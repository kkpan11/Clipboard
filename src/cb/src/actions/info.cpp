/*  The Clipboard Project - Cut, copy, and paste anything, anytime, anywhere, all from the terminal.
    Copyright (C) 2023 Jackson Huff and other contributors on GitHub.com
    SPDX-License-Identifier: GPL-3.0-or-later
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.*/
#include "../clipboard.hpp"

#if defined(_WIN32) || defined(_WIN64)
#include <fcntl.h>
#include <format>
#include <io.h>
#endif

#if defined(UNIX_OR_UNIX_LIKE)
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

namespace PerformAction {

void info() {
    stopIndicator();
    fprintf(stderr, "%s", formatColors("[info]┏━━[inverse] ").data());
    fprintf(stderr, clipboard_name_message().data(), clipboard_name.data());
    fprintf(stderr, "%s", formatColors(" [noinverse][info]━").data());
    int columns = thisTerminalSize().columns - ((columnLength(clipboard_name_message) - 2) + clipboard_name.length() + 7);
    for (int i = 0; i < columns; i++)
        fprintf(stderr, "━");
    fprintf(stderr, "%s", formatColors("┓[blank]\n").data());

    auto endbarStr = generatedEndbar();
    auto endbar = endbarStr.data();

    // creation date
#if defined(UNIX_OR_UNIX_LIKE)
    struct stat info;
    stat(path.string().data(), &info);
    std::string time(std::ctime(&info.st_ctime));
    std::erase(time, '\n');
    fprintf(stderr, formatColors("[info]%s┃ Created [help]%s[blank]\n").data(), endbar, time.data());
#elif defined(_WIN32) || defined(_WIN64)
    fprintf(stderr, formatColors("[info]┃ Created [help]n/a[blank]\n").data());
#endif

#if defined(UNIX_OR_UNIX_LIKE)
    time_t latest = 0;
    for (const auto& entry : fs::recursive_directory_iterator(path.data)) {
        struct stat info;
        stat(entry.path().string().data(), &info);
        if (info.st_ctime > latest) latest = info.st_ctime;
    }
    time = std::ctime(&latest);
    std::erase(time, '\n');
    fprintf(stderr, formatColors("[info]%s┃ Content last changed [help]%s[blank]\n").data(), endbar, time.data());
#elif defined(_WIN32) || defined(_WIN64)
    fprintf(stderr, formatColors("[info]┃ Content last changed [help]%s[blank]\n").data(), std::format("{}", fs::last_write_time(path)).data());
#endif

    fprintf(stderr, formatColors("[info]%s┃ Stored in [help]%s[blank]\n").data(), endbar, path.string().data());

#if defined(UNIX_OR_UNIX_LIKE)
    struct passwd* pw = getpwuid(info.st_uid);
    fprintf(stderr, formatColors("[info]%s┃ Owned by [help]%s[blank]\n").data(), endbar, pw->pw_name);
#elif defined(_WIN32) || defined(_WIN64)
    fprintf(stderr, formatColors("[info]┃ Owned by [help]n/a[blank]\n").data());
#endif

    fprintf(stderr, formatColors("[info]%s┃ Persistent? [help]%s[blank]\n").data(), endbar, path.is_persistent ? "Yes" : "No");

    auto totalEntries = path.totalEntries();
    auto totalSize = totalDirectorySize(path);
    auto spaceAvailable = fs::space(path).available;

    fprintf(stderr, formatColors("[info]%s┃ Total entries: [help]%zu[blank]\n").data(), endbar, totalEntries);
    fprintf(stderr, formatColors("[info]%s┃ Total clipboard size: [help]%s[blank]\n").data(), endbar, formatBytes(totalSize).data());
    fprintf(stderr, formatColors("[info]%s┃ Total space remaining: [help]%s[blank]\n").data(), endbar, formatBytes(spaceAvailable).data());
    fprintf(stderr, formatColors("[info]%s┃ Approx. entries remaining: [help]%s[blank]\n").data(), endbar, formatNumbers(spaceAvailable / (totalSize / totalEntries)).data());

    if (path.holdsRawDataInCurrentEntry()) {
        fprintf(stderr, formatColors("[info]%s┃ Content size: [help]%s[blank]\n").data(), endbar, formatBytes(fs::file_size(path.data.raw)).data());
        fprintf(stderr, formatColors("[info]%s┃ Content type: [help]%s[blank]\n").data(), endbar, inferMIMEType(fileContents(path.data.raw).value()).value_or("text/plain").data());
    } else {
        size_t files = 0;
        size_t directories = 0;
        fprintf(stderr, formatColors("[info]%s┃ Content size: [help]%s[blank]\n").data(), endbar, formatBytes(totalDirectorySize(path.data)).data());
        for (const auto& entry : fs::directory_iterator(path.data))
            entry.is_directory() ? directories++ : files++;
        fprintf(stderr, formatColors("[info]%s┃ Files: [help]%zu[blank]\n").data(), endbar, files);
        fprintf(stderr, formatColors("[info]%s┃ Directories: [help]%zu[blank]\n").data(), endbar, directories);
    }

    if (!available_mimes.empty()) {
        fprintf(stderr, formatColors("[info]%s┃ Available types from GUI: [help]").data(), endbar);
        for (const auto& mime : available_mimes) {
            fprintf(stderr, "%s", mime.data());
            if (mime != available_mimes.back()) fprintf(stderr, ", ");
        }
        fprintf(stderr, "%s", formatColors("[blank]\n").data());
    }
    fprintf(stderr, formatColors("[info]%s┃ Content cut? [help]%s[blank]\n").data(), endbar, fs::exists(path.metadata.originals) ? "Yes" : "No");

    fprintf(stderr, formatColors("[info]%s┃ Locked by another process? [help]%s[blank]\n").data(), endbar, path.isLocked() ? "Yes" : "No");

    if (path.isLocked()) {
        fprintf(stderr, formatColors("[info]%s┃ Locked by process with pid [help]%s[blank]\n").data(), endbar, fileContents(path.metadata.lock).value().data());
    }

    if (fs::exists(path.metadata.notes))
        fprintf(stderr, formatColors("[info]%s┃ Note: [help]%s[blank]\n").data(), endbar, fileContents(path.metadata.notes).value().data());
    else
        fprintf(stderr, formatColors("[info]%s┃ There is no note for this clipboard.[blank]\n").data(), endbar);

    if (path.holdsIgnoreRegexes()) {
        fprintf(stderr, formatColors("[info]%s┃ Ignore regexes: [help]").data(), endbar);
        auto regexes = fileLines(path.metadata.ignore);
        for (const auto& regex : regexes) {
            fprintf(stderr, "%s", regex.data());
            if (regex != regexes.back()) fprintf(stderr, ", ");
        }
        fprintf(stderr, "%s", formatColors("[blank]\n").data());
    } else
        fprintf(stderr, formatColors("[info]%s┃ There are no ignore regexes for this clipboard.[blank]\n").data(), endbar);

    if (fs::exists(path.metadata.ignore_secret)) {
        // list only how many ignore secrets there are
        auto secrets = fileLines(path.metadata.ignore_secret);
        fprintf(stderr, formatColors("[info]%s┃ There are %zu ignore secrets for this clipboard.[blank]\n").data(), endbar, secrets.size());
    } else {
        fprintf(stderr, formatColors("[info]%s┃ There are no ignore secrets for this clipboard.[blank]\n").data(), endbar);
    }

    if (fs::exists(path.metadata.script)) {
        auto script = fileContents(path.metadata.script).value();
        if (script.size() > 50) {
            fprintf(stderr, formatColors("[info]%s┃ Script preview: [help]%s...[blank]\n").data(), endbar, makeControlCharactersVisible(removeExcessWhitespace(script.substr(0, 50))).data());
        } else {
            fprintf(stderr, formatColors("[info]%s┃ Script preview: [help]%s[blank]\n").data(), endbar, makeControlCharactersVisible(removeExcessWhitespace(script)).data());
        }
        auto lines  = fileLines(path.metadata.script_config, true);
        if (lines[0] != "") {
            fprintf(stderr, formatColors("[info]%s┃ Script actions: [help]").data(), endbar);
            for (const auto& action : regexSplit(lines[0], std::regex(" "))) {
                fprintf(stderr, "%s", action.data());
                if (action != regexSplit(lines[0], std::regex(" ")).back()) fprintf(stderr, ", ");
            }
            fprintf(stderr, "%s", formatColors("[blank]\n").data());
        } else {
            fprintf(stderr, formatColors("[info]%s┃ This script is set to run for all actions.[blank]\n").data(), endbar);
        }
        if (lines.size() > 1 && lines[1] != "") {
            fprintf(stderr, formatColors("[info]%s┃ Script timings: [help]").data(), endbar);
            for (const auto& timing : regexSplit(lines[1], std::regex(" "))) {
                fprintf(stderr, "%s", timing.data());
                if (timing != regexSplit(lines[1], std::regex(" ")).back()) fprintf(stderr, ", ");
            }
            fprintf(stderr, "%s", formatColors("[blank]\n").data());
        }  else {
            fprintf(stderr, formatColors("[info]%s┃ This script is set to run before and after all actions.[blank]\n").data(), endbar);
        }
    } else {
        fprintf(stderr, formatColors("[info]%s┃ There is no script for this clipboard.[blank]\n").data(), endbar);
    }

    fprintf(stderr, "%s", formatColors("[info]┗").data());
    int cols = thisTerminalSize().columns;
    for (int i = 0; i < cols - 2; i++)
        fprintf(stderr, "━");
    fprintf(stderr, "%s", formatColors("┛[blank]\n").data());
}

void infoJSON() {
    printf("{\n");

    printf("    \"name\": \"%s\",\n", clipboard_name.data());

#if defined(UNIX_OR_UNIX_LIKE)
    struct stat info;
    stat(path.string().data(), &info);
    std::string time(std::ctime(&info.st_ctime));
    std::erase(time, '\n');
    printf("    \"created\": \"%s\",\n", time.data());
#elif defined(_WIN32) || defined(_WIN64)
    printf("    \"created\": \"n/a\",\n");
#endif

#if defined(UNIX_OR_UNIX_LIKE)
    time_t latest = 0;
    for (const auto& entry : fs::recursive_directory_iterator(path.data)) {
        struct stat info;
        stat(entry.path().string().data(), &info);
        if (info.st_ctime > latest) latest = info.st_ctime;
    }
    time = std::ctime(&latest);
    std::erase(time, '\n');
    printf("    \"contentLastChanged\": \"%s\",\n", time.data());
#elif defined(_WIN32) || defined(_WIN64)
    printf("    \"contentLastChanged\": \"%s\",\n", std::format("{}", fs::last_write_time(path)).data());
#endif

    printf("    \"path\": \"%s\",\n", JSONescape(path.string()).data());

#if defined(UNIX_OR_UNIX_LIKE)
    struct passwd* pw = getpwuid(getuid());
    printf("    \"owner\": \"%s\",\n", pw->pw_name);
#elif defined(_WIN32) || defined(_WIN64)
    printf("    \"owner\": \"n/a\",\n");
#endif

    auto totalEntries = path.totalEntries();
    auto totalSize = totalDirectorySize(path);
    auto spaceAvailable = fs::space(path).available;

    printf("    \"isPersistent\": %s,\n", path.is_persistent ? "true" : "false");
    printf("    \"totalEntries\": %zu,\n", totalEntries);
    printf("    \"totalBytesUsed\": %zu,\n", totalSize);
    printf("    \"totalBytesRemaining\": %zu,\n", spaceAvailable);
    printf("    \"approxEntriesRemaining\": %zu,\n", spaceAvailable / (totalSize / totalEntries));

    if (path.holdsRawDataInCurrentEntry()) {
        printf("    \"contentBytes\": %zu,\n", fs::file_size(path.data.raw));
        printf("    \"contentType\": \"%s\",\n", inferMIMEType(fileContents(path.data.raw).value()).value_or("text/plain").data());
    } else {
        size_t files = 0;
        size_t directories = 0;
        for (const auto& entry : fs::directory_iterator(path.data))
            entry.is_directory() ? directories++ : files++;
        printf("    \"contentBytes\": %zu,\n", totalDirectorySize(path.data));
        printf("    \"files\": %zu,\n", files);
        printf("    \"directories\": %zu,\n", directories);
    }

    if (!available_mimes.empty()) {
        printf("    \"availableTypes\": [");
        for (const auto& mime : available_mimes)
            printf("\"%s\"%s", mime.data(), mime != available_mimes.back() ? ", " : "");
        printf("],\n");
    }

    printf("    \"contentCut\": %s,\n", fs::exists(path.metadata.originals) ? "true" : "false");

    printf("    \"locked\": %s,\n", path.isLocked() ? "true" : "false");
    if (path.isLocked()) printf("    \"lockedBy\": \"%s\",\n", fileContents(path.metadata.lock).value().data());

    if (fs::exists(path.metadata.notes))
        printf("    \"note\": \"%s\",\n", JSONescape(fileContents(path.metadata.notes).value()).data());
    else
        printf("    \"note\": null,\n");

    if (path.holdsIgnoreRegexes()) {
        printf("    \"ignoreRegexes\": [");
        auto regexes = fileLines(path.metadata.ignore);
        for (const auto& regex : regexes)
            printf("\"%s\"%s", JSONescape(regex).data(), regex != regexes.back() ? ", " : "");
        printf("]\n");
    } else {
        printf("    \"ignoreRegexes\": []\n");
    }

    printf("}\n");
}

} // namespace PerformAction